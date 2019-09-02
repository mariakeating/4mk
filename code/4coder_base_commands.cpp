/*
4coder_base_commands.cpp - Base commands such as inserting characters, and 
moving the cursor, which work even without the default 4coder framework.
*/

// TOP

static void
write_character_parameter(Application_Links *app, u8 *character, u32 length){
    if (length != 0){
        View_ID view = get_active_view(app, AccessOpen);
        if_view_has_highlighted_range_delete_range(app, view);
        
        i64 pos = view_get_cursor_pos(app, view);
        pos = view_get_character_legal_pos_from_pos(app, view, pos);
        
        Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
        
        // NOTE(allen): consecutive inserts merge logic
        History_Record_Index first_index = buffer_history_get_current_state_index(app, buffer);
        b32 do_merge = false;
        if (character[0] != '\n'){
            Record_Info record = get_single_record(app, buffer, first_index);
            if (record.error == RecordError_NoError && record.kind == RecordKind_Single){
                String_Const_u8 string = record.single.string_forward;
                i32 last_end = (i32)(record.single.first + string.size);
                if (last_end == pos && string.size > 0){
                    char c = string.str[string.size - 1];
                    if (c != '\n'){
                        if (character_is_whitespace(character[0]) && character_is_whitespace(c)){
                            do_merge = true;
                        }
                        else if (character_is_alpha_numeric(character[0]) && character_is_alpha_numeric(c)){
                            do_merge = true;
                        }
                    }
                }
            }
        }
        
        // NOTE(allen): perform the edit
        b32 edit_success = buffer_replace_range(app, buffer, Ii64(pos), SCu8(character, length));
        
        // NOTE(allen): finish merging records if necessary
        if (do_merge){
            History_Record_Index last_index = buffer_history_get_current_state_index(app, buffer);
            buffer_history_merge_record_range(app, buffer, first_index, last_index, RecordMergeFlag_StateInRange_MoveStateForward);
        }
        
        // NOTE(allen): finish updating the cursor
        if (edit_success){
            view_set_cursor(app, view, seek_pos(pos + length), true);
        }
    }
}

CUSTOM_COMMAND_SIG(write_character)
CUSTOM_DOC("Inserts whatever character was used to trigger this command.")
{
    User_Input in = get_command_input(app);
    u8 character[4];
    u32 length = to_writable_character(in, character);
    write_character_parameter(app, character, length);
}

CUSTOM_COMMAND_SIG(write_underscore)
CUSTOM_DOC("Inserts an underscore.")
{
    u8 character = '_';
    write_character_parameter(app, &character, 1);
}

CUSTOM_COMMAND_SIG(delete_char)
CUSTOM_DOC("Deletes the character to the right of the cursor.")
{
    View_ID view = get_active_view(app, AccessOpen);
    if (!if_view_has_highlighted_range_delete_range(app, view)){
        Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
        i64 start = view_get_cursor_pos(app, view);
        i64 buffer_size = buffer_get_size(app, buffer);
        if (0 <= start && start < buffer_size){
            Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(start));
            i64 character = view_relative_character_from_pos(app, view, cursor.line, cursor.pos);
            i64 end = view_pos_from_relative_character(app, view, cursor.line, character + 1);
            buffer_replace_range(app, buffer, Ii64(start, end), string_u8_empty);
        }
    }
}

CUSTOM_COMMAND_SIG(backspace_char)
CUSTOM_DOC("Deletes the character to the left of the cursor.")
{
    View_ID view = get_active_view(app, AccessOpen);
    if (!if_view_has_highlighted_range_delete_range(app, view)){
        Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
        i64 end = view_get_cursor_pos(app, view);
        i64 buffer_size = buffer_get_size(app, buffer);
        if (0 < end && end <= buffer_size){
            Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(end));
            i64 character = view_relative_character_from_pos(app, view, cursor.line, cursor.pos);
            i64 start = view_pos_from_relative_character(app, view, cursor.line, character - 1);
            if (buffer_replace_range(app, buffer, Ii64(start, end), string_u8_empty)){
                view_set_cursor(app, view, seek_pos(start), true);
            }
        }
    }
}

