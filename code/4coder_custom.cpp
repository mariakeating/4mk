/*
 * Example use of customization API
 */

#define FCPP_STRING_IMPLEMENTATION
#include "4coder_string.h"

// NOTE(allen): See exec_command and surrounding code in 4coder_helper.h
// to decide whether you want macro translations, without them you will
// have to manipulate the command and parameter stack through
// "app->" which may be more or less clear depending on your use.
//
// I suggest you try disabling macro translation and getting your code working
// that way, because I'll be removing it entirely sometime soon
#define DisableMacroTranslations 0

#include "4coder_custom.h"
#include "4coder_helper.h"

#ifndef literal
#define literal(s) s, (sizeof(s)-1)
#endif

// NOTE(allen|a3.3): All of your custom ids should be enumerated
// as shown here, they may start at 0, and you can only have
// 2^24 of them so don't be wasteful!
enum My_Maps{
    my_code_map,
    my_html_map
};

HOOK_SIG(my_start){
    exec_command(cmd_context, cmdid_open_panel_vsplit);
    exec_command(cmd_context, cmdid_change_active_panel);
}

char *get_extension(const char *filename, int len, int *extension_len){
    char *c = (char*)(filename + len - 1);
    char *end = c;
    while (*c != '.' && c > filename) --c;
    *extension_len = (int)(end - c);
    return c+1;
}

bool str_match(const char *a, int len_a, const char *b, int len_b){
    bool result = 0;
    if (len_a == len_b){
        char *end = (char*)(a + len_a);
        while (a < end && *a == *b){
            ++a; ++b;
        }
        if (a == end) result = 1;
    }
    return result;
}

HOOK_SIG(my_file_settings){
     Buffer_Summary buffer = app->get_active_buffer(cmd_context);
     
     // NOTE(allen|a3.4.2): Whenever you ask for a buffer, you can check that
     // the exists field is set to true.  Reasons why the buffer might not exist:
     //   -The active panel does not contain a buffer and get_active_buffer was used
     //   -The index provided to get_buffer was out of range [0,max) or that index is associated to a dummy buffer
     //   -The name provided to get_buffer_by_name did not match any of the existing buffers
     if (buffer.exists){
         int treat_as_code = 0;

         if (buffer.file_name && buffer.size < (16 << 20)){
             int extension_len;
             char *extension = get_extension(buffer.file_name, buffer.file_name_len, &extension_len);
             if (str_match(extension, extension_len, literal("cpp"))) treat_as_code = 1;
             else if (str_match(extension, extension_len, literal("h"))) treat_as_code = 1;
             else if (str_match(extension, extension_len, literal("c"))) treat_as_code = 1;
             else if (str_match(extension, extension_len, literal("hpp"))) treat_as_code = 1;
         }

         push_parameter(app, cmd_context, par_lex_as_cpp_file, treat_as_code);
         push_parameter(app, cmd_context, par_wrap_lines, !treat_as_code);
         push_parameter(app, cmd_context, par_key_mapid, (treat_as_code)?((int)my_code_map):((int)mapid_file));
         exec_command(cmd_context, cmdid_set_settings);
     }
}

CUSTOM_COMMAND_SIG(write_increment){
    char text[] = "++";
    int size = sizeof(text) - 1;
    Buffer_Summary buffer = app->get_active_buffer(cmd_context);
    app->buffer_replace_range(cmd_context, &buffer, buffer.file_cursor_pos, buffer.file_cursor_pos, text, size);
}

CUSTOM_COMMAND_SIG(write_decrement){
    char text[] = "--";
    int size = sizeof(text) - 1;
    Buffer_Summary buffer = app->get_active_buffer(cmd_context);
    app->buffer_replace_range(cmd_context, &buffer, buffer.file_cursor_pos, buffer.file_cursor_pos, text, size);
}

