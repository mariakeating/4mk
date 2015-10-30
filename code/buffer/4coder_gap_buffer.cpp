/* 
 * Mr. 4th Dimention - Allen Webster
 *  Four Tech
 *
 * public domain -- no warranty is offered or implied; use this code at your own risk
 * 
 * 23.10.2015
 * 
 * Buffer data object
 *  type - Gap Buffer
 * 
 */

// TOP

typedef struct{
    char *data;
    int size1, gap_size, size2, max;
    
    float *line_widths;
    int *line_starts;
    int line_count;
    int line_max;
    int widths_max;
} Gap_Buffer;

inline_4tech int
buffer_size(Gap_Buffer *buffer){
    int size;
    size = buffer->size1 + buffer->size2;
    return(size);
}

inline_4tech void
buffer_initialize(Gap_Buffer *buffer, char *data, int size){
    int osize1, size1, size2;
    
    assert_4tech(buffer->max >= size);
    size2 = size >> 1;
    size1 = osize1 = size - size2;
    
    if (size1 > 0){
        size1 = eol_convert_in(buffer->data, data, size1);
        if (size2 > 0){
            size2 = eol_convert_in(buffer->data + size1, data + osize1, size2);
        }
    }
    
    buffer->size1 = size1;
    buffer->size2 = size2;
    buffer->gap_size = buffer->max - size1 - size2;
    memmove_4tech(buffer->data + size1 + buffer->gap_size, buffer->data + size1, size2);
}

internal_4tech void*
buffer_relocate(Gap_Buffer *buffer, char *new_data, int new_max){
    void *result;
    int new_gap_size;
    
    assert_4tech(new_max >= buffer_size(buffer));
    
    result = buffer->data;
    new_gap_size = new_max - buffer_size(buffer);
    memcpy_4tech(new_data, buffer->data, buffer->size1);
    memcpy_4tech(new_data + buffer->size1 + new_gap_size, buffer->data + buffer->size1 + buffer->gap_size, buffer->size2);
    
    buffer->data = new_data;
    buffer->gap_size = new_gap_size;
    buffer->max = new_max;
    
    return(result);
}

typedef struct{
    Gap_Buffer *buffer;
    char *data, *base;
    int absolute_pos;
    int pos, end;
    int size;
    int page_size;
    int separated;
} Gap_Buffer_Stringify_Loop;

inline_4tech Gap_Buffer_Stringify_Loop
buffer_stringify_loop(Gap_Buffer *buffer, int start, int end, int page_size){
    Gap_Buffer_Stringify_Loop result;
    if (0 <= start && start < end && end <= buffer->size1 + buffer->size2){
        result.buffer = buffer;
        result.base = buffer->data;
        result.page_size = page_size;
        result.absolute_pos = start;
        
        if (end <= buffer->size1) result.end = end;
        else result.end = end + buffer->gap_size;
        
        if (start < buffer->size1){
            if (end <= buffer->size1) result.separated = 0;
            else result.separated = 1;
            result.pos = start;
        }
        else{
            result.separated = 0;
            result.pos = start + buffer->gap_size;
        }
        if (result.separated) result.size = buffer->size1 - start;
        else result.size = end - start;
        if (result.size > page_size) result.size = page_size;
        result.data = buffer->data + result.pos;
    }
    else result.buffer = 0;
    return(result);
}

inline_4tech int
buffer_stringify_good(Gap_Buffer_Stringify_Loop *loop){
    int result;
    result = (loop->buffer != 0);
    return(result);
}

inline_4tech void
buffer_stringify_next(Gap_Buffer_Stringify_Loop *loop){
    int size1, temp_end;
    if (loop->separated){
        size1 = loop->buffer->size1;
        if (loop->pos + loop->size == size1){
            loop->separated = 0;
            loop->pos = loop->buffer->gap_size + size1;
            loop->absolute_pos = size1;
            temp_end = loop->end;
        }
        else{
            loop->pos += loop->page_size;
            loop->absolute_pos += loop->page_size;
            temp_end = size1;
        }
    }
    else{
        if (loop->pos + loop->size == loop->end){
            loop->buffer = 0;
            temp_end = loop->pos;
        }
        else{
            loop->pos += loop->page_size;
            loop->absolute_pos += loop->page_size;
            temp_end = loop->end;
        }
    }
    loop->size = temp_end - loop->pos;
    if (loop->size > loop->page_size) loop->size = loop->page_size;
    loop->data = loop->base + loop->pos;
}

typedef struct{
    Gap_Buffer *buffer;
    char *data, *base;
    int pos, end;
    int size;
    int absolute_pos;
    int page_size;
    int separated;
} Gap_Buffer_Backify_Loop;

