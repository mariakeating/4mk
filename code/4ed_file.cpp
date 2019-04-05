/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 03.01.2017
 *
 * File layer for 4coder
 *
 */

// TOP

internal Buffer_Slot_ID
to_file_id(i32 id){
    Buffer_Slot_ID result;
    result.id = id;
    return(result);
}

////////////////////////////////

internal void
file_edit_positions_set_cursor(File_Edit_Positions *edit_pos, i32 pos){
    edit_pos->cursor_pos = pos;
    edit_pos->last_set_type = EditPos_CursorSet;
}

internal void
file_edit_positions_set_scroll(File_Edit_Positions *edit_pos, GUI_Scroll_Vars scroll, i32 max_y){
    scroll.target_y = clamp(0, scroll.target_y, max_y);
    edit_pos->scroll = scroll;
    edit_pos->last_set_type = EditPos_ScrollSet;
}

internal void
file_edit_positions_push(Editing_File *file, File_Edit_Positions edit_pos){
    if (file->state.edit_pos_stack_top + 1 < ArrayCount(file->state.edit_pos_stack)){
        file->state.edit_pos_stack_top += 1;
        file->state.edit_pos_stack[file->state.edit_pos_stack_top] = edit_pos;
    }
}

internal File_Edit_Positions
file_edit_positions_pop(Editing_File *file){
    File_Edit_Positions edit_pos = {};
    if (file->state.edit_pos_stack_top >= 0){
        edit_pos = file->state.edit_pos_stack[file->state.edit_pos_stack_top];
        file->state.edit_pos_stack_top -= 1;
    }
    else{
        edit_pos = file->state.edit_pos_most_recent;
    }
    return(edit_pos);
}

////////////////////////////////

internal u32
file_get_access_flags(Editing_File *file){
    u32 flags = 0;
    if (file->settings.read_only){
        flags |= AccessProtected;
    }
    return(flags);
}

internal b32
file_needs_save(Editing_File *file){
    b32 result = false;
    if (HasFlag(file->state.dirty, DirtyState_UnsavedChanges)){
        result = true;
    }
    return(result);
}

internal b32
file_can_save(Editing_File *file){
    b32 result = false;
    if (HasFlag(file->state.dirty, DirtyState_UnsavedChanges) ||
        HasFlag(file->state.dirty, DirtyState_UnloadedChanges)){
        result = true;
    }
    return(result);
}

internal b32
file_is_ready(Editing_File *file){
    b32 result = false;
    if (file != 0 && file->is_loading == 0){
        result = true;
    }
    return(result);
}

internal void
file_set_unimportant(Editing_File *file, b32 val){
    if (val){
        file->state.dirty = DirtyState_UpToDate;
    }
    file->settings.unimportant = (b8)(val != false);
}

internal void
file_set_to_loading(Editing_File *file){
    memset(&file->state, 0, sizeof(file->state));
    memset(&file->settings, 0, sizeof(file->settings));
    file->is_loading = true;
}

internal void
file_add_dirty_flag(Editing_File *file, Dirty_State state){
    if (!file->settings.unimportant){
        file->state.dirty |= state;
    }
    else{
        file->state.dirty = DirtyState_UpToDate;
    }
}

internal void
file_clear_dirty_flags(Editing_File *file){
    file->state.dirty = DirtyState_UpToDate;
}

////////////////////////////////

