/*
 * Helpers for ui data structures.
 */

// TOP

// TODO(allen): documentation comment here

////////////////////////////////

typedef u32 View_Get_UI_Flags;
enum{
    ViewGetUIFlag_KeepDataAsIs = 0,
    ViewGetUIFlag_ClearData = 1,
};

static b32
view_get_ui_data(Application_Links *app, View_ID view_id, View_Get_UI_Flags flags, UI_Data **ui_data_out, Arena **ui_arena_out){
    b32 result = false;
    Managed_Scope scope = 0;
    if (view_get_managed_scope(app, view_id, &scope)){
        Managed_Object ui_data_object = 0;
        if (managed_variable_get(app, scope, view_ui_data, &ui_data_object)){
            if (ui_data_object == 0){
                Managed_Object new_ui_data_object = alloc_managed_memory_in_scope(app, scope, sizeof(UI_Storage), 1);
                Managed_Object arena_object = alloc_managed_arena_in_scope(app, scope, (8 << 10));
                Arena *arena = 0;
                if (managed_object_load_data(app, arena_object, 0, 1, &arena)){
                    UI_Data *ui_data = push_array(arena, UI_Data, 1);
                    UI_Storage storage = {};
                    storage.data = ui_data;
                    storage.arena = arena;
                    storage.arena_object = arena_object;
                    storage.temp = begin_temp_memory(arena);
                    if (managed_object_store_data(app, new_ui_data_object, 0, 1, &storage)){
                        if (managed_variable_set(app, scope, view_ui_data, new_ui_data_object)){
                            ui_data_object = new_ui_data_object;
                        }
                    }
                }
            }
            if (ui_data_object != 0){
                UI_Storage storage = {};
                if (managed_object_load_data(app, ui_data_object, 0, 1, &storage)){
                    *ui_data_out = storage.data;
                    *ui_arena_out = storage.arena;
                    if ((flags & ViewGetUIFlag_ClearData) != 0){
                        end_temp_memory(storage.temp);
                    }
                    result = true;
                }
            }
        }
    }
    return(result);
}

static b32
view_clear_ui_data(Application_Links *app, View_ID view_id){
    b32 result = false;
    Managed_Scope scope = 0;
    if (view_get_managed_scope(app, view_id, &scope)){
        Managed_Object ui_data_object = 0;
        if (managed_variable_get(app, scope, view_ui_data, &ui_data_object)){
            if (ui_data_object != 0){
                UI_Storage storage = {};
                if (managed_object_load_data(app, ui_data_object, 0, 1, &storage)){
                    managed_object_free(app, storage.arena_object);
                    managed_object_free(app, ui_data_object);
                    managed_variable_set(app, scope, view_ui_data, 0);
                    result = true;
                }
            }
        }
    }
    return(result);
}

////////////////////////////////

static UI_Item*
ui_list_add_item(Arena *arena, UI_List *list, UI_Item item){
    UI_Item *node = push_array(arena, UI_Item, 1);
    memcpy(node, &item, sizeof(item));
    zdll_push_back(list->first, list->last, node);
    list->count += 1;
    return(node);
}

static i32_Rect
ui__rect_union(i32_Rect a, i32_Rect b){
    if (b.x1 > b.x0 && b.y1 > b.y0){
        if (a.x0 > b.x0){
            a.x0 = b.x0;
        }
        if (a.x1 < b.x1){
            a.x1 = b.x1;
        }
        if (a.y0 > b.y0){
            a.y0 = b.y0;
        }
        if (a.y1 < b.y1){
            a.y1 = b.y1;
        }
    }
    return(a);
}

static void
ui_data_compute_bounding_boxes(UI_Data *ui_data){
    i32_Rect neg_inf_rect = {};
    neg_inf_rect.x0 = INT32_MAX;
    neg_inf_rect.y0 = INT32_MAX;
    neg_inf_rect.x1 = INT32_MIN;
    neg_inf_rect.y1 = INT32_MIN;
    for (u32 i = 0; i < UICoordinates_COUNT; ++i){
        ui_data->bounding_box[i] = neg_inf_rect;
    }
    for (UI_Item *item = ui_data->list.first;
         item != 0;
         item = item->next){
        if (item->coordinates >= UICoordinates_COUNT){
            item->coordinates = UICoordinates_ViewSpace;
        }
        Rect_i32 *box = &ui_data->bounding_box[item->coordinates];
        *box = ui__rect_union(*box, item->rect_outer);
    }
}

