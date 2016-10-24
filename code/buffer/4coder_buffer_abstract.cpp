/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 24.10.2015
 *
 * Buffer features based on the stringify loop,
 *  and other abstract buffer features.
 *
 */

// TOP

#define Buffer_Init_Type cat_4tech(Buffer_Type, _Init)
#define Buffer_Stream_Type cat_4tech(Buffer_Type, _Stream)

inline_4tech void
buffer_stringify(Buffer_Type *buffer, i32 start, i32 end, char *out){
    Buffer_Stream_Type stream = {0};
    
    i32 i = start;
    if (buffer_stringify_loop(&stream, buffer, i, end)){
        b32 still_looping = 0;
        do{
            i32 size = stream.end - i;
            memcpy_4tech(out, stream.data + i, size);
            i = stream.end;
            out += size;
            still_looping = buffer_stringify_next(&stream);
        }while(still_looping);
    }
}

internal_4tech i32
buffer_convert_out(Buffer_Type *buffer, char *dest, i32 max){
    Buffer_Stream_Type stream = {0};
    i32 i = 0;
    i32 size = buffer_size(buffer);
    assert_4tech(size + buffer->line_count <= max);
    
    i32 pos = 0;
    if (buffer_stringify_loop(&stream, buffer, 0, size)){
        b32 still_looping = 0;
        do{
            i32 size = stream.end - i;
            i32 out_size = 0;
            i32 result = eol_convert_out(dest + pos, max - pos, stream.data + i, size, &out_size);
            assert_4tech(result);
            i = stream.end;
            pos += out_size;
            still_looping = buffer_stringify_next(&stream);
        }while(still_looping);
    }
    
    return(pos);
}

internal_4tech i32
buffer_count_newlines(Buffer_Type *buffer, i32 start, i32 end){
    Buffer_Stream_Type stream = {0};
    i32 i = start;
    i32 count = 0;
    
    assert_4tech(0 <= start);
    assert_4tech(start <= end);
    assert_4tech(end <= buffer_size(buffer));
    
    if (buffer_stringify_loop(&stream, buffer, i, end)){
        b32 still_looping = 0;
        do{
            for (; i < stream.end; ++i){
                count += (stream.data[i] == '\n');
            }
            still_looping = buffer_stringify_next(&stream);
        }while(still_looping);
    }
    
    return(count);
}

typedef struct Buffer_Measure_Starts{
    i32 i;
    i32 count;
    i32 start;
} Buffer_Measure_Starts;

// TODO(allen): Rewrite this with a duff routine
// Also make it so that the array goes one past the end
// and stores the size in the extra spot.
internal_4tech i32
buffer_measure_starts(Buffer_Measure_Starts *state, Buffer_Type *buffer){
    Buffer_Stream_Type stream = {0};
    i32 size = buffer_size(buffer);
    i32 start = state->start, i = state->i;
    i32 *start_ptr = buffer->line_starts + state->count;
    i32 *start_end = buffer->line_starts + buffer->line_max;
    i32 result = 1;
    
    if (buffer_stringify_loop(&stream, buffer, i, size)){
        b32 still_looping = 0;
        do{
            for (; i < stream.end; ++i){
                char ch = stream.data[i];
                if (ch == '\n'){
                    if (start_ptr == start_end){
                        goto buffer_measure_starts_widths_end;
                    }
                    
                    *start_ptr++ = start;
                    start = i + 1;
                }
            }
            still_looping = buffer_stringify_next(&stream);
        }while(still_looping);
    }
    
    assert_4tech(i == size);
    
    if (start_ptr == start_end){
        goto buffer_measure_starts_widths_end;
    }
    *start_ptr++ = start;
    result = 0;
    
    buffer_measure_starts_widths_end:;
    state->i = i;
    state->count = (i32)(start_ptr - buffer->line_starts);
    state->start = start;
    
    if (!result){
        buffer->line_count = state->count;
    }
    
    return(result);
}

internal_4tech void
buffer_measure_character_starts(Buffer_Type *buffer, i32 *character_starts, i32 mode, i32 virtual_white){
    assert_4tech(mode == 0);
    
    Buffer_Stream_Type stream = {0};
    i32 i = 0;
    i32 size = buffer_size(buffer);
    
    i32 line_index = 0;
    i32 character_index = 0;
    
    b32 skipping_whitespace = 0;
    
    character_starts[line_index++] = character_index;
    
    if (virtual_white){
        skipping_whitespace = 1;
    }
    
    stream.use_termination_character = 1;
    stream.terminator = '\n';
    if (buffer_stringify_loop(&stream, buffer, i, size)){
        b32 still_looping = 0;
        do{
            for (; i < stream.end; ++i){
                u8 ch = (u8)stream.data[i];
                if (ch == '\n'){
                    ++character_index;
                    character_starts[line_index++] = character_index;
                    if (virtual_white){
                        skipping_whitespace = 1;
                    }
                }
                else{
                    if (ch != ' ' && ch != '\t'){
                        skipping_whitespace = 0;
                    }
                    
                    if (!skipping_whitespace){
                        ++character_index;
                    }
                }
            }
            still_looping = buffer_stringify_next(&stream);
        }while(still_looping);
    }
    
    assert_4tech(line_index-1 == buffer->line_count);
}

enum{
    BLStatus_Finished,
    BLStatus_NeedWrapLineShift,
    BLStatus_NeedLineShift,
    BLStatus_NeedWrapDetermination,
};

struct Buffer_Layout_Stop{
    u32 status;
    i32 line_index;
    i32 wrap_line_index;
    i32 pos;
    i32 next_line_pos;
    f32 x;
};

