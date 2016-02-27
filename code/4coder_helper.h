/*
 * Bind helper struct and functions
 */

struct Bind_Helper{
    Binding_Unit *cursor, *start, *end;
    Binding_Unit *header, *group;
    int write_total;
    int error;
};

#define BH_ERR_NONE 0
#define BH_ERR_MISSING_END 1
#define BH_ERR_MISSING_BEGIN 2
#define BH_ERR_OUT_OF_MEMORY 3

inline void
copy(char *dest, const char *src, int len){
    for (int i = 0; i < len; ++i){
        *dest++ = *src++;
    }
}

inline Binding_Unit*
write_unit(Bind_Helper *helper, Binding_Unit unit){
    Binding_Unit *p = 0;
    helper->write_total += sizeof(*p);
    if (helper->error == 0 && helper->cursor != helper->end){
        p = helper->cursor++;
        *p = unit;
    }
    return p;
}

inline char*
write_inline_string(Bind_Helper *helper, char *value, int len){
    char *dest = 0;
    helper->write_total += len;
    if (helper->error == 0){
        dest = (char*)helper->cursor;
        int cursor_advance = len + sizeof(*helper->cursor) - 1;
        cursor_advance /= sizeof(*helper->cursor);
        cursor_advance *= sizeof(*helper->cursor);
        helper->cursor += cursor_advance;
        if (helper->cursor < helper->end){
            copy(dest, value, len);
        }
        else{
            helper->error = BH_ERR_OUT_OF_MEMORY;
        }
    }
    return dest;
}

inline Bind_Helper
begin_bind_helper(void *data, int size){
    Bind_Helper result;
    
    result.header = 0;
    result.group = 0;
    result.write_total = 0;
    result.error = 0;
    
    result.cursor = (Binding_Unit*)data;
    result.start = result.cursor;
    result.end = result.start + size / sizeof(*result.cursor);
    
    Binding_Unit unit;
    unit.type = unit_header;
    unit.header.total_size = sizeof(*result.header);
    result.header = write_unit(&result, unit);
    result.header->header.user_map_count = 0;
    
    return result;
}

inline void
begin_map(Bind_Helper *helper, int mapid){
    if (helper->group != 0 && helper->error == 0) helper->error = BH_ERR_MISSING_END;
    if (!helper->error && mapid < mapid_global) ++helper->header->header.user_map_count;
    
    Binding_Unit unit;
    unit.type = unit_map_begin;
    unit.map_begin.mapid = mapid;
    helper->group = write_unit(helper, unit);
    helper->group->map_begin.bind_count = 0;
}

inline void
end_map(Bind_Helper *helper){
    if (helper->group == 0 && helper->error == 0) helper->error = BH_ERR_MISSING_BEGIN;
    helper->group = 0;
}

inline void
bind(Bind_Helper *helper, short code, unsigned char modifiers, int cmdid){
    if (helper->group == 0 && helper->error == 0) helper->error = BH_ERR_MISSING_BEGIN;
    if (!helper->error) ++helper->group->map_begin.bind_count;
    
    Binding_Unit unit;
    unit.type = unit_binding;
    unit.binding.command_id = cmdid;
    unit.binding.code = code;
    unit.binding.modifiers = modifiers;
    
    write_unit(helper, unit);
}

inline void
bind(Bind_Helper *helper, short code, unsigned char modifiers, Custom_Command_Function *func){
    if (helper->group == 0 && helper->error == 0) helper->error = BH_ERR_MISSING_BEGIN;
    if (!helper->error) ++helper->group->map_begin.bind_count;
    
    Binding_Unit unit;
    unit.type = unit_callback;
    unit.callback.func = func;
    unit.callback.code = code;
    unit.callback.modifiers = modifiers;
    
    write_unit(helper, unit);
}

inline void
bind_vanilla_keys(Bind_Helper *helper, int cmdid){
    bind(helper, 0, 0, cmdid);
}