static void
ui_control_set_top(UI_Data *data, i32 top_y){
    data->bounding_box[UICoordinates_ViewSpace].y0 = top_y;
}

static void
ui_control_set_bottom(UI_Data *data, i32 bottom_y){
    data->bounding_box[UICoordinates_ViewSpace].y1 = bottom_y;
}

static UI_Item*
ui_control_get_mouse_hit(UI_Data *data, Vec2_i32 view_p, Vec2_i32 panel_p){
    UI_Item *result = 0;
    for (UI_Item *item = data->list.first;
         item != 0 && result == 0;
         item = item->next){
        i32_Rect r = item->rect_outer;
        switch (item->coordinates){
            case UICoordinates_ViewSpace:
            {
                if (hit_check(r, view_p)){
                    result = item;
                }
            }break;
            case UICoordinates_PanelSpace:
            {
                if (hit_check(r, panel_p)){
                    result = item;
                }
            }break;
        }
    }
    return(result);
}

static UI_Item*
ui_control_get_mouse_hit(UI_Data *data, i32 mx_scrolled, i32 my_scrolled, i32 mx_unscrolled, i32 my_unscrolled){
    return(ui_control_get_mouse_hit(data, V2i32(mx_scrolled, my_scrolled), V2i32(mx_unscrolled, my_unscrolled)));
}

////////////////////////////////

static void
view_zero_scroll(Application_Links *app, View_Summary *view){
    GUI_Scroll_Vars zero_scroll = {};
    view_set_scroll(app, view, zero_scroll);
}

static void
view_set_vertical_focus(Application_Links *app, View_Summary *view, i32 y_top, i32 y_bot){
    Rect_i32 buffer_region = {};
    view_get_buffer_region(app, view->view_id, &buffer_region);
    GUI_Scroll_Vars scroll = view->scroll_vars;
    i32 view_y_top = scroll.target_y;
    i32 view_y_dim = rect_height(buffer_region);
    i32 view_y_bot = view_y_top + view_y_dim;
    i32 line_dim = (i32)view->line_height;
    i32 hot_y_top = view_y_top + line_dim*3;
    i32 hot_y_bot = view_y_bot - line_dim*3;
    if (hot_y_bot - hot_y_top < line_dim*6){
        i32 quarter_view_y_dim = view_y_dim/4;
        hot_y_top = view_y_top + quarter_view_y_dim;
        hot_y_bot = view_y_bot - quarter_view_y_dim;
    }
    i32 hot_y_dim = hot_y_bot - hot_y_top;
    i32 skirt_dim = hot_y_top - view_y_top;
    i32 y_dim = y_bot - y_top;
    if (y_dim > hot_y_dim){
        scroll.target_y = y_top - skirt_dim;
        view_set_scroll(app, view, scroll);
    }
    else{
        if (y_top < hot_y_top){
            scroll.target_y = y_top - skirt_dim;
            view_set_scroll(app, view, scroll);
        }
        else if (y_bot > hot_y_bot){
            scroll.target_y = y_bot + skirt_dim - view_y_dim;
            view_set_scroll(app, view, scroll);
        }
    }
}

static Vec2
view_space_from_screen_space(Vec2 p, Vec2 file_region_p0, Vec2 scroll_p){
    return(p - file_region_p0 + scroll_p);
}

static Vec2_i32
view_space_from_screen_space(Vec2_i32 p, Vec2_i32 file_region_p0, Vec2_i32 scroll_p){
    return(p - file_region_p0 + scroll_p);
}

static Vec2_i32
get_mouse_position_in_view_space(Mouse_State mouse, Vec2_i32 file_region_p0, Vec2_i32 scroll_p){
    return(view_space_from_screen_space(mouse.p, file_region_p0, scroll_p));
}

static Vec2_i32
get_mouse_position_in_view_space(Application_Links *app, Vec2_i32 file_region_p0, Vec2_i32 scroll_p){
    return(get_mouse_position_in_view_space(get_mouse_state(app), file_region_p0, scroll_p));
}