struct Buffer_Measure_Wrap_Params{
    Buffer_Type *buffer;
    i32 *wrap_line_index;
    f32 *adv;
    f32 width;
    b32 virtual_white;
};

struct Buffer_Measure_Wrap_State{
    Buffer_Stream_Type stream;
    i32 i;
    i32 size;
    b32 still_looping;
    
    i32 line_index;
    
    i32 current_wrap_index;
    f32 current_adv;
    f32 x;
    
    b32 skipping_whitespace;
    i32 wrap_unit_end;
    b32 did_wrap;
    b32 first_of_the_line;
    
    u8 ch;
    
    i32 __pc__;
};

// duff-routine defines
#define DrCase(PC) case PC: goto resumespot_##PC
#define DrYield(PC, n) { *S_ptr = S; S_ptr->__pc__ = PC; return(n); resumespot_##PC:; }
#define DrReturn(n) { *S_ptr = S; S_ptr->__pc__ = -1; return(n); }

internal_4tech Buffer_Layout_Stop
buffer_measure_wrap_y(Buffer_Measure_Wrap_State *S_ptr, Buffer_Measure_Wrap_Params params,
                      f32 line_shift, b32 do_wrap, i32 wrap_unit_end){
    Buffer_Measure_Wrap_State S = *S_ptr;
    Buffer_Layout_Stop S_stop;
    
    switch (S.__pc__){
        DrCase(1);
        DrCase(2);
        DrCase(3);
        DrCase(4);
    }
    
    S.size = buffer_size(params.buffer);
    
    if (params.virtual_white){
        S_stop.status = BLStatus_NeedLineShift;
        S_stop.line_index = S.line_index;
        S_stop.wrap_line_index = S.current_wrap_index;
        S_stop.pos = S.i;
        DrYield(1, S_stop);
    }
    
    S.x = line_shift;
    params.wrap_line_index[S.line_index++] = 0;
    
    if (params.virtual_white){
        S.skipping_whitespace = 1;
    }
    
    S.first_of_the_line = 1;
    if (buffer_stringify_loop(&S.stream, params.buffer, S.i, S.size)){
        S.still_looping = 0;
        do{
            for (; S.i < S.stream.end; ++S.i){
                S.ch = (u8)S.stream.data[S.i];
                if (S.ch == '\n'){
                    ++S.current_wrap_index;
                    params.wrap_line_index[S.line_index++] = S.current_wrap_index;
                    
                    if (params.virtual_white){
                        S_stop.status          = BLStatus_NeedLineShift;
                        S_stop.line_index      = S.line_index - 1;
                        S_stop.wrap_line_index = S.current_wrap_index;
                        S_stop.pos             = S.i+1;
                        DrYield(2, S_stop);
                    }
                    
                    S.x = line_shift;
                    
                    if (params.virtual_white){
                        S.skipping_whitespace = 1;
                    }
                    S.first_of_the_line = 1;
                }
                else{
                    if (S.ch != ' ' && S.ch != '\t'){
                        S.skipping_whitespace = 0;
                    }
                    
                    if (!S.skipping_whitespace){
                        S.current_adv = params.adv[S.ch];
                        
                        S.did_wrap = 0;
                        if (S.i >= S.wrap_unit_end){
                            S_stop.status          = BLStatus_NeedWrapDetermination;
                            S_stop.line_index      = S.line_index - 1;
                            S_stop.wrap_line_index = S.current_wrap_index;
                            S_stop.pos             = S.i;
                            S_stop.x               = S.x;
                            DrYield(4, S_stop);
                            
                            S.wrap_unit_end = wrap_unit_end;
                            
                            if (do_wrap && !S.first_of_the_line){
                                S.did_wrap = 1;
                                ++S.current_wrap_index;
                                
                                if (params.virtual_white){
                                    S_stop.status          = BLStatus_NeedWrapLineShift;
                                    S_stop.line_index      = S.line_index - 1;
                                    S_stop.wrap_line_index = S.current_wrap_index;
                                    S_stop.pos             = S.i;
                                    DrYield(3, S_stop);
                                }
                                
                                S.x = line_shift + S.current_adv;
                            }
                        }
                        
                        if (!S.did_wrap){
                            S.x += S.current_adv;
                        }
                        
                        S.first_of_the_line = 0;
                    }
                }
            }
            S.still_looping = buffer_stringify_next(&S.stream);
        }while(S.still_looping);
    }
    
    ++S.current_wrap_index;
    params.wrap_line_index[S.line_index++] = S.current_wrap_index;
    
    assert_4tech(S.line_index-1 == params.buffer->line_count);
    
    S_stop.status = BLStatus_Finished;
    DrReturn(S_stop);
}

#undef DrCase
#undef DrYield
#undef DrReturn

