/*
4coder_lists.cpp - List helpers and list commands
such as open file, switch buffer, or kill buffer.
*/

// TOP

function void
generate_all_buffers_list__output_buffer(Application_Links *app, Lister *lister,
                                         Buffer_ID buffer){
    Dirty_State dirty = buffer_get_dirty_state(app, buffer);
    String_Const_u8 status = {};
    switch (dirty){
        case DirtyState_UnsavedChanges:  status = string_u8_litexpr("*"); break;
        case DirtyState_UnloadedChanges: status = string_u8_litexpr("!"); break;
        case DirtyState_UnsavedChangesAndUnloadedChanges: status = string_u8_litexpr("*!"); break;
    }
    Scratch_Block scratch(app);
    String_Const_u8 buffer_name = push_buffer_unique_name(app, scratch, buffer);
    lister_add_item(lister, buffer_name, status, IntAsPtr(buffer), 0);
}

function void
generate_all_buffers_list(Application_Links *app, Lister *lister){
    lister_begin_new_item_set(app, lister);
    
    Buffer_ID viewed_buffers[16];
    i32 viewed_buffer_count = 0;
    
    // List currently viewed buffers
    for (View_ID view = get_view_next(app, 0, Access_Always);
         view != 0;
         view = get_view_next(app, view, Access_Always)){
        Buffer_ID new_buffer_id = view_get_buffer(app, view, Access_Always);
        for (i32 i = 0; i < viewed_buffer_count; i += 1){
            if (new_buffer_id == viewed_buffers[i]){
                goto skip0;
            }
        }
        viewed_buffers[viewed_buffer_count++] = new_buffer_id;
        skip0:;
    }
    
    // Regular Buffers
    for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, Access_Always)){
        for (i32 i = 0; i < viewed_buffer_count; i += 1){
            if (buffer == viewed_buffers[i]){
                goto skip1;
            }
        }
        if (!buffer_has_name_with_star(app, buffer)){
            generate_all_buffers_list__output_buffer(app, lister, buffer);
        }
        skip1:;
    }
    
    // Buffers Starting with *
    for (Buffer_ID buffer = get_buffer_next(app, 0, Access_Always);
         buffer != 0;
         buffer = get_buffer_next(app, buffer, Access_Always)){
        for (i32 i = 0; i < viewed_buffer_count; i += 1){
            if (buffer == viewed_buffers[i]){
                goto skip2;
            }
        }
        if (buffer_has_name_with_star(app, buffer)){
            generate_all_buffers_list__output_buffer(app, lister, buffer);
        }
        skip2:;
    }
    
    // Buffers That Are Open in Views
    for (i32 i = 0; i < viewed_buffer_count; i += 1){
        generate_all_buffers_list__output_buffer(app, lister, viewed_buffers[i]);
    }
}

function Buffer_ID
get_buffer_from_user(Application_Links *app, String_Const_u8 query){
    Lister_Handlers handlers = lister_get_default_handlers();
    handlers.refresh = generate_all_buffers_list;
    Lister_Result l_result = run_lister_with_refresh_handler(app, query, handlers);
    Buffer_ID result = 0;
    if (!l_result.canceled){
        result = (Buffer_ID)(PtrAsInt(l_result.user_data));
    }
    return(result);
}

function Buffer_ID
get_buffer_from_user(Application_Links *app, char *query){
    return(get_buffer_from_user(app, SCu8(query)));
}

////////////////////////////////

function Custom_Command_Function*
get_command_from_user(Application_Links *app, String_Const_u8 query,
                      i32 *command_ids, i32 command_id_count){
    if (command_ids == 0){
        command_id_count = command_one_past_last_id;
    }
    
    Scratch_Block scratch(app, Scratch_Share);
    Lister *lister = begin_lister(app, scratch);
    lister_set_query(lister, query);
    lister->handlers = lister_get_default_handlers();
    
    for (i32 i = 0; i < command_id_count; i += 1){
        i32 j = i;
        if (command_ids != 0){
            j = command_ids[i];
        }
        j = clamp(0, j, command_one_past_last_id);
        lister_add_item(lister,
                        SCu8(fcoder_metacmd_table[j].name),
                        SCu8(fcoder_metacmd_table[j].description),
                        (void*)fcoder_metacmd_table[j].proc, 0);
    }
    
    Lister_Result l_result = run_lister(app, lister);
    
    Custom_Command_Function *result = 0;
    if (!l_result.canceled){
        result = (Custom_Command_Function*)l_result.user_data;
    }
    return(result);
}

