/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 20.02.2016
 *
 * GUI system for 4coder
 *
 */

// TOP

struct Query_Slot{
    Query_Slot *next;
    Query_Bar *query_bar;
};

struct Query_Set{
    Query_Slot slots[8];
    Query_Slot *free_slot;
    Query_Slot *used_slot;
};

internal void
init_query_set(Query_Set *set){
    Query_Slot *slot = set->slots;
    int i;
    
    set->free_slot = slot;
    set->used_slot = 0;
    for (i = 0; i+1 < ArrayCount(set->slots); ++i, ++slot){
        slot->next = slot + 1;
    }
}

internal Query_Slot*
alloc_query_slot(Query_Set *set){
    Query_Slot *slot = set->free_slot;
    if (slot != 0){
        set->free_slot = slot->next;
        slot->next = set->used_slot;
        set->used_slot = slot;
    }
    return(slot);
}

internal void
free_query_slot(Query_Set *set, Query_Bar *match_bar){
    Query_Slot *slot = 0, *prev = 0;
    
    for (slot = set->used_slot; slot != 0; slot = slot->next){
        if (slot->query_bar == match_bar) break;
        prev = slot;
    }

    if (slot){
        if (prev){
            prev->next = slot->next;
        }
        else{
            set->used_slot = slot->next;
        }
        slot->next = set->free_slot;
        set->free_slot = slot;
    }
}

struct Super_Color{
    Vec4 hsla;
    Vec4 rgba;
    u32 *out;
};

struct GUI_Target{
    Partition push;
    b32 show_file;
};

struct GUI_Header{
    i32 type;
    i32 size;
};

enum GUI_Command_Type{
    guicom_null,
    guicom_begin_overlap,
    guicom_end_overlap,
    guicom_begin_serial,
    guicom_end_serial,
    guicom_top_bar,
    guicom_file,
    guicom_text_field
};

internal void*
gui_push_command(GUI_Target *target, void *item, i32 size){
    void *dest = partition_allocate(&target->push, size);
    if (dest){
        memcpy(dest, item, size);
    }
    return(dest);
}

internal void*
gui_align(GUI_Target *target){
    void *ptr;
    partition_align(&target->push, 8);
    ptr = partition_current(&target->push);
    return(ptr);
}

internal GUI_Header*
gui_push_simple_command(GUI_Target *target, i32 type){
    GUI_Header *result = 0;
    GUI_Header item;
    item.type = type;
    item.size = sizeof(item);
    result = (GUI_Header*)gui_push_command(target, &item, item.size);
    return(result);
}

internal void
gui_push_string(GUI_Target *target, GUI_Header *h, String s){
    u8 *start, *end;
    i32 size;
    start = (u8*)gui_push_command(target, &s.size, sizeof(s.size));
    gui_push_command(target, s.str, s.size);
    end = (u8*)gui_align(target);
    size = (i32)(end - start);
    h->size += size;
}

internal void
gui_begin_top_level(GUI_Target *target){
    target->show_file = 0;
    target->push.pos = 0;
}

internal void
gui_end_top_level(GUI_Target *target){
    gui_push_simple_command(target, guicom_null);
}

internal void
gui_begin_overlap(GUI_Target *target){
    gui_push_simple_command(target, guicom_begin_overlap);
}

internal void
gui_end_overlap(GUI_Target *target){
    gui_push_simple_command(target, guicom_end_overlap);
}

internal void
gui_begin_serial_section(GUI_Target *target){
    gui_push_simple_command(target, guicom_begin_serial);
}

internal void
gui_end_serial_section(GUI_Target *target){
    gui_push_simple_command(target, guicom_end_serial);
}

internal void
gui_do_top_bar(GUI_Target *target){
    gui_push_simple_command(target, guicom_top_bar);
}

internal void
gui_do_file(GUI_Target *target){
    gui_push_simple_command(target, guicom_file);
    target->show_file = 1;
}

internal void
gui_do_text_field(GUI_Target *target, String p, String t){
    GUI_Header *h = gui_push_simple_command(target, guicom_text_field);
    gui_push_string(target, h, p);
    gui_push_string(target, h, t);
}


struct GUI_Section{
    b32 overlapped;
    i32 v;
    i32 max_v;
};

