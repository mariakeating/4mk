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
#define Buffer_Stringify_Type cat_4tech(Buffer_Type, _Stringify_Loop)

inline_4tech void
buffer_stringify(Buffer_Type *buffer, i32 start, i32 end, char *out){
    for (Buffer_Stringify_Type loop = buffer_stringify_loop(buffer, start, end);
         buffer_stringify_good(&loop);
         buffer_stringify_next(&loop)){
        memcpy_4tech(out, loop.data, loop.size);
        out += loop.size;
    }
}

internal_4tech i32
buffer_convert_out(Buffer_Type *buffer, char *dest, i32 max){
    Buffer_Stringify_Type loop;
    i32 size, out_size, pos, result;
    
    size = buffer_size(buffer);
    assert_4tech(size + buffer->line_count <= max);
    
    pos = 0;
    for (loop = buffer_stringify_loop(buffer, 0, size);
         buffer_stringify_good(&loop);
         buffer_stringify_next(&loop)){
        result = eol_convert_out(dest + pos, max - pos, loop.data, loop.size, &out_size);
        assert_4tech(result);
        pos += out_size;
    }
    
    return(pos);
}

internal_4tech i32
buffer_count_newlines(Buffer_Type *buffer, i32 start, i32 end){
    Buffer_Stringify_Type loop;
    i32 i;
    i32 count;
    
    assert_4tech(0 <= start);
    assert_4tech(start <= end);
    assert_4tech(end <= buffer_size(buffer));
    
    count = 0;
    
    for (loop = buffer_stringify_loop(buffer, start, end);
         buffer_stringify_good(&loop);
         buffer_stringify_next(&loop)){
        for (i = 0; i < loop.size; ++i){
            count += (loop.data[i] == '\n');
        }
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
    Buffer_Stringify_Type loop = {0};
    char *data = 0;
    i32 end = 0;
    i32 size = buffer_size(buffer);
    i32 start = state->start, i = state->i;
    i32 *start_ptr = buffer->line_starts + state->count;
    i32 *start_end = buffer->line_starts + buffer->line_max;
    i32 result = 1;
    char ch = 0;
    
    for (loop = buffer_stringify_loop(buffer, i, size);
         buffer_stringify_good(&loop);
         buffer_stringify_next(&loop)){
        end = loop.size + loop.absolute_pos;
        data = loop.data - loop.absolute_pos;
        for (; i < end; ++i){
            ch = data[i];
            if (ch == '\n'){
                if (start_ptr == start_end){
                    goto buffer_measure_starts_widths_end;
                }
                
                *start_ptr++ = start;
                start = i + 1;
            }
        }
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
    
    return(result);
}

internal_4tech void
buffer_measure_wrap_y(Buffer_Type *buffer, f32 *wraps, f32 font_height, f32 *adv, f32 max_width){
    Buffer_Stringify_Type loop = {0};
    char *data = 0;
    i32 end = 0;
    i32 i = 0;
    i32 size = buffer_size(buffer);
    
    i32 wrap_index = 0;
    f32 last_wrap = 0.f;
    f32 current_wrap = 0.f;
    
    f32 x = 0.f;
    
    for (loop = buffer_stringify_loop(buffer, i, size);
         buffer_stringify_good(&loop);
         buffer_stringify_next(&loop)){
        end = loop.size + loop.absolute_pos;
        data = loop.data - loop.absolute_pos;
        for (; i < end; ++i){
            u8 ch = (u8)data[i];
            if (ch == '\n'){
                wraps[wrap_index++] = last_wrap;
                current_wrap += font_height;
                last_wrap = current_wrap;
                x = 0.f;
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
    }
    
    wraps[wrap_index++] = last_wrap;
    wraps[wrap_index++] = current_wrap;
    
    assert_4tech(wrap_index-1 == buffer->line_count);
}

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
        memmove_4tech(starts + line_end + line_shift, starts + line_end,
                      sizeof(i32)*(line_count - line_end));
        
        new_line_count += line_shift;
        new_line_end += line_shift;
    }
    
    // Iteration data (yikes! Need better loop system)
    Buffer_Stringify_Type loop = {0};
    i32 end = 0;
    char *data = 0;
    i32 size = buffer_size(buffer);
    i32 char_i = starts[line_start];
    i32 line_i = line_start;
    
    // Line start measurement
    i32 start = char_i;
    
    for (loop = buffer_stringify_loop(buffer, char_i, size);
         buffer_stringify_good(&loop);
         buffer_stringify_next(&loop)){
        end = loop.size + loop.absolute_pos;
        data = loop.data - loop.absolute_pos;
        for (; char_i < end; ++char_i){
            u8 ch = (u8)data[char_i];
            
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
        memmove_4tech(wraps + line_end + line_shift, wraps + line_end,
                      sizeof(i32)*(line_count - line_end));
        
        line_count -= line_shift;
        new_line_end += line_shift;
    }
    
    // Iteration data (yikes! Need better loop system)
    Buffer_Stringify_Type loop = {0};
    i32 end = 0;
    char *data = 0;
    i32 size = buffer_size(buffer);
    i32 char_i = buffer->line_starts[line_start];
    i32 line_i = line_start;
    
    // Line wrap measurement
    f32 last_wrap = wraps[line_i];
    f32 current_wrap = last_wrap;
    f32 x = 0.f;
    
    for (loop = buffer_stringify_loop(buffer, char_i, size);
         buffer_stringify_good(&loop);
         buffer_stringify_next(&loop)){
        end = loop.size + loop.absolute_pos;
        data = loop.data - loop.absolute_pos;
        for (; char_i < end; ++char_i){
            u8 ch = (u8)data[char_i];
            
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
    }
    
    wraps[line_i++] = last_wrap;
    
    buffer_remeasure_wraps_end:;
    
    f32 y_shift = current_wrap - wraps[line_i];
    
    // Adjust
    if (y_shift != 0){
        wraps += line_i;
        for (; line_i <= new_line_count; ++line_i, ++wraps){
            *wraps += y_shift;
        }
    }
}

internal_4tech i32
buffer_get_line_index_range(Buffer_Type *buffer, i32 pos, i32 l_bound, i32 u_bound){
    i32 *lines = buffer->line_starts;
    i32 start = l_bound, end = u_bound;
    i32 i = 0;
    
    assert_4tech(0 <= l_bound);
    assert_4tech(l_bound <= u_bound);
    assert_4tech(u_bound <= buffer->line_count);
    
    assert_4tech(lines != 0);
    
    for (;;){
        i = (start + end) >> 1;
        if (lines[i] < pos){
            start = i;
        }
        else if (lines[i] > pos){
            end = i;
        }
        else{
            start = i;
            break;
        }
        assert_4tech(start < end);
        if (start == end - 1){
            break;
        }
    }
    
    return(start);
}

inline_4tech i32
buffer_get_line_index(Buffer_Type *buffer, i32 pos){
    i32 result = buffer_get_line_index_range(buffer, pos, 0, buffer->line_count);
    return(result);
}

internal_4tech i32
buffer_get_line_index_from_wrapped_y(f32 *wraps, f32 y, f32 font_height, i32 l_bound, i32 u_bound){
    i32 start, end, i, result;
    start = l_bound;
    end = u_bound;
    for (;;){
        i = (start + end) / 2;
        if (wraps[i]+font_height <= y) start = i;
        else if (wraps[i] > y) end = i;
        else{
            result = i;
            break;
        }
        if (start >= end - 1){
            result = start;
            break;
        }
    }
    return(result);
}

typedef struct Seek_State{
    Full_Cursor cursor;
    Full_Cursor prev_cursor;
} Seek_State;

internal_4tech i32
cursor_seek_step(Seek_State *state, Buffer_Seek seek, i32 xy_seek, f32 max_width,
                 f32 font_height, f32 *adv, i32 size, uint8_t ch){
    Full_Cursor cursor = state->cursor;
    Full_Cursor prev_cursor = cursor;
    i32 result = 1;
    f32 x, px, y;
    
    switch (ch){
        case '\n':
        ++cursor.line;
        cursor.unwrapped_y += font_height;
        cursor.wrapped_y += font_height;
        cursor.character = 1;
        cursor.unwrapped_x = 0;
        cursor.wrapped_x = 0;
        break;
        
        default:
        {
            f32 ch_width = adv[ch];
            
            if (cursor.wrapped_x + ch_width > max_width){
                cursor.wrapped_y += font_height;
                cursor.wrapped_x = 0;
                prev_cursor = cursor;
            }
            
            ++cursor.character;
            cursor.unwrapped_x += ch_width;
            cursor.wrapped_x += ch_width;
        }break;
    }
    
    ++cursor.pos;
    
    if (cursor.pos > size){
        cursor = prev_cursor;
        result = 0;
        goto cursor_seek_step_end;
    }
    
    x = y = px = 0;
    
    switch (seek.type){
        case buffer_seek_pos:
        if (cursor.pos > seek.pos){
            cursor = prev_cursor;
            result = 0;
            goto cursor_seek_step_end;
        }break;
        
        case buffer_seek_wrapped_xy:
        x = cursor.wrapped_x; px = prev_cursor.wrapped_x;
        y = cursor.wrapped_y; break;
        
        case buffer_seek_unwrapped_xy:
        x = cursor.unwrapped_x; px = prev_cursor.unwrapped_x;
        y = cursor.unwrapped_y; break;
        
        case buffer_seek_line_char:
        if (cursor.line == seek.line && cursor.character >= seek.character){
            result = 0;
            goto cursor_seek_step_end;
        }
        else if (cursor.line > seek.line){
            cursor = prev_cursor;
            result = 0;
            goto cursor_seek_step_end;
        }break;
    }
    
    if (xy_seek){
        if (y > seek.y){
            cursor = prev_cursor;
            result = 0;
            goto cursor_seek_step_end;
        }
        
        if (y > seek.y - font_height && x >= seek.x){
            if (!seek.round_down){
                if (ch != '\n' && (seek.x - px) < (x - seek.x)) cursor = prev_cursor;
                result = 0;
                goto cursor_seek_step_end;
            }
            
            if (x > seek.x){
                cursor = prev_cursor;
                result = 0;
                goto cursor_seek_step_end;
            }
        }
    }
    
    cursor_seek_step_end:;
    state->cursor = cursor;
    state->prev_cursor = prev_cursor;
    return(result);
}

internal_4tech Full_Cursor
buffer_cursor_seek(Buffer_Type *buffer, Buffer_Seek seek, f32 max_width,
                   f32 font_height, f32 *adv, Full_Cursor cursor){
    Buffer_Stringify_Type loop;
    char *data;
    i32 size, end;
    i32 i;
    i32 result;
    
    Seek_State state;
    i32 xy_seek;
    
    state.cursor = cursor;
    
    switch(seek.type){
        case buffer_seek_pos:
        if (cursor.pos >= seek.pos) goto buffer_cursor_seek_end;
        break;
        
        case buffer_seek_wrapped_xy:
        if (seek.x == 0 && cursor.wrapped_y >= seek.y) goto buffer_cursor_seek_end;
        break;
        
        case buffer_seek_unwrapped_xy:
        if (seek.x == 0 && cursor.unwrapped_y >= seek.y) goto buffer_cursor_seek_end;
        break;
        
        case buffer_seek_line_char:
        if (cursor.line >= seek.line && cursor.character >= seek.character) goto buffer_cursor_seek_end;
        break;
    }
    
    if (adv){
        size = buffer_size(buffer);
        xy_seek = (seek.type == buffer_seek_wrapped_xy || seek.type == buffer_seek_unwrapped_xy);
        
        result = 1;
        i = cursor.pos;
        for (loop = buffer_stringify_loop(buffer, i, size);
             buffer_stringify_good(&loop);
             buffer_stringify_next(&loop)){
            end = loop.size + loop.absolute_pos;
            data = loop.data - loop.absolute_pos;
            for (; i < end; ++i){
                result = cursor_seek_step(&state, seek, xy_seek, max_width,
                                          font_height, adv, size, data[i]);
                if (!result) goto buffer_cursor_seek_end;
            }
        }
        if (result){
            result = cursor_seek_step(&state, seek, xy_seek, max_width,
                                      font_height, adv, size, 0);
            assert_4tech(result == 0);
        }
    }
    
    buffer_cursor_seek_end:    
    return(state.cursor);
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

internal_4tech Full_Cursor
buffer_cursor_from_pos(Buffer_Type *buffer, f32 *wraps, i32 pos,
                       f32 max_width, f32 font_height, f32 *adv){
    Full_Cursor result = {0};
    i32 line_index = 0;
    i32 size = buffer_size(buffer);
    
    if (pos > size){
        pos = size;
    }
    if (pos < 0){
        pos = 0;
    }
    
    line_index = buffer_get_line_index_range(buffer, pos, 0, buffer->line_count);
    result = make_cursor_hint(line_index, buffer->line_starts, wraps, font_height);
    result = buffer_cursor_seek(buffer, seek_pos(pos), max_width, font_height, adv, result);
    
    return(result);
}

internal_4tech Partial_Cursor
buffer_partial_from_line_character(Buffer_Type *buffer, i32 line, i32 character){
    Partial_Cursor result = {0};
    
    i32 line_index = line - 1;
    if (line_index >= buffer->line_count) line_index = buffer->line_count - 1;
    if (line_index < 0) line_index = 0;
    
    int32_t size = buffer_size(buffer);
    i32 this_start = buffer->line_starts[line_index];
    i32 max_character = (size-this_start) + 1;
    if (line_index+1 < buffer->line_count){
        i32 next_start = buffer->line_starts[line_index+1];
        max_character = (next_start-this_start);
    }
    
    if (character <= 0) character = 1;
    if (character > max_character) character = max_character;
    
    result.pos = this_start + character - 1;
    result.line = line_index+1;
    result.character = character;
    
    return(result);
}

internal_4tech Full_Cursor
buffer_cursor_from_line_character(Buffer_Type *buffer, f32 *wraps, i32 line, i32 character,
                                  f32 max_width, f32 font_height, f32 *adv){
    Full_Cursor result = {0};
    i32 line_index = line - 1;
    
    if (line_index >= buffer->line_count) line_index = buffer->line_count - 1;
    if (line_index < 0) line_index = 0;
    
    result = make_cursor_hint(line_index, buffer->line_starts, wraps, font_height);
    result = buffer_cursor_seek(buffer, seek_line_char(line, character),
                                max_width, font_height, adv, result);
    
    return(result);
}

internal_4tech Full_Cursor
buffer_cursor_from_unwrapped_xy(Buffer_Type *buffer, f32 *wraps, f32 x, f32 y, i32 round_down,
                                f32 max_width, f32 font_height, f32 *adv){
    Full_Cursor result = {0};
    i32 line_index = (i32)(y / font_height);
    
    if (line_index >= buffer->line_count) line_index = buffer->line_count - 1;
    if (line_index < 0) line_index = 0;
    
    result = make_cursor_hint(line_index, buffer->line_starts, wraps, font_height);
    result = buffer_cursor_seek(buffer, seek_unwrapped_xy(x, y, round_down),
                                max_width, font_height, adv, result);
    
    return(result);
}

internal_4tech Full_Cursor
buffer_cursor_from_wrapped_xy(Buffer_Type *buffer, f32 *wraps, f32 x, f32 y, i32 round_down,
                              f32 max_width, f32 font_height, f32 *adv){
    Full_Cursor result = {0};
    i32 line_index = buffer_get_line_index_from_wrapped_y(wraps, y, font_height, 0, buffer->line_count);
    
    result = make_cursor_hint(line_index, buffer->line_starts, wraps, font_height);
    result = buffer_cursor_seek(buffer, seek_wrapped_xy(x, y, round_down),
                                max_width, font_height, adv, result);
    
    return(result);
}

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

internal_4tech Full_Cursor
buffer_get_start_cursor(Buffer_Type *buffer, f32 *wraps, f32 scroll_y,
                        i32 wrapped, f32 width, f32 *adv, f32 font_height){
    Full_Cursor result;
    
    if (wrapped){
        result = buffer_cursor_from_wrapped_xy(buffer, wraps, 0, scroll_y, 0,
                                               width, font_height, adv);
    }
    else{
        result = buffer_cursor_from_unwrapped_xy(buffer, wraps, 0, scroll_y, 0,
                                                 width, font_height, adv);
    }
    
    return(result);
}

enum Buffer_Render_Flag{
    BRFlag_Special_Character = (1 << 0)
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
    f32 ch_width = measure_character(write.adv, (char)glyphid);
    
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
};

struct Buffer_Render_State{
    Buffer_Stringify_Type loop;
    char *data;
    i32 end;
    i32 size;
    i32 i;
    
    f32 ch_width;
    u8 ch;
    
    Render_Item_Write write;
    
    i32 line;
    b32 skipping_whitespace;
    
    i32 __pc__;
};

// duff-routine defines
#define DrCase(PC) case PC: goto resumespot_##PC
#define DrYield(PC, n) { *S_ptr = S; S_ptr->__pc__ = PC; return(n); resumespot_##PC:; }
#define DrReturn(n) { *S_ptr = S; S_ptr->__pc__ = -1; return(n); }

enum{
    RenderStatus_Finished,
    RenderStatus_NeedLineShift
};

struct Buffer_Render_Stop{
    u32 status;
    i32 line_index;
    i32 pos;
};

internal_4tech Buffer_Render_Stop
buffer_render_data(Buffer_Render_State *S_ptr, Buffer_Render_Params params, f32 line_shift){
    Buffer_Render_State S = *S_ptr;
    Buffer_Render_Stop S_stop;
    
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
    }
    
    S.line = params.start_cursor.line - 1;
    
    if (params.virtual_white){
        S_stop.status     = RenderStatus_NeedLineShift;
        S_stop.line_index = S.line;
        S_stop.pos        = params.start_cursor.pos;
        DrYield(1, S_stop);
    }
    
    S.write.item        = params.items;
    S.write.x           = shift_x + line_shift;
    S.write.y           = shift_y;
    S.write.adv         = params.adv;
    S.write.font_height = params.font_height;
    S.write.x_min       = params.port_x;
    S.write.x_max       = params.port_x + params.clip_w;
    
    if (params.adv){
        for (S.loop = buffer_stringify_loop(params.buffer, params.start_cursor.pos, size);
             buffer_stringify_good(&S.loop) && S.write.item < item_end;
             buffer_stringify_next(&S.loop)){
            
            S.end = S.loop.size + S.loop.absolute_pos;
            S.data = S.loop.data - S.loop.absolute_pos;
            
            for (S.i = S.loop.absolute_pos; S.i < S.end; ++S.i){
                S.ch = (uint8_t)S.data[S.i];
                S.ch_width = measure_character(params.adv, S.ch);
                
                if (S.ch_width + S.write.x > params.width + shift_x && S.ch != '\n' && params.wrapped){
                    if (params.virtual_white){
                        S_stop.status     = RenderStatus_NeedLineShift;
                        S_stop.line_index = S.line;
                        S_stop.pos        = S.i+1;
                        DrYield(2, S_stop);
                    }
                    
                    S.write.x = shift_x + line_shift;
                    S.write.y += params.font_height;
                }
                
                if (S.write.y > params.height + shift_y){
                    goto buffer_get_render_data_end;
                }
                
                if (S.ch != ' ' && S.ch != '\t'){
                    S.skipping_whitespace = 0;
                }
                
                if (!S.skipping_whitespace){
                    switch (S.ch){
                        case '\n':
                        if (S.write.item < item_end){
                            S.write = write_render_item(S.write, S.i, ' ', 0);
                            
                            if (params.virtual_white){
                                S_stop.status     = RenderStatus_NeedLineShift;
                                S_stop.line_index = S.line+1;
                                S_stop.pos        = S.i+1;
                                DrYield(3, S_stop);
                                
                                S.skipping_whitespace = 1;
                            }
                            
                            ++S.line;
                            
                            S.write.x = shift_x + line_shift;
                            S.write.y += params.font_height;
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
                                
                                char ch = S.ch;
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
                
                if (S.write.y > params.height + shift_y){
                    goto buffer_get_render_data_end;
                }
            }
        }
        
        buffer_get_render_data_end:;
        if (S.write.y <= params.height + shift_y || S.write.item == params.items){
            if (S.write.item < item_end){
                S.write = write_render_item(S.write, size, ' ', 0);
            }
        }
    }
    else{
        f32 zero = 0;
        S.write.adv = &zero;
        
        if (S.write.item < item_end){
            S.write = write_render_item(S.write, size, 0, 0);
        }
    }
    
    *params.count = (i32)(S.write.item - params.items);
    assert_4tech(*params.count <= params.max);
    
    S_stop.status = RenderStatus_Finished;
    DrReturn(S_stop);
}

#undef DrYield
#undef DrReturn
#undef DrCase

// BOTTOM