CUSTOM_COMMAND_SIG(set_mark)
CUSTOM_DOC("Sets the mark to the current position of the cursor.")
{
    View_ID view = get_active_view(app, AccessProtected);
    i64 pos = view_get_cursor_pos(app, view);
    view_set_mark(app, view, seek_pos(pos));
    view_set_cursor(app, view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(cursor_mark_swap)
CUSTOM_DOC("Swaps the position of the cursor and the mark.")
{
    View_ID view = get_active_view(app, AccessProtected);
    i64 cursor = view_get_cursor_pos(app, view);
    i64 mark = view_get_mark_pos(app, view);
    view_set_cursor(app, view, seek_pos(mark), true);
    view_set_mark(app, view, seek_pos(cursor));
}

CUSTOM_COMMAND_SIG(delete_range)
CUSTOM_DOC("Deletes the text in the range between the cursor and the mark.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    Range_i64 range = get_view_range(app, view);
    buffer_replace_range(app, buffer, range, string_u8_empty);
}

static void
current_view_boundary_delete(Application_Links *app, Scan_Direction direction, Boundary_Function_List funcs){
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    Range_i64 range = {};
    range.first = view_get_cursor_pos(app, view);
    range.one_past_last = scan(app, funcs, buffer, direction, range.first);
    range = rectify(range);
    buffer_replace_range(app, buffer, range, string_u8_empty);
}

CUSTOM_COMMAND_SIG(backspace_alpha_numeric_boundary)
CUSTOM_DOC("Delete characters between the cursor position and the first alphanumeric boundary to the left.")
{
    Scratch_Block scratch(app);
    current_view_boundary_delete(app, Scan_Backward,
                                 push_boundary_list(scratch, boundary_alpha_numeric));
}

CUSTOM_COMMAND_SIG(delete_alpha_numeric_boundary)
CUSTOM_DOC("Delete characters between the cursor position and the first alphanumeric boundary to the right.")
{
    Scratch_Block scratch(app);
    current_view_boundary_delete(app, Scan_Forward,
                                 push_boundary_list(scratch, boundary_alpha_numeric));
}

#define backspace_word backspace_alpha_numeric_boundary
#define delete_word    delete_alpha_numeric_boundary

static void
current_view_snipe_delete(Application_Links *app, Scan_Direction direction, Boundary_Function_List funcs){
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    i64 pos = view_get_cursor_pos(app, view);
    Range_i64 range = get_snipe_range(app, funcs, buffer, pos, direction);
    buffer_replace_range(app, buffer, range, string_u8_litexpr(""));
}

CUSTOM_COMMAND_SIG(snipe_backward_whitespace_or_token_boundary)
CUSTOM_DOC("Delete a single, whole token on or to the left of the cursor and post it to the clipboard.")
{
    Scratch_Block scratch(app);
    current_view_snipe_delete(app, Scan_Backward,
                              push_boundary_list(scratch, boundary_token, boundary_non_whitespace));
}

CUSTOM_COMMAND_SIG(snipe_forward_whitespace_or_token_boundary)
CUSTOM_DOC("Delete a single, whole token on or to the right of the cursor and post it to the clipboard.")
{
    Scratch_Block scratch(app);
    current_view_snipe_delete(app, Scan_Forward,
                              push_boundary_list(scratch, boundary_token, boundary_non_whitespace));
}

#define snipe_token_or_word       snipe_backward_whitespace_or_token_boundary
#define snipe_token_or_word_right snipe_forward_whitespace_or_token_boundary

////////////////////////////////

CUSTOM_COMMAND_SIG(center_view)
CUSTOM_DOC("Centers the view vertically on the line on which the cursor sits.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Rect_f32 region = view_get_buffer_region(app, view);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    f32 view_height = rect_height(region);
    Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
    scroll.target.line_number = cursor.line;
    scroll.target.pixel_shift.y = -view_height*0.5f;
    view_set_buffer_scroll(app, view, scroll);
}

CUSTOM_COMMAND_SIG(left_adjust_view)
CUSTOM_DOC("Sets the left size of the view near the x position of the cursor.")
{
    View_ID view = get_active_view(app, AccessProtected);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    Vec2_f32 p = view_relative_xy_of_pos(app, view, cursor.line, pos);
    Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
    scroll.target.pixel_shift.x = clamp_bot(0.f, p.x - 30.f);
    view_set_buffer_scroll(app, view, scroll);
}

CUSTOM_COMMAND_SIG(click_set_cursor_and_mark)
CUSTOM_DOC("Sets the cursor position and mark to the mouse position.")
{
    View_ID view = get_active_view(app, AccessProtected);
    if (!view_is_in_ui_mode(app, view)){
        Mouse_State mouse = get_mouse_state(app);
        i64 pos = view_pos_from_xy(app, view, V2(mouse.p));
        view_set_cursor(app, view, seek_pos(pos), true);
        view_set_mark(app, view, seek_pos(pos));
    }
}

CUSTOM_COMMAND_SIG(click_set_cursor)
CUSTOM_DOC("Sets the cursor position to the mouse position.")
{
    View_ID view = get_active_view(app, AccessProtected);
    if (!view_is_in_ui_mode(app, view)){
        Mouse_State mouse = get_mouse_state(app);
        i64 pos = view_pos_from_xy(app, view, V2(mouse.p));
        view_set_cursor(app, view, seek_pos(pos), true);
        no_mark_snap_to_cursor(app, view);
    }
}

CUSTOM_COMMAND_SIG(click_set_cursor_if_lbutton)
CUSTOM_DOC("If the mouse left button is pressed, sets the cursor position to the mouse position.")
{
    View_ID view = get_active_view(app, AccessProtected);
    if (!view_is_in_ui_mode(app, view)){
        Mouse_State mouse = get_mouse_state(app);
        if (mouse.l){
            i64 pos = view_pos_from_xy(app, view, V2(mouse.p));
            view_set_cursor(app, view, seek_pos(pos), true);
        }
        no_mark_snap_to_cursor(app, view);
    }
}

CUSTOM_COMMAND_SIG(click_set_mark)
CUSTOM_DOC("Sets the mark position to the mouse position.")
{
    View_ID view = get_active_view(app, AccessProtected);
    if (!view_is_in_ui_mode(app, view)){
        Mouse_State mouse = get_mouse_state(app);
        i64 pos = view_pos_from_xy(app, view, V2(mouse.p));
        view_set_mark(app, view, seek_pos(pos));
        no_mark_snap_to_cursor(app, view);
    }
}

CUSTOM_COMMAND_SIG(mouse_wheel_scroll)
CUSTOM_DOC("Reads the scroll wheel value from the mouse state and scrolls accordingly.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Mouse_State mouse = get_mouse_state(app);
    if (mouse.wheel != 0){
        if (view_is_in_ui_mode(app, view)){
            Basic_Scroll scroll = view_get_basic_scroll(app, view);
            scroll.target.y += mouse.wheel;
            view_set_basic_scroll(app, view, scroll);
        }
        else{
            Buffer_Scroll scroll = view_get_buffer_scroll(app, view);
            scroll.target = view_move_buffer_point(app, view, scroll.target, V2f32(0.f, (f32)mouse.wheel));
            view_set_buffer_scroll(app, view, scroll);
        }
    }
}

////////////////////////////////

internal void
move_vertical_pixels(Application_Links *app, View_ID view, f32 pixels){
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    Vec2_f32 p = view_relative_xy_of_pos(app, view, cursor.line, pos);
    p.y += pixels;
    i64 new_pos = view_pos_at_relative_xy(app, view, cursor.line, p);
    view_set_cursor(app, view, seek_pos(new_pos), false);
    no_mark_snap_to_cursor_if_shift(app, view);
}

internal void
move_vertical_pixels(Application_Links *app, f32 pixels){
    View_ID view = get_active_view(app, AccessProtected);
    move_vertical_pixels(app, view, pixels);
}

internal void
move_vertical_lines(Application_Links *app, View_ID view, f32 lines){
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics metrics = get_face_metrics(app, face_id);
    
    f32 delta_y = lines*metrics.line_height;
    move_vertical_pixels(app, delta_y);
}

internal void
move_vertical_lines(Application_Links *app, f32 lines){
    View_ID view = get_active_view(app, AccessProtected);
    move_vertical_lines(app, view, lines);
}

#define move_vertical move_vertical_lines

internal f32
get_page_jump(Application_Links *app, View_ID view){
    Rect_f32 region = view_get_buffer_region(app, view);
    return(rect_height(region)*.9f);
}

CUSTOM_COMMAND_SIG(move_up)
CUSTOM_DOC("Moves the cursor up one line.")
{
    move_vertical(app, -1.f);
}

CUSTOM_COMMAND_SIG(move_down)
CUSTOM_DOC("Moves the cursor down one line.")
{
    move_vertical(app, 1.f);
}

CUSTOM_COMMAND_SIG(move_up_10)
CUSTOM_DOC("Moves the cursor up ten lines.")
{
    move_vertical(app, -10.f);
}

CUSTOM_COMMAND_SIG(move_down_10)
CUSTOM_DOC("Moves the cursor down ten lines.")
{
    move_vertical(app, 10.f);
}

CUSTOM_COMMAND_SIG(move_down_textual)
CUSTOM_DOC("Moves down to the next line of actual text, regardless of line wrapping.")
{
    View_ID view = get_active_view(app, AccessOpen);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    i64 next_line = cursor.line + 1;
    view_set_cursor(app, view, seek_line_col(next_line, 1), true);
}

CUSTOM_COMMAND_SIG(page_up)
CUSTOM_DOC("Scrolls the view up one view height and moves the cursor up one view height.")
{
    View_ID view = get_active_view(app, AccessProtected);
    f32 page_jump = get_page_jump(app, view);
    move_vertical_pixels(app, -page_jump);
}

CUSTOM_COMMAND_SIG(page_down)
CUSTOM_DOC("Scrolls the view down one view height and moves the cursor down one view height.")
{
    View_ID view = get_active_view(app, AccessProtected);
    f32 page_jump = get_page_jump(app, view);
    move_vertical_pixels(app, page_jump);
}

internal void
seek_blank_line(Application_Links *app, Scan_Direction direction, Position_Within_Line position){
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    i64 pos = view_get_cursor_pos(app, view);
    i64 new_pos = get_pos_of_blank_line_grouped(app, buffer, direction, pos);
    switch (position){
        case PositionWithinLine_SkipLeadingWhitespace:
        {
            new_pos = get_pos_past_lead_whitespace(app, buffer, new_pos);
        }break;
        
        case PositionWithinLine_End:
        {
            new_pos = get_line_side_pos_from_pos(app, buffer, new_pos, Side_Max);
        }break;
    }
    view_set_cursor(app, view, seek_pos(new_pos), true);
    no_mark_snap_to_cursor_if_shift(app, view);
}

CUSTOM_COMMAND_SIG(move_up_to_blank_line)
CUSTOM_DOC("Seeks the cursor up to the next blank line.")
{
    seek_blank_line(app, Scan_Backward, PositionWithinLine_Start);
}

CUSTOM_COMMAND_SIG(move_down_to_blank_line)
CUSTOM_DOC("Seeks the cursor down to the next blank line.")
{
    seek_blank_line(app, Scan_Forward, PositionWithinLine_Start);
}

CUSTOM_COMMAND_SIG(move_up_to_blank_line_skip_whitespace)
CUSTOM_DOC("Seeks the cursor up to the next blank line and places it at the end of the line.")
{
    seek_blank_line(app, Scan_Backward, PositionWithinLine_SkipLeadingWhitespace);
}

CUSTOM_COMMAND_SIG(move_down_to_blank_line_skip_whitespace)
CUSTOM_DOC("Seeks the cursor down to the next blank line and places it at the end of the line.")
{
    seek_blank_line(app, Scan_Forward, PositionWithinLine_SkipLeadingWhitespace);
}

CUSTOM_COMMAND_SIG(move_up_to_blank_line_end)
CUSTOM_DOC("Seeks the cursor up to the next blank line and places it at the end of the line.")
{
    seek_blank_line(app, Scan_Backward, PositionWithinLine_End);
}

CUSTOM_COMMAND_SIG(move_down_to_blank_line_end)
CUSTOM_DOC("Seeks the cursor down to the next blank line and places it at the end of the line.")
{
    seek_blank_line(app, Scan_Forward, PositionWithinLine_End);
}

#define seek_whitespace_up            move_up_to_blank_line
#define seek_whitespace_down          move_down_to_blank_line
#define seek_whitespace_up_end_line   move_up_to_blank_line_skip_whitespace
#define seek_whitespace_down_end_line move_down_to_blank_line_skip_whitespace

CUSTOM_COMMAND_SIG(move_left)
CUSTOM_DOC("Moves the cursor one character to the left.")
{
    View_ID view = get_active_view(app, AccessProtected);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    i64 character = view_relative_character_from_pos(app, view, cursor.line, pos);
    i64 new_pos = view_pos_from_relative_character(app, view, cursor.line, character - 1);
    view_set_cursor(app, view, seek_pos(new_pos), true);
    no_mark_snap_to_cursor_if_shift(app, view);
}

CUSTOM_COMMAND_SIG(move_right)
CUSTOM_DOC("Moves the cursor one character to the right.")
{
    View_ID view = get_active_view(app, AccessProtected);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    i64 character = view_relative_character_from_pos(app, view, cursor.line, pos);
    i64 new_pos = view_pos_from_relative_character(app, view, cursor.line, character + 1);
    view_set_cursor(app, view, seek_pos(new_pos), true);
    no_mark_snap_to_cursor_if_shift(app, view);
}

static void
current_view_scan_move(Application_Links *app, Scan_Direction direction, Boundary_Function_List funcs){
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    i64 cursor_pos = view_get_cursor_pos(app, view);
    i64 pos = scan(app, funcs, buffer, direction, cursor_pos);
    view_set_cursor(app, view, seek_pos(pos), true);
    no_mark_snap_to_cursor_if_shift(app, view);
}

CUSTOM_COMMAND_SIG(move_right_whitespace_boundary)
CUSTOM_DOC("Seek right for the next boundary between whitespace and non-whitespace.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Forward,
                           push_boundary_list(scratch, boundary_non_whitespace));
}

CUSTOM_COMMAND_SIG(move_left_whitespace_boundary)
CUSTOM_DOC("Seek left for the next boundary between whitespace and non-whitespace.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Backward,
                           push_boundary_list(scratch, boundary_non_whitespace));
}