internal_4tech void
buffer_remeasure_starts(Buffer_Type *buffer, i32 line_start, i32 line_end, i32 line_shift, i32 text_shift){
    i32 *starts = buffer->line_starts;
    i32 line_count = buffer->line_count;
    
    assert_4tech(0 <= line_start);
    assert_4tech(line_start <= line_end);
    assert_4tech(line_end < line_count);
    assert_4tech(line_count + line_shift <= buffer->line_max);
    
    ++line_end;
    
    // Adjust
    if (text_shift != 0){
        i32 line_i = line_end;
        starts += line_i;
        for (; line_i < line_count; ++line_i, ++starts){
            *starts += text_shift;
        }
        starts = buffer->line_starts;
    }
    
    // Shift
    i32 new_line_count = line_count;
    i32 new_line_end = line_end;
    if (line_shift != 0){
        new_line_count += line_shift;
        new_line_end += line_shift;
        
        memmove_4tech(starts + line_end + line_shift, starts + line_end,
                      sizeof(i32)*(line_count - line_end));
    }
    
    // Iteration data (yikes! Need better loop system)
    Buffer_Stream_Type stream = {0};
    i32 size = buffer_size(buffer);
    i32 char_i = starts[line_start];
    i32 line_i = line_start;
    
    // Line start measurement
    i32 start = char_i;
    
    if (buffer_stringify_loop(&stream, buffer, char_i, size)){
        b32 still_looping = 0;
        do{
            for (; char_i < stream.end; ++char_i){
                u8 ch = (u8)stream.data[char_i];
                
                if (ch == '\n'){
                    starts[line_i] = start;
                    ++line_i;
                    start = char_i + 1;
                    
                    // TODO(allen): I would like to know that I am not guessing here,
                    // so let's try to turn the && into an Assert.
                    if (line_i >= new_line_end && (line_i >= new_line_count || start == starts[line_i])){
                        goto buffer_remeasure_starts_end;
                    }
                }
            }
            still_looping = buffer_stringify_next(&stream);
        }while(still_looping);
    }
    
    // TODO(allen): I suspect this can just go away.
    if (char_i == size){
        starts[line_i++] = start;
    }
    
    buffer_remeasure_starts_end:;
    assert_4tech(line_count >= 1);
    buffer->line_count = new_line_count;
}

internal_4tech void
buffer_remeasure_character_starts(Buffer_Type *buffer, i32 line_start, i32 line_end, i32 line_shift,
                                  i32 *character_starts, i32 mode, i32 virtual_whitespace){
    assert_4tech(mode == 0);
    
    i32 new_line_count = buffer->line_count;
    
    assert_4tech(0 <= line_start);
    assert_4tech(line_start <= line_end);
    assert_4tech(line_end < new_line_count - line_shift);
    
    ++line_end;
    
    // Shift
    i32 line_count = new_line_count;
    i32 new_line_end = line_end;
    if (line_shift != 0){
        line_count -= line_shift;
        new_line_end += line_shift;
        
        memmove_4tech(character_starts + line_end + line_shift, character_starts + line_end,
                      sizeof(i32)*(line_count - line_end + 1));
    }
    
    // Iteration data (yikes! Need better loop system)
    Buffer_Stream_Type stream = {0};
    i32 size = buffer_size(buffer);
    i32 char_i = buffer->line_starts[line_start];
    i32 line_i = line_start;
    
    // Character measurement
    i32 last_char_start = character_starts[line_i];
    i32 current_char_start = last_char_start;
    
    b32 skipping_whitespace = 0;
    
    if (virtual_whitespace){
        skipping_whitespace = 1;
    }
    
    stream.use_termination_character = 1;
    stream.terminator = '\n';
    if (buffer_stringify_loop(&stream, buffer, char_i, size)){
        b32 still_looping = 0;
        do{
            for (; char_i < stream.end; ++char_i){
                u8 ch = (u8)stream.data[char_i];
                if (ch == '\n'){
                    character_starts[line_i++] = last_char_start;
                    ++current_char_start;
                    last_char_start = current_char_start;
                    if (virtual_whitespace){
                        skipping_whitespace = 1;
                    }
                    
                    if (line_i >= new_line_end){
                        goto buffer_remeasure_character_starts_end;
                    }
                }
                else{
                    if (ch != ' ' && ch != '\t'){
                        skipping_whitespace = 0;
                    }
                    
                    if (!skipping_whitespace){
                        ++current_char_start;
                    }
                }
            }
            still_looping = buffer_stringify_next(&stream);
        }while(still_looping);
    }
    
    assert_4tech(line_i >= new_line_end);
    
    buffer_remeasure_character_starts_end:;
    
    // Adjust
    if (line_i <= new_line_end){
        i32 character_shift = current_char_start - character_starts[line_i];
        
        if (character_shift != 0){
            character_starts += line_i;
            for (; line_i <= new_line_count; ++line_i, ++character_starts){
                *character_starts += character_shift;
            }
        }
    }
}

internal_4tech void
buffer_remeasure_wrap_y(Buffer_Type *buffer, i32 line_start, i32 line_end, i32 line_shift,
                        f32 *wraps, f32 font_height, f32 *adv, f32 max_width){
    i32 new_line_count = buffer->line_count;
    
    assert_4tech(0 <= line_start);
    assert_4tech(line_start <= line_end);
    assert_4tech(line_end < new_line_count - line_shift);
    
    ++line_end;
    
    // Shift
    i32 line_count = new_line_count;
    i32 new_line_end = line_end;
    if (line_shift != 0){
        line_count -= line_shift;
        new_line_end += line_shift;
        
        memmove_4tech(wraps + line_end + line_shift, wraps + line_end,
                      sizeof(i32)*(line_count - line_end + 1));
    }
    
    // Iteration data (yikes! Need better loop system)
    Buffer_Stream_Type stream = {0};
    i32 size = buffer_size(buffer);
    i32 char_i = buffer->line_starts[line_start];
    i32 line_i = line_start;
    
    // Line wrap measurement
    f32 last_wrap = wraps[line_i];
    f32 current_wrap = last_wrap;
    f32 x = 0.f;
    
    if (buffer_stringify_loop(&stream, buffer, char_i, size)){
        b32 still_looping = 0;
        do{
            for (; char_i < stream.end; ++char_i){
                u8 ch = (u8)stream.data[char_i];
                
                if (ch == '\n'){
                    wraps[line_i] = last_wrap;
                    ++line_i;
                    current_wrap += font_height;
                    last_wrap = current_wrap;
                    x = 0.f;
                    
                    // TODO(allen): I would like to know that I am not guessing here.
                    if (line_i >= new_line_end){
                        goto buffer_remeasure_wraps_end;
                    }
                }
                else{
                    f32 current_adv = adv[ch];
                    if (x + current_adv > max_width){
                        current_wrap += font_height;
                        x = current_adv;
                    }
                    else{
                        x += current_adv;
                    }
                }
            }
            still_looping = buffer_stringify_next(&stream);
        }while(still_looping);
    }
    
    wraps[line_i++] = last_wrap;
    
    buffer_remeasure_wraps_end:;
    
    // Adjust
    if (line_i <= new_line_end){
        f32 y_shift = current_wrap - wraps[line_i];
        
        if (y_shift != 0){
            wraps += line_i;
            for (; line_i <= new_line_count; ++line_i, ++wraps){
                *wraps += y_shift;
            }
        }
    }
}

