/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 14.11.2015
 *
 * Linux layer for project codename "4ed"
 *
 */

// TOP

#include "4ed_config.h"


#include "4ed_meta.h"

#define FCPP_FORBID_MALLOC

#include "4cpp_types.h"
#define FCPP_STRING_IMPLEMENTATION
#include "4coder_string.h"

#include "4ed_mem.cpp"
#include "4ed_math.cpp"

#include "4coder_default_bindings.cpp"

#undef exec_command
#undef exec_command_keep_stack
#undef clear_parameters

#include "4ed_system.h"
#include "4ed_rendering.h"
#include "4ed.h"

#include <stdio.h>
#include <memory.h>
#include <math.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xfixes.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <xmmintrin.h>
#include <linux/fs.h>
//#include <X11/extensions/XInput2.h>
#include <linux/input.h>
#include <time.h>
#include <dlfcn.h>
#include <unistd.h>

#include "4ed_internal.h"
#include "4ed_linux_keyboard.cpp"
#include "system_shared.h"

#include <stdlib.h>
#include <locale.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ucontext.h>

//#define FRED_USE_FONTCONFIG 1

#if FRED_USE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

enum {
    LINUX_4ED_EVENT_X11,
    LINUX_4ED_EVENT_FILE,
    LINUX_4ED_EVENT_STEP,
    LINUX_4ED_EVENT_STEP_TIMER,
    LINUX_4ED_EVENT_CLI,
};

struct Linux_Coroutine {
	Coroutine coroutine;
	Linux_Coroutine *next;
	ucontext_t ctx, yield_ctx;
    stack_t stack;
	b32 done;
};

struct Thread_Context{
    u32 job_id;
    b32 running;
    
    Work_Queue *queue;
    u32 id;
    pthread_t handle;
};

struct Thread_Group{
    Thread_Context *threads;
    i32 count;
};

struct Linux_Vars{
    Display *XDisplay;
    Window XWindow;
    Render_Target target;

    XIM input_method;
    XIMStyle input_style;
    XIC input_context;

    Key_Input_Data key_data;
    Mouse_State mouse_data;

    String clipboard_contents;
    String clipboard_outgoing;
    b32 new_clipboard;

    Atom atom_CLIPBOARD;
    Atom atom_UTF8_STRING;
    Atom atom_NET_WM_STATE;
    Atom atom_NET_WM_STATE_MAXIMIZED_HORZ;
    Atom atom_NET_WM_STATE_MAXIMIZED_VERT;
    Atom atom_NET_WM_PING;
    Atom atom_WM_DELETE_WINDOW;

    b32 has_xfixes;
    int xfixes_selection_event;

    int epoll;

    int step_timer_fd;
    int step_event_fd;
    int x11_fd;
    int inotify_fd;

    u64 last_step;

    b32 keep_running;

    Application_Mouse_Cursor cursor;

#if FRED_USE_FONTCONFIG
    FcConfig *fontconfig;
#endif

    void *app_code;
    void *custom;

    Thread_Memory *thread_memory;
    Thread_Group groups[THREAD_GROUP_COUNT];
    sem_t thread_semaphores[THREAD_GROUP_COUNT];
    pthread_mutex_t locks[LOCK_COUNT];

    Plat_Settings settings;
    System_Functions *system;
    App_Functions app;
    Custom_API custom_api;
    b32 first;
    b32 redraw;
    b32 vsync;

#if FRED_INTERNAL
    Sys_Bubble internal_bubble;
#endif

    Font_Load_System fnt;

    Linux_Coroutine coroutine_data[18];
    Linux_Coroutine *coroutine_free;
};

#define LINUX_MAX_PASTE_CHARS 0x10000L
#define FPS 60L
#define frame_useconds (1000000UL / FPS)

#if FRED_INTERNAL
#define LINUX_FN_DEBUG(fmt, ...) do { \
        fprintf(stderr, "%s: " fmt "\n", __func__, ##__VA_ARGS__); \
    } while(0)
#else
#define LINUX_FN_DEBUG(fmt, ...)
#endif

globalvar Linux_Vars linuxvars;
globalvar Application_Memory memory_vars;
globalvar Exchange exchange_vars;

internal void LinuxScheduleStep(void);

#define LinuxGetMemory(size) LinuxGetMemory_(size, __LINE__, __FILE__)