CUSTOM_COMMAND_SIG(open_long_braces){
    File_View_Summary view;
    Buffer_Summary buffer;
    char text[] = "{\n\n}";
    int size = sizeof(text) - 1;
    int pos;
    
    view = app->get_active_file_view(cmd_context);
    buffer = app->get_buffer(cmd_context, view.file_id);
    
    pos = view.cursor.pos;
    app->buffer_replace_range(cmd_context, &buffer, pos, pos, text, size);
    app->view_set_cursor(cmd_context, &view, seek_pos(pos + 2), 1);
    app->view_set_mark(cmd_context, &view, seek_pos(pos + 4));
    
    push_parameter(app, cmd_context, par_range_start, pos);
    push_parameter(app, cmd_context, par_range_end, pos + size);
    push_parameter(app, cmd_context, par_clear_blank_lines, 0);
    exec_command(cmd_context, cmdid_auto_tab_range);
}

CUSTOM_COMMAND_SIG(ifdef_off){
    File_View_Summary view;
    Buffer_Summary buffer;
    
    char text1[] = "#if 0\n";
    int size1 = sizeof(text1) - 1;
    
    char text2[] = "#endif\n";
    int size2 = sizeof(text2) - 1;
    
    Range range;
    int pos;

    view = app->get_active_file_view(cmd_context);
    buffer = app->get_active_buffer(cmd_context);

    range = get_range(&view);
    pos = range.min;

    app->buffer_replace_range(cmd_context, &buffer, pos, pos, text1, size1);

    push_parameter(app, cmd_context, par_range_start, pos);
    push_parameter(app, cmd_context, par_range_end, pos);
    exec_command(cmd_context, cmdid_auto_tab_range);
    
    app->refresh_file_view(cmd_context, &view);
    range = get_range(&view);
    pos = range.max;
    
    app->buffer_replace_range(cmd_context, &buffer, pos, pos, text2, size2);
    
    push_parameter(app, cmd_context, par_range_start, pos);
    push_parameter(app, cmd_context, par_range_end, pos);
    exec_command(cmd_context, cmdid_auto_tab_range);
}

CUSTOM_COMMAND_SIG(backspace_word){
    File_View_Summary view;
    Buffer_Summary buffer;
    int pos2, pos1;
    
    view = app->get_active_file_view(cmd_context);
    
    pos2 = view.cursor.pos;
    exec_command(cmd_context, cmdid_seek_alphanumeric_left);
    app->refresh_file_view(cmd_context, &view);
    pos1 = view.cursor.pos;
    
    buffer = app->get_buffer(cmd_context, view.file_id);
    app->buffer_replace_range(cmd_context, &buffer, pos1, pos2, 0, 0);
}

CUSTOM_COMMAND_SIG(switch_to_compilation){
    File_View_Summary view;
    Buffer_Summary buffer;
    
    char name[] = "*compilation*";
    int name_size = sizeof(name)-1;

    // TODO(allen): This will only work for file views for now.  Extend the API
    // a bit to handle a general view type which can be manipulated at least enough
    // to change the specific type of view and set files even when the view didn't
    // contain a file.
    view = app->get_active_file_view(cmd_context);
    buffer = app->get_buffer_by_name(cmd_context, make_string(name, name_size));
    
    app->view_set_file(cmd_context, &view, buffer.file_id);
}

CUSTOM_COMMAND_SIG(move_up_10){
    File_View_Summary view;
    float x, y;

    view = app->get_active_file_view(cmd_context);
    x = view.preferred_x;
    
    if (view.unwrapped_lines){
        y = view.cursor.unwrapped_y;
    }
    else{
        y = view.cursor.wrapped_y;
    }
    
    y -= 10*view.line_height;

    app->view_set_cursor(cmd_context, &view, seek_xy(x, y, 0, view.unwrapped_lines), 0);
}

CUSTOM_COMMAND_SIG(move_down_10){
    File_View_Summary view;
    float x, y;

    view = app->get_active_file_view(cmd_context);
    x = view.preferred_x;
    
    if (view.unwrapped_lines){
        y = view.cursor.wrapped_y;
    }
    else{
        y = view.cursor.wrapped_y;
    }
    
    y += 10*view.line_height;
    
    app->view_set_cursor(cmd_context, &view, seek_xy(x, y, 0, view.unwrapped_lines), 0);
}