function Custom_Command_Function*
get_command_from_user(Application_Links *app, String_Const_u8 query){
    return(get_command_from_user(app, query, 0, 0));
}

function Custom_Command_Function*
get_command_from_user(Application_Links *app, char *query,
                      i32 *command_ids, i32 command_id_count){
    return(get_command_from_user(app, SCu8(query), command_ids, command_id_count));
}

function Custom_Command_Function*
get_command_from_user(Application_Links *app, char *query){
    return(get_command_from_user(app, SCu8(query), 0, 0));
}

////////////////////////////////

function void
lister__write_character__file_path(Application_Links *app){
    View_ID view = get_this_ctx_view(app, Access_Always);
    Lister *lister = view_get_lister(view);
    if (lister != 0){
        User_Input in = get_current_input(app);
        String_Const_u8 string = to_writable(&in);
        if (string.str != 0 && string.size > 0){
            lister_append_text_field(lister, string);
            String_Const_u8 front_name = string_front_of_path(lister->text_field.string);
            lister_set_key(lister, front_name);
            if (character_is_slash(string.str[0])){
                String_Const_u8 new_hot = lister->text_field.string;
                set_hot_directory(app, new_hot);
                lister_call_refresh_handler(app, lister);
            }
            lister->item_index = 0;
            lister_zero_scroll(lister);
            lister_update_filtered_list(app, lister);
        }
    }
}

function void
lister__backspace_text_field__file_path(Application_Links *app){
    View_ID view = get_this_ctx_view(app, Access_Always);
    Lister *lister = view_get_lister(view);
    if (lister != 0){
        if (lister->text_field.size > 0){
            char last_char = lister->text_field.str[lister->text_field.size - 1];
            lister->text_field.string = backspace_utf8(lister->text_field.string);
            if (character_is_slash(last_char)){
                User_Input input = get_current_input(app);
                String_Const_u8 text_field = lister->text_field.string;
                String_Const_u8 new_hot = string_remove_last_folder(text_field);
                b32 is_modified = has_modifier(&input, KeyCode_Control);
                b32 whole_word_backspace = (is_modified == global_config.lister_whole_word_backspace_when_modified);
                if (whole_word_backspace){
                    lister->text_field.size = new_hot.size;
                }
                set_hot_directory(app, new_hot);
                // TODO(allen): We have to protect against lister_call_refresh_handler
                // changing the text_field here. Clean this up.
                String_u8 dingus = lister->text_field;
                lister_call_refresh_handler(app, lister);
                lister->text_field = dingus;
            }
            else{
                String_Const_u8 text_field = lister->text_field.string;
                String_Const_u8 new_key = string_front_of_path(text_field);
                lister_set_key(lister, new_key);
            }
            
            lister->item_index = 0;
            lister_zero_scroll(lister);
            lister_update_filtered_list(app, lister);
        }
    }
}