inline void
bind_vanilla_keys(Bind_Helper *helper, Custom_Command_Function *func){
    bind(helper, 0, 0, func);
}

inline void
bind_vanilla_keys(Bind_Helper *helper, unsigned char modifiers, int cmdid){
    bind(helper, 0, modifiers, cmdid);
}

inline void
bind_vanilla_keys(Bind_Helper *helper, unsigned char modifiers, Custom_Command_Function *func){
    bind(helper, 0, modifiers, func);
}

inline void
inherit_map(Bind_Helper *helper, int mapid){
    if (helper->group == 0 && helper->error == 0) helper->error = BH_ERR_MISSING_BEGIN;
    if (!helper->error && mapid < mapid_global) ++helper->header->header.user_map_count;
    
    Binding_Unit unit;
    unit.type = unit_inherit;
    unit.map_inherit.mapid = mapid;
    
    write_unit(helper, unit);
}

inline void
set_hook(Bind_Helper *helper, int hook_id, Custom_Command_Function *func){
    Binding_Unit unit;
    unit.type = unit_hook;
    unit.hook.hook_id = hook_id;
    unit.hook.func = func;
    
    write_unit(helper, unit);
}

inline void
end_bind_helper(Bind_Helper *helper){
    if (helper->header){
        helper->header->header.total_size = (int)(helper->cursor - helper->start);
        helper->header->header.error = helper->error;
    }
}

// NOTE(allen): Useful functions and overloads on app links
inline void
push_parameter(Application_Links *app, int param, int value){
    app->push_parameter(app, dynamic_int(param), dynamic_int(value));
}

inline void
push_parameter(Application_Links *app, int param, const char *value, int value_len){
    char *value_copy = app->push_memory(app, value_len+1);
    copy(value_copy, value, value_len);
    value_copy[value_len] = 0;
    app->push_parameter(app, dynamic_int(param), dynamic_string(value_copy, value_len));
}

inline void
push_parameter(Application_Links *app, const char *param, int param_len, int value){
    char *param_copy = app->push_memory(app, param_len+1);
    copy(param_copy, param, param_len);
    param_copy[param_len] = 0;
    app->push_parameter(app, dynamic_string(param_copy, param_len), dynamic_int(value));
}

inline void
push_parameter(Application_Links *app, const char *param, int param_len, const char *value, int value_len){
    char *param_copy = app->push_memory(app, param_len+1);
    char *value_copy = app->push_memory(app, value_len+1);
    copy(param_copy, param, param_len);
    copy(value_copy, value, value_len);
    value_copy[value_len] = 0;
    param_copy[param_len] = 0;
    
    app->push_parameter(app, dynamic_string(param_copy, param_len), dynamic_string(value_copy, value_len));
}

inline String
push_directory(Application_Links *app){
    String result;
    result.memory_size = 512;
    result.str = app->push_memory(app, result.memory_size);
    result.size = app->directory_get_hot(app, result.str, result.memory_size);
    return(result);
}

inline Range
get_range(File_View_Summary *view){
    Range range;
    range = make_range(view->cursor.pos, view->mark.pos);
    return(range);
}

inline void
exec_command(Application_Links *app, Command_ID id){
    app->exec_command_keep_stack(app, id);
    app->clear_parameters(app);
}

inline void
exec_command(Application_Links *app, Custom_Command_Function *func){
    func(app);
    app->clear_parameters(app);
}

inline void
active_view_to_line(Application_Links *app, int line_number){
    File_View_Summary view;
    view = app->get_active_file_view(app);
    
    // NOTE(allen|a3.4.4): We don't have to worry about whether this is a valid line number.
    // When the position specified isn't possible for whatever reason it will set the cursor to
    // a nearby valid position.
    app->view_set_cursor(app, &view, seek_line_char(line_number, 0), 1);
}

inline int
key_is_unmodified(Key_Event_Data *key){
    char *mods = key->modifiers;
    int unmodified = !mods[MDFR_CONTROL_INDEX] && !mods[MDFR_ALT_INDEX];
    return(unmodified);
}

