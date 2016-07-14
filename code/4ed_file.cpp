/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 20.02.2016
 *
 * File editing view for 4coder
 *
 */

// TOP

#include "buffer/4coder_shared.cpp"

#if BUFFER_EXPERIMENT_SCALPEL == 0
#include "buffer/4coder_golden_array.cpp"
#define Buffer_Type Buffer
#elif BUFFER_EXPERIMENT_SCALPEL == 1
#include "buffer/4coder_gap_buffer.cpp"
#define Buffer_Type Gap_Buffer
#elif BUFFER_EXPERIMENT_SCALPEL == 2
#include "buffer/4coder_multi_gap_buffer.cpp"
#define Buffer_Type Multi_Gap_Buffer
#else
#include "buffer/4coder_rope_buffer.cpp"
#define Buffer_Type Rope_Buffer
#endif

#include "buffer/4coder_buffer_abstract.cpp"

enum Edit_Pos_Set_Type{
    EditPos_None,
    EditPos_CursorSet,
    EditPos_ScrollSet
};
struct File_Edit_Positions{
    GUI_Scroll_Vars scroll;
    Full_Cursor cursor;
    i32 mark;
    f32 preferred_x;
    i32 scroll_i;
    i32 last_set_type;
    b32 in_view;
};
inline File_Edit_Positions
file_edit_positions_zero(){
    File_Edit_Positions data = {0};
    return(data);
}

enum Edit_Type{
    ED_NORMAL,
    ED_REVERSE_NORMAL,
    ED_UNDO,
    ED_REDO,
};

struct Edit_Step{
    Edit_Type type;
    union{
        struct{
            b32 can_merge;
            Buffer_Edit edit;
            i32 next_block;
            i32 prev_block;
        };
        struct{
            i32 first_child;
            i32 inverse_first_child;
            i32 inverse_child_count;
            i32 special_type;
        };
    };
    i32 child_count;
};

struct Edit_Stack{
    u8 *strings;
    i32 size, max;
    
    Edit_Step *edits;
    i32 edit_count, edit_max;
};

struct Small_Edit_Stack{
    u8 *strings;
    i32 size, max;
    
    Buffer_Edit *edits;
    i32 edit_count, edit_max;
};

struct Undo_Data{
    Edit_Stack undo;
    Edit_Stack redo;
    Edit_Stack history;
    Small_Edit_Stack children;
    
    i32 history_block_count, history_head_block;
    i32 edit_history_cursor;
    b32 current_block_normal;
};

enum File_Sync_State{
    SYNC_GOOD,
    SYNC_BEHIND_OS,
    SYNC_UNSAVED
};

struct Text_Effect{
    i32 start, end;
    u32 color;
    f32 seconds_down, seconds_max;
};

// NOTE(allen): The Editing_File struct is now divided into two
// parts.  Variables in the Settings part can be set even when the
// file is still streaming in, and persists thorugh all operations except
// for the initial allocation of the file.
struct Editing_File_Settings{
    i32 base_map_id;
    i32 dos_write_mode;
    i16 font_id;
    b8 unwrapped_lines;
    b8 tokens_exist;
    b8 is_initialized;
    b8 unimportant;
    b8 read_only;
    b8 never_kill;
};

// NOTE(allen): This part of the Editing_File is cleared whenever
// the contents of the file is set.
struct Editing_File_State{
    Buffer_Type buffer;
    
    Undo_Data undo;
    
    Cpp_Token_Stack token_stack;
    Cpp_Token_Stack swap_stack;
    u32 lex_job;
    b32 tokens_complete;
    b32 still_lexing;
    
    Text_Effect paste_effect;
    
    File_Sync_State sync;
    u64 last_4ed_write_time;
    u64 last_4ed_edit_time;
    u64 last_sys_write_time;
    
    File_Edit_Positions edit_pos_space[16];
    File_Edit_Positions *edit_poss[16];
    i32 edit_poss_count;
};

struct Editing_File_Name{
    char live_name_[256];
    char source_path_[256];
    char extension_[16];
    String live_name;
    String source_path;
    String extension;
};

struct File_Node{
    File_Node *next, *prev;
};

union Buffer_Slot_ID{
    i32 id;
    i16 part[2];
};

inline Buffer_Slot_ID
to_file_id(i32 id){
    Buffer_Slot_ID result;
    result.id = id;
    return(result);
}