internal_4tech i32
binary_search(i32 *array, i32 value, i32 l_bound, i32 u_bound){
    i32 start = l_bound, end = u_bound;
    i32 i = 0;
    
    for (;;){
        i = (start + end) >> 1;
        if (array[i] < value){
            start = i;
        }
        else if (array[i] > value){
            end = i;
        }
        else{
            break;
        }
        assert_4tech(start < end);
        if (start == end - 1){
            i = start;
            break;
        }
    }
    
    return(i);
}

inline_4tech i32
buffer_get_line_index_range(Buffer_Type *buffer, i32 pos, i32 l_bound, i32 u_bound){
    assert_4tech(0 <= l_bound);
    assert_4tech(l_bound <= u_bound);
    assert_4tech(u_bound <= buffer->line_count);
    
    assert_4tech(buffer->line_starts != 0);
    
    i32 i = binary_search(buffer->line_starts, pos, l_bound, u_bound);
    return(i);
}

inline_4tech i32
buffer_get_line_index(Buffer_Type *buffer, i32 pos){
    i32 result = buffer_get_line_index_range(buffer, pos, 0, buffer->line_count);
    return(result);
}

inline_4tech i32
buffer_get_line_index_from_character_pos(i32 *character_starts, i32 pos, i32 l_bound, i32 u_bound){
    i32 i = binary_search(character_starts, pos, l_bound, u_bound);
    return(i);
}

inline_4tech i32
buffer_get_line_index_from_wrapped_y(i32 *wrap_line_index, f32 y, f32 line_height, i32 l_bound, i32 u_bound){
    i32 wrap_index = FLOOR32(y/line_height);
    i32 i = binary_search(wrap_line_index, wrap_index, l_bound, u_bound);
    return(i);
}

internal_4tech Partial_Cursor
buffer_partial_from_pos(Buffer_Type *buffer, i32 pos){
    Partial_Cursor result = {0};
    
    int32_t size = buffer_size(buffer);
    if (pos > size){
        pos = size;
    }
    if (pos < 0){
        pos = 0;
    }
    
    i32 line_index = buffer_get_line_index_range(buffer, pos, 0, buffer->line_count);
    result.pos = pos;
    result.line = line_index+1;
    result.character = pos - buffer->line_starts[line_index] + 1;
    
    return(result);
}

internal_4tech Partial_Cursor
buffer_partial_from_line_character(Buffer_Type *buffer, i32 line, i32 character){
    Partial_Cursor result = {0};
    
    i32 line_index = line - 1;
    if (line_index >= buffer->line_count){
        line_index = buffer->line_count - 1;
    }
    if (line_index < 0){
        line_index = 0;
    }
    
    i32 size = buffer_size(buffer);
    i32 this_start = buffer->line_starts[line_index];
    i32 max_character = (size-this_start) + 1;
    if (line_index+1 < buffer->line_count){
        i32 next_start = buffer->line_starts[line_index+1];
        max_character = (next_start-this_start);
    }
    
    if (character <= 0){
        character = 1;
    }
    if (character > max_character){
        character = max_character;
    }
    
    result.pos = this_start + character - 1;
    result.line = line_index + 1;
    result.character = character;
    
    return(result);
}

struct Buffer_Cursor_Seek_Params{
    Buffer_Type *buffer;
    Buffer_Seek seek;
    f32 width;
    f32 font_height;
    f32 *adv;
    i32 *wrap_line_index;
    i32 *character_starts;
    b32 virtual_white;
    b32 return_hint;
    Full_Cursor *cursor_out;
};

struct Buffer_Cursor_Seek_State{
    Full_Cursor next_cursor;
    Full_Cursor this_cursor;
    Full_Cursor prev_cursor;
    
    Buffer_Stream_Type stream;
    b32 still_looping;
    i32 i;
    i32 size;
    i32 wrap_unit_end;
    
    b32 first_of_the_line;
    b32 xy_seek;
    f32 ch_width;
    u8 ch;
    
    i32 __pc__;
};

// dialogical-routine defines
#define DrCase(PC) case PC: goto resumespot_##PC
#define DrYield(PC, n) { *S_ptr = S; S_ptr->__pc__ = PC; return(n); resumespot_##PC:; }
#define DrReturn(n) { *S_ptr = S; S_ptr->__pc__ = -1; return(n); }

