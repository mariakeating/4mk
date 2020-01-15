/* Mac Objective-C layer for 4coder */

#define FPS 60
#define frame_useconds (1000000 / FPS)

#include "4coder_base_types.h"
#include "4coder_version.h"
#include "4coder_events.h"

#include "4coder_table.h"

// NOTE(allen): This is a very unfortunate hack, but hopefully there will never be a need to use the Marker
// type in the platform layer. If that changes then instead change the name of Marker and make a transition
// macro that is only included in custom code.
#define Marker Marker__SAVE_THIS_IDENTIFIER
#include "4coder_types.h"
#undef Marker

#include "4coder_default_colors.h"

#include "4coder_system_types.h"
#define STATIC_LINK_API
#include "generated/system_api.h"

#include "4ed_font_interface.h"
#define STATIC_LINK_API
#include "generated/graphics_api.h"
#define STATIC_LINK_API
#include "generated/font_api.h"

#include "4ed_font_set.h"
#include "4ed_render_target.h"
#include "4ed_search_list.h"
#include "4ed.h"

#include "generated/system_api.cpp"
#include "generated/graphics_api.cpp"
#include "generated/font_api.cpp"

#include "4coder_base_types.cpp"
#include "4coder_stringf.cpp"
#include "4coder_events.cpp"
#include "4coder_hash_functions.cpp"
#include "4coder_table.cpp"
#include "4coder_log.cpp"

#include "4ed_search_list.cpp"

#include "mac_objective_c_to_cpp_links.h"

#undef function
#undef internal
#undef global
#undef external
#import <Cocoa/Cocoa.h>

#include <libproc.h> // NOTE(yuval): Used for proc_pidpath
#include <Carbon/Carbon.h> // NOTE(yuval): Used for virtual key codes
#include <mach/mach_time.h> // NOTE(yuval): Used for mach_absolute_time, mach_timebase_info, mach_timebase_info_data_t

#include <dirent.h> // NOTE(yuval): Used for opendir, readdir
#include <dlfcn.h> // NOTE(yuval): Used for dlopen, dlclose, dlsym
#include <errno.h> // NOTE(yuval): Used for errno
#include <fcntl.h> // NOTE(yuval): Used for open
#include <pthread.h> // NOTE(yuval): Used for threads, mutexes, cvs
#include <unistd.h> // NOTE(yuval): Used for getcwd, read, write, getpid
#include <sys/mman.h> // NOTE(yuval): Used for mmap, munmap, mprotect
#include <sys/stat.h> // NOTE(yuval): Used for stat
#include <sys/types.h> // NOTE(yuval): Used for struct stat, pid_t

#include <stdlib.h> // NOTE(yuval): Used for free

#define function static
#define internal static
#define global static
#define external extern "C"

struct Control_Keys{
    b8 l_ctrl;
    b8 r_ctrl;
    b8 l_alt;
    b8 r_alt;
    b8 l_shift;
    b8 r_shift;
    b8 l_command;
    b8 r_command;
};

struct Mac_Input_Chunk_Transient{
    Input_List event_list;
    b8 mouse_l_press;
    b8 mouse_l_release;
    b8 mouse_r_press;
    b8 mouse_r_release;
    b8 out_of_window;
    i8 mouse_wheel;
    b8 trying_to_kill;
};

struct Mac_Input_Chunk_Persistent{
    Vec2_i32 mouse;
    Control_Keys controls;
    Input_Modifier_Set_Fixed modifiers;
    b8 mouse_l;
    b8 mouse_r;
};

struct Mac_Input_Chunk{
    Mac_Input_Chunk_Transient trans;
    Mac_Input_Chunk_Persistent pers;
};

////////////////////////////////

#define SLASH '/'
#define DLL "so"

#include "4coder_hash_functions.cpp"
#include "4coder_system_allocator.cpp"
#include "4coder_malloc_allocator.cpp"
#include "4coder_codepoint_map.cpp"

#include "4ed_mem.cpp"
#include "4ed_font_set.cpp"

////////////////////////////////

@interface FCoder_App_Delegate : NSObject<NSApplicationDelegate>
@end

@interface FCoder_Window_Delegate : NSObject<NSWindowDelegate>
- (void)process_focus_event;
@end

@interface FCoder_View : NSView
- (void)request_display;
- (void)check_clipboard;
- (void)process_keyboard_event:(NSEvent*)event down:(b8)down;
- (void)process_mouse_move_event:(NSEvent*)event;
@end

////////////////////////////////

typedef i32 Mac_Object_Kind;
enum{
    MacObjectKind_ERROR = 0,
    MacObjectKind_Timer = 1,
    MacObjectKind_Thread = 2,
    MacObjectKind_Mutex = 3,
    MacObjectKind_CV = 4,
};

struct Mac_Object{
    Node node;
    Mac_Object_Kind kind;
    
    union{
        NSTimer* timer;
        
        struct{
            pthread_t thread;
            Thread_Function *proc;
            void *ptr;
        } thread;
        
        pthread_mutex_t mutex;
        pthread_cond_t cv;
    };
};

struct Mac_Vars {
    i32 width, height;
    
    Thread_Context *tctx;
    
    Arena *frame_arena;
    Input_Event *active_key_stroke;
    Input_Event *active_text_input;
    Mac_Input_Chunk input_chunk;
    b8 lctrl_lalt_is_altgr;
    
    b8 full_screen;
    b8 do_toggle;
    b32 send_exit_signal;
    
    i32 cursor_show;
    i32 prev_cursor_show;
    NSCursor *cursor_ibeam;
    NSCursor *cursor_arrow;
    NSCursor *cursor_leftright;
    NSCursor *cursor_updown;
    
    String_Const_u8 binary_path;
    
    Arena *clipboard_arena;
    String_Const_u8 clipboard_contents;
    u32 clipboard_change_count;
    b32 next_clipboard_is_self;
    
    Arena clip_post_arena;
    String_Const_u8 clip_post;
    
    NSWindow *window;
    FCoder_View *view;
    f32 screen_scale_factor;
    