struct Editing_File{
    // NOTE(allen): node must be the first member of Editing_File!
    File_Node node;
    Editing_File_Settings settings;
    struct{
        b32 is_loading;
        b32 is_dummy;
        Editing_File_State state;
    };
    Editing_File_Name name;
    Buffer_Slot_ID id;
    u64 unique_buffer_id;
};

struct Non_File_Table_Entry{
    String name;
    Buffer_Slot_ID id;
};

struct File_Array{
    Editing_File *files;
    i32 size;
};

struct Working_Set{
    File_Array *file_arrays;
    i32 file_count, file_max;
    i16 array_count, array_max;
    
	File_Node free_sentinel;
    File_Node used_sentinel;
    
    Table table;
    
	String clipboards[64];
	i32 clipboard_size, clipboard_max_size;
	i32 clipboard_current, clipboard_rolling;
    
    u64 unique_file_counter;
};

struct File_Entry{
    String short_name;
    String long_name;
    Buffer_Slot_ID id;
};

struct File_Entry_Comparison{
    File_Entry entry;
    Unique_Hash hash;
    b32 use_hash;
};

internal i32
tbl_file_compare(void *a, void *b, void *arg){
    File_Entry_Comparison *fa = (File_Entry_Comparison*)a;
    File_Entry *fb = (File_Entry*)b;
    System_Functions *system = (System_Functions*)arg;
    Unique_Hash uhash;
    i32 result = 1;
    b32 success;
    
    if (fa->use_hash){
        uhash = system->file_unique_hash(fb->long_name, &success);
        if (success && uhash_equal(uhash, fa->hash)){
            result = 0;
		}
	}
    else{
        if (match(fa->entry.short_name, fb->short_name)){
            result = 0;
		}
	}
    
    return(result);
}

//
// File_Edit_Positions stuff
//

internal void
edit_pos_set_cursor_(File_Edit_Positions *edit_pos,
                     Full_Cursor cursor,
                     b32 set_preferred_x,
                     b32 unwrapped_lines){
    edit_pos->cursor = cursor;
    if (set_preferred_x){
        edit_pos->preferred_x = cursor.wrapped_x;
        if (unwrapped_lines){
            edit_pos->preferred_x = cursor.unwrapped_x;
        }
    }
    edit_pos->last_set_type = EditPos_CursorSet;
}

internal void
edit_pos_set_scroll_(File_Edit_Positions *edit_pos, GUI_Scroll_Vars scroll){
    edit_pos->scroll = scroll;
    edit_pos->last_set_type = EditPos_ScrollSet;
}

internal i32
edit_pos_get_index(Editing_File *file, File_Edit_Positions *edit_pos){
    i32 edit_pos_index = -1;
    
    i32 count = file->state.edit_poss_count;
    File_Edit_Positions **edit_poss = file->state.edit_poss;
    for (i32 i = 0; i < count; ++i){
        if (edit_poss[i] == edit_pos){
            edit_pos_index = i;
            break;
        }
    }
    
    return(edit_pos_index);
}

internal b32
edit_pos_move_to_front(Editing_File *file, File_Edit_Positions *edit_pos){
    b32 result = false;
    
    if (file && edit_pos){
        i32 edit_pos_index = edit_pos_get_index(file, edit_pos);
        Assert(edit_pos_index != -1);
        
        File_Edit_Positions **edit_poss = file->state.edit_poss;
        
        memmove(edit_poss + 1, edit_poss, edit_pos_index*sizeof(*edit_poss));
        
        edit_poss[0] = edit_pos;
        result = true;
    }
    
    return(result);
}

internal b32
edit_pos_unset(Editing_File *file, File_Edit_Positions *edit_pos){
    b32 result = false;
    
    if (file && edit_pos){
        i32 edit_pos_index = edit_pos_get_index(file, edit_pos);
        Assert(edit_pos_index != -1);
        
        i32 count = file->state.edit_poss_count;
        File_Edit_Positions **edit_poss = file->state.edit_poss;
        
        memmove(edit_poss + edit_pos_index,
                edit_poss + edit_pos_index + 1,
                (count - edit_pos_index - 1)*sizeof(*edit_poss));
        
        edit_pos->in_view = false;
        
        if (file->state.edit_poss_count > 1){
            file->state.edit_poss_count -= 1;
        }
        result = true;
    }
    
    return(result);
}