internal void*
LinuxGetMemory_(i32 size, i32 line_number, char *file_name){
    void *result = 0;
    
    Assert(size != 0);
    
#if FRED_INTERNAL
    Sys_Bubble *bubble;
    
    result = malloc(size + sizeof(Sys_Bubble));
    bubble = (Sys_Bubble*)result;
    bubble->flags = MEM_BUBBLE_SYS_DEBUG;
    bubble->line_number = line_number;
    bubble->file_name = file_name;
    bubble->size = size;
    
    // TODO(allen): make Sys_Bubble list thread safe
    insert_bubble(&linuxvars.internal_bubble, bubble);
    result = bubble + 1;
    
#else
    size_t real_size = size + sizeof(size_t);
    result = mmap(0, real_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if(result == MAP_FAILED){
        perror("mmap");
        result = NULL;
    } else {
        memcpy(result, &real_size, sizeof(size_t));
        result = (char*)result + sizeof(size_t);
    }
#endif
    
    return(result);
}

internal void
LinuxFreeMemory(void *block){
    if (block){
#if FRED_INTERNAL
        Sys_Bubble *bubble;
        
        bubble = (Sys_Bubble*)block;
        --bubble;
        Assert((bubble->flags & MEM_BUBBLE_DEBUG_MASK) == MEM_BUBBLE_SYS_DEBUG);

        // TODO(allen): make Sys_Bubble list thread safe
        remove_bubble(bubble);
        
        free(bubble);
#else
        block = (char*)block - sizeof(size_t);
        size_t len = *(size_t*)block;
        munmap(block, len);
#endif
    }
}

internal Partition
LinuxScratchPartition(i32 size){
    Partition part;
    void *data;
    data = LinuxGetMemory(size);
    part = partition_open(data, size);
    return(part);
}

internal void
LinuxScratchPartitionGrow(Partition *part, i32 new_size){
    void *data;
    if (new_size > part->max){
        data = LinuxGetMemory(new_size);
        memcpy(data, part->base, part->pos);
        LinuxFreeMemory(part->base);
        part->base = (u8*)data;
    }
}

internal void
LinuxScratchPartitionDouble(Partition *part){
    LinuxScratchPartitionGrow(part, part->max*2);
}

internal
Sys_Get_Memory_Sig(system_get_memory_){
    return(LinuxGetMemory_(size, line_number, file_name));
}

internal
Sys_Free_Memory_Sig(system_free_memory){
    LinuxFreeMemory(block);
}

internal void
LinuxStringDup(String* str, void* data, size_t size){
    if(str->memory_size < size){
        if(str->str){
            LinuxFreeMemory(str->str);
        }
        str->memory_size = size;
        str->str = (char*)LinuxGetMemory(size);
        //TODO(inso): handle alloc failure case
    }

    str->size = size;
    memcpy(str->str, data, size);
}

#if defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || _POSIX_C_SOURCE >= 200809L || _XOPEN_SOURCE >= 700
    #define OLD_STAT_NANO_TIME 0
#else
    #define OLD_STAT_NANO_TIME 1
#endif

Sys_File_Time_Stamp_Sig(system_file_time_stamp){
    struct stat info = {};
    u64 microsecond_timestamp = 0;

    if(stat(filename, &info) == 0){
#if OLD_STAT_NANO_TIME
        microsecond_timestamp =
            (info.st_mtime * UINT64_C(1000000)) +
            (info.st_mtimensec / UINT64_C(1000));
#else
        microsecond_timestamp =
            (info.st_mtim.tv_sec * UINT64_C(1000000)) +
            (info.st_mtim.tv_nsec / UINT64_C(1000));
#endif
    }

    LINUX_FN_DEBUG("%s = %" PRIu64, filename, microsecond_timestamp);

    return(microsecond_timestamp);
}

// TODO(allen): DOES THIS AGREE WITH THE FILESTAMP TIMES?
// NOTE(inso): I changed it to CLOCK_REALTIME, which should agree with file times
Sys_Time_Sig(system_time){
    struct timespec spec;
    u64 result;
    
    clock_gettime(CLOCK_REALTIME, &spec);
    result = (spec.tv_sec * UINT64_C(1000000)) + (spec.tv_nsec / UINT64_C(1000));

    //LINUX_FN_DEBUG("ts: %" PRIu64, result);

    return(result);
}

Sys_Set_File_List_Sig(system_set_file_list){
    DIR *d;
    struct dirent *entry;
    char *fname, *cursor, *cursor_start;
    File_Info *info_ptr;
    i32 count, file_count, size, required_size;

    if(directory.size <= 0){
        if(!directory.str){
            system_free_memory(file_list->block);
            file_list->block = 0;
            file_list->block_size = 0;
        }
        file_list->infos = 0;
        file_list->count = 0;
        return;
    }

    char* dir = (char*) alloca(directory.size + 1);
    memcpy(dir, directory.str, directory.size);
    dir[directory.size] = 0;

    LINUX_FN_DEBUG("%s", dir);

    d = opendir(dir);
    if (d){
        count = 0;
        file_count = 0;
        for (entry = readdir(d);
             entry != 0;
             entry = readdir(d)){
            fname = entry->d_name;
            if(fname[0] == '.' && (fname[1] == 0 || (fname[1] == '.' && fname[2] == 0))){
                continue;
            }
            ++file_count;            
            for (size = 0; fname[size]; ++size);
            count += size + 1;
        }

        required_size = count + file_count * sizeof(File_Info);
        if (file_list->block_size < required_size){
            system_free_memory(file_list->block);
            file_list->block = system_get_memory(required_size);
        }
        
        file_list->infos = (File_Info*)file_list->block;
        cursor = (char*)(file_list->infos + file_count);
        
        rewinddir(d);
        info_ptr = file_list->infos;
        for (entry = readdir(d);
            entry != 0;
            entry = readdir(d)){
            fname = entry->d_name;
            if(fname[0] == '.' && (fname[1] == 0 || (fname[1] == '.' && fname[2] == 0))){
                continue;
            }
            cursor_start = cursor;
            for (; *fname; ) *cursor++ = *fname++;

#ifdef _DIRENT_HAVE_D_TYPE
            if(entry->d_type != DT_UNKNOWN){
                info_ptr->folder = entry->d_type == DT_DIR;
            } else
#endif
            {
                struct stat st;
                if(lstat(entry->d_name, &st) != -1){
                    info_ptr->folder = S_ISDIR(st.st_mode);
                } else {
                    info_ptr->folder = 0;
                }
            }

            info_ptr->filename.str = cursor_start;
            info_ptr->filename.size = (i32)(cursor - cursor_start);
            *cursor++ = 0;
            info_ptr->filename.memory_size = info_ptr->filename.size + 1;
            ++info_ptr;
        }

        file_list->count = file_count;

        closedir(d);
    } else {
        system_free_memory(file_list->block);
        file_list->block = 0;
        file_list->block_size = 0;
        file_list->infos = 0;
        file_list->count = 0;
    }
}

internal
Sys_File_Track_Sig(system_file_track){
    if(filename.size <= 0 || !filename.str) return;

    char* fname = (char*) alloca(filename.size+1);
    memcpy(fname, filename.str, filename.size);
    fname[filename.size] = 0;

    int wd = inotify_add_watch(linuxvars.inotify_fd, fname, IN_ALL_EVENTS);
    if(wd == -1){
        perror("inotify_add_watch");
    } else {
        printf("watch %s\n", fname);
        // TODO: store the wd somewhere so Untrack can use it
    }
}

internal
Sys_File_Untrack_Sig(system_file_untrack){
    //TODO
    //    inotify_rm_watch(...)
}

static_assert(
    (sizeof(((struct stat*)0)->st_dev) + 
    sizeof(((struct stat*)0)->st_ino)) <= 
    sizeof(Unique_Hash),
    "Unique_Hash too small"
);

Sys_File_Unique_Hash_Sig(system_file_unique_hash){
    Unique_Hash result = {};
    struct stat st = {};
    *success = 0;

    if(stat(filename.str, &st) == 0){
        memcpy(&result, &st.st_dev, sizeof(st.st_dev));
        memcpy((char*)&result + sizeof(st.st_dev), &st.st_ino, sizeof(st.st_ino));
        *success = 1;
    }

//    LINUX_FN_DEBUG("%s = %ld:%ld", filename.str, (long)st.st_dev, (long)st.st_ino);

    return result;
}

Sys_Post_Clipboard_Sig(system_post_clipboard){
    LinuxStringDup(&linuxvars.clipboard_outgoing, str.str, str.size);
    XSetSelectionOwner(linuxvars.XDisplay, linuxvars.atom_CLIPBOARD, linuxvars.XWindow, CurrentTime);
}

internal Linux_Coroutine*
LinuxAllocCoroutine(){
    Linux_Coroutine *result = linuxvars.coroutine_free;
    Assert(result != 0);
	if(getcontext(&result->ctx) == -1){
		perror("getcontext");
	}
    result->ctx.uc_stack = result->stack;
    linuxvars.coroutine_free = result->next;
    return(result);
}

internal void
LinuxFreeCoroutine(Linux_Coroutine *data){
    data->next = linuxvars.coroutine_free;
    linuxvars.coroutine_free = data;
}

internal void
LinuxCoroutineMain(void *arg_){
    Linux_Coroutine *c = (Linux_Coroutine*)arg_;
    c->coroutine.func(&c->coroutine);
    c->done = 1;
    LinuxFreeCoroutine(c);
    setcontext((ucontext_t*)c->coroutine.yield_handle);
}

static_assert(sizeof(Plat_Handle) >= sizeof(ucontext_t*), "Plat handle not big enough");

internal
Sys_Create_Coroutine_Sig(system_create_coroutine){
    Linux_Coroutine *c = LinuxAllocCoroutine();
    c->done = 0;
    
	makecontext(&c->ctx, (void (*)())LinuxCoroutineMain, 1, &c->coroutine);
   
    *(ucontext_t**)&c->coroutine.plat_handle = &c->ctx;
    c->coroutine.func = func;
    
    return(&c->coroutine);
}

internal
Sys_Launch_Coroutine_Sig(system_launch_coroutine){
    Linux_Coroutine *c = (Linux_Coroutine*)coroutine;
    ucontext_t* ctx = *(ucontext**)&coroutine->plat_handle;

    coroutine->yield_handle = &c->yield_ctx;
    coroutine->in = in;
    coroutine->out = out;
    
	swapcontext(&c->yield_ctx, ctx); 
    
    if (c->done){
        LinuxFreeCoroutine(c);
        coroutine = 0;
    }
    
    return(coroutine);
}

Sys_Resume_Coroutine_Sig(system_resume_coroutine){
    Linux_Coroutine *c = (Linux_Coroutine*)coroutine;
    void *fiber;
    
    Assert(!c->done);
    
    coroutine->yield_handle = &c->yield_ctx;
    coroutine->in = in;
    coroutine->out = out;
    
    ucontext *ctx = *(ucontext**)&coroutine->plat_handle;
    
    swapcontext(&c->yield_ctx, ctx);
    
    if (c->done){
        LinuxFreeCoroutine(c);
        coroutine = 0;
    }
    
    return(coroutine);
}

Sys_Yield_Coroutine_Sig(system_yield_coroutine){
	swapcontext(*(ucontext_t**)&coroutine->plat_handle, (ucontext*)coroutine->yield_handle);
}

Sys_CLI_Call_Sig(system_cli_call){
    LINUX_FN_DEBUG("%s %s", path, script_name);

    int pipe_fds[2];
    if(pipe(pipe_fds) == -1){
        perror("system_cli_call: pipe");
        return 0;
    }

    pid_t child_pid = fork();
    if(child_pid == -1){
        perror("system_cli_call: fork");
        return 0;
    }
    
    enum { PIPE_FD_READ, PIPE_FD_WRITE };

    // child
    if(child_pid == 0){
        close(pipe_fds[PIPE_FD_READ]);
        dup2(pipe_fds[PIPE_FD_WRITE], STDOUT_FILENO);
        dup2(pipe_fds[PIPE_FD_WRITE], STDERR_FILENO);

        if(chdir(path) == -1){
            perror("system_cli_call: chdir");
            exit(1);
        };

        char* argv[] = { "sh", "-c", script_name, NULL };

        if(execv("/bin/sh", argv) == -1){
            perror("system_cli_call: execv");
        }
        exit(1);
    } else {
        close(pipe_fds[PIPE_FD_WRITE]);

        *(pid_t*)&cli_out->proc = child_pid;
        *(int*)&cli_out->out_read = pipe_fds[PIPE_FD_READ];
        *(int*)&cli_out->out_write = pipe_fds[PIPE_FD_WRITE];

        struct epoll_event e = {};
        e.events = EPOLLIN | EPOLLET;
        e.data.u64 = LINUX_4ED_EVENT_CLI;
        epoll_ctl(linuxvars.epoll, EPOLL_CTL_ADD, pipe_fds[PIPE_FD_READ], &e);
    }

    return 1;
}

Sys_CLI_Begin_Update_Sig(system_cli_begin_update){
    // NOTE(inso): I don't think anything needs to be done here.
}

Sys_CLI_Update_Step_Sig(system_cli_update_step){
    
    int pipe_read_fd = *(int*)&cli->out_read;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(pipe_read_fd, &fds);

    struct timeval tv = {};

    size_t space_left = max;
    char* ptr = dest;

    while(space_left > 0 && select(pipe_read_fd + 1, &fds, NULL, NULL, &tv) == 1){
        ssize_t num = read(pipe_read_fd, ptr, space_left);
        if(num == -1){
            perror("system_cli_update_step: read");
        } else if(num == 0){
            // NOTE(inso): EOF
            break;
        } else {
            ptr += num;
            space_left -= num;
        }
    }

    *amount = (ptr - dest);
    return (ptr - dest) > 0;
}

Sys_CLI_End_Update_Sig(system_cli_end_update){
    pid_t pid = *(pid_t*)&cli->proc;
    b32 close_me = 0;

    int status;
    if(pid && waitpid(pid, &status, WNOHANG) > 0){
        close_me = 1;

        cli->exit = WEXITSTATUS(status);

        struct epoll_event e = {};
        epoll_ctl(linuxvars.epoll, EPOLL_CTL_DEL, *(int*)&cli->out_read, &e);

        close(*(int*)&cli->out_read);
        close(*(int*)&cli->out_write);
    }

    return close_me;
}

static_assert(sizeof(Plat_Handle) >= sizeof(sem_t*), "Plat_Handle not big enough");

internal Plat_Handle
LinuxSemToHandle(sem_t* sem){
    return *(Plat_Handle*)&sem;
}

internal sem_t*
LinuxHandleToSem(Plat_Handle h){
    return *(sem_t**)&h;
}

internal void*
ThreadProc(void* arg){
    Thread_Context *thread = (Thread_Context*)arg;
    Work_Queue *queue = thread->queue;
    
    for (;;){
        u32 read_index = queue->read_position;
        u32 write_index = queue->write_position;
        
        if (read_index != write_index){
            u32 next_read_index = (read_index + 1) % JOB_ID_WRAP;
            u32 safe_read_index =
                __sync_val_compare_and_swap(&queue->read_position,
                                           read_index, next_read_index);
            
            if (safe_read_index == read_index){
                Full_Job_Data *full_job = queue->jobs + (safe_read_index % QUEUE_WRAP);
                // NOTE(allen): This is interlocked so that it plays nice
                // with the cancel job routine, which may try to cancel this job
                // at the same time that we try to run it
                
                i32 safe_running_thread =
                    __sync_val_compare_and_swap(&full_job->running_thread,
                                               THREAD_NOT_ASSIGNED, thread->id);
                
                if (safe_running_thread == THREAD_NOT_ASSIGNED){
                    thread->job_id = full_job->id;
                    thread->running = 1;
                    Thread_Memory *thread_memory = 0;
                    
                    // TODO(allen): remove memory_request
                    if (full_job->job.memory_request != 0){
                        thread_memory = linuxvars.thread_memory + thread->id - 1;
                        if (thread_memory->size < full_job->job.memory_request){
                            if (thread_memory->data){
                                LinuxFreeMemory(thread_memory->data);
                            }
                            i32 new_size = LargeRoundUp(full_job->job.memory_request, Kbytes(4));
                            thread_memory->data = LinuxGetMemory(new_size);
                            thread_memory->size = new_size;
                        }
                    }
                    full_job->job.callback(linuxvars.system, thread, thread_memory,
                                           &exchange_vars.thread, full_job->job.data);
                    full_job->running_thread = 0;
                    thread->running = 0;

                    LinuxScheduleStep();
                }
            }
        }
        else{
            sem_wait(LinuxHandleToSem(queue->semaphore));
        }
    }
}

Sys_Post_Job_Sig(system_post_job){
    Work_Queue *queue = exchange_vars.thread.queues + group_id;
    
    Assert((queue->write_position + 1) % QUEUE_WRAP != queue->read_position % QUEUE_WRAP);
    
    b32 success = 0;
    u32 result = 0;
    while (!success){
        u32 write_index = queue->write_position;
        u32 next_write_index = (write_index + 1) % JOB_ID_WRAP;
        u32 safe_write_index =
            __sync_val_compare_and_swap(&queue->write_position,
                                       write_index, next_write_index);
        if (safe_write_index  == write_index){
            result = write_index;
            write_index = write_index % QUEUE_WRAP;
            queue->jobs[write_index].job = job;
            queue->jobs[write_index].running_thread = THREAD_NOT_ASSIGNED;
            queue->jobs[write_index].id = result;
            success = 1;
        }
    }
    
    sem_post(LinuxHandleToSem(queue->semaphore));

    return result;
}

Sys_Acquire_Lock_Sig(system_acquire_lock){
    pthread_mutex_lock(linuxvars.locks + id);
}

Sys_Release_Lock_Sig(system_release_lock){
    pthread_mutex_unlock(linuxvars.locks + id);
}

Sys_Cancel_Job_Sig(system_cancel_job){
    Work_Queue *queue = exchange_vars.thread.queues + group_id;
    Thread_Group *group = linuxvars.groups + group_id;
    
    u32 job_index;
    u32 thread_id;
    Full_Job_Data *full_job;
    Thread_Context *thread;
    
    job_index = job_id % QUEUE_WRAP;
    full_job = queue->jobs + job_index;
    
    Assert(full_job->id == job_id);
    thread_id =
        __sync_val_compare_and_swap(&full_job->running_thread,
                                   THREAD_NOT_ASSIGNED, 0);
    
    if (thread_id != THREAD_NOT_ASSIGNED){
        system_acquire_lock(CANCEL_LOCK0 + thread_id - 1);
        thread = group->threads + thread_id - 1;
        pthread_kill(thread->handle, SIGINT); //NOTE(inso) SIGKILL if you really want it to die.
        pthread_create(&thread->handle, NULL, &ThreadProc, thread);
        system_release_lock(CANCEL_LOCK0 + thread_id - 1);
        thread->running = 0;

        LinuxScheduleStep();
    }

}

Sys_Grow_Thread_Memory_Sig(system_grow_thread_memory){
    void *old_data;
    i32 old_size, new_size;
    
    system_acquire_lock(CANCEL_LOCK0 + memory->id - 1);
    old_data = memory->data;
    old_size = memory->size;
    new_size = LargeRoundUp(memory->size*2, Kbytes(4));
    memory->data = system_get_memory(new_size);
    memory->size = new_size;
    if (old_data){
        memcpy(memory->data, old_data, old_size);
        system_free_memory(old_data);
    }
    system_release_lock(CANCEL_LOCK0 + memory->id - 1);
}

INTERNAL_Sys_Sentinel_Sig(internal_sentinel){
    Bubble *result;
#if FRED_INTERNAL
    result = &linuxvars.internal_bubble;
#else
    result = 0;
#endif
    return(result);
}

INTERNAL_Sys_Get_Thread_States_Sig(internal_get_thread_states){
    Work_Queue *queue = exchange_vars.thread.queues + id;
    u32 write = queue->write_position;
    u32 read = queue->read_position;
    if (write < read) write += JOB_ID_WRAP;
    *pending = (i32)(write - read);
    
    Thread_Group *group = linuxvars.groups + id;
    for (i32 i = 0; i < group->count; ++i){
        running[i] = (group->threads[i].running != 0);
    }
}

INTERNAL_Sys_Debug_Message_Sig(internal_debug_message){
    fprintf(stderr, "%s", message);
}

internal
FILE_EXISTS_SIG(system_file_exists){
    int result = 0;
    char buff[PATH_MAX] = {};

    if(len + 1 > PATH_MAX){
        fputs("system_directory_has_file: path too long\n", stderr);
    } else {
        memcpy(buff, filename, len);
        buff[len] = 0;
        struct stat st;
        result = stat(buff, &st) == 0 && S_ISREG(st.st_mode);
    }

    LINUX_FN_DEBUG("%s: %d", buff, result);

    return(result);
}


DIRECTORY_CD_SIG(system_directory_cd){
    String directory = make_string(dir, *len, capacity);
    b32 result = 0;
    i32 old_size;
    
    if (rel_path[0] != 0){
        if (rel_path[0] == '.' && rel_path[1] == 0){
            result = 1;
        }
        else if (rel_path[0] == '.' && rel_path[1] == '.' && rel_path[2] == 0){
            result = remove_last_folder(&directory);
            terminate_with_null(&directory);
        }
        else{
            if (directory.size + rel_len + 1 > directory.memory_size){
                old_size = directory.size;
                append_partial(&directory, rel_path);
                append_partial(&directory, "/");
                terminate_with_null(&directory);

                struct stat st;
                if (stat(directory.str, &st) == 0 && S_ISDIR(st.st_mode)){
                    result = 1;
                }
                else{
                    directory.size = old_size;
                }
            }
        }
    }
    
    *len = directory.size;

    LINUX_FN_DEBUG("%.*s: %d", directory.size, directory.str, result);
    
    return(result);
}

internal
Sys_File_Can_Be_Made(system_file_can_be_made){
    b32 result = access(filename, W_OK) == 0;
    LINUX_FN_DEBUG("%s = %d", filename, result);
    return(result);
}

internal
Sys_Load_File_Sig(system_load_file){
    Data result = {};
    struct stat info = {};
    int fd;
    u8 *ptr, *read_ptr;
    size_t bytes_to_read;
    ssize_t num;

    LINUX_FN_DEBUG("%s", filename);

    fd = open(filename, O_RDONLY);
    if(fd < 0){
        fprintf(stderr, "sys_open_file: open '%s': %s\n", filename, strerror(errno));
        goto out;
    }
    if(fstat(fd, &info) < 0){
        fprintf(stderr, "sys_open_file: stat '%s': %s\n", filename, strerror(errno));
        goto out;
    }
    if(info.st_size < 0){
        fprintf(stderr, "sys_open_file: st_size < 0: %ld\n", info.st_size);
        goto out;
    }

    // NOTE(inso): add 1 extra byte since malloc(0) can return NULL
    ptr = (u8*)LinuxGetMemory(info.st_size + 1);
    if(!ptr){
        fputs("sys_open_file: null pointer from LGM", stderr);
        goto out;
    }

    // NOTE(inso): might as well null terminate with the extra byte
    ptr[info.st_size] = 0;
    read_ptr = ptr;
    bytes_to_read = info.st_size;

    do {
        num = read(fd, read_ptr, bytes_to_read);
        if(num < 0){
            if(errno == EINTR){
                continue;
            } else {
                //TODO(inso): error handling
                perror("sys_load_file: read");
                LinuxFreeMemory(ptr);
                goto out;
            }
        } else {
            bytes_to_read -= num;
            read_ptr += num;
        }
    } while(bytes_to_read);

    result.size = info.st_size;
    result.data = ptr;

out:
    if(fd >= 0) close(fd);
    return(result);
}

internal
Sys_Save_File_Sig(system_save_file){
    b32 result = 0;
    int fd = open(filename, O_WRONLY | O_TRUNC | O_CREAT, 00640);

    LINUX_FN_DEBUG("%s %d", filename, size);

    if(fd < 0){
        fprintf(stderr, "system_save_file: open '%s': %s\n", filename, strerror(errno));
    } else {
        do {
            ssize_t written = write(fd, data, size);
            if(written == -1){
                if(errno != EINTR){
                    perror("system_save_file: write");
                    close(fd);
                    return result;
                }
            } else {
                size -= written;
                data += written;
            }
        } while(size);

        close(fd);
        result = 1;
    }

    return result;
}

//NOTE(inso): this is a version that writes to a temp file & renames, but might be causing save issues?
#if 0
internal
Sys_Save_File_Sig(system_save_file){
    b32 result = 0;

    const size_t save_fsz   = strlen(filename);
    const char   tmp_end[]  = ".4ed.XXXXXX";
    char*        tmp_fname  = (char*) alloca(save_fsz + sizeof(tmp_end));

    memcpy(tmp_fname, filename, save_fsz);
    memcpy(tmp_fname + save_fsz, tmp_end, sizeof(tmp_end));

    int tmp_fd = mkstemp(tmp_fname);
    if(tmp_fd == -1){
        perror("system_save_file: mkstemp");
        return result;
    }

    do {
        ssize_t written = write(tmp_fd, data, size);
        if(written == -1){
            if(errno == EINTR){
                continue;
            } else {
                perror("system_save_file: write");
                unlink(tmp_fname);
                return result;
            }
        } else {
            size -= written;
            data += written;
        }
    } while(size);

    if(rename(tmp_fname, filename) == -1){
        perror("system_save_file: rename");
        unlink(tmp_fname);
        return result;
    }

    result = 1;
    return(result);
}
#endif

#if FRED_USE_FONTCONFIG
internal char*
LinuxFontConfigGetName(char* approx_name, double pts){
    char* result = 0;

    FcPattern* pat = FcPatternBuild(
        NULL,
        FC_POSTSCRIPT_NAME, FcTypeString, approx_name,
        FC_SIZE, FcTypeDouble, pts,
        FC_FONTFORMAT, FcTypeString, "TrueType",
        NULL
    );

    FcConfigSubstitute(linuxvars.fontconfig, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult res;
    FcPattern* font = FcFontMatch(linuxvars.fontconfig, pat, &res);
    FcChar8* fname = 0;

    if(font){
        FcPatternGetString(font, FC_FILE, 0, &fname);
        if(fname){
            result = strdup((char*)fname);
            fprintf(stderr, "Got system font from FontConfig: %s\n", result);
        }
        FcPatternDestroy(font);
    }

    FcPatternDestroy(pat);

    if(!result){
        result = strdup(approx_name);
    }

    return result; 
}
#endif

#include "system_shared.cpp"
#include "4ed_rendering.cpp"

internal
Font_Load_Sig(system_draw_font_load){
    Font_Load_Parameters *params;
    b32 free_name = 0;
    char* chosen_name = filename;

#if FRED_USE_FONTCONFIG
    if(access(filename, F_OK) != 0){
        chosen_name = LinuxFontConfigGetName(basename(filename), pt_size);
        free_name = 1;
    }
#endif

    system_acquire_lock(FONT_LOCK);
    params = linuxvars.fnt.free_param.next;
    fnt__remove(params);
    fnt__insert(&linuxvars.fnt.used_param, params);
    system_release_lock(FONT_LOCK);

    if (linuxvars.fnt.part.base == 0){
        linuxvars.fnt.part = LinuxScratchPartition(Mbytes(8));
    }

    b32 success = 0;
    i32 attempts = 0;

    i32 oversample = 2;

    for(; attempts < 3; ++attempts){
        success = draw_font_load(
            linuxvars.fnt.part.base,
            linuxvars.fnt.part.max,
            font_out,
            chosen_name,
            pt_size,
            tab_width,
            oversample
        );

        if(success){
            break;
        } else {
            fprintf(stderr, "draw_font_load failed, %d\n", linuxvars.fnt.part.max);
            LinuxScratchPartitionDouble(&linuxvars.fnt.part);
        }
    }

    if(success){
        system_acquire_lock(FONT_LOCK);
        fnt__remove(params);
        fnt__insert(&linuxvars.fnt.free_param, params);
        system_release_lock(FONT_LOCK);
    }

    if(free_name){
        free(chosen_name);
    }

    return success;
}

internal
Sys_To_Binary_Path(system_to_binary_path){
    b32 translate_success = 0;
    i32 max = out_filename->memory_size;
    i32 size = readlink("/proc/self/exe", out_filename->str, max);

    LINUX_FN_DEBUG("%d, %d", max, size);

    if (size > 0 && size < max-1){
        out_filename->size = size;
        remove_last_folder(out_filename);
        if (append(out_filename, filename) && terminate_with_null(out_filename)){
            LINUX_FN_DEBUG("%.*s", out_filename->size, out_filename->str);
            translate_success = 1;
        }
    }
    return (translate_success);
}

internal b32
LinuxLoadAppCode(String* base_dir){
    b32 result = 0;
    App_Get_Functions *get_funcs = 0;

    if(!system_to_binary_path(base_dir, "4ed_app.so")){
        return 0;
    }

    linuxvars.app_code = dlopen(base_dir->str, RTLD_LAZY);
    if (linuxvars.app_code){
        get_funcs = (App_Get_Functions*)
            dlsym(linuxvars.app_code, "app_get_functions");
    } else {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
    }

    if (get_funcs){
        result = 1;
        linuxvars.app = get_funcs();
    }

    return(result);
}

internal void
LinuxLoadSystemCode(){
    linuxvars.system->file_time_stamp = system_file_time_stamp;
    linuxvars.system->set_file_list = system_set_file_list;
    linuxvars.system->file_track = system_file_track;
    linuxvars.system->file_untrack = system_file_untrack;

    linuxvars.system->file_exists = system_file_exists;
    linuxvars.system->directory_cd = system_directory_cd;
    linuxvars.system->file_unique_hash = system_file_unique_hash;

    linuxvars.system->post_clipboard = system_post_clipboard;
    linuxvars.system->time = system_time;
    
    linuxvars.system->create_coroutine = system_create_coroutine;
    linuxvars.system->launch_coroutine = system_launch_coroutine;
    linuxvars.system->resume_coroutine = system_resume_coroutine;
    linuxvars.system->yield_coroutine = system_yield_coroutine;

    linuxvars.system->cli_call = system_cli_call;
    linuxvars.system->cli_begin_update = system_cli_begin_update;
    linuxvars.system->cli_update_step = system_cli_update_step;
    linuxvars.system->cli_end_update = system_cli_end_update;

    linuxvars.system->post_job = system_post_job;
    linuxvars.system->cancel_job = system_cancel_job;
    linuxvars.system->grow_thread_memory = system_grow_thread_memory;
    linuxvars.system->acquire_lock = system_acquire_lock;
    linuxvars.system->release_lock = system_release_lock;
    
    linuxvars.system->internal_sentinel = internal_sentinel;
    linuxvars.system->internal_get_thread_states = internal_get_thread_states;
    linuxvars.system->internal_debug_message = internal_debug_message;

    linuxvars.system->slash = '/';
}

internal void
LinuxLoadRenderCode(){
    linuxvars.target.push_clip = draw_push_clip;
    linuxvars.target.pop_clip = draw_pop_clip;
    linuxvars.target.push_piece = draw_push_piece;
    
    linuxvars.target.font_set.font_info_load = draw_font_info_load;
    linuxvars.target.font_set.font_load = system_draw_font_load;
    linuxvars.target.font_set.release_font = draw_release_font;
}

internal void
LinuxRedrawTarget(){
    system_acquire_lock(RENDER_LOCK);
    launch_rendering(&linuxvars.target);
    system_release_lock(RENDER_LOCK);
//    glFlush();
    glXSwapBuffers(linuxvars.XDisplay, linuxvars.XWindow);
}

internal void
LinuxResizeTarget(i32 width, i32 height){
    if (width > 0 && height > 0){
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);
        glScissor(0, 0, width, height);
        
        linuxvars.target.width = width;
        linuxvars.target.height = height;
        linuxvars.redraw = 1;
    }
}

// NOTE(allen): Thanks to Casey for providing the linux OpenGL launcher.
static bool ctxErrorOccurred = false;
static int XInput2OpCode = 0;
internal int
ctxErrorHandler( Display *dpy, XErrorEvent *ev )
{
    ctxErrorOccurred = true;
    return 0;
}

#if FRED_INTERNAL
static void gl_log(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam
                   ){
    fprintf(stderr, "GL DEBUG: %s\n", message);
}
#endif

internal GLXContext
InitializeOpenGLContext(Display *XDisplay, Window XWindow, GLXFBConfig &bestFbc, b32 &IsLegacy)
{
    IsLegacy = false;
    
    typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
    const char *glxExts = glXQueryExtensionsString(XDisplay, DefaultScreen(XDisplay));
    
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
        glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );
 
    GLXContext ctx = 0;
    ctxErrorOccurred = false;
    int (*oldHandler)(Display*, XErrorEvent*) =
        XSetErrorHandler(&ctxErrorHandler);
    if (!glXCreateContextAttribsARB)
    {
        fprintf(stderr,  "glXCreateContextAttribsARB() not found"
                " ... using old-style GLX context\n" );
        ctx = glXCreateNewContext( XDisplay, bestFbc, GLX_RGBA_TYPE, 0, True );
    } 
    else
    {
        int context_attribs[] =
            {
                GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
                GLX_CONTEXT_MINOR_VERSION_ARB, 3,
                GLX_CONTEXT_PROFILE_MASK_ARB , GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#if FRED_INTERNAL
                GLX_CONTEXT_FLAGS_ARB        , GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
                None
            };

        fprintf(stderr, "Attribs: %d %d %d %d %d\n",
               context_attribs[0],
               context_attribs[1],
               context_attribs[2],
               context_attribs[3],
               context_attribs[4]);
 
        fprintf(stderr,  "Creating context\n" );
        ctx = glXCreateContextAttribsARB( XDisplay, bestFbc, 0,
                                          True, context_attribs );
 
        XSync( XDisplay, False );
        if ( !ctxErrorOccurred && ctx )
        {
            fprintf(stderr,  "Created GL 4.3 context\n" );
        }
        else
        {
            ctxErrorOccurred = false;

            context_attribs[1] = 3;
            context_attribs[3] = 2;
 
            fprintf(stderr,  "Creating context\n" );
            ctx = glXCreateContextAttribsARB( XDisplay, bestFbc, 0,
                                              True, context_attribs );
 
            XSync( XDisplay, False );

            if ( !ctxErrorOccurred && ctx )
            {
                fprintf(stderr,  "Created GL 3.2 context\n" );
            }
            else
            {
                context_attribs[1] = 1;
                context_attribs[3] = 2;
 
                ctxErrorOccurred = false;
 
                fprintf(stderr,  "Failed to create GL 3.2 context"
                        " ... using old-style GLX context\n" );
                ctx = glXCreateContextAttribsARB( XDisplay, bestFbc, 0, 
                                                  True, context_attribs );

                IsLegacy = true;
            }
        }
    }
 
    XSync( XDisplay, False );
    XSetErrorHandler( oldHandler );
 
    if ( ctxErrorOccurred || !ctx )
    {
        fprintf(stderr,  "Failed to create an OpenGL context\n" );
        exit(1);
    }
 
    if ( ! glXIsDirect ( XDisplay, ctx ) )
    {
        fprintf(stderr,  "Indirect GLX rendering context obtained\n" );
    }
    else
    {
        fprintf(stderr,  "Direct GLX rendering context obtained\n" );
    }
    
    fprintf(stderr,  "Making context current\n" );
    glXMakeCurrent( XDisplay, XWindow, ctx );
    
    GLint n;
    char *Vendor = (char *)glGetString(GL_VENDOR);
    char *Renderer = (char *)glGetString(GL_RENDERER);
    char *Version = (char *)glGetString(GL_VERSION);

    //TODO(inso): glGetStringi is required in core profile if the GL version is >= 3.0
    char *Extensions = (char *)glGetString(GL_EXTENSIONS);

    fprintf(stderr, "GL_VENDOR: %s\n", Vendor);
    fprintf(stderr, "GL_RENDERER: %s\n", Renderer);
    fprintf(stderr, "GL_VERSION: %s\n", Version);
//    fprintf(stderr, "GL_EXTENSIONS: %s\n", Extensions);

    //NOTE(inso): enable vsync if available. this should probably be optional
    if(strstr(glxExts, "GLX_EXT_swap_control ")){
        PFNGLXSWAPINTERVALEXTPROC glx_swap_interval_ext =
            (PFNGLXSWAPINTERVALEXTPROC) glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalEXT");

        if(glx_swap_interval_ext){
            glx_swap_interval_ext(XDisplay, XWindow, 1);

            unsigned int swap_val = 0;
            glXQueryDrawable(XDisplay, XWindow, GLX_SWAP_INTERVAL_EXT, &swap_val);
            linuxvars.vsync = swap_val == 1;
            fprintf(stderr, "VSync enabled? %s.\n", linuxvars.vsync ? "Yes" : "No");
        }
    } else if(strstr(glxExts, "GLX_MESA_swap_control ")){
        PFNGLXSWAPINTERVALMESAPROC glx_swap_interval_mesa = 
            (PFNGLXSWAPINTERVALMESAPROC) glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalMESA");

        PFNGLXGETSWAPINTERVALMESAPROC glx_get_swap_interval_mesa =
            (PFNGLXGETSWAPINTERVALMESAPROC) glXGetProcAddressARB((const GLubyte*)"glXGetSwapIntervalMESA");

        if(glx_swap_interval_mesa){
            glx_swap_interval_mesa(1);
            if(glx_get_swap_interval_mesa){
                linuxvars.vsync = glx_get_swap_interval_mesa();
                fprintf(stderr, "VSync enabled? %s (MESA)\n", linuxvars.vsync ? "Yes" : "No");
            } else {
                // NOTE(inso): assume it worked?
                linuxvars.vsync = 1;
                fputs("VSync enabled? possibly (MESA)", stderr);
            }
        }
    } else if(strstr(glxExts, "GLX_SGI_swap_control ")){
        PFNGLXSWAPINTERVALSGIPROC glx_swap_interval_sgi = 
            (PFNGLXSWAPINTERVALSGIPROC) glXGetProcAddressARB((const GLubyte*)"glXSwapIntervalSGI");

        if(glx_swap_interval_sgi){
            glx_swap_interval_sgi(1);
            //NOTE(inso): The SGI one doesn't seem to have a way to confirm we got it...
            linuxvars.vsync = 1;
            fputs("VSync enabled? hopefully (SGI)", stderr);
        }
    } else {
        fputs("VSync enabled? nope, no suitable extension", stderr);
    }

#if FRED_INTERNAL
    PFNGLDEBUGMESSAGECALLBACKARBPROC gl_dbg_callback = (PFNGLDEBUGMESSAGECALLBACKARBPROC)glXGetProcAddress((const GLubyte*)"glDebugMessageCallback");
    if(gl_dbg_callback){
        fputs("enabling gl debug\n", stderr);
        gl_dbg_callback(&gl_log, 0);
        glEnable(GL_DEBUG_OUTPUT);
    }
#endif

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    return(ctx);
}