static Vec2
panel_space_from_screen_space(Vec2 p, Vec2 file_region_p0){
    return(p - file_region_p0);
}

static Vec2_i32
panel_space_from_screen_space(Vec2_i32 p, Vec2_i32 file_region_p0){
    return(p - file_region_p0);
}

static Vec2_i32
get_mouse_position_in_panel_space(Mouse_State mouse, Vec2_i32 file_region_p0){
    return(panel_space_from_screen_space(mouse.p, file_region_p0));
}

static Vec2_i32
get_mouse_position_in_panel_space(Application_Links *app, Vec2_i32 file_region_p0){
    return(get_mouse_position_in_panel_space(get_mouse_state(app), file_region_p0));
}

////////////////////////////////

Lister_State global_lister_state_[16] = {};
Lister_State *global_lister_state = global_lister_state_ - 1;

static Lister_State*
view_get_lister_state(View_Summary *view){
    return(&global_lister_state[view->view_id]);
}

static i32
lister_standard_arena_size_round_up(i32 arena_size){
    if (arena_size < (64 << 10)){
        arena_size = (64 << 10);
    }
    else{
        arena_size += (4 << 10) - 1;
        arena_size -= arena_size%(4 << 10);
    }
    return(arena_size);
}

static void
init_lister_state(Application_Links *app, Lister_State *state, Heap *heap){
    state->initialized = true;
    state->hot_user_data = 0;
    state->item_index = 0;
    state->set_view_vertical_focus_to_item = false;
    state->item_count_after_filter = 0;
    arena_release_all(&state->lister.arena);
    memset(&state->lister, 0, sizeof(state->lister));
}

UI_QUIT_FUNCTION(lister_quit_function){
    Lister_State *state = view_get_lister_state(&view);
    state->initialized = false;
    view_clear_ui_data(app, view.view_id);
}

static UI_Item
lister_get_clicked_item(Application_Links *app, View_ID view_id, Partition *scratch){
    UI_Item result = {};
    Temp_Memory temp = begin_temp_memory(scratch);
    UI_Data *ui_data = 0;
    Arena *ui_arena = 0;
    if (view_get_ui_data(app, view_id, ViewGetUIFlag_KeepDataAsIs, &ui_data, &ui_arena)){
        View_Summary view = {};
        get_view_summary(app, view_id, AccessAll, &view);
        Mouse_State mouse = get_mouse_state(app);
        Rect_i32 buffer_region = {};
        view_get_buffer_region(app, view_id, &buffer_region);
        Vec2_i32 region_p0 = buffer_region.p0;
        Vec2_i32 m_view_space = get_mouse_position_in_view_space(mouse, region_p0, V2i32(view.scroll_vars.scroll_p));
        Vec2_i32 m_panel_space = get_mouse_position_in_panel_space(mouse, region_p0);
        UI_Item *clicked = ui_control_get_mouse_hit(ui_data, m_view_space, m_panel_space);
        if (clicked != 0){
            result = *clicked;
        }
    }
    end_temp_memory(temp);
    return(result);
}

static i32
lister_get_line_height(View_Summary *view){
    return((i32)view->line_height);
}

static i32
lister_get_text_field_height(View_Summary *view){
    return((i32)view->line_height);
}

static i32
lister_get_block_height(i32 line_height, b32 is_theme_list){
    i32 block_height = 0;
    if (is_theme_list){
        block_height = line_height*3 + 6;
    }
    else{
        block_height = line_height*2;
    }
    return(block_height);
}