    mach_timebase_info_data_t timebase_info;
    b32 first;
    void *base_ptr;
    
    u64 timer_start;
    b32 step_requested;
    i32 running_cli;
    
    Node free_mac_objects;
    Node timer_objects;
    
    pthread_mutex_t thread_launch_mutex;
    pthread_cond_t thread_launch_cv;
    b32 waiting_for_launch;
    
    System_Mutex global_frame_mutex;
    
    Log_Function *log_string;
};

////////////////////////////////

#include "mac_4ed_renderer.h"

////////////////////////////////

global Mac_Vars mac_vars;
global Mac_Renderer *renderer;
global Render_Target target;
global App_Functions app;

////////////////////////////////

function Mac_Object*
mac_alloc_object(Mac_Object_Kind kind){
    Mac_Object *result = 0;
    
    if (mac_vars.free_mac_objects.next != &mac_vars.free_mac_objects){
        result = CastFromMember(Mac_Object, node, mac_vars.free_mac_objects.next);
    }
    
    if (!result){
        i32 count = 512;
        Mac_Object *objects = (Mac_Object*)system_memory_allocate(count * sizeof(Mac_Object), file_name_line_number_lit_u8);
        
        // NOTE(yuval): Link the first node of the dll to the sentinel
        objects[0].node.prev = &mac_vars.free_mac_objects;
        mac_vars.free_mac_objects.next = &objects[0].node;
        
        // NOTE(yuval): Link all dll nodes to each other
        for (i32 chain_index = 1; chain_index < count; chain_index += 1){
            objects[chain_index - 1].node.next = &objects[chain_index].node;
            objects[chain_index].node.prev = &objects[chain_index - 1].node;
        }
        
        // NOTE(yuval): Link the last node of the dll to the sentinel
        objects[count - 1].node.next = &mac_vars.free_mac_objects;
        mac_vars.free_mac_objects.prev = &objects[count - 1].node;
        
        result = CastFromMember(Mac_Object, node, mac_vars.free_mac_objects.next);
    }
    
    Assert(result);
    dll_remove(&result->node);
    block_zero_struct(result);
    result->kind = kind;
    
    return(result);
}

function void
mac_free_object(Mac_Object *object){
    if (object->node.next != 0){
        dll_remove(&object->node);
    }
    
    dll_insert(&mac_vars.free_mac_objects, &object->node);
}

function inline Plat_Handle
mac_to_plat_handle(Mac_Object *object){
    Plat_Handle result = *(Plat_Handle*)(&object);
    return(result);
}

function inline Mac_Object*
mac_to_object(Plat_Handle handle){
    Mac_Object *result = *(Mac_Object**)(&handle);
    return(result);
}

////////////////////////////////

function void
mac_error_box(char *msg, b32 shutdown = true){
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    
    NSString *title_string = @"Error";
    NSString *message_string = [NSString stringWithUTF8String:msg];
    [alert setMessageText:title_string];
    [alert setInformativeText:message_string];
    
    [alert runModal];
    
    if (shutdown){
        exit(1);
    }
}

////////////////////////////////

#if defined(FRED_INTERNAL)
function inline void
mac_profile(char *name, u64 begin, u64 end){
    printf("%s Time: %fs\n", name, ((end - begin) / 1000000.0f));
}

#define MacProfileScope(name) for (u64 glue(_i_, __LINE__) = 0, glue(_begin_, __LINE__) = system_now_time();\
glue(_i_, __LINE__) == 0;\
glue(_i_, __LINE__) = 1, mac_profile(name, glue(_begin_, __LINE__), system_now_time()))
#else
# define mac_profile(...)
# define MacProfileScope(...)
#endif

////////////////////////////////

#import "mac_4ed_renderer.mm"

#include "4ed_font_provider_freetype.h"
#include "4ed_font_provider_freetype.cpp"

#import "mac_4ed_functions.mm"

////////////////////////////////

global_const u8 kVK_Menu = 0x6E;

global Key_Code keycode_lookup_table[255] = {};