internal_4tech Buffer_Layout_Stop
buffer_cursor_seek(Buffer_Cursor_Seek_State *S_ptr, Buffer_Cursor_Seek_Params params,
                   f32 line_shift, b32 do_wrap, i32 wrap_unit_end){
    Buffer_Cursor_Seek_State S = *S_ptr;
    Buffer_Layout_Stop S_stop;
    
    switch (S.__pc__){
        DrCase(1);
        DrCase(2);
        DrCase(3);
        DrCase(4);
    }
    
    S.xy_seek = (params.seek.type == buffer_seek_wrapped_xy || params.seek.type == buffer_seek_unwrapped_xy);
    S.size = buffer_size(params.buffer);
    
    // Get cursor hint
    {
        i32 line_index = 0;
        switch (params.seek.type){
            case buffer_seek_pos:
            {
                if (params.seek.pos > S.size){
                    params.seek.pos = S.size;
                }
                if (params.seek.pos < 0){
                    params.seek.pos = 0;
                }
                
                line_index = buffer_get_line_index(params.buffer, params.seek.pos);
            }break;
            
            case buffer_seek_character_pos:
            {
                i32 line_count = params.buffer->line_count;
                i32 max_character = params.character_starts[line_count] - 1;
                if (params.seek.pos > max_character){
                    params.seek.pos = max_character;
                }
                if (params.seek.pos < 0){
                    params.seek.pos = 0;
                }
                
                line_index = buffer_get_line_index_from_character_pos(params.character_starts, params.seek.pos,
                                                                      0, params.buffer->line_count);
            }break;
            
            case buffer_seek_line_char:
            {
                line_index = params.seek.line - 1;
                if (line_index >= params.buffer->line_count){
                    line_index = params.buffer->line_count - 1;
                }
                if (line_index < 0){
                    line_index = 0;
                }
            }break;
            
            case buffer_seek_unwrapped_xy:
            {
                line_index = (i32)(params.seek.y / params.font_height);
                if (line_index >= params.buffer->line_count){
                    line_index = params.buffer->line_count - 1;
                }
                if (line_index < 0){
                    line_index = 0;
                }
            }break;
            
            case buffer_seek_wrapped_xy:
            {
                line_index = buffer_get_line_index_from_wrapped_y(params.wrap_line_index, params.seek.y, params.font_height,
                                                                  0, params.buffer->line_count);
            }break;
            
            default: InvalidCodePath;
        }
        
        // Build the cursor hint
        S.next_cursor.pos = params.buffer->line_starts[line_index];
        S.next_cursor.character_pos = params.character_starts[line_index];
        S.next_cursor.line = line_index + 1;
        S.next_cursor.character = 1;
        S.next_cursor.wrap_line = params.wrap_line_index[line_index] + 1;
        S.next_cursor.unwrapped_y = (f32)(line_index * params.font_height);
        S.next_cursor.unwrapped_x = 0;
        S.next_cursor.wrapped_y = (f32)(params.wrap_line_index[line_index] * params.font_height);
        S.next_cursor.wrapped_x = 0;
    }
    
    // Get the initial line shift.
    // Adjust the non-screen based coordinates to point to the first
    // non-virtual character of the line.
    if (params.virtual_white){
        S_stop.status          = BLStatus_NeedLineShift;
        S_stop.line_index      = S.next_cursor.line-1;
        S_stop.wrap_line_index = S.next_cursor.wrap_line-1;
        DrYield(1, S_stop);
        
        S.next_cursor.unwrapped_x += line_shift;
        S.next_cursor.wrapped_x += line_shift;
        
        S.stream.use_termination_character = 1;
        S.stream.terminator = '\n';
        if (buffer_stringify_loop(&S.stream, params.buffer, S.next_cursor.pos, S.size)){
            do{
                for (; S.next_cursor.pos < S.stream.end; ++S.next_cursor.pos){
                    S.ch = (u8)S.stream.data[S.next_cursor.pos];
                    
                    if (S.ch != ' ' && S.ch != '\t'){
                        goto double_break_vwhite;
                    }
                    else{
                        ++S.next_cursor.character;
                    }
                }
                S.still_looping = buffer_stringify_next(&S.stream);
            }while(S.still_looping);
        }
        InvalidCodePath;
        double_break_vwhite:;
    }
    
    // If the user just wants the hint, return that now.
    if (params.return_hint){
        *params.cursor_out = S.next_cursor;
        S_stop.status = BLStatus_Finished;
        DrReturn(S_stop);
    }
    
    // If we are already passed the point we want to be at, then just take this.
    S.this_cursor = S.next_cursor;
    switch (params.seek.type){
        case buffer_seek_pos:
        {
            if (S.next_cursor.pos >= params.seek.pos){
                goto buffer_cursor_seek_end;
            }
        }break;
        
        case buffer_seek_character_pos:
        {
            if (S.next_cursor.character_pos >= params.seek.pos){
                goto buffer_cursor_seek_end;
            }
        }break;
        
        case buffer_seek_line_char:
        {
            if ((S.next_cursor.line == params.seek.line &&
                 S.next_cursor.character >= params.seek.character) ||
                S.next_cursor.line > params.seek.line){
                goto buffer_cursor_seek_end;
            }
        }break;
        
        case buffer_seek_unwrapped_xy:
        {
            if (S.next_cursor.unwrapped_y > params.seek.y){
                goto buffer_cursor_seek_end;
            }
        }break;
        
        case buffer_seek_wrapped_xy:
        {
            if (S.next_cursor.wrapped_y > params.seek.y){
                goto buffer_cursor_seek_end;
            }
        }break;
    }
    
    // Main seek loop
    S.i = S.next_cursor.pos;
    
    S.stream = null_buffer_stream;
    S.stream.use_termination_character = 1;
    S.stream.terminator = 0;
    
    S.first_of_the_line = 1;
    if (buffer_stringify_loop(&S.stream, params.buffer, S.i, S.size)){
        S.still_looping = 0;
        do{
            for (; S.i < S.stream.end; ++S.i){
                S.ch = (u8)S.stream.data[S.i];
                
                S.prev_cursor = S.this_cursor;
                S.this_cursor = S.next_cursor;
                
                switch (S.ch){
                    case '\n':
                    {
                        ++S.next_cursor.character_pos;
                        ++S.next_cursor.line;
                        ++S.next_cursor.wrap_line;
                        S.next_cursor.unwrapped_y += params.font_height;
                        S.next_cursor.wrapped_y += params.font_height;
                        S.next_cursor.character = 1;
                        S.next_cursor.unwrapped_x = 0;
                        
                        if (params.virtual_white){
                            S_stop.status          = BLStatus_NeedLineShift;
                            S_stop.line_index      = S.next_cursor.line-1;
                            S_stop.wrap_line_index = S.next_cursor.wrap_line-1;
                            DrYield(2, S_stop);
                        }
                        
                        S.next_cursor.wrapped_x = line_shift;
                        S.first_of_the_line = 1;
                    }break;
                    
                    default:
                    {
                        S.ch_width = params.adv[S.ch];
                        
                        if (S.i >= S.wrap_unit_end){
                            S_stop.status          = BLStatus_NeedWrapDetermination;
                            S_stop.line_index      = S.next_cursor.line-1;
                            S_stop.wrap_line_index = S.next_cursor.wrap_line-1;
                            S_stop.pos             = S.i;
                            S_stop.x               = S.next_cursor.wrapped_x;
                            DrYield(4, S_stop);
                            
                            S.wrap_unit_end = wrap_unit_end;
                            
                            if (do_wrap && !S.first_of_the_line){
                                S.next_cursor.wrapped_y += params.font_height;
                                
                                ++S.next_cursor.wrap_line;
                                if (params.virtual_white){
                                    S_stop.status          = BLStatus_NeedWrapLineShift;
                                    S_stop.line_index      = S.next_cursor.line-1;
                                    S_stop.wrap_line_index = S.next_cursor.wrap_line-1;
                                    DrYield(3, S_stop);
                                }
                                
                                S.next_cursor.wrapped_x = line_shift;
                                S.this_cursor = S.next_cursor;
                            }
                        }
                        
                        ++S.next_cursor.character_pos;
                        ++S.next_cursor.character;
                        S.next_cursor.unwrapped_x += S.ch_width;
                        S.next_cursor.wrapped_x += S.ch_width;
                        
                        S.first_of_the_line = 0;
                    }break;
                }
                
                ++S.next_cursor.pos;
                
                f32 x = 0, px = 0, y = 0, py = 0;
                switch (params.seek.type){
                    case buffer_seek_pos:
                    if (S.this_cursor.pos >= params.seek.pos){
                        goto buffer_cursor_seek_end;
                    }break;
                    
                    case buffer_seek_character_pos:
                    if (S.this_cursor.character_pos >= params.seek.pos){
                        goto buffer_cursor_seek_end;
                    }break;
                    
                    case buffer_seek_wrapped_xy:
                    {
                        x = S.this_cursor.wrapped_x;
                        px = S.prev_cursor.wrapped_x;
                        y = S.this_cursor.wrapped_y;
                        py = S.prev_cursor.wrapped_y;
                    }break;
                    
                    case buffer_seek_unwrapped_xy:
                    {
                        x = S.this_cursor.unwrapped_x;
                        px = S.prev_cursor.unwrapped_x;
                        y = S.this_cursor.unwrapped_y;
                        py = S.prev_cursor.wrapped_y;
                    }break;
                    
                    case buffer_seek_line_char:
                    if (S.this_cursor.line == params.seek.line && S.this_cursor.character >= params.seek.character){
                        goto buffer_cursor_seek_end;
                    }
                    else if (S.this_cursor.line > params.seek.line){
                        S.this_cursor = S.prev_cursor;
                        goto buffer_cursor_seek_end;
                    }break;
                }
                
                if (S.xy_seek){
                    if (y > params.seek.y){
                        S.this_cursor = S.prev_cursor;
                        goto buffer_cursor_seek_end;
                    }
                    
                    if (y > params.seek.y - params.font_height && x >= params.seek.x){
                        if (!params.seek.round_down){
                            if (py >= y && S.ch != '\n' && (params.seek.x - px) < (x - params.seek.x)){
                                S.this_cursor = S.prev_cursor;
                            }
                            goto buffer_cursor_seek_end;
                        }
                        
                        if (py >= y){
                            S.this_cursor = S.prev_cursor;
                        }
                        goto buffer_cursor_seek_end;
                    }
                }
                
                if (S.next_cursor.pos > S.size){
                    goto buffer_cursor_seek_end;
                }
            }
            S.still_looping = buffer_stringify_next(&S.stream);
        }while(S.still_looping);
    }
    
    InvalidCodePath;
    
    buffer_cursor_seek_end:;
    *params.cursor_out = S.this_cursor;
    S_stop.status = BLStatus_Finished;
    DrReturn(S_stop);
}