function void
generate_hot_directory_file_list(Application_Links *app, Lister *lister){
    Scratch_Block scratch(app);
    
    Temp_Memory temp = begin_temp(lister->arena);
    String_Const_u8 hot = push_hot_directory(app, lister->arena);
    if (!character_is_slash(string_get_character(hot, hot.size - 1))){
        hot = push_u8_stringf(lister->arena, "%.*s/", string_expand(hot));
    }
    lister_set_text_field(lister, hot);
    lister_set_key(lister, string_front_of_path(hot));
    
    File_List file_list = system_get_file_list(scratch, hot);
    end_temp(temp);
    
    File_Info **one_past_last = file_list.infos + file_list.count;
    
    lister_begin_new_item_set(app, lister);
    
    hot = push_hot_directory(app, lister->arena);
    push_align(lister->arena, 8);
    if (hot.str != 0){
        String_Const_u8 empty_string = string_u8_litexpr("");
        Lister_Prealloced_String empty_string_prealloced = lister_prealloced(empty_string);
        for (File_Info **info = file_list.infos;
             info < one_past_last;
             info += 1){
            if (!HasFlag((**info).attributes.flags, FileAttribute_IsDirectory)) continue;
            String_Const_u8 file_name = push_u8_stringf(lister->arena, "%.*s/",
                                                        string_expand((**info).file_name));
            lister_add_item(lister, lister_prealloced(file_name), empty_string_prealloced, file_name.str, 0);
        }
        
        for (File_Info **info = file_list.infos;
             info < one_past_last;
             info += 1){
            if (HasFlag((**info).attributes.flags, FileAttribute_IsDirectory)) continue;
            String_Const_u8 file_name = push_string_copy(lister->arena, (**info).file_name);
            char *is_loaded = "";
            char *status_flag = "";
            
            Buffer_ID buffer = {};
            
            {
                Temp_Memory path_temp = begin_temp(lister->arena);
                List_String_Const_u8 list = {};
                string_list_push(lister->arena, &list, hot);
                string_list_push_overlap(lister->arena, &list, '/', (**info).file_name);
                String_Const_u8 full_file_path = string_list_flatten(lister->arena, list);
                buffer = get_buffer_by_file_name(app, full_file_path, Access_Always);
                end_temp(path_temp);
            }
            
            if (buffer != 0){
                is_loaded = "LOADED";
                Dirty_State dirty = buffer_get_dirty_state(app, buffer);
                switch (dirty){
                    case DirtyState_UnsavedChanges:  status_flag = " *"; break;
                    case DirtyState_UnloadedChanges: status_flag = " !"; break;
                    case DirtyState_UnsavedChangesAndUnloadedChanges: status_flag = " *!"; break;
                }
            }
            String_Const_u8 status = push_u8_stringf(lister->arena, "%s%s", is_loaded, status_flag);
            lister_add_item(lister, lister_prealloced(file_name), lister_prealloced(status), file_name.str, 0);
        }
    }
}

struct File_Name_Result{
    b32 canceled;
    b32 clicked;
    b32 is_folder;
    String_Const_u8 file_name_activated;
    String_Const_u8 file_name_in_text_field;
    String_Const_u8 path_in_text_field;
};

function File_Name_Result
get_file_name_from_user(Application_Links *app, Arena *arena, String_Const_u8 query,
                        View_ID view){
    Lister_Handlers handlers = lister_get_default_handlers();
    handlers.refresh = generate_hot_directory_file_list;
    handlers.write_character = lister__write_character__file_path;
    handlers.backspace = lister__backspace_text_field__file_path;
    
    Lister_Result l_result =
        run_lister_with_refresh_handler(app, arena, query, handlers);
    
    File_Name_Result result = {};
    result.canceled = l_result.canceled;
    if (!l_result.canceled){
        result.clicked = l_result.activated_by_click;
        if (l_result.user_data != 0){
            String_Const_u8 name = SCu8((u8*)l_result.user_data);
            result.file_name_activated = name;
            result.is_folder =
                character_is_slash(string_get_character(name, name.size -1 ));
        }
        result.file_name_in_text_field = string_front_of_path(l_result.text_field);
        
        String_Const_u8 path = string_remove_front_of_path(l_result.text_field);
        if (character_is_slash(string_get_character(path, path.size - 1))){
            path = string_chop(path, 1);
        }
        result.path_in_text_field = path;
    }
    
    return(result);
}

function File_Name_Result
get_file_name_from_user(Application_Links *app, Arena *arena, char *query,
                        View_ID view){
    return(get_file_name_from_user(app, arena, SCu8(query), view));
}

////////////////////////////////

enum{
    SureToKill_NULL = 0,
    SureToKill_No = 1,
    SureToKill_Yes = 2,
    SureToKill_Save = 3,
};