internal b32
GLXCanUseFBConfig(Display *XDisplay)
{
    b32 Result = false;

    int GLXMajor, GLXMinor;

    char *XVendor = ServerVendor(XDisplay);
    fprintf(stderr, "XWindows vendor: %s\n", XVendor);
    if(glXQueryVersion(XDisplay, &GLXMajor, &GLXMinor))
    {
        fprintf(stderr, "GLX version %d.%d\n", GLXMajor, GLXMinor);
        if(((GLXMajor == 1 ) && (GLXMinor >= 3)) ||
           (GLXMajor > 1))
        {
            Result = true;
        }
    }

    return(Result);
}

typedef struct glx_config_result{
    b32 Found;
    GLXFBConfig BestConfig;
    XVisualInfo BestInfo;
} glx_config_result;

internal glx_config_result
ChooseGLXConfig(Display *XDisplay, int XScreenIndex)
{
    glx_config_result Result = {0};
    
    int DesiredAttributes[] =
	{
		GLX_X_RENDERABLE    , True,
		GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
		GLX_RENDER_TYPE     , GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
		GLX_RED_SIZE        , 8,
		GLX_GREEN_SIZE      , 8,
		GLX_BLUE_SIZE       , 8,
		GLX_ALPHA_SIZE      , 8,
		GLX_DEPTH_SIZE      , 24,
		GLX_STENCIL_SIZE    , 8,
		GLX_DOUBLEBUFFER    , True,
		//GLX_SAMPLE_BUFFERS  , 1,
		//GLX_SAMPLES         , 4,
		None
	};    

	int ConfigCount;
	GLXFBConfig *Configs = glXChooseFBConfig(XDisplay,
											 XScreenIndex,
											 DesiredAttributes,
											 &ConfigCount);
	if(ConfigCount > 0)
	{
		Result.Found = true;
		Result.BestConfig = *Configs;
		Result.BestInfo = *glXGetVisualFromFBConfig(XDisplay, *Configs);
	}

    return(Result);
}