CUSTOM_COMMAND_SIG(move_right_token_boundary)
CUSTOM_DOC("Seek right for the next end of a token.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Forward,
                           push_boundary_list(scratch, boundary_token));
}

CUSTOM_COMMAND_SIG(move_left_token_boundary)
CUSTOM_DOC("Seek left for the next beginning of a token.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Backward,
                           push_boundary_list(scratch, boundary_token));
}

CUSTOM_COMMAND_SIG(move_right_whitespace_or_token_boundary)
CUSTOM_DOC("Seek right for the next end of a token or boundary between whitespace and non-whitespace.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Forward,
                           push_boundary_list(scratch, boundary_token, boundary_non_whitespace));
}

CUSTOM_COMMAND_SIG(move_left_whitespace_or_token_boundary)
CUSTOM_DOC("Seek left for the next end of a token or boundary between whitespace and non-whitespace.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Backward,
                           push_boundary_list(scratch, boundary_token, boundary_non_whitespace));
}

CUSTOM_COMMAND_SIG(move_right_alpha_numeric_boundary)
CUSTOM_DOC("Seek right for boundary between alphanumeric characters and non-alphanumeric characters.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Forward,
                           push_boundary_list(scratch, boundary_alpha_numeric));
}

CUSTOM_COMMAND_SIG(move_left_alpha_numeric_boundary)
CUSTOM_DOC("Seek left for boundary between alphanumeric characters and non-alphanumeric characters.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Backward,
                           push_boundary_list(scratch, boundary_alpha_numeric));
}

CUSTOM_COMMAND_SIG(move_right_alpha_numeric_or_camel_boundary)
CUSTOM_DOC("Seek right for boundary between alphanumeric characters or camel case word and non-alphanumeric characters.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Forward,
                           push_boundary_list(scratch, boundary_alpha_numeric_camel));
}

CUSTOM_COMMAND_SIG(move_left_alpha_numeric_or_camel_boundary)
CUSTOM_DOC("Seek left for boundary between alphanumeric characters or camel case word and non-alphanumeric characters.")
{
    Scratch_Block scratch(app);
    current_view_scan_move(app, Scan_Backward,
                           push_boundary_list(scratch, boundary_alpha_numeric_camel));
}

#define seek_whitespace_right            move_right_whitespace_boundary
#define seek_whitespace_left             move_left_whitespace_boundary
#define seek_token_right                 move_right_token_boundary
#define seek_token_left                  move_left_token_boundary
#define seek_white_or_token_right        move_right_whitespace_or_token_boundary
#define seek_white_or_token_left         move_left_whitespace_or_token_boundary
#define seek_alphanumeric_right          move_right_alpha_numeric_boundary
#define seek_alphanumeric_left           move_left_alpha_numeric_boundary
#define seek_alphanumeric_or_camel_right move_right_alpha_numeric_or_camel_boundary 
#define seek_alphanumeric_or_camel_left  move_left_alpha_numeric_or_camel_boundary

////////////////////////////////

CUSTOM_COMMAND_SIG(select_all)
CUSTOM_DOC("Puts the cursor at the top of the file, and the mark at the bottom of the file.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    i32 buffer_size = (i32)buffer_get_size(app, buffer);
    view_set_cursor(app, view, seek_pos(0), true);
    view_set_mark(app, view, seek_pos(buffer_size));
    no_mark_snap_to_cursor(app, view);
}

////////////////////////////////

CUSTOM_COMMAND_SIG(to_uppercase)
CUSTOM_DOC("Converts all ascii text in the range between the cursor and the mark to uppercase.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    Range_i64 range = get_view_range(app, view);
    Scratch_Block scratch(app);
    String_Const_u8 string = push_buffer_range(app, scratch, buffer, range);
    string = string_mod_upper(string);
    buffer_replace_range(app, buffer, range, string);
    view_set_cursor(app, view, seek_pos(range.max), true);
}

CUSTOM_COMMAND_SIG(to_lowercase)
CUSTOM_DOC("Converts all ascii text in the range between the cursor and the mark to lowercase.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    Range_i64 range = get_view_range(app, view);
    Scratch_Block scratch(app);
    String_Const_u8 string = push_buffer_range(app, scratch, buffer, range);
    string = string_mod_lower(string);
    buffer_replace_range(app, buffer, range, string);
    view_set_cursor(app, view, seek_pos(range.max), true);
}

CUSTOM_COMMAND_SIG(clean_all_lines)
CUSTOM_DOC("Removes trailing whitespace from all lines in the current buffer.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    
    Scratch_Block scratch(app);
    Batch_Edit *batch_first = 0;
    Batch_Edit *batch_last = 0;
    
    String_Const_u8 text = push_whole_buffer(app, scratch, buffer);
    
    umem whitespace_start = 0;
    for (umem i = 0; i < text.size; i += 1){
        u8 v = string_get_character(text, i);
        if (v == '\n' || i + 1 == text.size){
            if (whitespace_start < i){
                Batch_Edit *batch = push_array(scratch, Batch_Edit, 1);
                sll_queue_push(batch_first, batch_last, batch);
                batch->edit.text = SCu8();
                batch->edit.range = Ii64(whitespace_start, i);
            }
            whitespace_start = i + 1;
        }
        else if (!character_is_whitespace(v)){
            whitespace_start = i + 1;
        }
    }
    
    buffer_batch_edit(app, buffer, batch_first);
}

////////////////////////////////

CUSTOM_COMMAND_SIG(basic_change_active_panel)
CUSTOM_DOC("Change the currently active panel, moving to the panel with the next highest view_id.  Will not skipe the build panel if it is open.")
{
    View_ID view = get_active_view(app, AccessAll);
    get_next_view_looped_all_panels(app, view, AccessAll);
    view_set_active(app, view);
}

CUSTOM_COMMAND_SIG(close_panel)
CUSTOM_DOC("Closes the currently active panel if it is not the only panel open.")
{
    View_ID view = get_active_view(app, AccessAll);
    view_close(app, view);
}

////////////////////////////////

CUSTOM_COMMAND_SIG(show_scrollbar)
CUSTOM_DOC("Sets the current view to show it's scrollbar.")
{
    View_ID view = get_active_view(app, AccessAll);
    view_set_setting(app, view, ViewSetting_ShowScrollbar, true);
}

CUSTOM_COMMAND_SIG(hide_scrollbar)
CUSTOM_DOC("Sets the current view to hide it's scrollbar.")
{
    View_ID view = get_active_view(app, AccessAll);
    view_set_setting(app, view, ViewSetting_ShowScrollbar, false);
}

CUSTOM_COMMAND_SIG(show_filebar)
CUSTOM_DOC("Sets the current view to show it's filebar.")
{
    View_ID view = get_active_view(app, AccessAll);
    view_set_setting(app, view, ViewSetting_ShowFileBar, true);
}

CUSTOM_COMMAND_SIG(hide_filebar)
CUSTOM_DOC("Sets the current view to hide it's filebar.")
{
    View_ID view = get_active_view(app, AccessAll);
    view_set_setting(app, view, ViewSetting_ShowFileBar, false);
}

CUSTOM_COMMAND_SIG(toggle_filebar)
CUSTOM_DOC("Toggles the visibility status of the current view's filebar.")
{
    View_ID view = get_active_view(app, AccessAll);
    b32 value = false;
    view_get_setting(app, view, ViewSetting_ShowFileBar, &value);
    view_set_setting(app, view, ViewSetting_ShowFileBar, !value);
}

CUSTOM_COMMAND_SIG(toggle_line_wrap)
CUSTOM_DOC("Toggles the current buffer's line wrapping status.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    b32 wrapped;
    buffer_get_setting(app, buffer, BufferSetting_WrapLine, &wrapped);
    buffer_set_setting(app, buffer, BufferSetting_WrapLine, !wrapped);
}

