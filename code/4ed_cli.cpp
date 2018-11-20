/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 17.07.2017
 *
 * CLI handling code.
 *
 */

// TOP

inline CLI_List
make_cli_list(Partition *part, u32 max){
    CLI_List list = {};
    partition_align(part, 8);
    list.procs = push_array(part, CLI_Process, max);
    list.max = max;
    return(list);
}

inline b32
cli_list_call(System_Functions *system, CLI_List *list, char *path, char *command, Editing_File *file, b32 cursor_at_end){
    b32 result = false;
    
    if (list->count < list->max){
        CLI_Process *proc = &list->procs[list->count++];
        proc->out_file = file;
        proc->cursor_at_end = cursor_at_end;
        result = system->cli_call(path, command, &proc->cli);
        if (!result){
            --list->count;
        }
    }
    
    return(result);
}

inline void
cli_list_free_proc(CLI_List *list, CLI_Process *proc){
    Assert(proc >= list->procs);
    Assert(proc < list->procs + list->count);
    *proc = list->procs[--list->count];
}

inline b32
cli_list_has_space(CLI_List *list){
    b32 has_space = (list->count < list->max);
    return(has_space);
}

// BOTTOM