CUSTOM_COMMAND_SIG(switch_to_file_in_quotes){
    File_View_Summary view;
    Buffer_Summary buffer;
    char short_file_name[128];
    int pos, start, end, size;
    
    view = app->get_active_file_view(cmd_context);
    if (view.exists){
        buffer = app->get_buffer(cmd_context, view.file_id);
        if (buffer.ready){
            pos = view.cursor.pos;
            app->buffer_seek_delimiter(cmd_context, &buffer, pos, '"', 1, &end);
            app->buffer_seek_delimiter(cmd_context, &buffer, pos, '"', 0, &start);
            
            ++start;
            
            size = end - start;
            if (size < sizeof(short_file_name)){
                char file_name_[256];
                String file_name = make_fixed_width_string(file_name_);
                
                app->buffer_read_range(cmd_context, &buffer, start, end, short_file_name);
                
                copy(&file_name, make_string(buffer.file_name, buffer.file_name_len));
                truncate_to_path_of_directory(&file_name);
                append(&file_name, make_string(short_file_name, size));
                
                buffer = app->get_buffer_by_name(cmd_context, file_name);
                exec_command(cmd_context, cmdid_change_active_panel);
                view = app->get_active_file_view(cmd_context);
                if (buffer.exists){
                    app->view_set_file(cmd_context, &view, buffer.file_id);
                }
                else{
                    push_parameter(app, cmd_context, par_name, expand_str(file_name));
                    exec_command(cmd_context, cmdid_interactive_open);
                }
            }
        }
    }
}

CUSTOM_COMMAND_SIG(open_in_other){
    exec_command(cmd_context, cmdid_change_active_panel);
    exec_command(cmd_context, cmdid_interactive_open);
}

CUSTOM_COMMAND_SIG(open_my_files){
    // NOTE(allen|a3.1): The command cmdid_interactive_open can now open
    // a file specified on the parameter stack.  If the file does not exist
    // cmdid_interactive_open behaves as usual.
    push_parameter(app, cmd_context, par_name, literal("w:/4ed/data/test/basic.cpp"));
    exec_command(cmd_context, cmdid_interactive_open);

    exec_command(cmd_context, cmdid_change_active_panel);

    char my_file[256];
    int my_file_len;

    my_file_len = sizeof("w:/4ed/data/test/basic.txt") - 1;
    for (int i = 0; i < my_file_len; ++i){
        my_file[i] = ("w:/4ed/data/test/basic.txt")[i];
    }

    // NOTE(allen|a3.1): null terminators are not needed for strings.
    push_parameter(app, cmd_context, par_name, my_file, my_file_len);
    exec_command(cmd_context, cmdid_interactive_open);

    exec_command(cmd_context, cmdid_change_active_panel);
}

CUSTOM_COMMAND_SIG(build_at_launch_location){
    // NOTE(allen|a3.3): An example of calling build by setting all
    // parameters directly. This only works if build.bat can be called
    // from the starting directory
    push_parameter(app, cmd_context, par_cli_overlap_with_conflict, 1);
    push_parameter(app, cmd_context, par_name, literal("*compilation*"));
    push_parameter(app, cmd_context, par_cli_path, literal("."));
    push_parameter(app, cmd_context, par_cli_command, literal("build"));
    exec_command(cmd_context, cmdid_build);
}

