
// TOP

#include "4coder_default_include.cpp"

// NOTE(allen|a3.3): All of your custom ids should be enumerated
// as shown here, they may start at 0, and you can only have
// 2^24 of them so don't be wasteful!
enum My_Maps{
    my_code_map,
    
    my_maps_count
};

CUSTOM_COMMAND_SIG(write_allen_todo){
    write_string(app, make_lit_string("// TODO(allen): "));
}

CUSTOM_COMMAND_SIG(write_allen_note){
    write_string(app, make_lit_string("// NOTE(allen): "));
}

CUSTOM_COMMAND_SIG(write_zero_struct){
    write_string(app, make_lit_string(" = {0};"));
}

CUSTOM_COMMAND_SIG(write_capital){
    User_Input command_in = app->get_command_input(app);
    char c = command_in.key.character_no_caps_lock;
    if (c != 0){
        c = char_to_upper(c);
        write_string(app, make_string(&c, 1));
    }
}

CUSTOM_COMMAND_SIG(switch_to_compilation){
    View_Summary view;
    Buffer_Summary buffer;
    
    char name[] = "*compilation*";
    int name_size = sizeof(name)-1;
    
    unsigned int access = AccessOpen;
    view = app->get_active_view(app, access);
    buffer = app->get_buffer_by_name(app, name, name_size, access);
    
    app->view_set_buffer(app, &view, buffer.buffer_id);
}

CUSTOM_COMMAND_SIG(rewrite_as_single_caps){
    View_Summary view;
    Buffer_Summary buffer;
    Full_Cursor cursor;
    Range range;
    String string;
    int is_first, i;
    
    unsigned int access = AccessOpen;
    view = app->get_active_view(app, access);
    cursor = view.cursor;
    
    exec_command(app, seek_token_left);
    app->refresh_view(app, &view);
    range.min = view.cursor.pos;
    
    exec_command(app, seek_token_right);
    app->refresh_view(app, &view);
    range.max = view.cursor.pos;
    
    string.str = (char*)app->memory;
    string.size = range.max - range.min;
    assert(string.size < app->memory_size);
    
    buffer = app->get_buffer(app, view.buffer_id, access);
    app->buffer_read_range(app, &buffer, range.min, range.max, string.str);
    
    is_first = 1;
    for (i = 0; i < string.size; ++i){
        if (char_is_alpha_true(string.str[i])){
            if (is_first) is_first = 0;
            else string.str[i] = char_to_lower(string.str[i]);
        }
        else{
            is_first = 1;
        }
    }
    
    app->buffer_replace_range(app, &buffer, range.min, range.max, string.str, string.size);
    
    app->view_set_cursor(app, &view,
                         seek_line_char(cursor.line+1, cursor.character),
                         1);
}

CUSTOM_COMMAND_SIG(open_my_files){
    // TODO(allen|a4.0.8): comment
    unsigned int access = AccessProtected|AccessHidden;
    View_Summary view = app->get_active_view(app, access);
    view_open_file(app, &view, literal("w:/4ed/data/test/basic.cpp"), false);
}

CUSTOM_COMMAND_SIG(build_at_launch_location){
    // TODO(allen|a4.0.8): comment
    unsigned int access = AccessProtected|AccessHidden;
    View_Summary view = app->get_active_view(app, access);
    app->exec_system_command(app, &view,
                             buffer_identifier(literal("*compilation*")),
                             literal("."),
                             literal("build"),
                             CLI_OverlapWithConflict);
}

HOOK_SIG(my_start){
    exec_command(app, cmdid_open_panel_vsplit);
    exec_command(app, cmdid_hide_scrollbar);
    exec_command(app, cmdid_change_active_panel);
    exec_command(app, cmdid_hide_scrollbar);
    
    app->change_theme(app, literal("4coder"));
    app->change_font(app, literal("Liberation Sans"));
    
    // Theme options:
    //  "4coder"
    //  "Handmade Hero"
    //  "Twilight"
    //  "Woverine"
    //  "stb"
    
    // Font options:
    //  "Liberation Sans"
    //  "Liberation Mono"
    //  "Hack"
    //  "Cutive Mono"
    //  "Inconsolata"
    
    // no meaning for return
    return(0);
}