internal File_Edit_Positions*
edit_pos_get_new(Editing_File *file, i32 index){
    File_Edit_Positions *result = 0;
    
    if (file && 0 <= index && index < 16){
        result = file->state.edit_pos_space + index;
        i32 edit_pos_index = edit_pos_get_index(file, result);
        
        if (edit_pos_index == -1){
            File_Edit_Positions **edit_poss = file->state.edit_poss;
            i32 count = file->state.edit_poss_count;
            
            if (count > 0){
                if (edit_poss[0]->in_view){
                    memcpy(result, edit_poss[0], sizeof(*result));
                    memmove(edit_poss+1, edit_poss, sizeof(*edit_poss)*count);
                    file->state.edit_poss_count = count + 1;
                }
                else{
                    Assert(count == 1);
                    memcpy(result, edit_poss[0], sizeof(*result));
                }
            }
            else{
                memset(result, 0, sizeof(*result));
                file->state.edit_poss_count = 1;
            }
            
            edit_poss[0] = result;
        }
        
        result->in_view = true;
    }
    
    return(result);
}

//
// Working_Set stuff
//

internal void
working_set_extend_memory(Working_Set *working_set, Editing_File *new_space, i16 number_of_files){
    Buffer_Slot_ID id;
    i16 i, high_part;
    Editing_File *file_ptr;
    File_Node *free_sentinel;
    
    Assert(working_set->array_count < working_set->array_max);
    
    high_part = working_set->array_count++;
    working_set->file_arrays[high_part].files = new_space;
    working_set->file_arrays[high_part].size = number_of_files;
    
    working_set->file_max += number_of_files;
    
    id.part[1] = high_part;
    
    file_ptr = new_space;
    free_sentinel = &working_set->free_sentinel;
    for (i = 0; i < number_of_files; ++i, ++file_ptr){
        id.part[0] = i;
        file_ptr->id = id;
        dll_insert(free_sentinel, &file_ptr->node);
    }
}

inline Editing_File
editing_file_zero(){
    Editing_File file = {0};
    return(file);
}

internal Editing_File*
working_set_alloc(Working_Set *working_set){
    Editing_File *result = 0;
    File_Node *node;
    Buffer_Slot_ID id;
    
    if (working_set->file_count < working_set->file_max){
        node = working_set->free_sentinel.next;
        Assert(node != &working_set->free_sentinel);
        result = (Editing_File*)node;
        
        dll_remove(node);
        id = result->id;
        *result = editing_file_zero();
        result->id = id;
        result->unique_buffer_id = ++working_set->unique_file_counter;
        dll_insert(&working_set->used_sentinel, node);
        ++working_set->file_count;
    }
    
    return result;
}

internal Editing_File*
working_set_alloc_always(Working_Set *working_set, General_Memory *general){
    Editing_File *result = 0;
    Editing_File *new_chunk;
    i32 full_new_count = working_set->file_max;
    i16 new_count;
    
    if (full_new_count > max_i16) new_count = max_i16;
    else new_count = (i16)full_new_count;
    
    if (working_set->file_count == working_set->file_max &&
            working_set->array_count < working_set->array_max){
        new_chunk = gen_array(general, Editing_File, new_count);
        working_set_extend_memory(working_set, new_chunk, new_count);
    }
    result = working_set_alloc(working_set);
    
    return(result);
}

inline void
working_set_free_file(Working_Set  *working_set, Editing_File *file){
    file->is_dummy = 1;
    dll_remove(&file->node);
    dll_insert(&working_set->free_sentinel, &file->node);
    --working_set->file_count;
}

inline Editing_File*
working_set_index(Working_Set *working_set, Buffer_Slot_ID id){
    Editing_File *result = 0;
    File_Array *array;
    
    if (id.part[1] >= 0 && id.part[1] < working_set->array_count){
        array = working_set->file_arrays + id.part[1];
        if (id.part[0] >= 0 && id.part[0] < array->size){
            result = array->files + id.part[0];
        }
    }
    
    return(result);
}

inline Editing_File*
working_set_index(Working_Set *working_set, i32 id){
    Editing_File *result;
    result = working_set_index(working_set, to_file_id(id));
    return(result);
}

inline Editing_File*
working_set_get_active_file(Working_Set *working_set, Buffer_Slot_ID id){
    Editing_File *result = 0;
    result = working_set_index(working_set, id);
    if (result && result->is_dummy){
        result = 0;
    }
    return(result);
}

inline Editing_File*
working_set_get_active_file(Working_Set *working_set, i32 id){
    Editing_File *result;
    result = working_set_get_active_file(working_set, to_file_id(id));
    return(result);
}

