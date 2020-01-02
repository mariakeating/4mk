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

@interface FCoderAppDelegate : NSObject<NSApplicationDelegate>
@end

@interface FCoderWindowDelegate : NSObject<NSWindowDelegate>
@end

@interface FCoderView : NSView
- (void)requestDisplay;
@end

////////////////////////////////

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
    Mac_Input_Chunk input_chunk;
    
    b8 full_screen;
    b8 do_toggle;
    b32 send_exit_signal;
    
    i32 cursor_show;
    i32 prev_cursor_show;
    
    String_Const_u8 binary_path;
    
    String_Const_u8 clipboard_contents;
    
    NSWindow *window;
    FCoderView *view;
    f32 screen_scale_factor;
    
    mach_timebase_info_data_t timebase_info;
    b32 first;
    void *base_ptr;
    u64 timer_start;
    
    Node free_mac_objects;
    Node timer_objects;
    
    pthread_mutex_t thread_launch_mutex;
    pthread_cond_t thread_launch_cv;
    b32 waiting_for_launch;
    
    System_Mutex global_frame_mutex;
    
    Log_Function *log_string;
};

////////////////////////////////

global Mac_Vars mac_vars;
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

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#import "mac_4ed_opengl.mm"

#include "4ed_font_provider_freetype.h"
#include "4ed_font_provider_freetype.cpp"

#import "mac_4ed_functions.mm"

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

function b32
mac_file_can_be_made(u8* filename){
    b32 result = access((char*)filename, W_OK) == 0;
    return(result);
}

function void
mac_resize(float width, float height){
    if ((width > 0.0f) && (height > 0.0f)){
        NSSize coord_size = NSMakeSize(width, height);
        NSSize backing_size = [mac_vars.view convertSizeToBacking:coord_size];
        
        mac_vars.width = (i32)backing_size.width;
        mac_vars.height = (i32)backing_size.height;
        
        target.width = (i32)backing_size.width;
        target.height = (i32)backing_size.height;
    }
    
    system_signal_step(0);
}

function inline void
mac_resize(NSWindow *window){
    NSRect bounds = [[window contentView] bounds];
    mac_resize(bounds.size.width, bounds.size.height);
}

////////////////////////////////

// TODO(yuval): mac_resize(bounds.size.width, bounds.size.height);

@implementation FCoderAppDelegate
- (void)applicationDidFinishLaunching:(id)sender{
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender{
    return(YES);
}

- (void)applicationWillTerminate:(NSNotification*)notification{
}
@end

@implementation FCoderWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    mac_vars.input_chunk.trans.trying_to_kill = true;
    system_signal_step(0);
    
    return(NO);
}

- (void)windowDidResize:(NSNotification*)notification {
    mac_resize(mac_vars.window);
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
}
@end

@implementation FCoderView
- (id)init{
    self = [super init];
    return(self);
}

- (void)dealloc{
    [super dealloc];
}

- (void)viewDidChangeBackingProperties{
    // NOTE(yuval): Screen scale factor calculation
    printf("Backing changed!\n");
    mac_resize(mac_vars.window);
}

- (void)drawRect:(NSRect)bounds{
    // NOTE(yuval): Read comment in win32_4ed.cpp's main loop
    system_mutex_release(mac_vars.global_frame_mutex);
    
    // NOTE(yuval): Prepare the Frame Input
    Mac_Input_Chunk input_chunk = mac_vars.input_chunk;
    Application_Step_Input input = {};
    
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
    
    // NOTE(yuval): See comment in win32_4ed.cpp's main loop
    if (mac_vars.send_exit_signal){
        input.trying_to_kill = true;
        mac_vars.send_exit_signal = false;
    }
    
    // NOTE(yuval): Application Core Update
    Application_Step_Result result = {};
    if (app.step != 0){
        result = app.step(mac_vars.tctx, &target, mac_vars.base_ptr, &input);
    }
    
    // NOTE(yuval): Quit the app
    if (result.perform_kill){
        printf("Terminating 4coder!\n");
        [NSApp terminate:nil];
    }
    
    mac_gl_render(&target);
    
    mac_vars.first = false;
    
    linalloc_clear(mac_vars.frame_arena);
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

- (void)keyDown:(NSEvent *)event{
    NSString* characters = [event characters];
    if ([characters length] != 0) {
        u32 character_code = [characters characterAtIndex:0];
        
        // NOTE(yuval): Control characters generate character_codes < 32
        if (character_code > 31) {
            // TODO(yuval): This is actually in utf16!!!
            String_Const_u32 str_32 = SCu32(&character_code, 1);
            String_Const_u8 str_8 = string_u8_from_string_u32(mac_vars.frame_arena, str_32).string;
            
            Input_Event event = {};
            event.kind = InputEventKind_TextInsert;
            event.text.string = str_8;
            push_input_event(mac_vars.frame_arena, &mac_vars.input_chunk.trans.event_list, &event);
            
            system_signal_step(0);
        }
    }
}

- (void)mouseMoved:(NSEvent*)event{
}

- (void)mouseDown:(NSEvent*)event{
}

- (void)requestDisplay{
    CGRect cg_rect = CGRectMake(0, 0, mac_vars.width, mac_vars.height);
    NSRect rect = NSRectFromCGRect(cg_rect);
    [self setNeedsDisplayInRect:rect];
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
        
        FCoderAppDelegate *app_delegate = [[FCoderAppDelegate alloc] init];
        [NSApp setDelegate:app_delegate];
        
        pthread_mutex_init(&memory_tracker_mutex, 0);
        
        // NOTE(yuval): Context Setup
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
        
        mac_vars.cursor_show = MouseCursorShow_Always;
        mac_vars.prev_cursor_show = MouseCursorShow_Always;
        
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
        // Window and GL View Initialization
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
        
        FCoderWindowDelegate *window_delegate = [[FCoderWindowDelegate alloc] init];
        [mac_vars.window setDelegate:window_delegate];
        
        [mac_vars.window setMinSize:NSMakeSize(100, 100)];
        [mac_vars.window setBackgroundColor:NSColor.blackColor];
        [mac_vars.window setTitle:@WINDOW_NAME];
        [mac_vars.window setAcceptsMouseMovedEvents:YES];
        
        NSView* content_view = [mac_vars.window contentView];
        
        // NOTE(yuval): Initialize the renderer
        mac_gl_init(mac_vars.window);
        
        // NOTE(yuval): Create the 4coder view
        mac_vars.view = [[FCoderView alloc] init];
        [mac_vars.view setFrame:[content_view bounds]];
        [mac_vars.view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        
        [content_view addSubview:mac_vars.view];
        [mac_vars.window makeKeyAndOrderFront:nil];
        
        mac_resize(w, h);
        
        //
        // TODO(yuval): Misc System Initializations
        //
        
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
        // Main loop
        //
        
        mac_vars.first = true;
        
        mac_vars.global_frame_mutex = system_mutex_make();
        system_mutex_acquire(mac_vars.global_frame_mutex);
        
        mac_vars.timer_start = system_now_time();
        
        // NOTE(yuval): Start the app's run loop
#if 1
        printf("Running using NSApp run\n");
        [NSApp run];
#else
        printf("Running using manual event loop\n");
        
        for (;;) {
            u64 count = 0;
            
            NSEvent* event;
            do {
                event = [NSApp nextEventMatchingMask:NSEventMaskAny
                        untilDate:[NSDate distantFuture]
                        inMode:NSDefaultRunLoopMode
                        dequeue:YES];
                
                [NSApp sendEvent:event];
            } while (event != nil);
        }
#endif
    }
}