#undef DrCase
#undef DrYield
#undef DrReturn

internal_4tech void
buffer_invert_edit_shift(Buffer_Type *buffer, Buffer_Edit edit, Buffer_Edit *inverse, char *strings,
                         i32 *str_pos, i32 max, i32 shift_amount){
    i32 pos = *str_pos;
    i32 len = edit.end - edit.start;
    assert_4tech(pos >= 0);
    assert_4tech(pos + len <= max);
    *str_pos = pos + len;
    
    inverse->str_start = pos;
    inverse->len = len;
    inverse->start = edit.start + shift_amount;
    inverse->end = edit.start + edit.len + shift_amount;
    buffer_stringify(buffer, edit.start, edit.end, strings + pos);
}

inline_4tech void
buffer_invert_edit(Buffer_Type *buffer, Buffer_Edit edit, Buffer_Edit *inverse, char *strings,
                   i32 *str_pos, i32 max){
    buffer_invert_edit_shift(buffer, edit, inverse, strings, str_pos, max, 0);
}

typedef struct Buffer_Invert_Batch{
    i32 i;
    i32 shift_amount;
    i32 len;
} Buffer_Invert_Batch;

internal_4tech i32
buffer_invert_batch(Buffer_Invert_Batch *state, Buffer_Type *buffer, Buffer_Edit *edits, i32 count,
                    Buffer_Edit *inverse, char *strings, i32 *str_pos, i32 max){
    i32 shift_amount = state->shift_amount;
    i32 i = state->i;
    Buffer_Edit *edit = edits + i;
    Buffer_Edit *inv_edit = inverse + i;
    i32 result = 0;
    
    for (; i < count; ++i, ++edit, ++inv_edit){
        if (*str_pos + edit->end - edit->start <= max){
            buffer_invert_edit_shift(buffer, *edit, inv_edit, strings, str_pos, max, shift_amount);
            shift_amount += (edit->len - (edit->end - edit->start));
        }
        else{
            result = 1;
            state->len = edit->end - edit->start;
        }
    }
    
    state->i = i;
    state->shift_amount = shift_amount;
    
    return(result);
}