internal void
working_set_init(Working_Set *working_set, Partition *partition, General_Memory *general){
    Editing_File *files, *null_file;
    void *mem;
    i32 mem_size, table_size;
    
    i32 item_size = sizeof(File_Entry);
    i16 init_count = 16;
    i16 array_init_count = 256;
    
    dll_init_sentinel(&working_set->free_sentinel);
    dll_init_sentinel(&working_set->used_sentinel);

    working_set->array_max = array_init_count;
    working_set->file_arrays = push_array(partition, File_Array, array_init_count);

    files = push_array(partition, Editing_File, init_count);
    working_set_extend_memory(working_set, files, init_count);

    null_file = working_set_index(working_set, 0);
    dll_remove(&null_file->node);
    null_file->is_dummy = 1;
    ++working_set->file_count;

    table_size = working_set->file_max;
    mem_size = table_required_mem_size(table_size, item_size);
    mem = general_memory_allocate(general, mem_size);
    memset(mem, 0, mem_size);
    table_init_memory(&working_set->table, mem, table_size, item_size);
}

inline void
working_set__grow_if_needed(Table *table, General_Memory *general, void *arg, Hash_Function *hash_func, Compare_Function *comp_func){
    Table btable;
    i32 new_max, mem_size;
    void *mem;
    
    if (table_at_capacity(table)){
        new_max = table->max * 2;
        mem_size = table_required_mem_size(new_max, table->item_size);
        mem = general_memory_allocate(general, mem_size);
        table_init_memory(&btable, mem, new_max, table->item_size);
        table_clear(&btable);
        table_rehash(table, &btable, 0, hash_func, comp_func);
        general_memory_free(general, table->hash_array);
        *table = btable;
    }
}

inline void
working_set__entry_comp(System_Functions *system, String filename, File_Entry_Comparison *out){
    out->entry.long_name = filename;
    out->entry.short_name = front_of_directory(filename);
    out->hash = system->file_unique_hash(filename, &out->use_hash);
}

inline Editing_File*
working_set_contains(System_Functions *system, Working_Set *working_set, String filename){
    File_Entry_Comparison entry_comp = {0};
    File_Entry *entry = 0;
    Editing_File *result = 0;
    working_set__entry_comp(system, filename, &entry_comp);
    entry = (File_Entry*)table_find_item(&working_set->table, &entry_comp, system, tbl_string_hash, tbl_file_compare);
    if (entry){
        result = working_set_index(working_set, entry->id);
    }
    return(result);
}

inline void
working_set_add(System_Functions *system, Working_Set *working_set, Editing_File *file, General_Memory *general){
    File_Entry_Comparison entry_comp;
    working_set__grow_if_needed(&working_set->table, general, system, tbl_string_hash, tbl_file_compare);
    working_set__entry_comp(system, file->name.source_path, &entry_comp);
    entry_comp.entry.id = file->id;
    if (table_add(&working_set->table, &entry_comp, system, tbl_string_hash, tbl_file_compare)){
        if (!uhash_equal(entry_comp.hash, uhash_zero())){
            system->file_track(file->name.source_path);
        }
    }
}

inline void
working_set_remove(System_Functions *system, Working_Set *working_set, String filename){
    File_Entry_Comparison entry_comp;
    working_set__entry_comp(system, filename, &entry_comp);
    table_remove_match(&working_set->table, &entry_comp, system, tbl_string_hash, tbl_file_compare);
    if (!uhash_equal(entry_comp.hash, uhash_zero())){
        system->file_untrack(filename);
    }
}

// TODO(allen): Pick better first options.
internal Editing_File*
working_set_lookup_file(Working_Set *working_set, String string){
    Editing_File *file = 0;
    
    {
        File_Node *node, *used_nodes;
        used_nodes = &working_set->used_sentinel;
        for (dll_items(node, used_nodes)){
            file = (Editing_File*)node;
            if (string.size == 0 || match(string, file->name.live_name)){
                break;
            }
        }
        if (node == used_nodes) file = 0;
    }
	
	if (!file){
        File_Node *node, *used_nodes;
        used_nodes = &working_set->used_sentinel;
        for (dll_items(node, used_nodes)){
            file = (Editing_File*)node;
            if (string.size == 0 || has_substr(file->name.live_name, string)){
                break;
            }
        }
        if (node == used_nodes) file = 0;
	}
    
	return (file);
}