static void
lister_update_ui(Application_Links *app, Partition *scratch, View_Summary *view, Lister_State *state){
    b32 is_theme_list = state->lister.data.theme_list;
    
    i32 x0 = 0;
    i32 x1 = rect_width(view->view_region);
    i32 line_height = lister_get_line_height(view);
    i32 block_height = lister_get_block_height(line_height, is_theme_list);
    i32 text_field_height = lister_get_text_field_height(view);
    
    Temp_Memory full_temp = begin_temp_memory(scratch);
    
    Rect_i32 buffer_region = {};
    view_get_buffer_region(app, view->view_id, &buffer_region);
    Vec2_i32 view_m = get_mouse_position_in_view_space(app, buffer_region.p0, V2i32(view->scroll_vars.scroll_p));
    
    refresh_view(app, view);
    i32 y_pos = text_field_height;
    
    state->raw_item_index = -1;
    
    i32 node_count = state->lister.data.options.count;
    Lister_Node_Ptr_Array exact_matches = {};
    exact_matches.node_ptrs = push_array(scratch, Lister_Node*, 1);
    Lister_Node_Ptr_Array before_extension_matches = {};
    before_extension_matches.node_ptrs = push_array(scratch, Lister_Node*, node_count);
    Lister_Node_Ptr_Array substring_matches = {};
    substring_matches.node_ptrs = push_array(scratch, Lister_Node*, node_count);
    
    String key = state->lister.data.key_string;
    Absolutes absolutes = {};
    get_absolutes(key, &absolutes, true, true);
    b32 has_wildcard = (absolutes.count > 3);
    
    for (Lister_Node *node = state->lister.data.options.first;
         node != 0;
         node = node->next){
        if (key.size == 0 ||
            wildcard_match_s(&absolutes, node->string, false)){
            if (match_insensitive(node->string, key) && exact_matches.count == 0){
                exact_matches.node_ptrs[exact_matches.count++] = node;
            }
            else if (!has_wildcard &&
                     match_part_insensitive(node->string, key) &&
                     node->string.size > key.size &&
                     node->string.str[key.size] == '.'){
                before_extension_matches.node_ptrs[before_extension_matches.count++] = node;
            }
            else{
                substring_matches.node_ptrs[substring_matches.count++] = node;
            }
        }
    }
    
    Lister_Node_Ptr_Array node_ptr_arrays[] = {
        exact_matches,
        before_extension_matches,
        substring_matches,
    };
    
    UI_Data *ui_data = 0;
    Arena *ui_arena = 0;
    if (view_get_ui_data(app, view->view_id, ViewGetUIFlag_ClearData, &ui_data, &ui_arena)){
        memset(ui_data, 0, sizeof(*ui_data));
        
        UI_Item *highlighted_item = 0;
        UI_Item *hot_item = 0;
        UI_Item *hovered_item = 0;
        i32 item_index_counter = 0;
        for (i32 array_index = 0; array_index < ArrayCount(node_ptr_arrays); array_index += 1){
            Lister_Node_Ptr_Array node_ptr_array = node_ptr_arrays[array_index];
            for (i32 node_index = 0; node_index < node_ptr_array.count; node_index += 1){
                Lister_Node *node = node_ptr_array.node_ptrs[node_index];
                
                i32_Rect item_rect = {};
                item_rect.x0 = x0;
                item_rect.y0 = y_pos;
                item_rect.x1 = x1;
                item_rect.y1 = y_pos + block_height;
                y_pos = item_rect.y1;
                
                UI_Item item = {};
                item.activation_level = UIActivation_None;
                item.coordinates = UICoordinates_ViewSpace;
                item.rect_outer = item_rect;
                item.inner_margin = 3;
                
                if (!is_theme_list){
                    Fancy_String_List list = {};
                    push_fancy_string (ui_arena, &list, fancy_id(Stag_Default), node->string);
                    push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Default), " ");
                    push_fancy_string (ui_arena, &list, fancy_id(Stag_Pop2   ), node->status);
                    item.lines[0] = list;
                    item.line_count = 1;
                }
                else{
                    //i32 style_index = node->index;
                    
                    String name = make_lit_string("name");
                    item.lines[0] = fancy_string_list_single(push_fancy_string(ui_arena, fancy_id(Stag_Default), name));
                    
                    Fancy_String_List list = {};
                    push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Keyword     ), "if ");
                    push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Default     ), "(x < ");
                    push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Int_Constant), "0");
                    push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Default     ), ") { x = ");
                    push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Int_Constant), "0");
                    push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Default     ), "; } ");
                    push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Comment     ), "// comment");
                    item.lines[1] = list;
                    
                    item.line_count = 2;
                }
                
                item.user_data = node->user_data;
                
                
                UI_Item *item_ptr = ui_list_add_item(ui_arena, &ui_data->list, item);
                if (hit_check(item_rect, view_m)){
                    hovered_item = item_ptr;
                }
                if (state->item_index == item_index_counter){
                    highlighted_item = item_ptr;
                    state->raw_item_index = node->raw_index;
                }
                item_index_counter += 1;
                if (node->user_data == state->hot_user_data && hot_item != 0){
                    hot_item = item_ptr;
                }
            }
        }
        state->item_count_after_filter = item_index_counter;
        
        if (hovered_item != 0){
            hovered_item->activation_level = UIActivation_Hover;
        }
        if (hot_item != 0){
            if (hot_item == hovered_item){
                hot_item->activation_level = UIActivation_Active;
            }
            else{
                hot_item->activation_level = UIActivation_Hover;
            }
        }
        if (highlighted_item != 0){
            highlighted_item->activation_level = UIActivation_Active;
        }
        
        if (state->set_view_vertical_focus_to_item){
            if (highlighted_item != 0){
                view_set_vertical_focus(app, view, highlighted_item->rect_outer.y0, highlighted_item->rect_outer.y1);
            }
            state->set_view_vertical_focus_to_item = false;
        }
        
        {
            i32_Rect item_rect = {};
            item_rect.x0 = x0;
            item_rect.y0 = 0;
            item_rect.x1 = x1;
            item_rect.y1 = item_rect.y0 + text_field_height;
            y_pos = item_rect.y1;
            
            UI_Item item = {};
            item.activation_level = UIActivation_Active;
            item.coordinates = UICoordinates_PanelSpace;
            item.rect_outer = item_rect;
            item.inner_margin = 0;
            {
                Fancy_String_List list = {};
                push_fancy_string (ui_arena, &list, fancy_id(Stag_Pop1   ), state->lister.data.query);
                push_fancy_stringf(ui_arena, &list, fancy_id(Stag_Pop1   ), " ");
                push_fancy_string (ui_arena, &list, fancy_id(Stag_Default), state->lister.data.text_field);
                item.lines[0] = list;
                item.line_count = 1;
            }
            item.user_data = 0;
            
            
            ui_list_add_item(ui_arena, &ui_data->list, item);
        }
        
        ui_data_compute_bounding_boxes(ui_data);
        
        // TODO(allen): what to do with control now?
        //UI_Control control = ui_list_to_ui_control(scratch, &list);
        view_set_quit_ui_handler(app, view->view_id, lister_quit_function);
    }
    
    end_temp_memory(full_temp);
}