CUSTOM_COMMAND_SIG(toggle_fps_meter)
CUSTOM_DOC("Toggles the visibility of the FPS performance meter")
{
    show_fps_hud = !show_fps_hud;
}

CUSTOM_COMMAND_SIG(increase_line_wrap)
CUSTOM_DOC("Increases the current buffer's width for line wrapping.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    i32 wrap = 0;
    buffer_get_setting(app, buffer, BufferSetting_WrapPosition, &wrap);
    buffer_set_setting(app, buffer, BufferSetting_WrapPosition, wrap + 10);
}

CUSTOM_COMMAND_SIG(decrease_line_wrap)
CUSTOM_DOC("Decrases the current buffer's width for line wrapping.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    i32 wrap = 0;
    buffer_get_setting(app, buffer, BufferSetting_WrapPosition, &wrap);
    buffer_set_setting(app, buffer, BufferSetting_WrapPosition, wrap - 10);
}

CUSTOM_COMMAND_SIG(increase_face_size)
CUSTOM_DOC("Increase the size of the face used by the current buffer.")
{
    View_ID view = get_active_view(app, AccessAll);
    Buffer_ID buffer = view_get_buffer(app, view, AccessAll);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Description description = get_face_description(app, face_id);
    ++description.parameters.pt_size;
    try_modify_face(app, face_id, &description);
}

CUSTOM_COMMAND_SIG(decrease_face_size)
CUSTOM_DOC("Decrease the size of the face used by the current buffer.")
{
    View_ID view = get_active_view(app, AccessAll);
    Buffer_ID buffer = view_get_buffer(app, view, AccessAll);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Description description = get_face_description(app, face_id);
    --description.parameters.pt_size;
    try_modify_face(app, face_id, &description);
}

CUSTOM_COMMAND_SIG(mouse_wheel_change_face_size)
CUSTOM_DOC("Reads the state of the mouse wheel and uses it to either increase or decrease the face size.")
{
    static Microsecond_Time_Stamp next_resize_time = 0;
    Microsecond_Time_Stamp now = get_microseconds_timestamp(app);
    if (now >= next_resize_time){
        next_resize_time = now + 50*1000;
        Mouse_State mouse = get_mouse_state(app);
        if (mouse.wheel > 0){
            decrease_face_size(app);
        }
        else if (mouse.wheel < 0){
            increase_face_size(app);
        }
    }
}

CUSTOM_COMMAND_SIG(toggle_virtual_whitespace)
CUSTOM_DOC("Toggles the current buffer's virtual whitespace status.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    i32 vwhite = 0;
    buffer_get_setting(app, buffer, BufferSetting_VirtualWhitespace, &vwhite);
    buffer_set_setting(app, buffer, BufferSetting_VirtualWhitespace, !vwhite);
}

CUSTOM_COMMAND_SIG(toggle_show_whitespace)
CUSTOM_DOC("Toggles the current buffer's whitespace visibility status.")
{
    View_ID view = get_active_view(app, AccessProtected);
    b32 show_whitespace;
    view_get_setting(app, view, ViewSetting_ShowWhitespace, &show_whitespace);
    view_set_setting(app, view, ViewSetting_ShowWhitespace, !show_whitespace);
}

CUSTOM_COMMAND_SIG(toggle_line_numbers)
CUSTOM_DOC("Toggles the left margin line numbers.")
{
    global_config.show_line_number_margins = !global_config.show_line_number_margins;
}

CUSTOM_COMMAND_SIG(eol_dosify)
CUSTOM_DOC("Puts the buffer in DOS line ending mode.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    buffer_set_setting(app, buffer, BufferSetting_Eol, 1);
}

CUSTOM_COMMAND_SIG(eol_nixify)
CUSTOM_DOC("Puts the buffer in NIX line ending mode.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    buffer_set_setting(app, buffer, BufferSetting_Eol, 0);
}

CUSTOM_COMMAND_SIG(exit_4coder)
CUSTOM_DOC("Attempts to close 4coder.")
{
    send_exit_signal(app);
}

////////////////////////////////

CUSTOM_COMMAND_SIG(goto_line)
CUSTOM_DOC("Queries the user for a number, and jumps the cursor to the corresponding line.")
{
    u8 string_space[256];
    Query_Bar bar = {};
    bar.prompt = string_u8_litexpr("Goto Line: ");
    bar.string = SCu8(string_space, (umem)0);
    bar.string_capacity = sizeof(string_space);
    if (query_user_number(app, &bar)){
        i32 line_number = (i32)string_to_integer(bar.string, 10);
        View_ID view = get_active_view(app, AccessProtected);
        view_set_cursor(app, view, seek_line_col(line_number, 0), true);
    }
}

CUSTOM_COMMAND_SIG(search);
CUSTOM_COMMAND_SIG(reverse_search);

static void
isearch__update_highlight(Application_Links *app, View_ID view, Range_i64 range){
    view_set_highlight_range(app, view, range);
    view_set_cursor(app, view, seek_pos(range.start), true);
}

static void
isearch(Application_Links *app, b32 start_reversed, String_Const_u8 query_init, b32 on_the_query_init_string){
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    if (!buffer_exists(app, buffer)){
        return;
    }
    
    Query_Bar bar = {};
    if (start_query_bar(app, &bar, 0) == 0){
        return;
    }
    
    b32 reverse = start_reversed;
    i64 first_pos = view_get_cursor_pos(app, view);
    
    i64 pos = first_pos;
    if (query_init.size != 0){
        pos += 2;
    }
    
    i64 start_pos = pos;
    Range_i64 match = Ii64(pos);
    
    u8 bar_string_space[256];
    bar.string = SCu8(bar_string_space, query_init.size);
    block_copy(bar.string.str, query_init.str, query_init.size);
    
    String_Const_char isearch_str = string_litexpr("I-Search: ");
    String_Const_char rsearch_str = string_litexpr("Reverse-I-Search: ");
    
    b32 first_step = true;
    
    // TODO(allen): rewrite
    isearch__update_highlight(app, view, match);
    cursor_is_hidden = true;
    
    User_Input in = {};
    for (;;){
        // NOTE(allen): Change the bar's prompt to match the current direction.
        if (reverse){
            bar.prompt = SCu8(rsearch_str);
        }
        else{
            bar.prompt = SCu8(isearch_str);
        }
        
        b32 step_forward = false;
        b32 step_backward = false;
        b32 backspace = false;
        b32 suppress_highligh_update = false;
        
        if (!first_step){
            in = get_user_input(app, EventOnAnyKey, EventOnEsc);
            if (in.abort) break;
            
            u8 character[4];
            u32 length = to_writable_character(in, character);
            
            b32 made_change = false;
            if (in.key.keycode == '\n' || in.key.keycode == '\t'){
                if (in.key.modifiers[MDFR_CONTROL_INDEX]){
                    bar.string.size = cstring_length(previous_isearch_query);
                    block_copy(bar.string.str, previous_isearch_query, bar.string.size);
                }
                else{
                    umem size = bar.string.size;
                    size = clamp_top(size, sizeof(previous_isearch_query) - 1);
                    block_copy(previous_isearch_query, bar.string.str, size);
                    previous_isearch_query[size] = 0;
                    break;
                }
            }
            else if (length != 0 && key_is_unmodified(&in.key)){
                String_u8 string = Su8(bar.string, sizeof(bar_string_space));
                string_append(&string, SCu8(character, length));
                bar.string = string.string;
                made_change = true;
            }
            else if (in.key.keycode == key_back){
                if (key_is_unmodified(&in.key)){
                    umem old_bar_string_size = bar.string.size;
                    bar.string = backspace_utf8(bar.string);
                    made_change = (bar.string.size < old_bar_string_size);
                    backspace = true;
                }
                else if (in.key.modifiers[MDFR_CONTROL_INDEX]){
                    if (bar.string.size > 0){
                        made_change = true;
                        bar.string.size = 0;
                        backspace = true;
                    }
                }
            }
            
            if ((in.command.command == search) || in.key.keycode == key_page_down || in.key.keycode == key_down){
                step_forward = true;
            }
            
            if (in.command.command == mouse_wheel_scroll){
                mouse_wheel_scroll(app);
                suppress_highligh_update = true;
            }
            
            if ((in.command.command == reverse_search) || in.key.keycode == key_page_up || in.key.keycode == key_up){
                step_backward = true;
            }
        }
        else{
            if (query_init.size != 0 && on_the_query_init_string){
                step_backward = true;
            }
            first_step = false;
        }
        
        start_pos = pos;
        if (step_forward && reverse){
            start_pos = match.start + 1;
            pos = start_pos;
            reverse = false;
            step_forward = false;
        }
        if (step_backward && !reverse){
            start_pos = match.start - 1;
            pos = start_pos;
            reverse = true;
            step_backward = false;
        }
        
        if (!backspace){
            if (reverse){
                i64 new_pos = 0;
                buffer_seek_string_insensitive_backward(app, buffer, start_pos + 1, 0, bar.string, &new_pos);
                if (new_pos >= 0){
                    if (step_backward){
                        pos = new_pos;
                        start_pos = new_pos;
                        buffer_seek_string_insensitive_backward(app, buffer, start_pos + 1, 0, bar.string, &new_pos);
                        if (new_pos < 0){
                            new_pos = start_pos;
                        }
                    }
                    match.start = new_pos;
                    match.end = match.start + (i32)bar.string.size;
                }
            }
            else{
                i64 new_pos = 0;
                buffer_seek_string_insensitive_forward(app, buffer, start_pos, 0, bar.string, &new_pos);
                i64 buffer_size = buffer_get_size(app, buffer);
                if (new_pos < buffer_size){
                    if (step_forward){
                        pos = new_pos;
                        start_pos = new_pos;
                        buffer_seek_string_insensitive_forward(app, buffer, start_pos, 0, bar.string, &new_pos);
                        if (new_pos >= buffer_size){
                            new_pos = start_pos;
                        }
                    }
                    match.start = new_pos;
                    match.end = match.start + (i32)bar.string.size;
                }
            }
        }
        else{
            if (match.end > match.start + (i32)bar.string.size){
                match.end = match.start + (i32)bar.string.size;
            }
        }
        
        if (!suppress_highligh_update){
            isearch__update_highlight(app, view, match);
        }
    }
    
    view_disable_highlight_range(app, view);
    cursor_is_hidden = false;
    
    if (in.abort){
        umem size = bar.string.size;
        size = clamp_top(size, sizeof(previous_isearch_query) - 1);
        block_copy(previous_isearch_query, bar.string.str, size);
        previous_isearch_query[size] = 0;
        view_set_cursor(app, view, seek_pos(first_pos), true);
    }
}