CUSTOM_COMMAND_SIG(build_search){
    // NOTE(allen|a3.3): An example of traversing the filesystem through parent
    // directories looking for a file, in this case a batch file to execute.
    //
    //
    // Step 1: push_directory returns a String containing the current "hot" directory
    // (whatever directory you most recently visited in the 4coder file browsing interface)
    //
    // Step 2: app->directory_has_file queries the file system to see if "build.bat" exists
    // If it does exist several parameters are pushed:
    //   - par_cli_overlap_with_conflict: whether to launch this process if an existing process
    //     is already being used for output on the same buffer
    //
    //   - par_name: the name of the buffer to fill with the output from the process
    //
    //   - par_cli_path: sets the path from which the command is executed
    //
    //   - par_cli_command: sets the actual command to be executed, this can be almost any command
    //     that you could execute through a command line interface
    //
    //
    //     To set par_cli_path: push_parameter makes a copy of the dir string on the stack
    //         because the string allocated by push_directory is going to change again
    //     To set par_cli_command: app->push_parameter does not make a copy of the dir because
    //         dir isn't going to change again.
    // 
    // Step 3: If the batch file did not exist try to move to the parent directory using
    // app->directory_cd. The cd function can also be used to navigate to subdirectories.
    // It returns true if it can actually move in the specified direction, and false otherwise.
    
    int keep_going = 1;
    String dir = push_directory(app, cmd_context);
    while (keep_going){
        if (app->directory_has_file(dir, "build.bat")){
            push_parameter(app, cmd_context, par_cli_overlap_with_conflict, 0);
            push_parameter(app, cmd_context, par_name, literal("*compilation*"));
            push_parameter(app, cmd_context, par_cli_path, dir.str, dir.size);
            
            if (append(&dir, "build")){
#if 1
                // NOTE(allen): This version avoids an unecessary copy, both equivalents are
                // included to demonstrate how using push_parameter without the helper looks.
                app->push_parameter(cmd_context,
                    dynamic_int(par_cli_command),
                    dynamic_string(dir.str, dir.size));
#else
                push_parameter(cmd_context, par_cli_command, dir.str, dir.size);
#endif
                
                exec_command(cmd_context, cmdid_build);
            }
            else{
                clear_parameters(cmd_context);
            }
            
            return;
        }

        if (app->directory_cd(&dir, "..") == 0){
            keep_going = 0;
        }
    }

    // TODO(allen): feedback message - couldn't find build.bat
}

CUSTOM_COMMAND_SIG(write_and_auto_tab){
    exec_command(cmd_context, cmdid_write_character);
    exec_command(cmd_context, cmdid_auto_tab_line_at_cursor);
}