static Lister_Prealloced_String
lister_prealloced(String string){
    Lister_Prealloced_String result = {};
    result.string = string;
    return(result);
}

static void
lister_first_init(Application_Links *app, Lister *lister, void *user_data, i32 user_data_size){
    memset(lister, 0, sizeof(*lister));
    lister->arena = make_arena(app, (16 << 10));
    lister->data.query      = make_fixed_width_string(lister->data.query_space);
    lister->data.text_field = make_fixed_width_string(lister->data.text_field_space);
    lister->data.key_string = make_fixed_width_string(lister->data.key_string_space);
    lister->data.user_data = push_array(&lister->arena, char, user_data_size);
    lister->data.user_data_size = user_data_size;
    push_align(&lister->arena, 8);
    if (user_data != 0){
        memcpy(lister->data.user_data, user_data, user_data_size);
    }
}

static void
lister_begin_new_item_set(Application_Links *app, Lister *lister, i32 list_memory_size){
    arena_release_all(&lister->arena);
    memset(&lister->data.options, 0, sizeof(lister->data.options));
}

static void*
lister_add_item(Lister *lister, Lister_Prealloced_String string, Lister_Prealloced_String status,
                void *user_data, i32 extra_space){
    Lister_Node *node = push_array(&lister->arena, Lister_Node, 1);
    node->string = string.string;
    node->status = status.string;
    node->user_data = user_data;
    node->raw_index = lister->data.options.count;
    zdll_push_back(lister->data.options.first, lister->data.options.last, node);
    lister->data.options.count += 1;
    void *result = push_array(&lister->arena, char, extra_space);
    push_align(&lister->arena, 8);
    return(result);
}