function b32
do_buffer_kill_user_check(Application_Links *app, Buffer_ID buffer, View_ID view){
    Scratch_Block scratch(app);
    Lister_Choice_List list = {};
    lister_choice(scratch, &list, "(N)o"  , "", KeyCode_N, SureToKill_No);
    lister_choice(scratch, &list, "(Y)es" , "", KeyCode_Y, SureToKill_Yes);
    lister_choice(scratch, &list, "(S)ave", "", KeyCode_S, SureToKill_Save);
    
    Lister_Choice *choice =
        get_choice_from_user(app, "There are unsaved changes, close anyway?", list);
    
    b32 do_kill = false;
    if (choice != 0){
        switch (choice->user_data){
            case SureToKill_No:
            {}break;
            
            case SureToKill_Yes:
            {
                do_kill = true;
            }break;
            
            case SureToKill_Save:
            {
                String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer);
                if (buffer_save(app, buffer, file_name, BufferSave_IgnoreDirtyFlag)){
                    do_kill = true;
                }
                else{
#define M "Did not close '%.*s' because it did not successfully save."
                    String_Const_u8 str =
                        push_u8_stringf(scratch, M, string_expand(file_name));
#undef M
                    print_message(app, str);
                }
            }break;
        }
    }
    
    return(do_kill);
}

function b32
do_4coder_close_user_check(Application_Links *app, View_ID view){
    Scratch_Block scratch(app);
    Lister_Choice_List list = {};
    lister_choice(scratch, &list, "(N)o"  , "", KeyCode_N, SureToKill_No);
    lister_choice(scratch, &list, "(Y)es" , "", KeyCode_Y, SureToKill_Yes);
    lister_choice(scratch, &list, "(S)ave all and close", "",
                  KeyCode_S, SureToKill_Save);
    
#define M "There are one or more buffers with unsave changes, close anyway?"
    Lister_Choice *choice = get_choice_from_user(app, M, list);
#undef M
    
    b32 do_exit = false;
    if (choice != 0){
        switch (choice->user_data){
            case SureToKill_No:
            {}break;
            
            case SureToKill_Yes:
            {
                allow_immediate_close_without_checking_for_changes = true;
                do_exit = true;
            }break;
            
            case SureToKill_Save:
            {
                save_all_dirty_buffers(app);
                allow_immediate_close_without_checking_for_changes = true;
                do_exit = true;
            }break;
        }
    }
    
    return(do_exit);
}

////////////////////////////////

CUSTOM_UI_COMMAND_SIG(interactive_switch_buffer)
CUSTOM_DOC("Interactively switch to an open buffer.")
{
    Buffer_ID buffer = get_buffer_from_user(app, "Switch:");
    if (buffer != 0){
    View_ID view = get_this_ctx_view(app, Access_Always);
        view_set_buffer(app, view, buffer, 0);
    }
}

CUSTOM_UI_COMMAND_SIG(interactive_kill_buffer)
CUSTOM_DOC("Interactively kill an open buffer.")
{
    Buffer_ID buffer = get_buffer_from_user(app, "Kill:");
    if (buffer != 0){
        View_ID view = get_this_ctx_view(app, Access_Always);
        try_buffer_kill(app, buffer, view, 0);
    }
}

////////////////////////////////

function Lister_Activation_Code
activate_open_or_new__generic(Application_Links *app, View_ID view,
                              String_Const_u8 path, String_Const_u8 file_name,
                              b32 is_folder,
                              Buffer_Create_Flag flags){
    Lister_Activation_Code result = 0;
    
    if (file_name.size == 0){
#define M "Zero length file_name passed to activate_open_or_new__generic\n"
        print_message(app, string_u8_litexpr(M));
#undef M
        result = ListerActivation_Finished;
    }
    else{
        Scratch_Block scratch(app, Scratch_Share);
        String_Const_u8 full_file_name = {};
        if (character_is_slash(string_get_character(path, path.size - 1))){
            path = string_chop(path, 1);
        }
        full_file_name = push_u8_stringf(scratch, "%.*s/%.*s", string_expand(path), string_expand(file_name));
        if (is_folder){
            set_hot_directory(app, full_file_name);
            result = ListerActivation_ContinueAndRefresh;
        }
        else{
            Buffer_ID buffer = create_buffer(app, full_file_name, flags);
            if (buffer != 0){
                view_set_buffer(app, view, buffer, SetBuffer_KeepOriginalGUI);
            }
            result = ListerActivation_Finished;
        }
    }
    
    return(result);
}