inline_4tech Gap_Buffer_Backify_Loop
buffer_backify_loop(Gap_Buffer *buffer, int start, int end, int page_size){
    Gap_Buffer_Backify_Loop result;
    int chunk2_start;
    
    ++start;
    if (0 <= end && end < start && start <= buffer->size1 + buffer->size2){
        chunk2_start = buffer->size1 + buffer->gap_size;
        
        result.buffer = buffer;
        result.base = buffer->data;
        result.page_size = page_size;
        
        if (end < buffer->size1) result.end = end;
        else result.end = end + buffer->gap_size;
        
        if (start <= buffer->size1){
            result.separated = 0;
            result.pos = start - page_size;
        }
        else{
            if (end < buffer->size1) result.separated = 1;
            else result.separated = 0;
            result.pos = start - page_size + buffer->gap_size;
        }
        if (result.separated){
            if (result.pos < chunk2_start) result.pos = chunk2_start;
        }
        else{
            if (result.pos < result.end) result.pos = result.end;
        }
        result.size = start - result.pos;
        result.absolute_pos = result.pos;
        if (result.absolute_pos > buffer->size1) result.absolute_pos -= buffer->gap_size;
        result.data = result.base + result.pos;
    }
    else result.buffer = 0;
    return(result);
}

inline_4tech int
buffer_backify_good(Gap_Buffer_Backify_Loop *loop){
    int result;
    result = (loop->buffer != 0);
    return(result);
}

inline_4tech void
buffer_backify_next(Gap_Buffer_Backify_Loop *loop){
    Gap_Buffer *buffer;
    int temp_end;
    int chunk2_start;
    buffer = loop->buffer;
    chunk2_start = buffer->size1 + buffer->gap_size;
    if (loop->separated){
        if (loop->pos == chunk2_start){
            loop->separated = 0;
            temp_end = buffer->size1;
            loop->pos = temp_end - loop->page_size;
            loop->absolute_pos = loop->pos;
            if (loop->pos < loop->end){
                loop->absolute_pos += (loop->end - loop->pos);
                loop->pos = loop->end;
            }
        }
        else{
            temp_end = loop->pos;
            loop->pos -= loop->page_size;
            loop->absolute_pos -= loop->page_size;
            if (loop->pos < chunk2_start){
                loop->pos = chunk2_start;
                loop->absolute_pos = buffer->size1;
            }
        }
    }
    else{
        if (loop->pos == loop->end){
            temp_end = 0;
            loop->buffer = 0;
        }
        else{
            temp_end = loop->pos;
            loop->pos -= loop->page_size;
            loop->absolute_pos -= loop->page_size;
            if (loop->pos < loop->end){
                loop->absolute_pos += (loop->end - loop->pos);
                loop->pos = loop->end;
            }
        }
    }
    loop->size = temp_end - loop->pos;
    loop->data = loop->base + loop->pos;
}

internal_4tech int
buffer_replace_range(Gap_Buffer *buffer, int start, int end, char *str, int len, int *shift_amount){
    char *data;
    int result;
    int size;
    int move_size;
    
    size = buffer_size(buffer);
    assert_4tech(0 <= start);
    assert_4tech(start <= end);
    assert_4tech(end <= size);
    
    *shift_amount = (len - (end - start));
    if (*shift_amount + size <= buffer->max){
        data = buffer->data;
        if (end < buffer->size1){
            move_size = buffer->size1 - end;
            memmove_4tech(data + buffer->size1 + buffer->gap_size - move_size, data + end, move_size);
            buffer->size1 -= move_size;
            buffer->size2 += move_size;
        }
        if (start > buffer->size1){
            move_size = start - buffer->size1;
            memmove_4tech(data + buffer->size1, data + buffer->size1 + buffer->gap_size, move_size);
            buffer->size1 += move_size;
            buffer->size2 -= move_size;
        }
        
        memcpy_4tech(data + start, str, len);
        buffer->size2 = size - end;
        buffer->size1 = start + len;
        buffer->gap_size -= *shift_amount;
        
        assert_4tech(buffer->size1 + buffer->size2 == size + *shift_amount);
        assert_4tech(buffer->size1 + buffer->gap_size + buffer->size2 == buffer->max);
        
        result = 0;
    }
    else{
        result = *shift_amount + size;
    }

    return(result);
}

// NOTE(allen): This could should be optimized for Gap_Buffer
internal_4tech int
buffer_batch_edit_step(Buffer_Batch_State *state, Gap_Buffer *buffer, Buffer_Edit *sorted_edits, char *strings, int edit_count){
    Buffer_Edit *edit;
    int i, result;
    int shift_total, shift_amount;
    
    result = 0;
    shift_total = state->shift_total;
    i = state->i;
    
    edit = sorted_edits + i;
    for (; i < edit_count; ++i, ++edit){
        result = buffer_replace_range(buffer, edit->start + shift_total, edit->end + shift_total,
                                      strings + edit->str_start, edit->len, &shift_amount);
        if (result) break;
        shift_total += shift_amount;
    }
    
    state->shift_total = shift_total;
    state->i = i;
    
    return(result);
}

// BOTTOM