function void
mac_keycode_init(void){
    keycode_lookup_table[kVK_ANSI_A] = KeyCode_A;
    keycode_lookup_table[kVK_ANSI_B] = KeyCode_B;
    keycode_lookup_table[kVK_ANSI_C] = KeyCode_C;
    keycode_lookup_table[kVK_ANSI_D] = KeyCode_D;
    keycode_lookup_table[kVK_ANSI_E] = KeyCode_E;
    keycode_lookup_table[kVK_ANSI_F] = KeyCode_F;
    keycode_lookup_table[kVK_ANSI_G] = KeyCode_G;
    keycode_lookup_table[kVK_ANSI_H] = KeyCode_H;
    keycode_lookup_table[kVK_ANSI_I] = KeyCode_I;
    keycode_lookup_table[kVK_ANSI_J] = KeyCode_J;
    keycode_lookup_table[kVK_ANSI_K] = KeyCode_K;
    keycode_lookup_table[kVK_ANSI_L] = KeyCode_L;
    keycode_lookup_table[kVK_ANSI_M] = KeyCode_M;
    keycode_lookup_table[kVK_ANSI_N] = KeyCode_N;
    keycode_lookup_table[kVK_ANSI_O] = KeyCode_O;
    keycode_lookup_table[kVK_ANSI_P] = KeyCode_P;
    keycode_lookup_table[kVK_ANSI_Q] = KeyCode_Q;
    keycode_lookup_table[kVK_ANSI_R] = KeyCode_R;
    keycode_lookup_table[kVK_ANSI_S] = KeyCode_S;
    keycode_lookup_table[kVK_ANSI_T] = KeyCode_T;
    keycode_lookup_table[kVK_ANSI_U] = KeyCode_U;
    keycode_lookup_table[kVK_ANSI_V] = KeyCode_V;
    keycode_lookup_table[kVK_ANSI_W] = KeyCode_W;
    keycode_lookup_table[kVK_ANSI_X] = KeyCode_X;
    keycode_lookup_table[kVK_ANSI_Y] = KeyCode_Y;
    keycode_lookup_table[kVK_ANSI_Z] = KeyCode_Z;
    
    keycode_lookup_table[kVK_ANSI_0] = KeyCode_0;
    keycode_lookup_table[kVK_ANSI_1] = KeyCode_1;
    keycode_lookup_table[kVK_ANSI_2] = KeyCode_2;
    keycode_lookup_table[kVK_ANSI_3] = KeyCode_3;
    keycode_lookup_table[kVK_ANSI_4] = KeyCode_4;
    keycode_lookup_table[kVK_ANSI_5] = KeyCode_5;
    keycode_lookup_table[kVK_ANSI_6] = KeyCode_6;
    keycode_lookup_table[kVK_ANSI_7] = KeyCode_7;
    keycode_lookup_table[kVK_ANSI_8] = KeyCode_8;
    keycode_lookup_table[kVK_ANSI_9] = KeyCode_9;
    
    keycode_lookup_table[kVK_Space] = KeyCode_Space;
    keycode_lookup_table[kVK_ANSI_Grave] = KeyCode_Tick;
    keycode_lookup_table[kVK_ANSI_Minus] = KeyCode_Minus;
    keycode_lookup_table[kVK_ANSI_LeftBracket] = KeyCode_LeftBracket;
    keycode_lookup_table[kVK_ANSI_RightBracket] = KeyCode_RightBracket;
    keycode_lookup_table[kVK_ANSI_Semicolon] = KeyCode_Semicolon;
    keycode_lookup_table[kVK_ANSI_Quote] = KeyCode_Quote;
    keycode_lookup_table[kVK_ANSI_Comma] = KeyCode_Comma;
    keycode_lookup_table[kVK_ANSI_Period] = KeyCode_Period;
    keycode_lookup_table[kVK_ANSI_Slash] = KeyCode_ForwardSlash;
    keycode_lookup_table[kVK_ANSI_Backslash] = KeyCode_BackwardSlash;
    
    keycode_lookup_table[kVK_Tab] = KeyCode_Tab;
    // NOTE(yuval): No Pause key on macOS!
    keycode_lookup_table[kVK_Escape] = KeyCode_Escape;
    
    keycode_lookup_table[kVK_UpArrow] = KeyCode_Up;
    keycode_lookup_table[kVK_DownArrow] = KeyCode_Down;
    keycode_lookup_table[kVK_LeftArrow] = KeyCode_Left;
    keycode_lookup_table[kVK_RightArrow] = KeyCode_Right;
    
    keycode_lookup_table[kVK_Delete] = KeyCode_Backspace;
    keycode_lookup_table[kVK_Return] = KeyCode_Return;
    
    keycode_lookup_table[kVK_ForwardDelete] = KeyCode_Delete;
    //keycode_lookup_table[] = KeyCode_Insert; // TODO(yuval): Figure how to get keyDown events for the insert key
    keycode_lookup_table[kVK_Home] = KeyCode_Home;
    keycode_lookup_table[kVK_End] = KeyCode_End;
    keycode_lookup_table[kVK_PageUp] = KeyCode_PageUp;
    keycode_lookup_table[kVK_PageDown] = KeyCode_PageDown;
    
    keycode_lookup_table[kVK_CapsLock] = KeyCode_CapsLock;
    keycode_lookup_table[kVK_ANSI_KeypadClear] = KeyCode_NumLock;
    // NOTE(yuval): No Scroll Lock key on macOS!
    keycode_lookup_table[kVK_Menu] = KeyCode_Menu;
    
    keycode_lookup_table[kVK_Shift] = KeyCode_Shift;
    keycode_lookup_table[kVK_RightShift] = KeyCode_Shift;
    
    keycode_lookup_table[kVK_Control] = KeyCode_Control;
    keycode_lookup_table[kVK_RightControl] = KeyCode_Control;
    
    keycode_lookup_table[kVK_Option] = KeyCode_Alt;
    keycode_lookup_table[kVK_RightOption] = KeyCode_Alt;
    
    keycode_lookup_table[kVK_Command] = KeyCode_Command;
    keycode_lookup_table[kVK_RightCommand] = KeyCode_Command; // NOTE(yuval): Right Command
    
    keycode_lookup_table[kVK_F1] = KeyCode_F1;
    keycode_lookup_table[kVK_F2] = KeyCode_F2;
    keycode_lookup_table[kVK_F3] = KeyCode_F3;
    keycode_lookup_table[kVK_F4] = KeyCode_F4;
    keycode_lookup_table[kVK_F5] = KeyCode_F5;
    keycode_lookup_table[kVK_F6] = KeyCode_F6;
    keycode_lookup_table[kVK_F7] = KeyCode_F7;
    keycode_lookup_table[kVK_F8] = KeyCode_F8;
    keycode_lookup_table[kVK_F9] = KeyCode_F9;
    
    keycode_lookup_table[kVK_F10] = KeyCode_F10;
    keycode_lookup_table[kVK_F11] = KeyCode_F11;
    keycode_lookup_table[kVK_F12] = KeyCode_F12;
    keycode_lookup_table[kVK_F13] = KeyCode_F13;
    keycode_lookup_table[kVK_F14] = KeyCode_F14;
    keycode_lookup_table[kVK_F15] = KeyCode_F15;
    keycode_lookup_table[kVK_F16] = KeyCode_F16;
}

////////////////////////////////

function b32
mac_file_can_be_made(u8* filename){
    b32 result = access((char*)filename, W_OK) == 0;
    return(result);
}

////////////////////////////////

function void
mac_resize(float width, float height){
    if ((width > 0.0f) && (height > 0.0f)){
#if 1
        NSSize coord_size = NSMakeSize(width, height);
        NSSize backing_size = [mac_vars.view convertSizeToBacking:coord_size];
        
        mac_vars.width = (i32)backing_size.width;
        mac_vars.height = (i32)backing_size.height;
        
        target.width = (i32)backing_size.width;
        target.height = (i32)backing_size.height;
#else
        mac_vars.width = (i32)width;
        mac_vars.height = (i32)height;
        
        target.width = (i32)width;
        target.height = (i32)height;
#endif
    }
    
    system_signal_step(0);
}