HOOK_SIG(my_file_settings){
    // NOTE(allen|a4): In hooks that want parameters, such as this file
    // opened hook.  The file created hook is guaranteed to have only
    // and exactly one buffer parameter.  In normal command callbacks
    // there are no parameter buffers.
    unsigned int access = AccessProtected|AccessHidden;
    Buffer_Summary buffer = app->get_parameter_buffer(app, 0, access);
    assert(buffer.exists);
    
    int treat_as_code = 0;
    int wrap_lines = 1;
    
    if (buffer.file_name && buffer.size < (16 << 20)){
        String ext = file_extension(make_string(buffer.file_name, buffer.file_name_len));
        if (match(ext, make_lit_string("cpp"))) treat_as_code = 1;
        else if (match(ext, make_lit_string("h"))) treat_as_code = 1;
        else if (match(ext, make_lit_string("c"))) treat_as_code = 1;
        else if (match(ext, make_lit_string("hpp"))) treat_as_code = 1;
    }
    
    if (treat_as_code){
        wrap_lines = 0;
    }
    if (buffer.file_name[0] == '*'){
        wrap_lines = 0;
    }
    
    app->buffer_set_setting(app, &buffer, BufferSetting_Lex, treat_as_code);
    app->buffer_set_setting(app, &buffer, BufferSetting_WrapLine, wrap_lines);
    app->buffer_set_setting(app, &buffer, BufferSetting_MapID, (treat_as_code)?((int)my_code_map):((int)mapid_file));
    
    // TODO(allen): eliminate this hook if you can.
    // no meaning for return
    return(0);
}