internal b32
save_file_to_name(System_Functions *system, Models *models, Editing_File *file, char *file_name){
    b32 result = false;
    b32 using_actual_file_name = false;
    
    if (file_name == 0){
        terminate_with_null(&file->canon.name);
        file_name = file->canon.name.str;
        using_actual_file_name = true;
    }
    
    if (file_name != 0){
        Mem_Options *mem = &models->mem;
        if (models->hook_save_file != 0){
            models->hook_save_file(&models->app_links, file->id.id);
        }
        
        Gap_Buffer *buffer = &file->state.buffer;
        b32 dos_write_mode = file->settings.dos_write_mode;
        
        i32 max = 0;
        if (dos_write_mode){
            max = buffer_size(buffer) + buffer->line_count + 1;
        }
        else{
            max = buffer_size(buffer);
        }
        
        b32 used_heap = 0;
        Temp_Memory temp = begin_temp_memory(&mem->part);
        char empty = 0;
        char *data = 0;
        if (max == 0){
            data = &empty;
        }
        else{
            data = (char*)push_array(&mem->part, char, max);
            if (!data){
                used_heap = 1;
                data = heap_array(&mem->heap, char, max);
            }
        }
        Assert(data != 0);
        
        i32 size = 0;
        if (dos_write_mode){
            size = buffer_convert_out(buffer, data, max);
        }
        else{
            size = max;
            buffer_stringify(buffer, 0, size, data);
        }
        
        if (!using_actual_file_name && file->canon.name.str != 0){
            char space[512];
            u32 length = str_size(file_name);
            system->get_canonical(file_name, length, space, sizeof(space));
            
            char *source_path = file->canon.name.str;
            if (match(space, source_path)){
                using_actual_file_name = true;
            }
        }
        
        File_Attributes new_attributes = system->save_file(file_name, data, size);
        if (new_attributes.last_write_time > 0){
            if (using_actual_file_name){
                file->state.ignore_behind_os = 1;
            }
            file->attributes = new_attributes;
        }
        
        file_clear_dirty_flags(file);
        
        if (used_heap){
            heap_free(&mem->heap, data);
        }
        end_temp_memory(temp);
        
    }
    
    return(result);
}

internal b32
save_file(System_Functions *system, Models *models, Editing_File *file){
    b32 result = save_file_to_name(system, models, file, 0);
    return(result);
}

////////////////////////////////

internal b32
file_compute_partial_cursor(Editing_File *file, Buffer_Seek seek, Partial_Cursor *cursor){
    b32 result = true;
    switch (seek.type){
        case buffer_seek_pos:
        {
            *cursor = buffer_partial_from_pos(&file->state.buffer, seek.pos);
        }break;
        
        case buffer_seek_line_char:
        {
            *cursor = buffer_partial_from_line_character(&file->state.buffer, seek.line, seek.character);
        }break;
        
        // TODO(allen): do(support buffer_seek_character_pos and character_pos coordiantes in partial cursor system)
        
        default:
        {
            result = false;
        }break;
    }
    return(result);
}

internal Full_Cursor
file_compute_cursor__inner(System_Functions *system, Editing_File *file, Buffer_Seek seek, b32 return_hint){
    Font_Pointers font = system->font.get_pointers_by_id(file->settings.font_id);
    Assert(font.valid);
    
    Full_Cursor result = {};
    
    Buffer_Cursor_Seek_Params params;
    params.buffer           = &file->state.buffer;
    params.seek             = seek;
    params.system           = system;
    params.font             = font;
    params.wrap_line_index  = file->state.wrap_line_index;
    params.character_starts = file->state.character_starts;
    params.virtual_white    = file->settings.virtual_white;
    params.return_hint      = return_hint;
    params.cursor_out       = &result;
    
    Buffer_Cursor_Seek_State state = {};
    Buffer_Layout_Stop stop = {};
    
    i32 size = buffer_size(params.buffer);
    
    f32 line_shift = 0.f;
    b32 do_wrap = false;
    i32 wrap_unit_end = 0;
    
    b32 first_wrap_determination = true;
    i32 wrap_array_index = 0;
    
    do{
        stop = buffer_cursor_seek(&state, params, line_shift, do_wrap, wrap_unit_end);
        switch (stop.status){
            case BLStatus_NeedWrapDetermination:
            {
                if (stop.pos >= size){
                    do_wrap = false;
                    wrap_unit_end = max_i32;
                }
                else{
                    if (first_wrap_determination){
                        wrap_array_index = binary_search(file->state.wrap_positions, stop.pos, 0, file->state.wrap_position_count);
                        ++wrap_array_index;
                        if (file->state.wrap_positions[wrap_array_index] == stop.pos){
                            do_wrap = true;
                            wrap_unit_end = file->state.wrap_positions[wrap_array_index];
                        }
                        else{
                            do_wrap = false;
                            wrap_unit_end = file->state.wrap_positions[wrap_array_index];
                        }
                        first_wrap_determination = false;
                    }
                    else{
                        Assert(stop.pos == wrap_unit_end);
                        do_wrap = true;
                        ++wrap_array_index;
                        wrap_unit_end = file->state.wrap_positions[wrap_array_index];
                    }
                }
            }break;
            
            case BLStatus_NeedWrapLineShift:
            case BLStatus_NeedLineShift:
            {
                line_shift = file->state.line_indents[stop.wrap_line_index];
            }break;
        }
    }while(stop.status != BLStatus_Finished);
    
    return(result);
}