enum Buffer_Render_Flag{
    BRFlag_Special_Character = (1 << 0),
        BRFlag_Ghost_Character = (1 << 1)
};

typedef struct Buffer_Render_Item{
    i32 index;
    u16 glyphid;
    u16 flags;
    f32 x0, y0;
    f32 x1, y1;
} Buffer_Render_Item;

typedef struct Render_Item_Write{
    Buffer_Render_Item *item;
    f32 x, y;
    f32 *adv;
    f32 font_height;
    f32 x_min;
    f32 x_max;
} Render_Item_Write;

inline_4tech Render_Item_Write
write_render_item(Render_Item_Write write, i32 index, u16 glyphid, u16 flags){
    f32 ch_width = write.adv[(u8)glyphid];
    
    if (write.x <= write.x_max && write.x + ch_width >= write.x_min){
        write.item->index = index;
        write.item->glyphid = glyphid;
        write.item->flags = flags;
        write.item->x0 = write.x;
        write.item->y0 = write.y;
        write.item->x1 = write.x + ch_width;
        write.item->y1 = write.y + write.font_height;
        
        ++write.item;
    }
    
    write.x += ch_width;
    
    return(write);
}

// TODO(allen): Reduce the number of parameters.
struct Buffer_Render_Params{
    Buffer_Type *buffer;
    Buffer_Render_Item *items;
    i32 max;
    i32 *count;
    f32 port_x;
    f32 port_y;
    f32 clip_w;
    f32 scroll_x;
    f32 scroll_y;
    f32 width;
    f32 height;
    Full_Cursor start_cursor;
    i32 wrapped;
    f32 font_height;
    f32 *adv;
    b32 virtual_white;
    i32 wrap_slashes;
};

enum Wrap_Slash_Mode{
    BRWrapSlash_Hide,
    BRWrapSlash_Show_After_Line,
    BRWrapSlash_Show_At_Wrap_Edge,
};

struct Buffer_Render_State{
    Buffer_Stream_Type stream;
    b32 still_looping;
    i32 i;
    
    f32 ch_width;
    u8 ch;
    
    Render_Item_Write write;
    
    i32 line;
    i32 wrap_line;
    b32 skipping_whitespace;
    b32 first_of_the_line;
    i32 wrap_unit_end;
    
    i32 __pc__;
};

// duff-routine defines
#define DrCase(PC) case PC: goto resumespot_##PC
#define DrYield(PC, n) { *S_ptr = S; S_ptr->__pc__ = PC; return(n); resumespot_##PC:; }
#define DrReturn(n) { *S_ptr = S; S_ptr->__pc__ = -1; return(n); }