extern "C" GET_BINDING_DATA(get_bindings){
    Bind_Helper context_actual = begin_bind_helper(data, size);
    Bind_Helper *context = &context_actual;

    // NOTE(allen|a3.1): Right now hooks have no loyalties to maps, all hooks are
    // global and once set they always apply, regardless of what map is active.
    set_hook(context, hook_start, my_start);
    set_hook(context, hook_open_file, my_file_settings);

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
    bind(context, 'x', MDFR_ALT, cmdid_open_menu);
    bind(context, 'o', MDFR_ALT, open_in_other);

    // NOTE(allen): These callbacks may not actually be useful to you, but
    // go look at them and see what they do.
    bind(context, 'M', MDFR_ALT | MDFR_CTRL, open_my_files);
    bind(context, 'M', MDFR_ALT, build_at_launch_location);
    bind(context, 'm', MDFR_ALT, build_search);

    end_map(context);


    begin_map(context, my_code_map);

    // NOTE(allen|a3.1): Set this map (my_code_map == mapid_user_custom) to
    // inherit from mapid_file.  When searching if a key is bound
    // in this map, if it is not found here it will then search mapid_file.
    //
    // If this is not set, it defaults to mapid_global.
    inherit_map(context, mapid_file);

    // NOTE(allen|a3.1): Children can override parent's bindings.
    bind(context, key_right, MDFR_CTRL, cmdid_seek_alphanumeric_or_camel_right);
    bind(context, key_left, MDFR_CTRL, cmdid_seek_alphanumeric_or_camel_left);

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
    bind(context, '\t', MDFR_CTRL, cmdid_auto_tab_range);
    bind(context, '\t', MDFR_SHIFT, cmdid_auto_tab_line_at_cursor);

    bind(context, '\n', MDFR_SHIFT, write_and_auto_tab);
    bind(context, ' ', MDFR_SHIFT, cmdid_write_character);
    
    bind(context, '=', MDFR_CTRL, write_increment);
    bind(context, '-', MDFR_CTRL, write_decrement);
    bind(context, '[', MDFR_CTRL, open_long_braces);
    bind(context, 'i', MDFR_ALT, ifdef_off);
    bind(context, '1', MDFR_ALT, switch_to_file_in_quotes);
    
    end_map(context);
    
    
    begin_map(context, mapid_file);
    
    // NOTE(allen|a3.1): Binding this essentially binds all key combos that
    // would normally insert a character into a buffer.
    // Or apply this rule (which always works): if the code for the key
    // is not in the codes struct, it is a vanilla key.
    // It is possible to override this binding for individual keys.
    bind_vanilla_keys(context, cmdid_write_character);
    
    bind(context, key_left, MDFR_NONE, cmdid_move_left);
    bind(context, key_right, MDFR_NONE, cmdid_move_right);
    bind(context, key_del, MDFR_NONE, cmdid_delete);
    bind(context, key_back, MDFR_NONE, cmdid_backspace);
    bind(context, key_up, MDFR_NONE, cmdid_move_up);
    bind(context, key_down, MDFR_NONE, cmdid_move_down);
    bind(context, key_end, MDFR_NONE, cmdid_seek_end_of_line);
    bind(context, key_home, MDFR_NONE, cmdid_seek_beginning_of_line);
    bind(context, key_page_up, MDFR_NONE, cmdid_page_up);
    bind(context, key_page_down, MDFR_NONE, cmdid_page_down);
    
    bind(context, key_right, MDFR_CTRL, cmdid_seek_whitespace_right);
    bind(context, key_left, MDFR_CTRL, cmdid_seek_whitespace_left);
    bind(context, key_up, MDFR_CTRL, cmdid_seek_whitespace_up);
    bind(context, key_down, MDFR_CTRL, cmdid_seek_whitespace_down);
    
    bind(context, key_up, MDFR_ALT, move_up_10);
    bind(context, key_down, MDFR_ALT, move_down_10);
    
    bind(context, key_back, MDFR_CTRL, backspace_word);
    
    bind(context, ' ', MDFR_CTRL, cmdid_set_mark);
    bind(context, 'm', MDFR_CTRL, cmdid_cursor_mark_swap);
    bind(context, 'c', MDFR_CTRL, cmdid_copy);
    bind(context, 'x', MDFR_CTRL, cmdid_cut);
    bind(context, 'v', MDFR_CTRL, cmdid_paste);
    bind(context, 'V', MDFR_CTRL, cmdid_paste_next);
    bind(context, 'Z', MDFR_CTRL, cmdid_timeline_scrub);
    bind(context, 'z', MDFR_CTRL, cmdid_undo);
    bind(context, 'y', MDFR_CTRL, cmdid_redo);
    bind(context, key_left, MDFR_ALT, cmdid_increase_rewind_speed);
    bind(context, key_right, MDFR_ALT, cmdid_increase_fastforward_speed);
    bind(context, key_down, MDFR_ALT, cmdid_stop_rewind_fastforward);
    bind(context, 'h', MDFR_CTRL, cmdid_history_backward);
    bind(context, 'H', MDFR_CTRL, cmdid_history_forward);
    bind(context, 'd', MDFR_CTRL, cmdid_delete_range);
    bind(context, 'l', MDFR_CTRL, cmdid_toggle_line_wrap);
    bind(context, 'L', MDFR_CTRL, cmdid_toggle_endline_mode);
    bind(context, 'u', MDFR_CTRL, cmdid_to_uppercase);
    bind(context, 'j', MDFR_CTRL, cmdid_to_lowercase);
    bind(context, '?', MDFR_CTRL, cmdid_toggle_show_whitespace);
    
    bind(context, '~', MDFR_CTRL, cmdid_clean_all_lines);
    bind(context, '1', MDFR_CTRL, cmdid_eol_dosify);
    bind(context, '!', MDFR_CTRL, cmdid_eol_nixify);
    
    bind(context, 'f', MDFR_CTRL, cmdid_search);
    bind(context, 'r', MDFR_CTRL, cmdid_reverse_search);
    bind(context, 'g', MDFR_CTRL, cmdid_goto_line);
    
    bind(context, 'K', MDFR_CTRL, cmdid_kill_buffer);
    bind(context, 'O', MDFR_CTRL, cmdid_reopen);
    bind(context, 'w', MDFR_CTRL, cmdid_interactive_save_as);
    bind(context, 's', MDFR_CTRL, cmdid_save);
    
    bind(context, ',', MDFR_ALT, switch_to_compilation);
    
    end_map(context);
    end_bind_helper(context);
    
    return context->write_total;
}