internal Full_Cursor
file_compute_cursor(System_Functions *system, Editing_File *file, Buffer_Seek seek){
    return(file_compute_cursor__inner(system, file, seek, false));
}

internal Full_Cursor
file_compute_cursor_hint(System_Functions *system, Editing_File *file, Buffer_Seek seek){
    return(file_compute_cursor__inner(system, file, seek, true));
}

////////////////////////////////

internal i32
file_grow_starts_as_needed(Heap *heap, Gap_Buffer *buffer, i32 additional_lines){
    b32 result = GROW_NOT_NEEDED;
    i32 max = buffer->line_max;
    i32 count = buffer->line_count;
    i32 target_lines = count + additional_lines;
    if (target_lines > max || max == 0){
        max = l_round_up_i32(target_lines + max, KB(1));
        i32 *new_lines = heap_array(heap, i32, max);
        if (new_lines != 0){
            result = GROW_SUCCESS;
            memcpy(new_lines, buffer->line_starts, sizeof(*new_lines)*count);
            heap_free(heap, buffer->line_starts);
            buffer->line_starts = new_lines;
            buffer->line_max = max;
        }
        else{
            result = GROW_FAILED;
        }
    }
    return(result);
}

internal void
file_measure_starts(Heap *heap, Gap_Buffer *buffer){
    if (buffer->line_starts == 0){
        i32 max = buffer->line_max = KB(1);
        buffer->line_starts = heap_array(heap, i32, max);
        Assert(buffer->line_starts != 0);
    }
    
    Buffer_Measure_Starts state = {};
    for (;buffer_measure_starts(&state, buffer);){
        i32 count = state.count;
        i32 max = ((buffer->line_max + 1) << 1);
        i32 *new_lines = heap_array(heap, i32, max);
        Assert(new_lines != 0);
        memcpy(new_lines, buffer->line_starts, sizeof(*new_lines)*count);
        heap_free(heap, buffer->line_starts);
        buffer->line_starts = new_lines;
        buffer->line_max = max;
    }
}

internal void
file_allocate_metadata_as_needed(Heap *heap, Gap_Buffer *buffer, void **mem, i32 *mem_max_count, i32 count, i32 item_size){
    if (*mem == 0){
        i32 max = l_round_up_i32(((count + 1)*2), KB(1));
        *mem = heap_allocate(heap, max*item_size);
        *mem_max_count = max;
        Assert(*mem != 0);
    }
    else if (*mem_max_count < count){
        i32 old_max = *mem_max_count;
        i32 max = l_round_up_i32(((count + 1)*2), KB(1));
        void *new_mem = heap_allocate(heap, item_size*max);
        memcpy(new_mem, *mem, item_size*old_max);
        heap_free(heap, *mem);
        *mem = new_mem;
        *mem_max_count = max;
        Assert(*mem != 0);
    }
}

internal void
file_allocate_character_starts_as_needed(Heap *heap, Editing_File *file){
    file_allocate_metadata_as_needed(heap,
                                     &file->state.buffer, (void**)&file->state.character_starts,
                                     &file->state.character_start_max, file->state.buffer.line_count + 1, sizeof(i32));
}

internal void
file_allocate_indents_as_needed(Heap *heap, Editing_File *file, i32 min_last_index){
    i32 min_amount = min_last_index + 1;
    file_allocate_metadata_as_needed(heap,
                                     &file->state.buffer, (void**)&file->state.line_indents,
                                     &file->state.line_indent_max, min_amount, sizeof(f32));
}

internal void
file_allocate_wraps_as_needed(Heap *heap, Editing_File *file){
    file_allocate_metadata_as_needed(heap,
                                     &file->state.buffer, (void**)&file->state.wrap_line_index,
                                     &file->state.wrap_max, file->state.buffer.line_count + 1, sizeof(f32));
}

internal void
file_allocate_wrap_positions_as_needed(Heap *heap, Editing_File *file, i32 min_last_index){
    i32 min_amount = min_last_index + 1;
    file_allocate_metadata_as_needed(heap,
                                     &file->state.buffer, (void**)&file->state.wrap_positions,
                                     &file->state.wrap_position_max, min_amount, sizeof(f32));
}