struct GUI_Session{
    i32_Rect full_rect;
    i32_Rect rect;
    i32_Rect clip_rect;
    
    i32 line_height;
    
    GUI_Section sections[64];
    i32 t;
};

internal void
gui_session_init(GUI_Session *session, i32_Rect full_rect, i32 line_height){
    GUI_Section *section;
    
    *session = {0};
    session->full_rect = full_rect;
    session->line_height = line_height;
    
    section = &session->sections[0];
    section->v = full_rect.y0;
    section->max_v = full_rect.y0;
}

internal void
gui_section_end_item(GUI_Section *section, i32 v){
    if (!section->overlapped){
        section->v = v;
	}
    section->max_v = v;
}

internal b32
gui_interpret(GUI_Session *session, GUI_Header *h){
    GUI_Section *section = 0;
    GUI_Section *new_section = 0;
    GUI_Section *prev_section = 0;
    GUI_Section *end_section = 0;
    b32 give_to_user = 0;
    i32_Rect rect = {0};
    i32 y = 0;
    i32 end_v = -1;
    
    Assert(session->t < ArrayCount(session->sections));
    section = session->sections + session->t;
    y = section->v;
    
    if (y < session->full_rect.y1){
        switch (h->type){
            case guicom_null: Assert(0); break;
            
            case guicom_begin_overlap:
            ++session->t;
            Assert(session->t < ArrayCount(session->sections));
            new_section = &session->sections[session->t];
            new_section->overlapped = 1;
            new_section->v = y;
            break;
            
            case guicom_end_overlap:
            Assert(session->t > 0);
            Assert(section->overlapped);
            prev_section = &session->sections[--session->t];
            end_v = section->max_v;
            end_section = prev_section;
            break;
            
            case guicom_begin_serial:
            ++session->t;
            Assert(session->t < ArrayCount(session->sections));
            new_section = &session->sections[session->t];
            new_section->overlapped = 0;
            new_section->v = y;
            break;
            
            case guicom_end_serial:
            Assert(session->t > 0);
            Assert(!section->overlapped);
            prev_section = &session->sections[--session->t];
            end_v = section->max_v;
            end_section = prev_section;
            break;
            
            case guicom_top_bar:
            give_to_user = 1;
            rect.y0 = y;
            rect.y1 = rect.y0 + session->line_height + 2;
            rect.x0 = session->full_rect.x0;
            rect.x1 = session->full_rect.x1;
            end_v = rect.y1;
            end_section = section;
            break;

            case guicom_file:
            give_to_user = 1;
            rect.y0 = y;
            rect.y1 = session->full_rect.y1;
            rect.x0 = session->full_rect.x0;
            rect.x1 = session->full_rect.x1;
            end_v = rect.y1;
            end_section = section;
            break;
            
            case guicom_text_field:
            give_to_user = 1;
            rect.y0 = y;
            rect.y1 = rect.y0 + session->line_height + 2;
            rect.x0 = session->full_rect.x0;
            rect.x1 = session->full_rect.x1;
            end_v = rect.y1;
            end_section = section;
            break;
        }
        
        if (give_to_user){
            GUI_Section *section = session->sections;
            i32 max_v = 0;
            i32 i = 0;
            
            for (i = 0; i <= session->t; ++i, ++section){
                if (section->overlapped){
                    max_v = Max(max_v, section->max_v);
				}
			}
            
            session->rect = rect;
            
            if (rect.y0 < max_v){
                rect.y0 = max_v;
			}
            
            session->clip_rect = rect;
        }
        
        if (end_section){
            gui_section_end_item(end_section, end_v);
		}
    }

    return(give_to_user);
}

#define NextHeader(h) ((GUI_Header*)((char*)(h) + (h)->size))

internal String
gui_read_string(void **ptr){
    String result;
    char *start, *end;
    i32 size;
    
    start = (char*)*ptr;
    result.size = *(i32*)*ptr;
    *ptr = ((i32*)*ptr) + 1;
    result.str = (char*)*ptr;
    end = result.str + result.size;
    
    size = (i32)(end - start);
    size = (size + 7) & (~7);
    
    *ptr = ((char*)start) + size;
    
    return(result);
}


// BOTTOM