internal void
touch_file(Working_Set *working_set, Editing_File *file){
    if (file){
        Assert(!file->is_dummy);
        dll_remove(&file->node);
        dll_insert(&working_set->used_sentinel, &file->node);
    }
}

// Hot Directory

struct Hot_Directory{
	String string;
	File_List file_list;
    char slash;
};

internal void
hot_directory_clean_end(Hot_Directory *hot_directory){
    String *str = &hot_directory->string;
    if (str->size != 0 && str->str[str->size-1] != hot_directory->slash){
        str->size = reverse_seek_slash(*str) + 1;
        str->str[str->size] = 0;
    }
}

internal i32
hot_directory_quick_partition(File_Info *infos, i32 start, i32 pivot){
    File_Info *p = infos + pivot;
    File_Info *a = infos + start;
    for (i32 i = start; i < pivot; ++i, ++a){
        i32 comp = 0;
        comp = p->folder - a->folder;
        if (comp == 0) comp = compare(a->filename, p->filename);
        if (comp < 0){
            Swap(File_Info, *a, infos[start]);
            ++start;
        }
    }
    Swap(File_Info, *p, infos[start]);
    return start;
}

internal void
hot_directory_quick_sort(File_Info *infos, i32 start, i32 pivot){
    i32 mid = hot_directory_quick_partition(infos, start, pivot);
    if (start < mid-1) hot_directory_quick_sort(infos, start, mid-1);
    if (mid+1 < pivot) hot_directory_quick_sort(infos, mid+1, pivot);
}

inline void
hot_directory_fixup(Hot_Directory *hot_directory, Working_Set *working_set){
    File_List *files = &hot_directory->file_list;
    if (files->count >= 2)
        hot_directory_quick_sort(files->infos, 0, files->count - 1);
}

inline void
hot_directory_set(System_Functions *system, Hot_Directory *hot_directory,
                  String str, Working_Set *working_set){
    b32 success = copy_checked(&hot_directory->string, str);
    terminate_with_null(&hot_directory->string);
    if (success){
        if (str.size > 0){
            system->set_file_list(&hot_directory->file_list, str);
        }
        else{
            system->set_file_list(&hot_directory->file_list, make_string((char*)1, 0));
        }
    }
    hot_directory_fixup(hot_directory, working_set);
}

inline void
hot_directory_reload(System_Functions *system, Hot_Directory *hot_directory, Working_Set *working_set){
    system->set_file_list(&hot_directory->file_list, hot_directory->string);
    hot_directory_fixup(hot_directory, working_set);
}

internal void
hot_directory_init(Hot_Directory *hot_directory, String base, String dir, char slash){
	hot_directory->string = base;
    hot_directory->string.str[255] = 0;
    hot_directory->string.size = 0;
    copy(&hot_directory->string, dir);
	append(&hot_directory->string, slash);
    hot_directory->slash = slash;
}

struct Hot_Directory_Match{
	String filename;
	b32 is_folder;
};

internal b32
filename_match(String query, Absolutes *absolutes, String filename, b32 case_sensitive){
    b32 result;
    result = (query.size == 0);
    if (!result) result = wildcard_match(absolutes, filename, case_sensitive);
    return result;
}

inline File_Sync_State
buffer_get_sync(Editing_File *file){
    File_Sync_State result = SYNC_GOOD;
    if (file->state.last_4ed_write_time != file->state.last_sys_write_time)
        result = SYNC_BEHIND_OS;
    else if (file->state.last_4ed_edit_time > file->state.last_sys_write_time)
        result = SYNC_UNSAVED;
    return result;
}

inline b32
buffer_needs_save(Editing_File *file){
    b32 result = 0;
    if (file->settings.unimportant == 0)
        if (buffer_get_sync(file) == SYNC_UNSAVED)
            result = 1;
    return(result);
}

inline b32
file_is_ready(Editing_File *file){
    b32 result = 0;
    if (file && file->is_loading == 0){
        result = 1;
    }
    return(result);
}

inline Editing_File_State
editing_file_state_zero(){
    Editing_File_State state={0};
    return(state);
}

inline Editing_File_Settings
editing_file_settings_zero(){
    Editing_File_Settings settings={0};
    return(settings);
}

inline void
file_set_to_loading(Editing_File *file){
    file->state = editing_file_state_zero();
    file->settings = editing_file_settings_zero();
    file->is_loading = 1;
}



// BOTTOM