struct Init_Input_Result{
    XIM input_method;
    XIMStyle best_style;
    XIC xic;
};

// NOTE(inso): doesn't actually use XInput anymore, i should change the name...
internal Init_Input_Result
InitializeXInput(Display *dpy, Window XWindow)
{
    Init_Input_Result result = {};
    XIMStyle style;
    XIMStyles *styles = 0;
    i32 i, count;

	setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
	fprintf(stderr, "Supported locale?: %s.\n", XSupportsLocale() ? "Yes" : "No");
    // TODO(inso): handle the case where it isn't supported somehow?

    XSelectInput(
        linuxvars.XDisplay,
        linuxvars.XWindow,
        ExposureMask |
        KeyPressMask | KeyReleaseMask |
        ButtonPressMask | ButtonReleaseMask |
        EnterWindowMask | LeaveWindowMask |
        PointerMotionMask |
        FocusChangeMask |
        StructureNotifyMask |
        MappingNotify |
        ExposureMask |
        VisibilityChangeMask
    );

    result.input_method = XOpenIM(dpy, 0, 0, 0);
    if (!result.input_method){
        // NOTE(inso): Try falling back to the internal XIM implementation that
        // should in theory always exist.

        XSetLocaleModifiers("@im=none");
        result.input_method = XOpenIM(dpy, 0, 0, 0);
    }

    if (result.input_method){

        if (!XGetIMValues(result.input_method, XNQueryInputStyle, &styles, NULL) &&
            styles){
            result.best_style = 0;
            count = styles->count_styles;
            for (i = 0; i < count; ++i){
                style = styles->supported_styles[i];
                if (style == (XIMPreeditNothing | XIMStatusNothing)){
                    result.best_style = style;
                    break;
                }
            }
            
            if (result.best_style){
                XFree(styles);

                result.xic =
                    XCreateIC(result.input_method, XNInputStyle, result.best_style,
                              XNClientWindow, XWindow,
                              XNFocusWindow, XWindow,
                              0, 0,
                              NULL);
            }
            else{
                result = {};
                fputs("Could not get minimum required input style", stderr);
            }
        }
    }
    else{
        result = {};
        fputs("Could not open X Input Method", stderr);
    }

    return(result);
}