function inline void
mac_resize(NSWindow *window){
    NSRect bounds = [[window contentView] bounds];
    mac_resize(bounds.size.width, bounds.size.height);
}

////////////////////////////////

function u32
mac_get_clipboard_change_count(void){
    NSPasteboard *board = [NSPasteboard generalPasteboard];
    u32 result = board.changeCount;
    
    return(result);
}

function b32
mac_read_clipboard_contents(Arena *scratch){
    b32 result = false;
    
    Temp_Memory temp = begin_temp(scratch);
    {
        NSPasteboard *board = [NSPasteboard generalPasteboard];
        NSString *utf8_type = @"public.utf8-plain-text";
        NSArray *types_array = [NSArray arrayWithObjects:utf8_type, nil];
        NSString *has_string = [board availableTypeFromArray:types_array];
        if (has_string != nil){
            NSData *data = [board dataForType:utf8_type];
            if (data != nil){
                u32 copy_length = data.length;
                if (copy_length > 0){
                    Arena *clip_arena = mac_vars.clipboard_arena;
                    linalloc_clear(clip_arena);
                    
                    mac_vars.clipboard_contents = string_const_u8_push(clip_arena, copy_length);
                    [data getBytes:mac_vars.clipboard_contents.str
                            length:mac_vars.clipboard_contents.size];
                    
                    result = true;
                }
            }
        }
    }
    end_temp(temp);
    
    return(result);
}

function void
mac_post_clipboard(Arena *scratch, char *text, i32 len){
    NSPasteboard *board = [NSPasteboard generalPasteboard];
    
    NSString *utf8_type = @"public.utf8-plain-text";
    NSArray<NSString*> *types_array = [NSArray arrayWithObjects:utf8_type, nil];
    [board declareTypes:types_array
            owner:nil];
    
    NSString *paste_string = [[NSString alloc] initWithBytes:text
            length:len
            encoding:NSUTF8StringEncoding];
    [board setString:paste_string
            forType:utf8_type];
    [paste_string release];
    
    mac_vars.next_clipboard_is_self = true;
}

////////////////////////////////

function void
mac_toggle_fullscreen(void){
    [mac_vars.window toggleFullScreen:nil];
}

////////////////////////////////

