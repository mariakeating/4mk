/*
4coder_clipboard.cpp - Copy paste commands and clipboard related setup.
*/

// TOP

 function b32
clipboard_post_buffer_range(Application_Links *app, i32 clipboard_index, Buffer_ID buffer, Range_i64 range){
    b32 success = false;
    Scratch_Block scratch(app);
    String_Const_u8 string = push_buffer_range(app, scratch, buffer, range);
    if (string.size > 0){
        clipboard_post(app, clipboard_index, string);
        success = true;
    }
    return(success);
}

CUSTOM_COMMAND_SIG(copy)
CUSTOM_DOC("Copy the text in the range from the cursor to the mark onto the clipboard.")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    Range_i64 range = get_view_range(app, view);
    clipboard_post_buffer_range(app, 0, buffer, range);
}

CUSTOM_COMMAND_SIG(cut)
CUSTOM_DOC("Cut the text in the range from the cursor to the mark onto the clipboard.")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    Range_i64 range = get_view_range(app, view);
    if (clipboard_post_buffer_range(app, 0, buffer, range)){
        buffer_replace_range(app, buffer, range, string_u8_empty);
    }
}

CUSTOM_COMMAND_SIG(paste)
CUSTOM_DOC("At the cursor, insert the text at the top of the clipboard.")
{
    i32 count = clipboard_count(app, 0);
    if (count > 0){
        View_ID view = get_active_view(app, Access_ReadWriteVisible);
        if_view_has_highlighted_range_delete_range(app, view);
        
        Managed_Scope scope = view_get_managed_scope(app, view);
        Rewrite_Type *next_rewrite = scope_attachment(app, scope, view_next_rewrite_loc, Rewrite_Type);
        *next_rewrite = Rewrite_Paste;
        i32 *paste_index = scope_attachment(app, scope, view_paste_index_loc, i32);
        *paste_index = 0;
        
        Scratch_Block scratch(app);
        
        String_Const_u8 string = push_clipboard_index(app, scratch, 0, *paste_index);
        if (string.size > 0){
            Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
            
            i64 pos = view_get_cursor_pos(app, view);
            buffer_replace_range(app, buffer, Ii64(pos), string);
            view_set_mark(app, view, seek_pos(pos));
            view_set_cursor_and_preferred_x(app, view, seek_pos(pos + (i32)string.size));
            
            // TODO(allen): Send this to all views.
            view_post_fade(app, view, 0.667f, Ii64_size(pos, string.size), fcolor_id(Stag_Paste));
        }
    }
}

CUSTOM_COMMAND_SIG(paste_next)
CUSTOM_DOC("If the previous command was paste or paste_next, replaces the paste range with the next text down on the clipboard, otherwise operates as the paste command.")
{
    Scratch_Block scratch(app);
    
    i32 count = clipboard_count(app, 0);
    if (count > 0){
        View_ID view = get_active_view(app, Access_ReadWriteVisible);
        Managed_Scope scope = view_get_managed_scope(app, view);
        no_mark_snap_to_cursor(app, scope);
        
        Rewrite_Type *rewrite = scope_attachment(app, scope, view_rewrite_loc, Rewrite_Type);
        if (*rewrite == Rewrite_Paste){
            Rewrite_Type *next_rewrite = scope_attachment(app, scope, view_next_rewrite_loc, Rewrite_Type);
            *next_rewrite = Rewrite_Paste;
            
            i32 *paste_index_ptr = scope_attachment(app, scope, view_paste_index_loc, i32);
            i32 paste_index = (*paste_index_ptr) + 1;
            *paste_index_ptr = paste_index;
            
            String_Const_u8 string = push_clipboard_index(app, scratch, 0, paste_index);
            
            Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
            
            Range_i64 range = get_view_range(app, view);
            i64 pos = range.min;
            
            buffer_replace_range(app, buffer, range, string);
            view_set_cursor_and_preferred_x(app, view, seek_pos(pos + string.size));
            
            view_post_fade(app, view, 0.667f, Ii64_size(pos, string.size), fcolor_id(Stag_Paste));
        }
        else{
            paste(app);
        }
    }
}

CUSTOM_COMMAND_SIG(paste_and_indent)
CUSTOM_DOC("Paste from the top of clipboard and run auto-indent on the newly pasted text.")
{
    paste(app);
    auto_indent_range(app);
}

CUSTOM_COMMAND_SIG(paste_next_and_indent)
CUSTOM_DOC("Paste the next item on the clipboard and run auto-indent on the newly pasted text.")
{
    paste_next(app);
    auto_indent_range(app);
}

// BOTTOM