internal void
LinuxPushKey(u8 code, u8 chr, u8 chr_nocaps, b8 (*mods)[MDFR_INDEX_COUNT], b32 is_hold)
{
    i32 *count;
    Key_Event_Data *data;

    if(is_hold){
        count = &linuxvars.key_data.hold_count;
        data = linuxvars.key_data.hold;
    } else {
        count = &linuxvars.key_data.press_count;
        data = linuxvars.key_data.press;
    }

	if(*count < KEY_INPUT_BUFFER_SIZE){
		data[*count].keycode = code;
		data[*count].character = chr;
		data[*count].character_no_caps_lock = chr_nocaps;

		memcpy(data[*count].modifiers, *mods, sizeof(*mods));

		++(*count);
	}
}

internal void
LinuxMaximizeWindow(Display* d, Window w, b32 maximize)
{
    //NOTE(inso): this will only work after it is mapped
    
    enum { STATE_REMOVE, STATE_ADD, STATE_TOGGLE };

    XEvent e = {};

    e.xany.type = ClientMessage;
    e.xclient.message_type = linuxvars.atom_NET_WM_STATE;
    e.xclient.format = 32;
    e.xclient.window = w;
    e.xclient.data.l[0] = maximize ? STATE_ADD : STATE_REMOVE;
    e.xclient.data.l[1] = linuxvars.atom_NET_WM_STATE_MAXIMIZED_VERT;
    e.xclient.data.l[2] = linuxvars.atom_NET_WM_STATE_MAXIMIZED_HORZ;
    e.xclient.data.l[3] = 0L;

    XSendEvent(
        d,
        RootWindow(d, 0),
        0,
        SubstructureNotifyMask | SubstructureRedirectMask,
        &e
    );
}

#include "linux_icon.h"
internal void
LinuxSetIcon(Display* d, Window w)
{
	Atom WM_ICON = XInternAtom(d, "_NET_WM_ICON", False);

	XChangeProperty(
		d,
		w,
		WM_ICON,
		XA_CARDINAL,
		32,
		PropModeReplace,
		(unsigned char*)linux_icon,
		sizeof(linux_icon) / sizeof(long)
	);
}

internal void
LinuxScheduleStep(void)
{
    u64 now  = system_time();
    u64 diff = (now - linuxvars.last_step);

    if(diff > (u64)frame_useconds){
        u64 ev = 1;
        write(linuxvars.step_event_fd, &ev, sizeof(ev));
    } else {
        struct itimerspec its = {};
        timerfd_gettime(linuxvars.step_timer_fd, &its);

        if(its.it_value.tv_sec == 0 && its.it_value.tv_nsec == 0){
            its.it_value.tv_nsec = (frame_useconds - diff) * 1000UL;
            timerfd_settime(linuxvars.step_timer_fd, 0, &its, NULL);
        }
    }
}