CUSTOM_COMMAND_SIG(search)
CUSTOM_DOC("Begins an incremental search down through the current buffer for a user specified string.")
{
    isearch(app, false, SCu8(), false);
}

CUSTOM_COMMAND_SIG(reverse_search)
CUSTOM_DOC("Begins an incremental search up through the current buffer for a user specified string.")
{
    isearch(app, true, SCu8(), false);
}

CUSTOM_COMMAND_SIG(search_identifier)
CUSTOM_DOC("Begins an incremental search down through the current buffer for the word or token under the cursor.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer_id = view_get_buffer(app, view, AccessProtected);
    i64 pos = view_get_cursor_pos(app, view);
    Scratch_Block scratch(app);
    String_Const_u8 query = push_enclose_range_at_pos(app, scratch, buffer_id, pos, enclose_alpha_numeric_underscore);
    isearch(app, false, query, true);
}

CUSTOM_COMMAND_SIG(reverse_search_identifier)
CUSTOM_DOC("Begins an incremental search up through the current buffer for the word or token under the cursor.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer_id = view_get_buffer(app, view, AccessProtected);
    i64 pos = view_get_cursor_pos(app, view);
    Scratch_Block scratch(app);
    String_Const_u8 query = push_enclose_range_at_pos(app, scratch, buffer_id, pos, enclose_alpha_numeric_underscore);
    isearch(app, true, query, true);
}

struct String_Pair{
    b32 valid;
    String_Const_u8 a;
    String_Const_u8 b;
};

internal String_Pair
query_user_replace_pair(Application_Links *app, Arena *arena){
    Query_Bar *replace = push_array(arena, Query_Bar, 1);
    u8 *replace_space = push_array(arena, u8, KB(1));
    replace->prompt = string_u8_litexpr("Replace: ");
    replace->string = SCu8(replace_space, (umem)0);
    replace->string_capacity = KB(1);
    
    Query_Bar *with = push_array(arena, Query_Bar, 1);
    u8 *with_space = push_array(arena, u8, KB(1));
    with->prompt = string_u8_litexpr("With: ");
    with->string = SCu8(with_space, (umem)0);
    with->string_capacity = KB(1);
    
    String_Pair result = {};
    if (query_user_string(app, replace) && replace->string.size != 0 && query_user_string(app, with)){
        result.valid = true;
        result.a = replace->string;
        result.b = with->string;
    }
    return(result);
}

// NOTE(allen): This is a bit of a hacky setup because of Query_Bar lifetimes.  This must be
// called as the last operation of a command.
internal void
replace_in_range_query_user(Application_Links *app, Buffer_ID buffer, Range_i64 range){
    Scratch_Block scratch(app);
    String_Pair pair = query_user_replace_pair(app, scratch);
    if (pair.valid){
        replace_in_range(app, buffer, range, pair.a, pair.b);
    }
}

CUSTOM_COMMAND_SIG(replace_in_range)
CUSTOM_DOC("Queries the user for a needle and string. Replaces all occurences of needle with string in the range between cursor and the mark in the active buffer.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    Range_i64 range = get_view_range(app, view);
    replace_in_range_query_user(app, buffer, range);
}

CUSTOM_COMMAND_SIG(replace_in_buffer)
CUSTOM_DOC("Queries the user for a needle and string. Replaces all occurences of needle with string in the active buffer.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    Range_i64 range = buffer_range(app, buffer);
    replace_in_range_query_user(app, buffer, range);
}

CUSTOM_COMMAND_SIG(replace_in_all_buffers)
CUSTOM_DOC("Queries the user for a needle and string. Replaces all occurences of needle with string in all editable buffers.")
{
    global_history_edit_group_begin(app);
    
    Scratch_Block scratch(app);
    String_Pair pair = query_user_replace_pair(app, scratch);
    for (Buffer_ID buffer = get_buffer_next(app, 0, AccessOpen);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, AccessOpen)){
        Range_i64 range = buffer_range(app, buffer);
        replace_in_range(app, buffer, range, pair.a, pair.b);
    }
    
    global_history_edit_group_end(app);
}

static void
query_replace_base(Application_Links *app, View_ID view, Buffer_ID buffer_id, i64 pos, String_Const_u8 r, String_Const_u8 w){
    i64 new_pos = 0;
    buffer_seek_string_forward(app, buffer_id, pos - 1, 0, r, &new_pos);
    
    cursor_is_hidden = true;
    
    i64 buffer_size = buffer_get_size(app, buffer_id);
    
    User_Input in = {};
    for (;new_pos < buffer_size;){
        Range_i64 match = Ii64(new_pos, new_pos + r.size);
        isearch__update_highlight(app, view, match);
        
        in = get_user_input(app, EventOnAnyKey, EventOnMouseLeftButton|EventOnMouseRightButton);
        if (in.abort || in.key.keycode == key_esc || !key_is_unmodified(&in.key)) break;
        
        if (in.key.character == 'y' || in.key.character == 'Y' ||
            in.key.character == '\n' || in.key.character == '\t'){
            buffer_replace_range(app, buffer_id, match, w);
            pos = match.start + w.size;
        }
        else{
            pos = match.max;
        }
        
        buffer_seek_string_forward(app, buffer_id, pos, 0, r, &new_pos);
    }
    
    view_disable_highlight_range(app, view);
    cursor_is_hidden = false;
    
    if (in.abort){
        return;
    }
    
    view_set_cursor(app, view, seek_pos(pos), true);
}

static void
query_replace_parameter(Application_Links *app, String_Const_u8 replace_str, i64 start_pos, b32 add_replace_query_bar){
    Query_Bar replace = {};
    replace.prompt = string_u8_litexpr("Replace: ");
    replace.string = replace_str;
    
    if (add_replace_query_bar){
        start_query_bar(app, &replace, 0);
    }
    
    Query_Bar with = {};
    u8 with_space[1024];
    with.prompt = string_u8_litexpr("With: ");
    with.string = SCu8(with_space, (umem)0);
    with.string_capacity = sizeof(with_space);
    
    if (query_user_string(app, &with)){
        String_Const_u8 r = replace.string;
        String_Const_u8 w = with.string;
        
        View_ID view = get_active_view(app, AccessProtected);
        Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
        i64 pos = start_pos;
        
        Query_Bar bar = {};
        bar.prompt = string_u8_litexpr("Replace? (y)es, (n)ext, (esc)\n");
        start_query_bar(app, &bar, 0);
        
        query_replace_base(app, view, buffer, pos, r, w);
    }
}