CUSTOM_UI_COMMAND_SIG(interactive_open_or_new)
CUSTOM_DOC("Interactively open a file out of the file system.")
{
    for (;;){
        Scratch_Block scratch(app);
        View_ID view = get_this_ctx_view(app, Access_Always);
        File_Name_Result result = get_file_name_from_user(app, scratch, "Open:",
                                                          view);
        if (result.canceled) break;
        
        String_Const_u8 file_name = result.file_name_activated;
        if (file_name.size == 0){
            file_name = result.file_name_in_text_field;
        }
        if (file_name.size == 0) break;
        
        String_Const_u8 path = result.path_in_text_field;
        String_Const_u8 full_file_name =
            push_u8_stringf(scratch, "%.*s/%.*s",
                            string_expand(path), string_expand(file_name));
        
        if (result.is_folder){
            set_hot_directory(app, full_file_name);
            continue;
        }
        
        Buffer_ID buffer = create_buffer(app, full_file_name, 0);
        if (buffer != 0){
            view_set_buffer(app, view, buffer, 0);
        }
        break;
    }
}

CUSTOM_UI_COMMAND_SIG(interactive_new)
CUSTOM_DOC("Interactively creates a new file.")
{
    for (;;){
        Scratch_Block scratch(app);
        View_ID view = get_this_ctx_view(app, Access_Always);
        File_Name_Result result = get_file_name_from_user(app, scratch, "New:",
                                                          view);
        if (result.canceled) break;
        
        // NOTE(allen): file_name from the text field always
        // unless this is a folder or a mouse click.
        String_Const_u8 file_name = result.file_name_in_text_field;
        if (result.is_folder || result.clicked){
            file_name = result.file_name_activated;
        }
        if (file_name.size == 0) break;
        
        String_Const_u8 path = result.path_in_text_field;
        String_Const_u8 full_file_name =
            push_u8_stringf(scratch, "%.*s/%.*s",
                            string_expand(path), string_expand(file_name));
        
        if (result.is_folder){
            set_hot_directory(app, full_file_name);
            continue;
        }
        
        Buffer_Create_Flag flags = BufferCreate_AlwaysNew;
        Buffer_ID buffer = create_buffer(app, full_file_name, flags);
        if (buffer != 0){
            view_set_buffer(app, view, buffer, 0);
        }
        break;
    }
}

CUSTOM_UI_COMMAND_SIG(interactive_open)
CUSTOM_DOC("Interactively opens a file.")
{
    for (;;){
        Scratch_Block scratch(app);
        View_ID view = get_this_ctx_view(app, Access_Always);
        File_Name_Result result = get_file_name_from_user(app, scratch, "Open:",
                                                          view);
        if (result.canceled) break;
        
        String_Const_u8 file_name = result.file_name_activated;
        if (file_name.size == 0) break;
        
        String_Const_u8 path = result.path_in_text_field;
        String_Const_u8 full_file_name =
            push_u8_stringf(scratch, "%.*s/%.*s",
                            string_expand(path), string_expand(file_name));
        
        if (result.is_folder){
            set_hot_directory(app, full_file_name);
            continue;
        }
        
        Buffer_Create_Flag flags = BufferCreate_NeverNew;
        Buffer_ID buffer = create_buffer(app, full_file_name, flags);
        if (buffer != 0){
            view_set_buffer(app, view, buffer, 0);
        }
        break;
    }
}

////////////////////////////////

CUSTOM_UI_COMMAND_SIG(command_lister)
CUSTOM_DOC("Opens an interactive list of all registered commands.")
{
    Custom_Command_Function *func = get_command_from_user(app, "Command:");
    if (func != 0){
        View_ID view = get_this_ctx_view(app, Access_Always);
        view_enqueue_command_function(app, view, func);
    }
}

// BOTTOM