internal void
LinuxHandleX11Events(void)
{
    XEvent PrevEvent = {};
    b32 should_step = 0;

    while(XPending(linuxvars.XDisplay))
    {
        XEvent Event;
        XNextEvent(linuxvars.XDisplay, &Event);

        if (XFilterEvent(&Event, None) == True){
            continue;
        }

        switch (Event.type){
            case KeyPress: {
                should_step = 1;

                b32 is_hold = (PrevEvent.type         == KeyRelease &&
                               PrevEvent.xkey.time    == Event.xkey.time &&
                               PrevEvent.xkey.keycode == Event.xkey.keycode);

                b8 mods[MDFR_INDEX_COUNT] = {};
                if(Event.xkey.state & ShiftMask) mods[MDFR_SHIFT_INDEX] = 1;
                if(Event.xkey.state & ControlMask) mods[MDFR_CONTROL_INDEX] = 1;
                if(Event.xkey.state & LockMask) mods[MDFR_CAPS_INDEX] = 1;
                if(Event.xkey.state & Mod1Mask) mods[MDFR_ALT_INDEX] = 1;

                Event.xkey.state &= ~(ControlMask);

                Status status;
                KeySym keysym = NoSymbol;
                char buff[32] = {};

                Xutf8LookupString(
                    linuxvars.input_context,
                    &Event.xkey,
                    buff,
                    sizeof(buff) - 1,
                    &keysym,
                    &status
                );

                if(status == XBufferOverflow){
                    //TODO(inso): handle properly
                    Xutf8ResetIC(linuxvars.input_context);
                    XSetICFocus(linuxvars.input_context);
                    fputs("FIXME: XBufferOverflow from LookupString.\n", stderr);
                }

                u8 key = *buff, key_no_caps = key;

                if(mods[MDFR_CAPS_INDEX] && status == XLookupBoth && Event.xkey.keycode){
                    char buff_no_caps[32] = {};
                    Event.xkey.state &= ~(LockMask);

                    XLookupString(
                        &Event.xkey,
                        buff_no_caps,
                        sizeof(buff_no_caps) - 1,
                        NULL,
                        NULL
                    );

                    if(*buff_no_caps){
                        key_no_caps = *buff_no_caps;
                    }
                }

                if(key         == '\r') key         = '\n';
                if(key_no_caps == '\r') key_no_caps = '\n';

                if(keysym == XK_ISO_Left_Tab){
                    key = key_no_caps = '\t';
                    mods[MDFR_SHIFT_INDEX] = 1;
                }

                u8 special_key = keycode_lookup(Event.xkey.keycode);

                if(special_key){
                    LinuxPushKey(special_key, 0, 0, &mods, is_hold);
                } else if(key < 128){
                    LinuxPushKey(key, key, key_no_caps, &mods, is_hold);
                } else {
                    LinuxPushKey(0, 0, 0, &mods, is_hold);
                }
            }break;

            case KeyRelease: {
                should_step = 1;
            }break;

            case MotionNotify: {
                should_step = 1;
                linuxvars.mouse_data.x = Event.xmotion.x;
                linuxvars.mouse_data.y = Event.xmotion.y;
            }break;

            case ButtonPress: {
                should_step = 1;
                switch(Event.xbutton.button){
                    case Button1: {
                        linuxvars.mouse_data.press_l = 1;
                        linuxvars.mouse_data.l = 1;
                    } break;
                    case Button3: {
                        linuxvars.mouse_data.press_r = 1;
                        linuxvars.mouse_data.r = 1;
                    } break;

                    //NOTE(inso): scroll up
                    case Button4: {
                        linuxvars.mouse_data.wheel = 1;
                    }break;

                    //NOTE(inso): scroll down
                    case Button5: {
                        linuxvars.mouse_data.wheel = -1;
                    }break;
                }
            }break;

            case ButtonRelease: {
                should_step = 1;
                switch(Event.xbutton.button){
                    case Button1: {
                        linuxvars.mouse_data.release_l = 1;
                        linuxvars.mouse_data.l = 0;
                    } break;
                    case Button3: {
                        linuxvars.mouse_data.release_r = 1;
                        linuxvars.mouse_data.r = 0;
                    } break;
                }
            }break;

            case EnterNotify: {
                should_step = 1;
                linuxvars.mouse_data.out_of_window = 0;
            }break;

            case LeaveNotify: {
                should_step = 1;
                linuxvars.mouse_data.out_of_window = 1;
            }break;

            case FocusIn:
            case FocusOut: {
                should_step = 1;
                linuxvars.mouse_data.l = 0;
                linuxvars.mouse_data.r = 0;
            }break;

            case ConfigureNotify: {
                should_step = 1;
                i32 w = Event.xconfigure.width, h = Event.xconfigure.height;

                if(w != linuxvars.target.width || h != linuxvars.target.height){
                    LinuxResizeTarget(Event.xconfigure.width, Event.xconfigure.height);
                }
            }break;

            case MappingNotify: {
                if(Event.xmapping.request == MappingModifier || Event.xmapping.request == MappingKeyboard){
                    XRefreshKeyboardMapping(&Event.xmapping);
                    keycode_init(linuxvars.XDisplay);
                }
            }break;

            case ClientMessage: {
                if ((Atom)Event.xclient.data.l[0] == linuxvars.atom_WM_DELETE_WINDOW) {
                    should_step = 1;
                    linuxvars.keep_running = 0;
                }
                else if ((Atom)Event.xclient.data.l[0] == linuxvars.atom_NET_WM_PING) {
                    Event.xclient.window = DefaultRootWindow(linuxvars.XDisplay);
                    XSendEvent(
                        linuxvars.XDisplay,
                        Event.xclient.window,
                        False,
                        SubstructureRedirectMask | SubstructureNotifyMask,
                        &Event
                    );
                }
            }break;

                // NOTE(inso): Someone wants us to give them the clipboard data.
            case SelectionRequest: {
                XSelectionRequestEvent request = Event.xselectionrequest;

                XSelectionEvent response = {};
                response.type = SelectionNotify;
                response.requestor = request.requestor;
                response.selection = request.selection;
                response.target = request.target;
                response.time = request.time;
                response.property = None;

                //TODO(inso): handle TARGETS negotiation instead of requiring UTF8_STRING
                if (
                    linuxvars.clipboard_outgoing.size &&
                    request.target == linuxvars.atom_UTF8_STRING &&
                    request.selection == linuxvars.atom_CLIPBOARD &&
                    request.property != None &&
                    request.display &&
                    request.requestor
                ){
                    XChangeProperty(
                        request.display,
                        request.requestor,
                        request.property,
                        request.target,
                        8,
                        PropModeReplace,
                        (unsigned char*)linuxvars.clipboard_outgoing.str,
                        linuxvars.clipboard_outgoing.size
                    );

                    response.property = request.property;
                }

                XSendEvent(request.display, request.requestor, True, 0, (XEvent*)&response);

            }break;

                // NOTE(inso): Another program is now the clipboard owner.
            case SelectionClear: {
                if(Event.xselectionclear.selection == linuxvars.atom_CLIPBOARD){
                    linuxvars.clipboard_outgoing.size = 0;
                }
            }break;

                // NOTE(inso): A program is giving us the clipboard data we asked for.
            case SelectionNotify: {
                XSelectionEvent* e = (XSelectionEvent*)&Event;
                if(
                    e->selection == linuxvars.atom_CLIPBOARD &&
                    e->target == linuxvars.atom_UTF8_STRING &&
                    e->property != None
                ){
                    Atom type;
                    int fmt;
                    unsigned long nitems, bytes_left;
                    u8 *data;

                    int result = XGetWindowProperty(
                        linuxvars.XDisplay,
                        linuxvars.XWindow,
                        linuxvars.atom_CLIPBOARD,
                        0L,
                        LINUX_MAX_PASTE_CHARS/4L,
                        False,
                        linuxvars.atom_UTF8_STRING,
                        &type,
                        &fmt,
                        &nitems,
                        &bytes_left,
                        &data
                    );

                    if(result == Success && fmt == 8){
                        LinuxStringDup(&linuxvars.clipboard_contents, data, nitems);
                        should_step = 1;
                        linuxvars.new_clipboard = 1;
                        XFree(data);
                    }
                }
            }break;

            case Expose:
            case VisibilityNotify: {
                should_step = 1;
                linuxvars.redraw = 1;
            }break;

            default: {
                if(Event.type == linuxvars.xfixes_selection_event){
                    XConvertSelection(
                        linuxvars.XDisplay,
                        linuxvars.atom_CLIPBOARD,
                        linuxvars.atom_UTF8_STRING,
                        linuxvars.atom_CLIPBOARD,
                        linuxvars.XWindow,
                        CurrentTime
                    );
                }
            }break;
        }

        PrevEvent = Event;
    }

    if(should_step){
        LinuxScheduleStep();
    }
}

internal void
LinuxHandleFileEvents()
{
    struct inotify_event* e;
    char buff[sizeof(*e) + NAME_MAX + 1];

    ssize_t num_bytes = read(linuxvars.inotify_fd, buff, sizeof(buff));
    for(char* p = buff; (p - buff) < num_bytes; p += (sizeof(*e) + e->len)){
        e = (struct inotify_event*)p;
        // TODO: do something with the e->wd / e->name & report that in app.step?
    }
}