void
default_keys(Bind_Helper *context){
    begin_map(context, mapid_global);
    
    bind(context, 'p', MDFR_CTRL, cmdid_open_panel_vsplit);
    bind(context, '_', MDFR_CTRL, cmdid_open_panel_hsplit);
    bind(context, 'P', MDFR_CTRL, cmdid_close_panel);
    bind(context, 'n', MDFR_CTRL, cmdid_interactive_new);
    bind(context, 'o', MDFR_CTRL, cmdid_interactive_open);
    bind(context, ',', MDFR_CTRL, cmdid_change_active_panel);
    bind(context, 'k', MDFR_CTRL, cmdid_interactive_kill_buffer);
    bind(context, 'i', MDFR_CTRL, cmdid_interactive_switch_buffer);
    bind(context, 'c', MDFR_ALT, cmdid_open_color_tweaker);
    bind(context, 'd', MDFR_ALT, cmdid_open_debug);
    bind(context, 'o', MDFR_ALT, open_in_other);
    bind(context, 'w', MDFR_CTRL, save_as);
    
    bind(context, 'm', MDFR_ALT, build_search);
    bind(context, 'x', MDFR_ALT, execute_arbitrary_command);
    bind(context, 'z', MDFR_ALT, execute_any_cli);
    bind(context, 'Z', MDFR_ALT, execute_previous_cli);
    
    // NOTE(allen): These callbacks may not actually be useful to you, but
    // go look at them and see what they do.
    bind(context, 'M', MDFR_ALT | MDFR_CTRL, open_my_files);
    bind(context, 'M', MDFR_ALT, build_at_launch_location);
    
    end_map(context);
    
    begin_map(context, my_code_map);
    
    // NOTE(allen|a3.1): Set this map (my_code_map == mapid_user_custom) to
    // inherit from mapid_file.  When searching if a key is bound
    // in this map, if it is not found here it will then search mapid_file.
    //
    // If this is not set, it defaults to mapid_global.
    inherit_map(context, mapid_file);
    
    // NOTE(allen|a3.1): Children can override parent's bindings.
    bind(context, key_right, MDFR_CTRL, seek_alphanumeric_or_camel_right);
    bind(context, key_left, MDFR_CTRL, seek_alphanumeric_or_camel_left);
    
    // NOTE(allen|a3.2): Specific keys can override vanilla keys,
    // and write character writes whichever character corresponds
    // to the key that triggered the command.
    bind(context, '\n', MDFR_NONE, write_and_auto_tab);
    bind(context, '}', MDFR_NONE, write_and_auto_tab);
    bind(context, ')', MDFR_NONE, write_and_auto_tab);
    bind(context, ']', MDFR_NONE, write_and_auto_tab);
    bind(context, ';', MDFR_NONE, write_and_auto_tab);
    bind(context, '#', MDFR_NONE, write_and_auto_tab);
    
    bind(context, '\t', MDFR_NONE, cmdid_word_complete);
    bind(context, '\t', MDFR_CTRL, auto_tab_range);
    bind(context, '\t', MDFR_SHIFT, auto_tab_line_at_cursor);
    
    bind(context, '=', MDFR_CTRL, write_increment);
    bind(context, 't', MDFR_ALT, write_allen_todo);
    bind(context, 'n', MDFR_ALT, write_allen_note);
    bind(context, '[', MDFR_CTRL, open_long_braces);
    bind(context, '{', MDFR_CTRL, open_long_braces_semicolon);
    bind(context, '}', MDFR_CTRL, open_long_braces_break);
    bind(context, 'i', MDFR_ALT, if0_off);
    bind(context, '1', MDFR_ALT, open_file_in_quotes);
    bind(context, '0', MDFR_CTRL, write_zero_struct);
    
    end_map(context);
    
    
    begin_map(context, mapid_file);
    
    // NOTE(allen|a3.4.4): Binding this essentially binds
    // all key combos that would normally insert a character
    // into a buffer. If the code for the key is not an enum
    // value such as key_left or key_back then it is a vanilla key.
    // It is possible to override this binding for individual keys.
    bind_vanilla_keys(context, write_character);
    
    // NOTE(allen|a4.0.7): You can now bind left and right clicks.
    // They only trigger on mouse presses.  Modifiers do work
    // so control+click shift+click etc can now have special meanings.
    bind(context, key_mouse_left, MDFR_NONE, click_set_cursor);
    bind(context, key_mouse_right, MDFR_NONE, click_set_mark);
    
    bind(context, key_left, MDFR_NONE, move_left);
    bind(context, key_right, MDFR_NONE, move_right);
    bind(context, key_del, MDFR_NONE, delete_char);
    bind(context, key_back, MDFR_NONE, backspace_char);
    bind(context, key_up, MDFR_NONE, move_up);
    bind(context, key_down, MDFR_NONE, move_down);
    bind(context, key_end, MDFR_NONE, seek_end_of_line);
    bind(context, key_home, MDFR_NONE, seek_beginning_of_line);
    bind(context, key_page_up, MDFR_NONE, cmdid_page_up);
    bind(context, key_page_down, MDFR_NONE, cmdid_page_down);
    
    bind(context, key_right, MDFR_CTRL, seek_whitespace_right);
    bind(context, key_left, MDFR_CTRL, seek_whitespace_left);
    bind(context, key_up, MDFR_CTRL, seek_whitespace_up);
    bind(context, key_down, MDFR_CTRL, seek_whitespace_down);
    
    bind(context, key_up, MDFR_ALT, move_up_10);
    bind(context, key_down, MDFR_ALT, move_down_10);
    
    bind(context, key_back, MDFR_CTRL, backspace_word);
    bind(context, key_del, MDFR_CTRL, delete_word);
    bind(context, key_back, MDFR_ALT, snipe_token_or_word);
    
    bind(context, ' ', MDFR_CTRL, set_mark);
    bind(context, 'a', MDFR_CTRL, replace_in_range);
    bind(context, 'c', MDFR_CTRL, copy);
    bind(context, 'd', MDFR_CTRL, delete_range);
    bind(context, 'e', MDFR_CTRL, cmdid_center_view);
    bind(context, 'E', MDFR_CTRL, cmdid_left_adjust_view);
    bind(context, 'f', MDFR_CTRL, search);
    bind(context, 'g', MDFR_CTRL, goto_line);
    bind(context, 'h', MDFR_CTRL, cmdid_history_backward);
    bind(context, 'H', MDFR_CTRL, cmdid_history_forward);
    bind(context, 'j', MDFR_CTRL, cmdid_to_lowercase);
    bind(context, 'K', MDFR_CTRL, cmdid_kill_buffer);
    bind(context, 'l', MDFR_CTRL, cmdid_toggle_line_wrap);
    bind(context, 'm', MDFR_CTRL, cursor_mark_swap);
    bind(context, 'O', MDFR_CTRL, cmdid_reopen);
    bind(context, 'q', MDFR_CTRL, query_replace);
    bind(context, 'r', MDFR_CTRL, reverse_search);
    bind(context, 's', MDFR_ALT, cmdid_show_scrollbar);
    bind(context, 's', MDFR_CTRL, cmdid_save);
    bind(context, 'u', MDFR_CTRL, cmdid_to_uppercase);
    bind(context, 'U', MDFR_CTRL, rewrite_as_single_caps);
    bind(context, 'v', MDFR_CTRL, paste);
    bind(context, 'V', MDFR_CTRL, paste_next);
    bind(context, 'w', MDFR_ALT, cmdid_hide_scrollbar);
    bind(context, 'x', MDFR_CTRL, cut);
    bind(context, 'y', MDFR_CTRL, cmdid_redo);
    bind(context, 'z', MDFR_CTRL, cmdid_undo);
    
    
    bind(context, '1', MDFR_CTRL, cmdid_eol_dosify);
    
    bind(context, '?', MDFR_CTRL, cmdid_toggle_show_whitespace);
    bind(context, '~', MDFR_CTRL, cmdid_clean_all_lines);
    bind(context, '!', MDFR_CTRL, cmdid_eol_nixify);
    bind(context, '\n', MDFR_SHIFT, write_and_auto_tab);
    bind(context, ' ', MDFR_SHIFT, write_character);
    
    end_map(context);
}

#ifndef NO_BINDING

extern "C" int
get_bindings(void *data, int size){
    Bind_Helper context_ = begin_bind_helper(data, size);
    Bind_Helper *context = &context_;
    
    // NOTE(allen|a3.1): Hooks have no loyalties to maps. All hooks are global
    // and once set they always apply, regardless of what map is active.
    set_hook(context, hook_start, my_start);
    set_hook(context, hook_open_file, my_file_settings);
    
    set_scroll_rule(context, smooth_scroll_rule);
    
    default_keys(context);
    
    int result = end_bind_helper(context);
    return(result);
}

#endif

// BOTTOM
