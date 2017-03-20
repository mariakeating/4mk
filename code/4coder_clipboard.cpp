/*
4coder_clipboard.cpp - Copy paste commands and clipboard related setup.

TYPE: 'drop-in-command-pack'
*/

// TOP

#if !defined(FCODER_CLIPBOARD_CPP)
#define FCODER_CLIPBOARD_CPP

#include "4coder_default_framework.h"

#include "4coder_helper/4coder_helper.h"

static bool32
clipboard_copy(Application_Links *app, size_t start, size_t end, Buffer_Summary *buffer_out, uint32_t access){
    View_Summary view = get_active_view(app, access);
    Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
    bool32 result = 0;
    
    if (buffer.exists){
        if (0 <= start && start <= end && end <= buffer.size){
            size_t size = (end - start);
            char *str = (char*)app->memory;
            
            if (size <= app->memory_size){
                buffer_read_range(app, &buffer, start, end, str);
                clipboard_post(app, 0, str, size);
                if (buffer_out){*buffer_out = buffer;}
                result = 1;
            }
        }
    }
    
    return(result);
}

static bool32
clipboard_cut(Application_Links *app, size_t start, size_t end, Buffer_Summary *buffer_out, uint32_t access){
    Buffer_Summary buffer = {0};
    bool32 result = false;
    
    if (clipboard_copy(app, start, end, &buffer, access)){
        buffer_replace_range(app, &buffer, start, end, 0, 0);
        if (buffer_out){
            *buffer_out = buffer;
        }
    }
    
    return(result);
}

CUSTOM_COMMAND_SIG(copy){
    uint32_t access = AccessProtected;
    View_Summary view = get_active_view(app, access);
    Range range = get_range(&view);
    clipboard_copy(app, range.min, range.max, 0, access);
}

CUSTOM_COMMAND_SIG(cut){
    uint32_t access = AccessOpen;
    View_Summary view = get_active_view(app, access);
    Range range = get_range(&view);
    clipboard_cut(app, range.min, range.max, 0, access);
}

CUSTOM_COMMAND_SIG(paste){
    uint32_t access = AccessOpen;
    int32_t count = clipboard_count(app, 0);
    if (count > 0){
        View_Summary view = get_active_view(app, access);
        
        view_paste_index[view.view_id].next_rewrite = RewritePaste;
        
        int32_t paste_index = 0;
        view_paste_index[view.view_id].index = paste_index;
        
        size_t len = clipboard_index(app, 0, paste_index, 0, 0);
        char *str = 0;
        
        if (len <= app->memory_size){
            str = (char*)app->memory;
        }
        
        if (str){
            clipboard_index(app, 0, paste_index, str, len);
            
            Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
            size_t pos = view.cursor.pos;
            buffer_replace_range(app, &buffer, pos, pos, str, len);
            view_set_mark(app, &view, seek_pos(pos));
            view_set_cursor(app, &view, seek_pos(pos + len), true);
            
            // TODO(allen): Send this to all views.
            Theme_Color paste;
            paste.tag = Stag_Paste;
            get_theme_colors(app, &paste, 1);
            view_post_fade(app, &view, 0.667f, pos, pos + len, paste.color);
        }
    }
}

CUSTOM_COMMAND_SIG(paste_next){
    uint32_t access = AccessOpen;
    int32_t count = clipboard_count(app, 0);
    if (count > 0){
        View_Summary view = get_active_view(app, access);
        
        if (view_paste_index[view.view_id].rewrite == RewritePaste){
            view_paste_index[view.view_id].next_rewrite = RewritePaste;
            
            int32_t paste_index = view_paste_index[view.view_id].index + 1;
            view_paste_index[view.view_id].index = paste_index;
            
            size_t len = clipboard_index(app, 0, paste_index, 0, 0);
            char *str = 0;
            
            if (len <= app->memory_size){
                str = (char*)app->memory;
            }
            
            if (str){
                clipboard_index(app, 0, paste_index, str, len);
                
                Buffer_Summary buffer = get_buffer(app, view.buffer_id, access);
                Range range = get_range(&view);
                size_t pos = range.min;
                
                buffer_replace_range(app, &buffer, range.min, range.max, str, len);
                view_set_cursor(app, &view, seek_pos(pos + len), true);
                
                // TODO(allen): Send this to all views.
                Theme_Color paste;
                paste.tag = Stag_Paste;
                get_theme_colors(app, &paste, 1);
                view_post_fade(app, &view, 0.667f, pos, pos + len, paste.color);
            }
        }
        else{
            exec_command(app, paste);
        }
    }
}

#endif

// BOTTOM