int
main(int argc, char **argv)
{

#if FRED_INTERNAL
    linuxvars.internal_bubble.next = &linuxvars.internal_bubble;
    linuxvars.internal_bubble.prev = &linuxvars.internal_bubble;
    linuxvars.internal_bubble.flags = MEM_BUBBLE_SYS_DEBUG;
#endif

    linuxvars.first = 1;

    char base_dir_mem[PATH_MAX];
    String base_dir = make_fixed_width_string(base_dir_mem);

    if (!LinuxLoadAppCode(&base_dir)){
        // TODO(allen): Failed to load app code, serious problem.
        return 99;
    }

    System_Functions system_;
    System_Functions *system = &system_;
    linuxvars.system = system;
    LinuxLoadSystemCode();

    linuxvars.coroutine_free = linuxvars.coroutine_data;
    for (i32 i = 0; i+1 < ArrayCount(linuxvars.coroutine_data); ++i){
        linuxvars.coroutine_data[i].next = linuxvars.coroutine_data + i + 1;
    }

    const size_t stack_size = Mbytes(16);
    for (i32 i = 0; i < ArrayCount(linuxvars.coroutine_data); ++i){
        linuxvars.coroutine_data[i].stack.ss_size = stack_size;
        linuxvars.coroutine_data[i].stack.ss_sp = system_get_memory(stack_size);
    }

    memory_vars.vars_memory_size = Mbytes(2);
    memory_vars.vars_memory = system_get_memory(memory_vars.vars_memory_size);
    memory_vars.target_memory_size = Mbytes(512);
    memory_vars.target_memory = system_get_memory(memory_vars.target_memory_size);
    memory_vars.user_memory_size = Mbytes(2);
    memory_vars.user_memory = system_get_memory(memory_vars.user_memory_size);

    String current_directory;
    i32 curdir_req, curdir_size;
    char *curdir_mem;

    curdir_req = (1 << 9);
    curdir_mem = (char*)system_get_memory(curdir_req);
    for (; getcwd(curdir_mem, curdir_req) == 0 && curdir_req < (1 << 13);){
        system_free_memory(curdir_mem);
        curdir_req *= 4;
        curdir_mem = (char*)system_get_memory(curdir_req);
    }

    if (curdir_req >= (1 << 13)){
        // TODO(allen): bullshit string APIs makin' me pissed
        return 57;
    }

    for (curdir_size = 0; curdir_mem[curdir_size]; ++curdir_size);

    current_directory = make_string(curdir_mem, curdir_size, curdir_req);

    Command_Line_Parameters clparams;
    clparams.argv = argv;
    clparams.argc = argc;

    char **files;
    i32 *file_count;
    i32 output_size;

    output_size =
    linuxvars.app.read_command_line(system,
                                    &memory_vars,
                                    current_directory,
                                    &linuxvars.settings,
                                    &files, &file_count,
                                    clparams);

    if (output_size > 0){
        // TODO(allen): crt free version
        fprintf(stdout, "%.*s", output_size, (char*)memory_vars.target_memory);
    }
    if (output_size != 0) return 0;

    sysshared_filter_real_files(files, file_count);

    linuxvars.XDisplay = XOpenDisplay(0);

    if(!linuxvars.XDisplay){
        fprintf(stderr, "Can't open display!\n");
        return 1;
    }

    keycode_init(linuxvars.XDisplay);

#ifdef FRED_SUPER
    char *custom_file_default = "4coder_custom.so";
    system_to_binary_path(&base_dir, custom_file_default);
    custom_file_default = base_dir.str;

    char *custom_file;
    if (linuxvars.settings.custom_dll){
        custom_file = linuxvars.settings.custom_dll;
    } else {
        custom_file = custom_file_default;
    }

    linuxvars.custom = dlopen(custom_file, RTLD_LAZY);
    if (!linuxvars.custom && custom_file != custom_file_default){
        if (!linuxvars.settings.custom_dll_is_strict){
            linuxvars.custom = dlopen(custom_file_default, RTLD_LAZY);
        }
    }

    if (linuxvars.custom){
        linuxvars.custom_api.get_alpha_4coder_version = (_Get_Version_Function*)
        dlsym(linuxvars.custom, "get_alpha_4coder_version");

        if (linuxvars.custom_api.get_alpha_4coder_version == 0 ||
            linuxvars.custom_api.get_alpha_4coder_version(MAJOR, MINOR, PATCH) == 0){
            fprintf(stderr, "failed to use 4coder_custom.so: version mismatch\n");
        }
        else{
            linuxvars.custom_api.get_bindings = (Get_Binding_Data_Function*)
            dlsym(linuxvars.custom, "get_bindings");

            if (linuxvars.custom_api.get_bindings == 0){
                fprintf(stderr, "failed to use 4coder_custom.so: get_bindings not exported\n");
            }
            else{
                fprintf(stderr, "successfully loaded 4coder_custom.so\n");
            }
        }
    }
#endif

    if (linuxvars.custom_api.get_bindings == 0){
        linuxvars.custom_api.get_bindings = get_bindings;
    }

    Thread_Context background[4] = {};
    linuxvars.groups[BACKGROUND_THREADS].threads = background;
    linuxvars.groups[BACKGROUND_THREADS].count = ArrayCount(background);

    Thread_Memory thread_memory[ArrayCount(background)];
    linuxvars.thread_memory = thread_memory;

    sem_init(&linuxvars.thread_semaphores[BACKGROUND_THREADS], 0, 0);

    exchange_vars.thread.queues[BACKGROUND_THREADS].semaphore = 
    LinuxSemToHandle(&linuxvars.thread_semaphores[BACKGROUND_THREADS]);

    for(i32 i = 0; i < linuxvars.groups[BACKGROUND_THREADS].count; ++i){
        Thread_Context *thread = linuxvars.groups[BACKGROUND_THREADS].threads + i;
        thread->id = i + 1;

        Thread_Memory *memory = linuxvars.thread_memory + i;
        *memory = {};
        memory->id = thread->id;

        thread->queue = &exchange_vars.thread.queues[BACKGROUND_THREADS];
        pthread_create(&thread->handle, NULL, &ThreadProc, thread);
    }

    for(i32 i = 0; i < LOCK_COUNT; ++i){
        pthread_mutex_init(linuxvars.locks + i, NULL);
    }

#if FRED_USE_FONTCONFIG
    linuxvars.fontconfig = FcInitLoadConfigAndFonts();
#endif

    LinuxLoadRenderCode();
    linuxvars.target.max = Mbytes(1);
    linuxvars.target.push_buffer = (byte*)system_get_memory(linuxvars.target.max);

    File_Slot file_slots[32] = {};
    sysshared_init_file_exchange(&exchange_vars, file_slots, ArrayCount(file_slots), 0);

    Font_Load_Parameters params[8];
    sysshared_init_font_params(&linuxvars.fnt, params, ArrayCount(params));

    // NOTE(allen): Here begins the linux screen setup stuff.
    // Behold the true nature of this wonderful OS:
    // (thanks again to Casey for providing this stuff)
    Colormap cmap;
    XSetWindowAttributes swa;
    int WinWidth, WinHeight;
    b32 window_setup_success = 0;

    if (linuxvars.settings.set_window_size){
        WinWidth = linuxvars.settings.window_w;
        WinHeight = linuxvars.settings.window_h;
    } else {
        WinWidth = 800;
        WinHeight = 600;
    }

    int XScreenCount = ScreenCount(linuxvars.XDisplay);
    glx_config_result Config = {};

    if(!GLXCanUseFBConfig(linuxvars.XDisplay)){
        fprintf(stderr, "Your GLX version is too old.\n");
        exit(1);
    }

    for(int XScreenIndex = 0;
        XScreenIndex < XScreenCount;
        ++XScreenIndex)
    {
        Screen *XScreen = ScreenOfDisplay(linuxvars.XDisplay, XScreenIndex);

        i32 ScrnWidth, ScrnHeight;
        ScrnWidth = WidthOfScreen(XScreen);
        ScrnHeight = HeightOfScreen(XScreen);

        if (ScrnWidth + 50 < WinWidth) WinWidth = ScrnWidth + 50;
        if (ScrnHeight + 50 < WinHeight) WinHeight = ScrnHeight + 50;

        Config = ChooseGLXConfig(linuxvars.XDisplay, XScreenIndex);
        if(Config.Found)
        {
            swa.colormap = cmap = XCreateColormap(linuxvars.XDisplay,
                                                  RootWindow(linuxvars.XDisplay, Config.BestInfo.screen ), 
                                                  Config.BestInfo.visual, AllocNone);
            swa.background_pixmap = None;
            swa.border_pixel = 0;
            swa.event_mask = StructureNotifyMask;

            linuxvars.XWindow =
            XCreateWindow(linuxvars.XDisplay,
                          RootWindow(linuxvars.XDisplay, Config.BestInfo.screen),
                          0, 0, WinWidth, WinHeight,
                          0, Config.BestInfo.depth, InputOutput, 
                          Config.BestInfo.visual, 
                          CWBorderPixel|CWColormap|CWEventMask, &swa);

            if(linuxvars.XWindow)
            {
                window_setup_success = 1;
                break;
            }
        }
    }

    if (!window_setup_success){
        fprintf(stderr, "Error creating window.\n");
        exit(1);
    }

    //NOTE(inso): Set the window's type to normal
    Atom _NET_WM_WINDOW_TYPE = XInternAtom(linuxvars.XDisplay, "_NET_WM_WINDOW_TYPE", False);
    Atom _NET_WIN_TYPE_NORMAL = XInternAtom(linuxvars.XDisplay, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    XChangeProperty(
        linuxvars.XDisplay,
        linuxvars.XWindow,
        _NET_WM_WINDOW_TYPE,
        XA_ATOM,
        32,
        PropModeReplace,
        (unsigned char*)&_NET_WIN_TYPE_NORMAL,
        1
    );

    //NOTE(inso): window managers want the PID as a window property for some reason.
    Atom _NET_WM_PID = XInternAtom(linuxvars.XDisplay, "_NET_WM_PID", False);
    pid_t pid = getpid();
    XChangeProperty(
        linuxvars.XDisplay,
        linuxvars.XWindow,
        _NET_WM_PID,
        XA_CARDINAL,
        32,
        PropModeReplace,
        (unsigned char*)&pid,
        1
    );

#define WINDOW_NAME "4coder 4linux: " VERSION

    //NOTE(inso): set wm properties
    XStoreName(linuxvars.XDisplay, linuxvars.XWindow, WINDOW_NAME);

    char* win_name_list[] = { WINDOW_NAME };
    XTextProperty win_name;

    XStringListToTextProperty(win_name_list, 1, &win_name);

    XSizeHints *sz_hints = XAllocSizeHints();
    XWMHints   *wm_hints = XAllocWMHints();
    XClassHint *cl_hints = XAllocClassHint();

    if(linuxvars.settings.set_window_pos){
        sz_hints->flags |= USPosition;
        sz_hints->x = linuxvars.settings.window_x;
        sz_hints->y = linuxvars.settings.window_y;
    }

    wm_hints->flags |= InputHint;
    wm_hints->input = True;

    cl_hints->res_name = "4coder";
    cl_hints->res_class = "4coder";

    XSetWMProperties(
        linuxvars.XDisplay,
        linuxvars.XWindow,
        &win_name,
        NULL,
        argv,
        argc,
        sz_hints,
        wm_hints,
        cl_hints
    );

    XFree(sz_hints);
    XFree(wm_hints);
    XFree(cl_hints);

    LinuxSetIcon(linuxvars.XDisplay, linuxvars.XWindow);

    //NOTE(inso): make the window visible
    XMapWindow(linuxvars.XDisplay, linuxvars.XWindow);

    Init_Input_Result input_result = 
    InitializeXInput(linuxvars.XDisplay, linuxvars.XWindow);

    linuxvars.input_method = input_result.input_method;
    linuxvars.input_style = input_result.best_style;
    linuxvars.input_context = input_result.xic;

    b32 IsLegacy = false;
    GLXContext GLContext =
    InitializeOpenGLContext(linuxvars.XDisplay, linuxvars.XWindow, Config.BestConfig, IsLegacy);

    XWindowAttributes WinAttribs;
    if(XGetWindowAttributes(linuxvars.XDisplay, linuxvars.XWindow, &WinAttribs))
    {
        WinWidth = WinAttribs.width;
        WinHeight = WinAttribs.height;
    }

    XRaiseWindow(linuxvars.XDisplay, linuxvars.XWindow);
    XSync(linuxvars.XDisplay, False);

    if (linuxvars.settings.set_window_pos){
        XMoveWindow(
            linuxvars.XDisplay,
            linuxvars.XWindow,
            linuxvars.settings.window_x,
            linuxvars.settings.window_y
        );
    }

    Cursor xcursors[APP_MOUSE_CURSOR_COUNT] = {
        None,
        XCreateFontCursor(linuxvars.XDisplay, XC_arrow),
        XCreateFontCursor(linuxvars.XDisplay, XC_xterm),
        XCreateFontCursor(linuxvars.XDisplay, XC_sb_h_double_arrow),
        XCreateFontCursor(linuxvars.XDisplay, XC_sb_v_double_arrow)
    };

    XSetICFocus(linuxvars.input_context);

    linuxvars.atom_CLIPBOARD = XInternAtom(linuxvars.XDisplay, "CLIPBOARD", False);
    linuxvars.atom_UTF8_STRING = XInternAtom(linuxvars.XDisplay, "UTF8_STRING", False);
    linuxvars.atom_NET_WM_STATE = XInternAtom(linuxvars.XDisplay, "_NET_WM_STATE", False);
    linuxvars.atom_NET_WM_STATE_MAXIMIZED_HORZ = XInternAtom(linuxvars.XDisplay, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    linuxvars.atom_NET_WM_STATE_MAXIMIZED_VERT = XInternAtom(linuxvars.XDisplay, "_NET_WM_STATE_MAXIMIZED_VERT", False);

    if (linuxvars.settings.maximize_window){
        LinuxMaximizeWindow(linuxvars.XDisplay, linuxvars.XWindow, 1);
    }

    int xfixes_version_unused, xfixes_err_unused;

    linuxvars.has_xfixes = XQueryExtension(
        linuxvars.XDisplay,
        "XFIXES",
        &xfixes_version_unused,
        &linuxvars.xfixes_selection_event,
        &xfixes_err_unused
    ) == True;

    if(linuxvars.has_xfixes){
        XFixesSelectSelectionInput(
            linuxvars.XDisplay,
            linuxvars.XWindow,
            linuxvars.atom_CLIPBOARD,
            XFixesSetSelectionOwnerNotifyMask
        );
    }

    linuxvars.atom_WM_DELETE_WINDOW = XInternAtom(linuxvars.XDisplay, "WM_DELETE_WINDOW", False);
    linuxvars.atom_NET_WM_PING = XInternAtom(linuxvars.XDisplay, "_NET_WM_PING", False);

    Atom wm_protos[] = {
        linuxvars.atom_WM_DELETE_WINDOW,
        linuxvars.atom_NET_WM_PING
    };
    XSetWMProtocols(linuxvars.XDisplay, linuxvars.XWindow, wm_protos, 2);

    linuxvars.app.init(linuxvars.system, &linuxvars.target, &memory_vars, &exchange_vars,
                       linuxvars.clipboard_contents, current_directory,
                       linuxvars.custom_api);

    LinuxResizeTarget(WinWidth, WinHeight);

    linuxvars.x11_fd        = ConnectionNumber(linuxvars.XDisplay);
    linuxvars.inotify_fd    = inotify_init1(IN_NONBLOCK);
    linuxvars.step_event_fd = eventfd(0, EFD_NONBLOCK);
    linuxvars.step_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

    linuxvars.epoll = epoll_create(16);

    {
        struct epoll_event e = {};
        e.events = EPOLLIN | EPOLLET;

        e.data.u64 = LINUX_4ED_EVENT_X11;
        epoll_ctl(linuxvars.epoll, EPOLL_CTL_ADD, linuxvars.x11_fd, &e);

        e.data.u64 = LINUX_4ED_EVENT_FILE;
        epoll_ctl(linuxvars.epoll, EPOLL_CTL_ADD, linuxvars.inotify_fd, &e);

        e.data.u64 = LINUX_4ED_EVENT_STEP;
        epoll_ctl(linuxvars.epoll, EPOLL_CTL_ADD, linuxvars.step_event_fd, &e);

        e.data.u64 = LINUX_4ED_EVENT_STEP_TIMER;
        epoll_ctl(linuxvars.epoll, EPOLL_CTL_ADD, linuxvars.step_timer_fd, &e);
    }

    LinuxScheduleStep();
    linuxvars.keep_running = 1;

    while(1){

        struct epoll_event events[16];
        int num_events = epoll_wait(linuxvars.epoll, events, ArrayCount(events), -1);

        if(num_events == -1){
            if(errno != EINTR){
                perror("epoll_wait");
            }
            continue;
        }

        system_acquire_lock(FRAME_LOCK);

        b32 do_step = 0;

        for(int i = 0; i < num_events; ++i){
            switch(events[i].data.u64){
                case LINUX_4ED_EVENT_X11: {
                    LinuxHandleX11Events();
                } break;

                case LINUX_4ED_EVENT_FILE: {
                    LinuxHandleFileEvents();
                } break;

                case LINUX_4ED_EVENT_STEP: {
                    u64 ev;
                    while(read(linuxvars.step_event_fd, &ev, 8) == -1){
                        if(errno != EINTR && errno != EAGAIN){
                            perror("eventfd read");
                            break;
                        }
                    }
                    do_step = 1;
                } break;

                case LINUX_4ED_EVENT_STEP_TIMER: {
                    u64 count;
                    while(read(linuxvars.step_timer_fd, &count, 8) == -1){
                        if(errno != EINTR && errno != EAGAIN){
                            perror("timerfd read");
                            break;
                        }
                    }
                    do_step = 1;
                } break;

                case LINUX_4ED_EVENT_CLI: {
                    LinuxScheduleStep();
                } break;
            }
        }

        if(do_step){
            linuxvars.last_step = system_time();

            // TODO(inso): not all events should require a redraw?
            linuxvars.redraw = 1;

            if(linuxvars.first || !linuxvars.has_xfixes){
                XConvertSelection(
                    linuxvars.XDisplay,
                    linuxvars.atom_CLIPBOARD,
                    linuxvars.atom_UTF8_STRING,
                    linuxvars.atom_CLIPBOARD,
                    linuxvars.XWindow,
                    CurrentTime
                );
            }

            Application_Step_Result result = {};
            result.mouse_cursor_type = APP_MOUSE_CURSOR_DEFAULT;
            result.trying_to_kill = !linuxvars.keep_running;

#if 0
            if(__sync_bool_compare_and_swap(&exchange_vars.thread.force_redraw, 1, 0)){
                linuxvars.redraw = 1;
            }
#endif

            String clipboard = {};
            if(linuxvars.new_clipboard){
                clipboard = linuxvars.clipboard_contents;
                linuxvars.new_clipboard = 0;
            }

            f32 dt = frame_useconds / 1000000.f;

            linuxvars.app.step(
                linuxvars.system,
                &linuxvars.key_data,
                &linuxvars.mouse_data,
                &linuxvars.target,
                &memory_vars,
                &exchange_vars,
                clipboard,
                dt,
                linuxvars.first,
                &result
            );

            if(result.perform_kill){
                break;
            } else {
                linuxvars.keep_running = 1;
            }

            if(result.animating){
                linuxvars.redraw = 1;
                LinuxScheduleStep();
            }

            if(linuxvars.redraw){
                LinuxRedrawTarget();
                linuxvars.redraw = 0;
            }

            if(result.mouse_cursor_type != linuxvars.cursor){
                Cursor c = xcursors[result.mouse_cursor_type];
                XDefineCursor(linuxvars.XDisplay, linuxvars.XWindow, c);
                linuxvars.cursor = result.mouse_cursor_type;
            }

            linuxvars.first = 0;
            linuxvars.redraw = 0;
            linuxvars.key_data = {};
            linuxvars.mouse_data.press_l = 0;
            linuxvars.mouse_data.release_l = 0;
            linuxvars.mouse_data.press_r = 0;
            linuxvars.mouse_data.release_r = 0;
            linuxvars.mouse_data.wheel = 0;

            File_Slot *file;
            int d = 0;

            for (file = exchange_vars.file.active.next;
                file != &exchange_vars.file.active;
                file = file->next){
                ++d;

                if (file->flags & FEx_Save){
                    Assert((file->flags & FEx_Request) == 0);
                    file->flags &= (~FEx_Save);
                    if (system_save_file(file->filename, (char*)file->data, file->size)){
                        file->flags |= FEx_Save_Complete;
                    }
                    else{
                        file->flags |= FEx_Save_Failed;
                    }

                    LinuxScheduleStep();
                }

                if (file->flags & FEx_Request){
                    Assert((file->flags & FEx_Save) == 0);
                    file->flags &= (~FEx_Request);
                    Data sysfile = system_load_file(file->filename);
                    if (sysfile.data == 0){
                        file->flags |= FEx_Not_Exist;
                    }
                    else{
                        file->flags |= FEx_Ready;
                        file->data = sysfile.data;
                        file->size = sysfile.size;
                    }

                    LinuxScheduleStep();
                }
            }

            int free_list_count = 0;
            for (file = exchange_vars.file.free_list.next;
                file != &exchange_vars.file.free_list;
                file = file->next){
                ++free_list_count;
                if (file->data){
                    system_free_memory(file->data);
                }
            }

            if (exchange_vars.file.free_list.next != &exchange_vars.file.free_list){
                Assert(free_list_count != 0);
                ex__insert_range(exchange_vars.file.free_list.next, exchange_vars.file.free_list.prev,
                                 &exchange_vars.file.available);

                exchange_vars.file.num_active -= free_list_count;
            }

            ex__check(&exchange_vars.file);
        }

        system_release_lock(FRAME_LOCK);
    }

    return 0;
}

// BOTTOM