////////////////////////////////

internal void
file_create_from_string(System_Functions *system, Models *models, Editing_File *file, String val, File_Attributes attributes){
    Heap *heap = &models->mem.heap;
    Partition *part = &models->mem.part;
    Application_Links *app_links = &models->app_links;
    
    block_zero_struct(&file->state);
    Gap_Buffer_Init init = buffer_begin_init(&file->state.buffer, val.str, val.size);
    for (;buffer_init_need_more(&init);){
        i32 page_size = buffer_init_page_size(&init);
        page_size = l_round_up_i32(page_size, KB(4));
        if (page_size < KB(4)){
            page_size = KB(4);
        }
        void *data = heap_allocate(heap, page_size);
        buffer_init_provide_page(&init, data, page_size);
    }
    
    i32 scratch_size = part_remaining(part);
    Assert(scratch_size > 0);
    b32 init_success = buffer_end_init(&init, part->base + part->pos, scratch_size);
    AllowLocal(init_success); Assert(init_success);
    
    if (buffer_size(&file->state.buffer) < val.size){
        file->settings.dos_write_mode = true;
    }
    file_clear_dirty_flags(file);
    file->attributes = attributes;
    
    Face_ID font_id = models->global_font_id;
    file->settings.font_id = font_id;
    Font_Pointers font = system->font.get_pointers_by_id(font_id);
    Assert(font.valid);
    
    file_measure_starts(heap, &file->state.buffer);
    
    file_allocate_character_starts_as_needed(heap, file);
    buffer_measure_character_starts(system, font, &file->state.buffer, file->state.character_starts, 0, file->settings.virtual_white);
    
    file_measure_wraps(system, &models->mem, file, font);
    //adjust_views_looking_at_files_to_new_cursor(system, models, file);
    
    file->lifetime_object = lifetime_alloc_object(heap, &models->lifetime_allocator, DynamicWorkspace_Buffer, file);
    history_init(&models->app_links, &file->state.history);
    
    // TODO(allen): do(cleanup the create and settings dance)
    // Right now we have this thing where we call hook_open_file, which may or may not
    // trigger a lex job or serial lex, or might just flag the buffer to say it wants
    // tokens.  Then we check if we need to lex still and do it here too.  Better would
    // be to make sure it always happens in one way.
    Open_File_Hook_Function *hook_open_file = models->hook_open_file;
    if (hook_open_file != 0){
        hook_open_file(app_links, file->id.id);
    }
    
    if (file->settings.tokens_exist){
        if (file->state.token_array.tokens == 0){
            file_first_lex(system, models, file);
        }
    }
    else{
        file_mark_edit_finished(&models->working_set, file);
    }
    
    file->settings.is_initialized = true;
}

internal void
file_free(System_Functions *system, Heap *heap, Lifetime_Allocator *lifetime_allocator, Editing_File *file){
    if (file->state.still_lexing){
        system->cancel_job(BACKGROUND_THREADS, file->state.lex_job);
        if (file->state.swap_array.tokens){
            heap_free(heap, file->state.swap_array.tokens);
            file->state.swap_array.tokens = 0;
        }
    }
    if (file->state.token_array.tokens){
        heap_free(heap, file->state.token_array.tokens);
    }
    
    lifetime_free_object(heap, lifetime_allocator, file->lifetime_object);
    
    Gap_Buffer *buffer = &file->state.buffer;
    if (buffer->data){
        heap_free(heap, buffer->data);
        heap_free(heap, buffer->line_starts);
    }
    
    heap_free(heap, file->state.wrap_line_index);
    heap_free(heap, file->state.character_starts);
    heap_free(heap, file->state.line_indents);
    
    history_free(heap, &file->state.history);
    
    file_unmark_edit_finished(file);
}

////////////////////////////////

internal i32
file_get_current_record_index(Editing_File *file){
    return(file->state.current_record_index);
}

internal b32
file_tokens_are_ready(Editing_File *file){
    return(file->state.token_array.tokens != 0 && file->state.tokens_complete && !file->state.still_lexing);
}

internal Managed_Scope
file_get_managed_scope(Editing_File *file){
    Managed_Scope scope = 0;
    if (file != 0){
        Assert(file->lifetime_object != 0);
        scope = (Managed_Scope)file->lifetime_object->workspace.scope_id;
    }
    return(scope);
}

// BOTTOM