@implementation FCoder_App_Delegate
- (void)applicationDidFinishLaunching:(id)sender{
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender{
    return(YES);
}

- (void)applicationWillTerminate:(NSNotification*)notification{
}
@end

@implementation FCoder_Window_Delegate
- (BOOL)windowShouldClose:(id)sender{
    mac_vars.input_chunk.trans.trying_to_kill = true;
    system_signal_step(0);
    
    return(NO);
}

- (void)windowDidResize:(NSNotification*)notification{
    mac_resize(mac_vars.window);
}

- (void)windowDidMiniaturize:(NSNotification*)notification{
}

- (void)windowDidDeminiaturize:(NSNotification*)notification{
}

- (void)windowDidBecomeKey:(NSNotification *)notification{
    // NOTE(yuval): The window is the focused window
    [self process_focus_event];
}

- (void)windowDidResignKey:(NSNotification *)notification{
    // NOTE(yuval): The window has lost focus
    [self process_focus_event];
}

- (void)process_focus_event{
    mac_vars.input_chunk.pers.mouse_l = false;
    mac_vars.input_chunk.pers.mouse_r = false;
    block_zero_struct(&mac_vars.input_chunk.pers.controls);
    block_zero_struct(&mac_vars.input_chunk.pers.modifiers);
    mac_vars.active_key_stroke = 0;
    mac_vars.active_text_input = 0;
    
    system_signal_step(0);
}
@end

@implementation FCoder_View
- (id)init{
    self = [super init];
    return(self);
}

- (void)dealloc{
    [super dealloc];
}

- (void)viewDidChangeBackingProperties{
    // TODO(yuval): If the screen scale factor changed, modify the current face to use the new screen scale factor.
    mac_resize(mac_vars.window);
}

- (BOOL)wantsUpdateLayer
{
    return YES;
}

- (void)updateLayer{
    u64 prev_timer_start;
    
    MacProfileScope("Draw Rect"){
        mac_vars.step_requested = false;
        
        MacProfileScope("Acquire Frame Mutex"){
            // NOTE(yuval): Read comment in win32_4ed.cpp's main loop
            system_mutex_acquire(mac_vars.global_frame_mutex);
        }
        
        Application_Step_Input input = {};
        
        // NOTE(yuval): Prepare the Frame Input
        MacProfileScope("Prepare Input"){
            Mac_Input_Chunk input_chunk = mac_vars.input_chunk;
            
            input.first_step = mac_vars.first;
            input.dt = frame_useconds / 1000000.0f;
            input.events = input_chunk.trans.event_list;
            
            input.mouse.out_of_window = input_chunk.trans.out_of_window;
            
            input.mouse.l = input_chunk.pers.mouse_l;
            input.mouse.press_l = input_chunk.trans.mouse_l_press;
            input.mouse.release_l = input_chunk.trans.mouse_l_release;
            
            input.mouse.r = input_chunk.pers.mouse_r;
            input.mouse.press_r = input_chunk.trans.mouse_r_press;
            input.mouse.release_r = input_chunk.trans.mouse_r_release;
            
            input.mouse.wheel = input_chunk.trans.mouse_wheel;
            input.mouse.p = input_chunk.pers.mouse;
            
            input.trying_to_kill = input_chunk.trans.trying_to_kill;
            
            block_zero_struct(&mac_vars.input_chunk.trans);
            mac_vars.active_key_stroke = 0;
            mac_vars.active_text_input = 0;
            
            // NOTE(yuval): See comment in win32_4ed.cpp's main loop
            if (mac_vars.send_exit_signal){
                input.trying_to_kill = true;
                mac_vars.send_exit_signal = false;
            }
        }
        
        // NOTE(yuval): Frame clipboard input
        MacProfileScope("Frame Clipboard Input"){
            block_zero_struct(&mac_vars.clipboard_contents);
            input.clipboard_changed = false;
            
            if (mac_vars.clipboard_change_count != 0){
                u32 change_count = mac_get_clipboard_change_count();
                if (change_count != mac_vars.clipboard_change_count){
                    if (mac_vars.next_clipboard_is_self){
                        mac_vars.next_clipboard_is_self = false;
                    } else{
                        for (i32 r = 0; r < 4; ++r){
                            Scratch_Block scratch(mac_vars.tctx, Scratch_Share);
                            
                            if (mac_read_clipboard_contents(scratch)){
                                input.clipboard_changed = true;
                                break;
                            }
                        }
                    }
                    
                    mac_vars.clipboard_change_count = change_count;
                }
            }
            
            input.clipboard = mac_vars.clipboard_contents;
        }
        
        mac_vars.clip_post.size = 0;
        
        // NOTE(yuval): Application Core Update
        Application_Step_Result result = {};
        MacProfileScope("Step"){
            if (app.step != 0){
                result = app.step(mac_vars.tctx, &target, mac_vars.base_ptr, &input);
            }
        }
        
        // NOTE(yuval): Quit the app if requested by the application core
        MacProfileScope("Perform Kill"){
            if (result.perform_kill){
                [NSApp terminate:nil];
            }
        }
        
        // NOTE(yuval): Post new clipboard content
        MacProfileScope("Post Clipboard"){
            if (mac_vars.clip_post.size > 0){
                Scratch_Block scratch(mac_vars.tctx, Scratch_Share);
                mac_post_clipboard(scratch, (char*)mac_vars.clip_post.str, (i32)mac_vars.clip_post.size);
            }
        }
        
        // NOTE(yuval): Switch to a new title
        MacProfileScope("Switch Title"){
            if (result.has_new_title){
                NSString *str = [NSString stringWithUTF8String:result.title_string];
                [mac_vars.window setTitle:str];
            }
        }
        
        // NOTE(yuval): Switch to new cursor
        MacProfileScope("Switch Cursor"){
            // NOTE(yuval): Switch cursor type
            switch (result.mouse_cursor_type){
                case APP_MOUSE_CURSOR_ARROW:
                {
                    [mac_vars.cursor_arrow set];
                } break;
                
                case APP_MOUSE_CURSOR_IBEAM:
                {
                    [mac_vars.cursor_ibeam set];
                } break;
                
                case APP_MOUSE_CURSOR_LEFTRIGHT:
                {
                    [mac_vars.cursor_leftright set];
                } break;
                
                case APP_MOUSE_CURSOR_UPDOWN:
                {
                    [mac_vars.cursor_updown set];
                } break;
            }
            
            // NOTE(yuval): Show or hide cursor
            if (mac_vars.cursor_show != mac_vars.prev_cursor_show){
                switch (mac_vars.cursor_show){
                    case MouseCursorShow_Never:
                    {
                        [NSCursor hide];
                    } break;
                    
                    case MouseCursorShow_Always:
                    {
                        [NSCursor unhide];
                    } break;
                }
                
                mac_vars.prev_cursor_show = mac_vars.cursor_show;
            }
        }
        
        // NOTE(yuval): Update lctrl_lalt_is_altgr status
        mac_vars.lctrl_lalt_is_altgr = (b8)result.lctrl_lalt_is_altgr;
        
        // NOTE(yuval): Render
        MacProfileScope("Render"){
            renderer->render(renderer, &target);
        }
        
        // NOTE(yuval): Toggle full screen
        MacProfileScope("Toggle Full Screen"){
            if (mac_vars.do_toggle){
                mac_toggle_fullscreen();
                mac_vars.do_toggle = false;
            }
        }
        
        // NOTE(yuval): Schedule another step if needed
        MacProfileScope("Schedule Step"){
            if (result.animating || (mac_vars.running_cli > 0)){
                system_signal_step(0);
            }
        }
        
        // NOTE(yuval): Sleep a bit to cool off
        MacProfileScope("Cool Down"){
            system_mutex_release(mac_vars.global_frame_mutex);
            {
                u64 timer_end = system_now_time();
                u64 end_target = (mac_vars.timer_start + frame_useconds);
                
                if (timer_end < end_target){
                    if ((end_target - timer_end) > 1000){
                        // NOTE(yuval): Sleep until the end target minus a millisecond (to allow the scheduler to wake the process in time)
                        system_sleep(end_target - timer_end - 1000);
                    }
                    
                    // NOTE(yuval): Iterate through the rest of the time that's left using a regular for loop to make sure that we hit the end target
                    u64 now = system_now_time();
                    while (now < end_target){
                        now = system_now_time();
                    }
                }
                
                prev_timer_start = mac_vars.timer_start;
                mac_vars.timer_start = system_now_time();
            }
            system_mutex_acquire(mac_vars.global_frame_mutex);
        }
        
        MacProfileScope("Cleanup"){
            mac_vars.first = false;
            
            linalloc_clear(mac_vars.frame_arena);
            
            // NOTE(yuval): Release the global frame mutex until the next drawRect call
            system_mutex_release(mac_vars.global_frame_mutex);
        }
    }
    
    mac_profile("Frame", prev_timer_start, mac_vars.timer_start);
#if FRED_INTERNAL
    printf("\n");
#endif
}

- (BOOL)acceptsFirstResponder{
    return(YES);
}

- (BOOL)becomeFirstResponder{
    return(YES);
}

- (BOOL)resignFirstResponder{
    return(YES);
}

- (void)keyDown:(NSEvent*)event{
    // NOTE(yuval): Process keyboard event
    [self process_keyboard_event:event down:true];
    
    // NOTE(yuval): Process TextInsert event
    {
        NSString *characters = [event characters];
        if ([characters length] > 0){
            // NOTE(yuval): Get the first utf-16 character
            u32 c = [characters characterAtIndex:0];
            if (c == '\r'){
                c = '\n';
            }
            
            // NOTE(yuval): Check for a valid text input
            if ((c > 127) || ((' ' <= c) && (c <= '~')) || (c == '\t') || (c == '\n') || (c == '\r')){
                String_Const_u16 str_16 = SCu16((u16*)&c, 1);
                String_Const_u8 str_8 = string_u8_from_string_u16(mac_vars.frame_arena, str_16).string;
                
                Input_Event *event = push_input_event(mac_vars.frame_arena, &mac_vars.input_chunk.trans.event_list);
                event->kind = InputEventKind_TextInsert;
                event->text.string = str_8;
                event->text.next_text = 0;
                event->text.blocked = false;
                if (mac_vars.active_text_input){
                    mac_vars.active_text_input->text.next_text = event;
                } else if (mac_vars.active_key_stroke){
                    mac_vars.active_key_stroke->key.first_dependent_text = event;
                }
                
                mac_vars.active_text_input = event;
                
                system_signal_step(0);
            }
        }
    }
}

- (void)keyUp:(NSEvent*)event{
    [self process_keyboard_event:event down:false];
}

- (void)flagsChanged:(NSEvent *)event{
    NSEventModifierFlags flags = [event modifierFlags];
    b8 ctrl_pressed = ((flags & NSEventModifierFlagControl) != 0);
    b8 alt_pressed = ((flags & NSEventModifierFlagOption) != 0);
    b8 shift_pressed = ((flags & NSEventModifierFlagShift) != 0);
    b8 command_pressed = ((flags & NSEventModifierFlagCommand) != 0);
    
    Control_Keys *controls = &mac_vars.input_chunk.pers.controls;
    u16 event_key_code = [event keyCode];
    if (event_key_code == kVK_Control){
        controls->l_ctrl = ctrl_pressed;
        [self process_keyboard_event:event down:ctrl_pressed];
    } else if (event_key_code == kVK_RightControl){
        controls->r_ctrl = ctrl_pressed;
        [self process_keyboard_event:event down:ctrl_pressed];
    } else if (event_key_code == kVK_Option){
        controls->l_alt = alt_pressed;
        [self process_keyboard_event:event down:alt_pressed];
    }  else if (event_key_code == kVK_RightOption){
        controls->r_alt = alt_pressed;
        [self process_keyboard_event:event down:alt_pressed];
    } else if (event_key_code == kVK_Shift){
        controls->l_shift = shift_pressed;
        [self process_keyboard_event:event down:shift_pressed];
    } else if (event_key_code == kVK_RightShift){
        controls->r_shift = shift_pressed;
        [self process_keyboard_event:event down:shift_pressed];
    } else if (event_key_code == kVK_Command){
        controls->l_command = command_pressed;
        [self process_keyboard_event:event down:command_pressed];
    } else if (event_key_code == kVK_RightCommand){
        controls->r_command = command_pressed;
        [self process_keyboard_event:event down:command_pressed];
    }
}

- (void)mouseMoved:(NSEvent*)event{
    [self process_mouse_move_event:event];
}

- (void)mouseDragged:(NSEvent*)event{
    [self process_mouse_move_event:event];
}

- (void)scrollWheel:(NSEvent *)event{
    float dx = event.scrollingDeltaX;
    float dy = event.scrollingDeltaY;
    
    i8 scroll_speed = 100;
    if (dy > 0){
        scroll_speed *= -1;
    }
    mac_vars.input_chunk.trans.mouse_wheel = scroll_speed;
    
    system_signal_step(0);
}

- (void)mouseDown:(NSEvent*)event{
    mac_vars.input_chunk.trans.mouse_l_press = true;
    mac_vars.input_chunk.pers.mouse_l = true;
    
    system_signal_step(0);
}

- (void)mouseUp:(NSEvent*)event{
    mac_vars.input_chunk.trans.mouse_l_release = true;
    mac_vars.input_chunk.pers.mouse_l = false;
    
    system_signal_step(0);
}

- (void)rightMouseDown:(NSEvent*)event{
    [super rightMouseDown:event];
    
    mac_vars.input_chunk.trans.mouse_r_press = true;
    mac_vars.input_chunk.pers.mouse_r = true;
    
    system_signal_step(0);
}

- (void)rightMouseUp:(NSEvent*)event{
    mac_vars.input_chunk.trans.mouse_r_release = true;
    mac_vars.input_chunk.pers.mouse_r = false;
    
    system_signal_step(0);
}

- (void)request_display{
    CGRect cg_rect = CGRectMake(0, 0, mac_vars.width, mac_vars.height);
    NSRect rect = NSRectFromCGRect(cg_rect);
    [self setNeedsDisplayInRect:rect];
}

- (void)check_clipboard{
    u32 change_count = mac_get_clipboard_change_count();
    if (change_count != mac_vars.clipboard_change_count){
        system_signal_step(0);
    }
}

- (void)process_keyboard_event:(NSEvent*)event down:(b8)down{
    b8 release = !down;
    
    Input_Modifier_Set_Fixed *mods = &mac_vars.input_chunk.pers.modifiers;
    
    // NOTE(yuval): Set control modifiers
    {
        Control_Keys *controls = &mac_vars.input_chunk.pers.controls;
        
        b8 ctrl = (controls->r_ctrl || (controls->l_ctrl && !controls->r_alt));
        b8 alt = (controls->l_alt || (controls->r_alt && !controls->l_ctrl));
        if (mac_vars.lctrl_lalt_is_altgr && controls->l_alt && controls->l_ctrl){
            ctrl = false;
            alt = false;
        }
        
        b8 shift = (controls->r_shift || controls->l_shift);
        b8 command = (controls->r_command || controls->l_command);
        
        set_modifier(mods, KeyCode_Control, ctrl);
        set_modifier(mods, KeyCode_Alt, alt);
        set_modifier(mods, KeyCode_Shift, shift);
        set_modifier(mods, KeyCode_Command, command);
    }
    
    // NOTE(yuval): Process KeyStroke / KeyRelease event
    {
        u16 event_key_code = [event keyCode];
        Key_Code key = keycode_lookup_table[(u8)event_key_code];
        if (down){
            if (key != 0){
                add_modifier(mods, key);
                
                Input_Event *event = push_input_event(mac_vars.frame_arena, &mac_vars.input_chunk.trans.event_list);
                event->kind = InputEventKind_KeyStroke;
                event->key.code = key;
                event->key.modifiers = copy_modifier_set(mac_vars.frame_arena, mods);
                
                mac_vars.active_key_stroke = event;
                
                system_signal_step(0);
            }
        } else{
            mac_vars.active_key_stroke = 0;
            mac_vars.active_text_input = 0;
            
            if (key != 0){
                Input_Event *event = push_input_event(mac_vars.frame_arena, &mac_vars.input_chunk.trans.event_list);
                event->kind = InputEventKind_KeyRelease;
                event->key.code = key;
                event->key.modifiers = copy_modifier_set(mac_vars.frame_arena, mods);
                
                remove_modifier(mods, key);
            }
            
            system_signal_step(0);
        }
    }
}

- (void)process_mouse_move_event:(NSEvent*)event{
    NSPoint location = [event locationInWindow];
    NSPoint backing_location = [self convertPointToBacking:location];
    
    Vec2_i32 new_m = V2i32(backing_location.x, mac_vars.height - backing_location.y);
    if (new_m != mac_vars.input_chunk.pers.mouse){
        mac_vars.input_chunk.pers.mouse = new_m;
        
        Rect_i32 screen = Ri32(0, 0, target.width, target.height);
        mac_vars.input_chunk.trans.out_of_window = !rect_contains_point(screen, new_m);
        
    }
    
    system_signal_step(0);
}
@end

////////////////////////////////

int
main(int arg_count, char **args){
    @autoreleasepool{
        // NOTE(yuval): Create NSApplication & Delegate
        [NSApplication sharedApplication];
        Assert(NSApp != nil);
        
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        FCoder_App_Delegate *app_delegate = [[FCoder_App_Delegate alloc] init];
        [NSApp setDelegate:app_delegate];
        
        pthread_mutex_init(&memory_tracker_mutex, 0);
        
        // NOTE(yuval): Context setup
        Thread_Context _tctx = {};
        thread_ctx_init(&_tctx, ThreadKind_Main,
                        get_base_allocator_system(),
                        get_base_allocator_system());
        
        block_zero_struct(&mac_vars);
        mac_vars.tctx = &_tctx;
        
        API_VTable_system system_vtable = {};
        system_api_fill_vtable(&system_vtable);
        
        API_VTable_graphics graphics_vtable = {};
        graphics_api_fill_vtable(&graphics_vtable);
        
        API_VTable_font font_vtable = {};
        font_api_fill_vtable(&font_vtable);
        
        // NOTE(yuval): Memory
        mac_vars.frame_arena = reserve_arena(mac_vars.tctx);
        target.arena = make_arena_system(KB(256));
        
        dll_init_sentinel(&mac_vars.free_mac_objects);
        dll_init_sentinel(&mac_vars.timer_objects);
        
        pthread_mutex_init(&mac_vars.thread_launch_mutex, 0);
        pthread_cond_init(&mac_vars.thread_launch_cv, 0);
        
        // NOTE(yuval): Screen scale factor calculation
        {
            NSScreen* screen = [NSScreen mainScreen];
            NSDictionary* desc = [screen deviceDescription];
            NSSize size = [[desc valueForKey:NSDeviceResolution] sizeValue];
            f32 max_dpi = Max(size.width, size.height);
            mac_vars.screen_scale_factor = (max_dpi / 72.0f);
        }
        
        // NOTE(yuval): Load core
        System_Library core_library = {};
        {
            App_Get_Functions *get_funcs = 0;
            Scratch_Block scratch(mac_vars.tctx, Scratch_Share);
            Path_Search_List search_list = {};
            search_list_add_system_path(scratch, &search_list, SystemPath_Binary);
            
            String_Const_u8 core_path = get_full_path(scratch, &search_list, SCu8("4ed_app.so"));
            if (system_load_library(scratch, core_path, &core_library)){
                get_funcs = (App_Get_Functions*)system_get_proc(core_library, "app_get_functions");
                if (get_funcs != 0){
                    app = get_funcs();
                }
                else{
                    char msg[] = "Failed to get application code from '4ed_app.so'.";
                    mac_error_box(msg);
                }
            }
            else{
                char msg[] = "Could not load '4ed_app.so'. This file should be in the same directory as the main '4ed' executable.";
                mac_error_box(msg);
            }
        }
        
        // NOTE(yuval): Send api vtables to core
        app.load_vtables(&system_vtable, &font_vtable, &graphics_vtable);
        mac_vars.log_string = app.get_logger();
        
        // NOTE(yuval): Init & command line parameters
        Plat_Settings plat_settings = {};
        mac_vars.base_ptr = 0;
        {
            Scratch_Block scratch(mac_vars.tctx, Scratch_Share);
            String_Const_u8 curdir = system_get_path(scratch, SystemPath_CurrentDirectory);
            curdir = string_mod_replace_character(curdir, '\\', '/');
            char **files = 0;
            i32 *file_count = 0;
            mac_vars.base_ptr = app.read_command_line(mac_vars.tctx, curdir, &plat_settings, &files, &file_count, arg_count, args);
            {
                i32 end = *file_count;
                i32 i = 0, j = 0;
                for (; i < end; ++i){
                    if (mac_file_can_be_made((u8*)files[i])){
                        files[j] = files[i];
                        ++j;
                    }
                }
                *file_count = j;
            }
        }
        
        // NOTE(yuval): Load custom layer
        System_Library custom_library = {};
        Custom_API custom = {};
        {
            char custom_not_found_msg[] = "Did not find a library for the custom layer.";
            char custom_fail_version_msg[] = "Failed to load custom code due to missing version information or a version mismatch.  Try rebuilding with buildsuper.";
            char custom_fail_init_apis[] = "Failed to load custom code due to missing 'init_apis' symbol.  Try rebuilding with buildsuper";
            
            Scratch_Block scratch(mac_vars.tctx, Scratch_Share);
            String_Const_u8 default_file_name = string_u8_litexpr("custom_4coder.so");
            Path_Search_List search_list = {};
            search_list_add_system_path(scratch, &search_list, SystemPath_CurrentDirectory);
            search_list_add_system_path(scratch, &search_list, SystemPath_Binary);
            String_Const_u8 custom_file_names[2] = {};
            i32 custom_file_count = 1;
            if (plat_settings.custom_dll != 0){
                custom_file_names[0] = SCu8(plat_settings.custom_dll);
                if (!plat_settings.custom_dll_is_strict){
                    custom_file_names[1] = default_file_name;
                    custom_file_count += 1;
                }
            }
            else{
                custom_file_names[0] = default_file_name;
            }
            String_Const_u8 custom_file_name = {};
            for (i32 i = 0; i < custom_file_count; i += 1){
                custom_file_name = get_full_path(scratch, &search_list, custom_file_names[i]);
                if (custom_file_name.size > 0){
                    break;
                }
            }
            b32 has_library = false;
            if (custom_file_name.size > 0){
                if (system_load_library(scratch, custom_file_name, &custom_library)){
                    has_library = true;
                }
            }
            
            if (!has_library){
                mac_error_box(custom_not_found_msg);
            }
            custom.get_version = (_Get_Version_Type*)system_get_proc(custom_library, "get_version");
            if (custom.get_version == 0 || custom.get_version(MAJOR, MINOR, PATCH) == 0){
                mac_error_box(custom_fail_version_msg);
            }
            custom.init_apis = (_Init_APIs_Type*)system_get_proc(custom_library, "init_apis");
            if (custom.init_apis == 0){
                mac_error_box(custom_fail_init_apis);
            }
        }
        
        //
        // Window and Renderer Initialization
        //
        
        // NOTE(yuval): Create Window & Window Delegate
        i32 w;
        i32 h;
        if (plat_settings.set_window_size){
            w = plat_settings.window_w;
            h = plat_settings.window_h;
        } else{
            w = 800;
            h = 600;
        }
        
        NSRect screen_rect = [[NSScreen mainScreen] frame];
        NSRect initial_frame = NSMakeRect((f32)(screen_rect.size.width - w) * 0.5f, (f32)(screen_rect.size.height - h) * 0.5f, w, h);
        
        u32 style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
        
        mac_vars.window = [[NSWindow alloc] initWithContentRect:initial_frame
                styleMask:style_mask
                backing:NSBackingStoreBuffered
                defer:NO];
        
        FCoder_Window_Delegate *window_delegate = [[FCoder_Window_Delegate alloc] init];
        [mac_vars.window setDelegate:window_delegate];
        
        [mac_vars.window setMinSize:NSMakeSize(100, 100)];
        [mac_vars.window setBackgroundColor:NSColor.blackColor];
        [mac_vars.window setTitle:@"GRAPHICS"];
        [mac_vars.window setAcceptsMouseMovedEvents:YES];
        
        NSView* content_view = [mac_vars.window contentView];
        
        // NOTE(yuval): Create the 4coder view
        mac_vars.view = [[FCoder_View alloc] init];
        [mac_vars.view setFrame:[content_view bounds]];
        [mac_vars.view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        mac_vars.view.wantsLayer = true;
        
        // NOTE(yuval): Display window and view
        [content_view addSubview:mac_vars.view];
        [mac_vars.window makeKeyAndOrderFront:nil];
        
        // NOTE(yuval): Initialize the renderer
        renderer = mac_init_renderer(MacRenderer_OpenGL, mac_vars.window, &target);
        
        mac_resize(w, h);
        
        //
        // NOTE(yuval): Misc System Initializations
        //
        
        // NOTE(yuval): Initialize clipboard
        {
            mac_vars.clipboard_arena = reserve_arena(mac_vars.tctx);
            mac_vars.clipboard_change_count = mac_get_clipboard_change_count();
            mac_vars.next_clipboard_is_self = false;
            
            // NOTE(yuval): Read the current clipboard
            {
                Scratch_Block scratch(mac_vars.tctx, Scratch_Share);
                mac_read_clipboard_contents(scratch);
            }
            
            // NOTE(yuval): Start the clipboard polling timer
            [NSTimer scheduledTimerWithTimeInterval: 0.5
                    target:mac_vars.view
                    selector:@selector(check_clipboard)
                    userInfo:nil repeats:YES];
        }
        
        // NOTE(yuval): Initialize the virtul keycodes table
        mac_keycode_init();
        
        // NOTE(yuval): Initialize cursors
        {
            mac_vars.cursor_show = MouseCursorShow_Always;
            mac_vars.prev_cursor_show = MouseCursorShow_Always;
            
            mac_vars.cursor_arrow = [NSCursor arrowCursor];
            mac_vars.cursor_ibeam = [NSCursor IBeamCursor];
            mac_vars.cursor_leftright = [NSCursor resizeLeftRightCursor];
            mac_vars.cursor_updown = [NSCursor resizeUpDownCursor];
        }
        
        // NOTE(yuval): Get the timebase info
        mach_timebase_info(&mac_vars.timebase_info);
        
        //
        // App init
        //
        
        {
            Scratch_Block scratch(mac_vars.tctx, Scratch_Share);
            String_Const_u8 curdir = system_get_path(scratch, SystemPath_CurrentDirectory);
            curdir = string_mod_replace_character(curdir, '\\', '/');
            app.init(mac_vars.tctx, &target, mac_vars.base_ptr, mac_vars.clipboard_contents, curdir, custom);
        }
        
        //
        // Start Main Loop
        //
        
        mac_vars.first = true;
        mac_vars.step_requested = false;
        mac_vars.running_cli = 0;
        
        if (plat_settings.fullscreen_window){
            mac_toggle_fullscreen();
        }
        
        mac_vars.global_frame_mutex = system_mutex_make();
        
        mac_vars.timer_start = system_now_time();
        
        // NOTE(yuval): Start the app's run loop
        [NSApp run];
    }
}