internal_4tech Buffer_Layout_Stop
buffer_render_data(Buffer_Render_State *S_ptr, Buffer_Render_Params params, f32 line_shift, b32 do_wrap, i32 wrap_unit_end){
    Buffer_Render_State S = *S_ptr;
    Buffer_Layout_Stop S_stop;
    
    i32 size = buffer_size(params.buffer);
    f32 shift_x = params.port_x - params.scroll_x;
    f32 shift_y = params.port_y - params.scroll_y;
    
    if (params.wrapped){
        shift_y += params.start_cursor.wrapped_y;
    }
    else{
        shift_y += params.start_cursor.unwrapped_y;
    }
    
    Buffer_Render_Item *item_end = params.items + params.max;
    
    switch (S.__pc__){
        DrCase(1);
        DrCase(2);
        DrCase(3);
        DrCase(4);
    }
    
    S.line = params.start_cursor.line - 1;
    S.wrap_line = params.start_cursor.wrap_line - 1;
    
    if (params.virtual_white){
        S_stop.status          = BLStatus_NeedLineShift;
        S_stop.line_index      = S.line;
        S_stop.wrap_line_index = S.wrap_line;
        DrYield(1, S_stop);
    }
    
    S.write.item        = params.items;
    S.write.x           = shift_x + line_shift;
    S.write.y           = shift_y;
    S.write.adv         = params.adv;
    S.write.font_height = params.font_height;
    S.write.x_min       = params.port_x;
    S.write.x_max       = params.port_x + params.clip_w;
    
    if (params.virtual_white){
        S.skipping_whitespace = 1;
    }
    
    S.first_of_the_line = 1;
    S.i = params.start_cursor.pos;
    if (buffer_stringify_loop(&S.stream, params.buffer, S.i, size)){
        do{
            for (; S.i < S.stream.end; ++S.i){
                S.ch = (u8)S.stream.data[S.i];
                S.ch_width = params.adv[S.ch];
                
                if (S.ch != '\n' && S.i >= S.wrap_unit_end){
                    S_stop.status          = BLStatus_NeedWrapDetermination;
                    S_stop.line_index      = S.line;
                    S_stop.wrap_line_index = S.wrap_line;
                    S_stop.pos             = S.i;
                    S_stop.x               = S.write.x - shift_x;
                    DrYield(4, S_stop);
                    
                    S.wrap_unit_end = wrap_unit_end;
                    
                    if (do_wrap && !S.first_of_the_line){
                        if (params.virtual_white){
                            S_stop.status          = BLStatus_NeedWrapLineShift;
                            S_stop.line_index      = S.line;
                            S_stop.wrap_line_index = S.wrap_line + 1;
                            DrYield(2, S_stop);
                        }
                        
                        ++S.wrap_line;
                        
                        if (params.wrapped){
                            switch (params.wrap_slashes){
                                case BRWrapSlash_Show_After_Line:
                                {
                            S.write = write_render_item(S.write, S.i-1, '\\', BRFlag_Ghost_Character);
                                }break;
                                
                                case BRWrapSlash_Show_At_Wrap_Edge:
                                {
                                    if (S.write.x < shift_x + params.width){
                                        S.write.x = shift_x + params.width;
                                    }
                                    S.write = write_render_item(S.write, S.i-1, '\\', BRFlag_Ghost_Character);
                                }break;
                            }
                            
                            S.write.x = shift_x + line_shift;
                            S.write.y += params.font_height;
                        }
                    }
                }
                
                if (S.write.y > params.height + params.port_y){
                    goto buffer_get_render_data_end;
                }
                
                if (S.ch != ' ' && S.ch != '\t'){
                    S.skipping_whitespace = 0;
                }
                
                if (!S.skipping_whitespace){
                    S.first_of_the_line = 0;
                    switch (S.ch){
                        case '\n':
                        if (S.write.item < item_end){
                            S.write = write_render_item(S.write, S.i, ' ', 0);
                            
                            if (params.virtual_white){
                                S_stop.status          = BLStatus_NeedLineShift;
                                S_stop.line_index      = S.line+1;
                                S_stop.wrap_line_index = S.wrap_line+1;
                                DrYield(3, S_stop);
                                
                                S.skipping_whitespace = 1;
                            }
                            
                            ++S.line;
                            ++S.wrap_line;
                            
                            S.write.x = shift_x + line_shift;
                            S.write.y += params.font_height;
                            
                            S.first_of_the_line = 1;
                        }
                        break;
                        
                        case '\r':
                        if (S.write.item < item_end){
                            S.write = write_render_item(S.write, S.i, '\\', BRFlag_Special_Character);
                            
                            if (S.write.item < item_end){
                                S.write = write_render_item(S.write, S.i, 'r', BRFlag_Special_Character);
                            }
                        }
                        break;
                        
                        case '\t':
                        if (S.write.item < item_end){
                            f32 new_x = S.write.x + S.ch_width;
                            S.write = write_render_item(S.write, S.i, ' ', 0);
                            S.write.x = new_x;
                        }
                        break;
                        
                        default:
                        if (S.write.item < item_end){
                            if (S.ch >= ' ' && S.ch <= '~'){
                                S.write = write_render_item(S.write, S.i, S.ch, 0);
                            }
                            else{
                                S.write = write_render_item(S.write, S.i, '\\', BRFlag_Special_Character);
                                
                                u8 ch = S.ch;
                                char C = '0' + (ch / 0x10);
                                if ((ch / 0x10) > 0x9){
                                    C = ('A' - 0xA) + (ch / 0x10);
                                }
                                
                                if (S.write.item < item_end){
                                    S.write = write_render_item(S.write, S.i, C, BRFlag_Special_Character);
                                }
                                
                                ch = (ch % 0x10);
                                C = '0' + ch;
                                if (ch > 0x9){
                                    C = ('A' - 0xA) + ch;
                                }
                                
                                if (S.write.item < item_end){
                                    S.write = write_render_item(S.write, S.i, C, BRFlag_Special_Character);
                                }
                            }
                        }
                        break;
                    }
                }
            }
            S.still_looping = buffer_stringify_next(&S.stream);
        }while(S.still_looping);
    }
    
    buffer_get_render_data_end:;
    if (S.write.y <= params.height + shift_y || S.write.item == params.items){
        if (S.write.item < item_end){
            S.write = write_render_item(S.write, size, ' ', 0);
        }
    }
    
    *params.count = (i32)(S.write.item - params.items);
    assert_4tech(*params.count <= params.max);
    
    S_stop.status = BLStatus_Finished;
    DrReturn(S_stop);
}

#undef DrYield
#undef DrReturn
#undef DrCase

// BOTTOM