CUSTOM_COMMAND_SIG(query_replace)
CUSTOM_DOC("Queries the user for two strings, and incrementally replaces every occurence of the first string with the second string.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    if (buffer != 0){
        Query_Bar replace = {};
        u8 replace_space[1024];
        replace.prompt = string_u8_litexpr("Replace: ");
        replace.string = SCu8(replace_space, (umem)0);
        replace.string_capacity = sizeof(replace_space);
        if (query_user_string(app, &replace)){
            if (replace.string.size > 0){
                i64 pos = view_get_cursor_pos(app, view);
                query_replace_parameter(app, replace.string, pos, false);
            }
        }
    }
}

CUSTOM_COMMAND_SIG(query_replace_identifier)
CUSTOM_DOC("Queries the user for a string, and incrementally replace every occurence of the word or token found at the cursor with the specified string.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    if (buffer != 0){
        Scratch_Block scratch(app);
        i64 pos = view_get_cursor_pos(app, view);
        Range_i64 range = enclose_pos_alpha_numeric_underscore(app, buffer, pos);
        String_Const_u8 replace = push_buffer_range(app, scratch, buffer, range);
        if (replace.size != 0){
            query_replace_parameter(app, replace, range.min, true);
        }
    }
}

CUSTOM_COMMAND_SIG(query_replace_selection)
CUSTOM_DOC("Queries the user for a string, and incrementally replace every occurence of the string found in the selected range with the specified string.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    if (buffer != 0){
        Scratch_Block scratch(app);
        Range_i64 range = get_view_range(app, view);
        String_Const_u8 replace = push_buffer_range(app, scratch, buffer, range);
        if (replace.size != 0){
            query_replace_parameter(app, replace, range.min, true);
        }
    }
}

////////////////////////////////

static void
save_all_dirty_buffers_with_postfix(Application_Links *app, String_Const_u8 postfix){
    Arena *scratch = context_get_arena(app);
    for (Buffer_ID buffer = get_buffer_next(app, 0, AccessOpen);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, AccessOpen)){
        Dirty_State dirty = buffer_get_dirty_state(app, buffer);
        if (dirty == DirtyState_UnsavedChanges){
            Temp_Memory temp = begin_temp(scratch);
            String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
            if (string_match(string_postfix(file_name, postfix.size), postfix)){
                buffer_save(app, buffer, file_name, 0);
            }
            end_temp(temp);
        }
    }
}

CUSTOM_COMMAND_SIG(save_all_dirty_buffers)
CUSTOM_DOC("Saves all buffers marked dirty (showing the '*' indicator).")
{
    String_Const_u8 empty = {};
    save_all_dirty_buffers_with_postfix(app, empty);
}

static void
delete_file_base(Application_Links *app, String_Const_u8 file_name, Buffer_ID buffer_id){
    String_Const_u8 path = string_remove_last_folder(file_name);
    Scratch_Block scratch(app);
    List_String_Const_u8 list = {};
#if OS_WINDOWS
    string_list_push_u8_lit(scratch, &list, "del ");
#elif OS_LINUX || OS_MAC
    string_list_push_u8_lit(scratch, &list, "rm ");
#else
# error no delete file command for this platform
#endif
    string_list_pushf(scratch, &list, "\"%.*s\"", string_expand(file_name));
    String_Const_u8 cmd = string_list_flatten(scratch, list, StringFill_NullTerminate);
    exec_system_command(app, 0, buffer_identifier(0), path, cmd, 0);
    buffer_kill(app, buffer_id, BufferKill_AlwaysKill);
}

CUSTOM_COMMAND_SIG(delete_file_query)
CUSTOM_DOC("Deletes the file of the current buffer if 4coder has the appropriate access rights. Will ask the user for confirmation first.")
{
    View_ID view = get_active_view(app, AccessAll);
    Buffer_ID buffer = view_get_buffer(app, view, AccessAll);
    Scratch_Block scratch(app);
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
    if (file_name.size > 0){
        Query_Bar bar = {};
        bar.prompt = push_u8_stringf(scratch, "Delete '%.*s' (Y)es, (n)o", string_expand(file_name));
        if (start_query_bar(app, &bar, 0) != 0){
            b32 cancelled = false;
            for (;!cancelled;){
                User_Input in = get_user_input(app, EventOnAnyKey, 0);
                switch (in.key.keycode){
                    case 'Y':
                    {
                        delete_file_base(app, file_name, buffer);
                        cancelled = true;
                    }break;
                    
                    case key_shift:
                    case key_ctrl:
                    case key_alt:
                    case key_cmnd:
                    case key_caps:
                    {}break;
                    
                    default:
                    {
                        cancelled = true;
                    }break;
                }
            }
        }
    }
}

CUSTOM_COMMAND_SIG(save_to_query)
CUSTOM_DOC("Queries the user for a file name and saves the contents of the current buffer, altering the buffer's name too.")
{
    View_ID view = get_active_view(app, AccessAll);
    Buffer_ID buffer = view_get_buffer(app, view, AccessAll);
    
    Arena *scratch = context_get_arena(app);
    Temp_Memory temp = begin_temp(scratch);
    String_Const_u8 buffer_name = push_buffer_unique_name(app, scratch, buffer);
    
    // Query the user
    u8 name_space[4096];
    Query_Bar bar = {};
    bar.prompt = push_u8_stringf(scratch, "Save '%.*s' to: ", string_expand(buffer_name));
    bar.string = SCu8(name_space, (umem)0);
    bar.string_capacity = sizeof(name_space);
    if (query_user_string(app, &bar)){
        if (bar.string.size != 0){
            List_String_Const_u8 new_file_name_list = {};
            string_list_push(scratch, &new_file_name_list, push_hot_directory(app, scratch));
            string_list_push(scratch, &new_file_name_list, bar.string);
            String_Const_u8 new_file_name = string_list_flatten(scratch, new_file_name_list);
            if (buffer_save(app, buffer, new_file_name, BufferSave_IgnoreDirtyFlag)){
                Buffer_ID new_buffer = create_buffer(app, new_file_name, BufferCreate_NeverNew|BufferCreate_JustChangedFile);
                if (new_buffer != 0 && new_buffer != buffer){
                    buffer_kill(app, buffer, BufferKill_AlwaysKill);
                    view_set_buffer(app, view, new_buffer, 0);
                }
            }
        }
    }
    
    end_temp(temp);
}

CUSTOM_COMMAND_SIG(rename_file_query)
CUSTOM_DOC("Queries the user for a new name and renames the file of the current buffer, altering the buffer's name too.")
{
    View_ID view = get_active_view(app, AccessAll);
    Buffer_ID buffer = view_get_buffer(app, view, AccessAll);
    
    Arena *scratch = context_get_arena(app);
    Temp_Memory temp = begin_temp(scratch);
    
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
    if (file_name.size > 0){
        // Query the user
        String_Const_u8 front = string_front_of_path(file_name);
        u8 name_space[4096];
        Query_Bar bar = {};
        bar.prompt = push_u8_stringf(scratch, "Rename '%.*s' to: ", string_expand(front));
        bar.string = SCu8(name_space, (umem)0);
        bar.string_capacity = sizeof(name_space);
        if (query_user_string(app, &bar)){
            if (bar.string.size != 0){
                // TODO(allen): There should be a way to say, "detach a buffer's file" and "attach this file to a buffer"
                List_String_Const_u8 new_file_name_list = {};
                string_list_push(scratch, &new_file_name_list, file_name);
                string_list_push(scratch, &new_file_name_list, bar.string);
                String_Const_u8 new_file_name = string_list_flatten(scratch, new_file_name_list, StringFill_NullTerminate);
                if (buffer_save(app, buffer, new_file_name, BufferSave_IgnoreDirtyFlag)){
                    Buffer_ID new_buffer = create_buffer(app, new_file_name, BufferCreate_NeverNew|BufferCreate_JustChangedFile);
                    if (new_buffer != 0 && new_buffer != buffer){
                        delete_file_base(app, file_name, buffer);
                        view_set_buffer(app, view, new_buffer, 0);
                    }
                }
            }
        }
        
    }
    
    end_temp(temp);
}

CUSTOM_COMMAND_SIG(make_directory_query)
CUSTOM_DOC("Queries the user for a name and creates a new directory with the given name.")
{
    Scratch_Block scratch(app);
    
    String_Const_u8 hot = push_hot_directory(app, scratch);
    
    // Query the user
    u8 name_space[4096];
    Query_Bar bar = {};
    bar.prompt = push_u8_stringf(scratch, "Make directory at '%.*s': ", string_expand(hot));
    bar.string = SCu8(name_space, (umem)0);
    bar.string_capacity = sizeof(name_space);
    
    if (!query_user_string(app, &bar)) return;
    if (bar.string.size == 0) return;
    
    String_Const_u8 cmd = push_u8_stringf(scratch, "mkdir %.*s", string_expand(bar.string));
    exec_system_command(app, 0, buffer_identifier(0), hot, cmd, 0);
}

////////////////////////////////