static void*
lister_add_item(Lister *lister, Lister_Prealloced_String string, String status,
                void *user_data, i32 extra_space){
    return(lister_add_item(lister, string, lister_prealloced(string_push_copy(&lister->arena, status)),
                           user_data, extra_space));
}

static void*
lister_add_item(Lister *lister, String string, Lister_Prealloced_String status,
                void *user_data, i32 extra_space){
    return(lister_add_item(lister, lister_prealloced(string_push_copy(&lister->arena, string)), status,
                           user_data, extra_space));
}

static void*
lister_add_item(Lister *lister, String string, String status, void *user_data, i32 extra_space){
    return(lister_add_item(lister,
                           lister_prealloced(string_push_copy(&lister->arena, string)),
                           lister_prealloced(string_push_copy(&lister->arena, status)),
                           user_data, extra_space));
}

static void*
lister_add_theme_item(Lister *lister,
                      Lister_Prealloced_String string, i32 index,
                      void *user_data, i32 extra_space){
    Lister_Node *node = push_array(&lister->arena, Lister_Node, 1);
    node->string = string.string;
    node->index = index;
    node->user_data = user_data;
    node->raw_index = lister->data.options.count;
    zdll_push_back(lister->data.options.first, lister->data.options.last, node);
    lister->data.options.count += 1;
    void *result = push_array(&lister->arena, char, extra_space);
    push_align(&lister->arena, 8);
    return(result);
}

static void*
lister_add_theme_item(Lister *lister, String string, i32 index,
                      void *user_data, i32 extra_space){
    return(lister_add_theme_item(lister, lister_prealloced(string_push_copy(&lister->arena, string)), index,
                                 user_data, extra_space));
}

static void*
lister_get_user_data(Lister_Data *lister_data, i32 index){
    void *result = 0;
    if (0 <= index && index < lister_data->options.count){
        i32 counter = 0;
        for (Lister_Node *node = lister_data->options.first;
             node != 0;
             node = node->next, counter += 1){
            if (counter == index){
                result = node->user_data;
                break;
            }
        }
    }
    return(result);
}

static void
lister_call_refresh_handler(Application_Links *app, Lister *lister){
    if (lister->data.handlers.refresh != 0){
        lister->data.handlers.refresh(app, lister);
    }
}

static void
lister_default(Application_Links *app, Partition *scratch, Heap *heap,
               View_Summary *view, Lister_State *state,
               Lister_Activation_Code code){
    switch (code){
        case ListerActivation_Finished:
        {
            view_end_ui_mode(app, view);
            state->initialized = false;
            arena_release_all(&state->lister.arena);
        }break;
        
        case ListerActivation_Continue:
        {
            view_begin_ui_mode(app, view);
        }break;
        
        case ListerActivation_ContinueAndRefresh:
        {
            view_begin_ui_mode(app, view);
            state->item_index = 0;
            lister_call_refresh_handler(app, &state->lister);
            lister_update_ui(app, scratch, view, state);
        }break;
    }
}

static void
lister_call_activate_handler(Application_Links *app, Partition *scratch, Heap *heap,
                             View_Summary *view, Lister_State *state,
                             void *user_data, b32 activated_by_mouse){
    Lister_Data *lister = &state->lister.data;
    if (lister->handlers.activate != 0){
        lister->handlers.activate(app, scratch, heap, view, state,
                                  lister->text_field, user_data, activated_by_mouse);
    }
    else{
        lister_default(app, scratch, heap, view, state, ListerActivation_Finished);
    }
}

static void
lister_set_query_string(Lister_Data *lister, char *string){
    copy(&lister->query, string);
}

static void
lister_set_query_string(Lister_Data *lister, String string){
    copy(&lister->query, string);
}

static void
lister_set_text_field_string(Lister_Data *lister, char *string){
    copy(&lister->text_field, string);
}

static void
lister_set_text_field_string(Lister_Data *lister, String string){
    copy(&lister->text_field, string);
}

static void
lister_set_key_string(Lister_Data *lister, char *string){
    copy(&lister->key_string, string);
}

static void
lister_set_key_string(Lister_Data *lister, String string){
    copy(&lister->key_string, string);
}

// BOTTOM