internal void
current_view_move_line(Application_Links *app, Scan_Direction direction){
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    i64 pos = view_get_cursor_pos(app, view);
    i64 line_number = get_line_number_from_pos(app, buffer, pos);
    pos = move_line(app, buffer, line_number, direction);
    view_set_cursor(app, view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(move_line_up)
CUSTOM_DOC("Swaps the line under the cursor with the line above it, and moves the cursor up with it.")
{
    current_view_move_line(app, Scan_Backward);
}

CUSTOM_COMMAND_SIG(move_line_down)
CUSTOM_DOC("Swaps the line under the cursor with the line below it, and moves the cursor down with it.")
{
    current_view_move_line(app, Scan_Forward);
}

CUSTOM_COMMAND_SIG(duplicate_line)
CUSTOM_DOC("Create a copy of the line on which the cursor sits.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    i64 pos = view_get_cursor_pos(app, view);
    i64 line = get_line_number_from_pos(app, buffer, pos);
    Scratch_Block scratch(app);
    String_Const_u8 s = push_buffer_line(app, scratch, buffer, line);
    s = push_u8_stringf(scratch, "%.*s\n", string_expand(s));
    pos = get_line_side_pos(app, buffer, line, Side_Min);
    buffer_replace_range(app, buffer, Ii64(pos), s);
}

CUSTOM_COMMAND_SIG(delete_line)
CUSTOM_DOC("Delete the line the on which the cursor sits.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    i64 pos = view_get_cursor_pos(app, view);
    i64 line = get_line_number_from_pos(app, buffer, pos);
    Range_i64 range = get_line_pos_range(app, buffer, line);
    range.end += 1;
    i32 size = (i32)buffer_get_size(app, buffer);
    range.end = clamp_top(range.end, size);
    if (range_size(range) == 0 ||
        buffer_get_char(app, buffer, range.end - 1) != '\n'){
        range.start -= 1;
        range.first = clamp_bot(0, range.first);
    }
    buffer_replace_range(app, buffer, range, string_u8_litexpr(""));
}

////////////////////////////////

static b32
get_cpp_matching_file(Application_Links *app, Buffer_ID buffer, Buffer_ID *buffer_out){
    b32 result = false;
    Scratch_Block scratch(app);
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
    if (file_name.size > 0){
        String_Const_u8 extension = string_file_extension(file_name);
        String_Const_u8 new_extensions[2] = {};
        i32 new_extensions_count = 0;
        if (string_match(extension, string_u8_litexpr("cpp")) || string_match(extension, string_u8_litexpr("cc"))){
            new_extensions[0] = string_u8_litexpr("h");
            new_extensions[1] = string_u8_litexpr("hpp");
            new_extensions_count = 2;
        }
        else if (string_match(extension, string_u8_litexpr("c"))){
            new_extensions[0] = string_u8_litexpr("h");
            new_extensions_count = 1;
        }
        else if (string_match(extension, string_u8_litexpr("h"))){
            new_extensions[0] = string_u8_litexpr("c");
            new_extensions[1] = string_u8_litexpr("cpp");
            new_extensions_count = 2;
        }
        else if (string_match(extension, string_u8_litexpr("hpp"))){
            new_extensions[0] = string_u8_litexpr("cpp");
            new_extensions_count = 1;
        }
        
        String_Const_u8 file_without_extension = string_file_without_extension(file_name);
        for (i32 i = 0; i < new_extensions_count; i += 1){
            Temp_Memory temp = begin_temp(scratch);
            String_Const_u8 new_extension = new_extensions[i];
            String_Const_u8 new_file_name = push_u8_stringf(scratch, "%.*s.%.*s", string_expand(file_without_extension), string_expand(new_extension));
            if (open_file(app, buffer_out, new_file_name, false, true)){
                result = true;
                break;
            }
            end_temp(temp);
        }
    }
    
    return(result);
}

CUSTOM_COMMAND_SIG(open_file_in_quotes)
CUSTOM_DOC("Reads a filename from surrounding '\"' characters and attempts to open the corresponding file.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    if (buffer_exists(app, buffer)){
        Scratch_Block scratch(app);
        
        i64 pos = view_get_cursor_pos(app, view);
        
        Range_i64 range = enclose_pos_inside_quotes(app, buffer, pos);
        
        String_Const_u8 quoted_name = push_buffer_range(app, scratch, buffer, range);
        
        String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
        String_Const_u8 path = string_remove_last_folder(file_name);
        
        if (character_is_slash(string_get_character(path, path.size - 1))){
            path = string_chop(path, 1);
        }
        
        String_Const_u8 new_file_name = push_u8_stringf(scratch, "%.*s/%.*s", string_expand(path), string_expand(quoted_name));
        
        view = get_next_view_looped_primary_panels(app, view, AccessAll);
        if (view != 0){
            if (view_open_file(app, view, new_file_name, true)){
                view_set_active(app, view);
            }
        }
    }
}

CUSTOM_COMMAND_SIG(open_matching_file_cpp)
CUSTOM_DOC("If the current file is a *.cpp or *.h, attempts to open the corresponding *.h or *.cpp file in the other view.")
{
    View_ID view = get_active_view(app, AccessAll);
    Buffer_ID buffer = view_get_buffer(app, view, AccessAll);
    Buffer_ID new_buffer = 0;
    if (get_cpp_matching_file(app, buffer, &new_buffer)){
        view = get_next_view_looped_primary_panels(app, view, AccessAll);
        view_set_buffer(app, view, new_buffer, 0);
        view_set_active(app, view);
    }
}

CUSTOM_COMMAND_SIG(view_buffer_other_panel)
CUSTOM_DOC("Set the other non-active panel to view the buffer that the active panel views, and switch to that panel.")
{
    View_ID view = get_active_view(app, AccessAll);
    Buffer_ID buffer = view_get_buffer(app, view, AccessAll);
    i64 pos = view_get_cursor_pos(app, view);
    change_active_panel(app);
    view = get_active_view(app, AccessAll);
    view_set_buffer(app, view, buffer, 0);
    view_set_cursor(app, view, seek_pos(pos), true);
}

CUSTOM_COMMAND_SIG(swap_buffers_between_panels)
CUSTOM_DOC("Set the other non-active panel to view the buffer that the active panel views, and switch to that panel.")
{
    View_ID view1 = get_active_view(app, AccessAll);
    change_active_panel(app);
    View_ID view2 = get_active_view(app, AccessAll);
    
    if (view1 != view2 &&
        !view_is_in_ui_mode(app, view1) &&
        !view_is_in_ui_mode(app, view2)){
        Buffer_ID buffer1 = view_get_buffer(app, view1, AccessAll);
        Buffer_ID buffer2 = view_get_buffer(app, view2, AccessAll);
        if (buffer1 != buffer2){
            view_set_buffer(app, view1, buffer2, 0);
            view_set_buffer(app, view2, buffer1, 0);
        }
        else{
            i64 p1 = view_get_cursor_pos(app, view1);
            i64 m1 = view_get_mark_pos(app, view1);
            Buffer_Scroll sc1 = view_get_buffer_scroll(app, view1);
            i64 p2 = view_get_cursor_pos(app, view2);
            i64 m2 = view_get_mark_pos(app, view2);
            Buffer_Scroll sc2 = view_get_buffer_scroll(app, view2);
            
            view_set_cursor(app, view1, seek_pos(p2), true);
            view_set_mark  (app, view1, seek_pos(m2));
            view_set_buffer_scroll(app, view1, sc2);
            view_set_cursor(app, view2, seek_pos(p1), true);
            view_set_mark  (app, view2, seek_pos(m1));
            view_set_buffer_scroll(app, view2, sc1);
        }
    }
}

////////////////////////////////

CUSTOM_COMMAND_SIG(kill_buffer)
CUSTOM_DOC("Kills the current buffer.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    try_buffer_kill(app, buffer, view, 0);
}

CUSTOM_COMMAND_SIG(save)
CUSTOM_DOC("Saves the current buffer.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    Arena *scratch = context_get_arena(app);
    Temp_Memory temp = begin_temp(scratch);
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
    buffer_save(app, buffer, file_name, 0);
    end_temp(temp);
}

CUSTOM_COMMAND_SIG(reopen)
CUSTOM_DOC("Reopen the current buffer from the hard drive.")
{
    View_ID view = get_active_view(app, AccessProtected);
    Buffer_ID buffer = view_get_buffer(app, view, AccessProtected);
    buffer_reopen(app, buffer, 0);
}

////////////////////////////////

static i32
record_get_new_cursor_position_undo(Application_Links *app, Buffer_ID buffer_id, History_Record_Index index, Record_Info record){
    i32 new_edit_position = 0;
    switch (record.kind){
        default:
        case RecordKind_Single:
        {
            new_edit_position = (i32)(record.single.first + record.single.string_backward.size);
        }break;
        case RecordKind_Group:
        {
            Record_Info sub_record = buffer_history_get_group_sub_record(app, buffer_id, index, 0);
            new_edit_position = (i32)(sub_record.single.first + sub_record.single.string_backward.size);
        }break;
    }
    return(new_edit_position);
}

static i32
record_get_new_cursor_position_undo(Application_Links *app, Buffer_ID buffer_id, History_Record_Index index){
    Record_Info record = buffer_history_get_record_info(app, buffer_id, index);
    return(record_get_new_cursor_position_undo(app, buffer_id, index, record));
}

static i32
record_get_new_cursor_position_redo(Application_Links *app, Buffer_ID buffer_id, History_Record_Index index, Record_Info record){
    i64 new_edit_position = 0;
    switch (record.kind){
        default:
        case RecordKind_Single:
        {
            new_edit_position = record.single.first + record.single.string_forward.size;
        }break;
        case RecordKind_Group:
        {
            Record_Info sub_record = buffer_history_get_group_sub_record(app, buffer_id, index, record.group.count - 1);
            new_edit_position = sub_record.single.first + sub_record.single.string_forward.size;
        }break;
    }
    return((i32)(new_edit_position));
}

static i32
record_get_new_cursor_position_redo(Application_Links *app, Buffer_ID buffer_id, History_Record_Index index){
    Record_Info record = buffer_history_get_record_info(app, buffer_id, index);
    return(record_get_new_cursor_position_redo(app, buffer_id, index, record));
}

CUSTOM_COMMAND_SIG(undo)
CUSTOM_DOC("Advances backwards through the undo history of the current buffer.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    History_Record_Index current = buffer_history_get_current_state_index(app, buffer);
    if (current > 0){
        i32 new_position = record_get_new_cursor_position_undo(app, buffer, current);
        buffer_history_set_current_state_index(app, buffer, current - 1);
        view_set_cursor(app, view, seek_pos(new_position), true);
    }
}

CUSTOM_COMMAND_SIG(redo)
CUSTOM_DOC("Advances forwards through the undo history of the current buffer.")
{
    View_ID view = get_active_view(app, AccessOpen);
    Buffer_ID buffer = view_get_buffer(app, view, AccessOpen);
    History_Record_Index current = buffer_history_get_current_state_index(app, buffer);
    History_Record_Index max_index = buffer_history_get_max_record_index(app, buffer);
    if (current < max_index){
        i32 new_position = record_get_new_cursor_position_redo(app, buffer, current + 1);
        buffer_history_set_current_state_index(app, buffer, current + 1);
        view_set_cursor(app, view, seek_pos(new_position), true);
    }
}

CUSTOM_COMMAND_SIG(undo_all_buffers)
CUSTOM_DOC("Advances backward through the undo history in the buffer containing the most recent regular edit.")
{
    Arena *scratch = context_get_arena(app);
    i32 highest_edit_number = -1;
    Buffer_ID first_buffer_match = 0;
    Buffer_ID last_buffer_match = 0;
    i32 match_count = 0;
    
    {
        for (Buffer_ID buffer = get_buffer_next(app, 0, AccessAll);
             buffer != 0;
             buffer = get_buffer_next(app, buffer, AccessAll)){
            History_Record_Index index = buffer_history_get_current_state_index(app, buffer);
            if (index > 0){
                Record_Info record = buffer_history_get_record_info(app, buffer, index);
                if (record.edit_number > highest_edit_number){
                    highest_edit_number = record.edit_number;
                    first_buffer_match = buffer;
                    last_buffer_match = buffer;
                    match_count = 1;
                }
                else if (record.edit_number == highest_edit_number){
                    last_buffer_match = buffer;
                    match_count += 1;
                }
            }
        }
    }
    
    Temp_Memory temp = begin_temp(scratch);
    Buffer_ID *match_buffers = push_array(scratch, Buffer_ID, match_count);
    i32 *new_positions = push_array(scratch, i32, match_count);
    match_count = 0;
    
    if (highest_edit_number != -1){
        for (Buffer_ID buffer = first_buffer_match;
             buffer != 0;
             buffer = get_buffer_next(app, buffer, AccessAll)){
            b32 did_match = false;
            i32 new_edit_position = 0;
            for (;;){
                History_Record_Index index = buffer_history_get_current_state_index(app, buffer);
                if (index > 0){
                    Record_Info record = buffer_history_get_record_info(app, buffer, index);
                    if (record.edit_number == highest_edit_number){
                        did_match = true;
                        new_edit_position = record_get_new_cursor_position_undo(app, buffer, index, record);
                        buffer_history_set_current_state_index(app, buffer, index - 1);
                    }
                    else{
                        break;
                    }
                }
                else{
                    break;
                }
            }
            if (did_match){
                match_buffers[match_count] = buffer;
                new_positions[match_count] = new_edit_position;
                match_count += 1;
            }
            if (buffer == last_buffer_match){
                break;
            }
        }
    }
    
    view_buffer_set(app, match_buffers, new_positions, match_count);
    
    end_temp(temp);
}

CUSTOM_COMMAND_SIG(redo_all_buffers)
CUSTOM_DOC("Advances forward through the undo history in the buffer containing the most recent regular edit.")
{
    Arena *scratch = context_get_arena(app);
    i32 lowest_edit_number = 0x7FFFFFFF;
    Buffer_ID first_buffer_match = 0;
    Buffer_ID last_buffer_match = 0;
    i32 match_count = 0;
    
    {
        for (Buffer_ID buffer = get_buffer_next(app, 0, AccessAll);
             buffer != 0;
             buffer = get_buffer_next(app, buffer, AccessAll)){
            History_Record_Index max_index = buffer_history_get_max_record_index(app, buffer);
            History_Record_Index index = buffer_history_get_current_state_index(app, buffer);
            if (index < max_index){
                Record_Info record = buffer_history_get_record_info(app, buffer, index + 1);
                if (record.edit_number < lowest_edit_number){
                    lowest_edit_number = record.edit_number;
                    first_buffer_match = buffer;
                    last_buffer_match = buffer;
                    match_count = 1;
                }
                else if (record.edit_number == lowest_edit_number){
                    last_buffer_match = buffer;
                    match_count += 1;
                }
            }
        }
    }
    
    Temp_Memory temp = begin_temp(scratch);
    Buffer_ID *match_buffers = push_array(scratch, Buffer_ID, match_count);
    i32 *new_positions = push_array(scratch, i32, match_count);
    match_count = 0;
    
    if (lowest_edit_number != -1){
        for (Buffer_ID buffer = first_buffer_match;
             buffer != 0;
             buffer = get_buffer_next(app, buffer, AccessAll)){
            b32 did_match = false;
            i32 new_edit_position = 0;
            History_Record_Index max_index = buffer_history_get_max_record_index(app, buffer);
            for (;;){
                History_Record_Index index = buffer_history_get_current_state_index(app, buffer);
                if (index < max_index){
                    Record_Info record = buffer_history_get_record_info(app, buffer, index + 1);
                    if (record.edit_number == lowest_edit_number){
                        did_match = true;
                        new_edit_position = record_get_new_cursor_position_redo(app, buffer, index + 1, record);
                        buffer_history_set_current_state_index(app, buffer, index + 1);
                    }
                    else{
                        break;
                    }
                }
                else{
                    break;
                }
            }
            if (did_match){
                match_buffers[match_count] = buffer;
                new_positions[match_count] = new_edit_position;
                match_count += 1;
            }
            if (buffer == last_buffer_match){
                break;
            }
        }
    }
    
    view_buffer_set(app, match_buffers, new_positions, match_count);
    
    end_temp(temp);
}

////////////////////////////////

#if 0
CUSTOM_COMMAND_SIG(reload_themes)
CUSTOM_DOC("Loads all the theme files in the theme folder, replacing duplicates with the new theme data.")
{
    String fcoder_extension = make_lit_string(".4coder");
    save_all_dirty_buffers_with_postfix(app, fcoder_extension);
    
    Partition *scratch = &global_part;
    Temp_Memory temp = begin_temp_memory(scratch);
    load_folder_of_themes_into_live_set(app, scratch, "themes");
    String name = get_theme_name(app, scratch, 0);
    i32 theme_count = get_theme_count(app);
    for (i32 i = 1; i < theme_count; i += 1){
        Temp_Memory sub_temp = begin_temp_memory(scratch);
        String style_name = get_theme_name(app, scratch, i);
        if (match(name, style_name)){
            change_theme_by_index(app, i);
            break;
        }
        end_temp_memory(sub_temp);
    }
    end_temp_memory(temp);
}
#endif

CUSTOM_COMMAND_SIG(open_in_other)
CUSTOM_DOC("Interactively opens a file in the other panel.")
{
    change_active_panel(app);
    interactive_open_or_new(app);
}

// BOTTOM

