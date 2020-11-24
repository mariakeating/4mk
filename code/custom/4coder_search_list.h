/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 01.10.2019
 *
 * Search list helper.
 *
 */

// TOP

#if !defined(FRED_SEARCH_LIST_H)
#define FRED_SEARCH_LIST_H

////////////////////////////////
// NOTE(allen): Search List Builders

function void def_search_add_path(Arena *arena, List_String_Const_u8 *list, String_Const_u8 path);
function void def_search_add_system_path(Arena *arena, List_String_Const_u8 *list, System_Path_Code path);

global String_Const_u8 def_search_project_path = {};

function void def_search_normal_load_list(Arena *arena, List_String_Const_u8 *list);

////////////////////////////////
// NOTE(allen): Search List Functions

function String_Const_u8 def_search_get_full_path(Arena *arena, List_String_Const_u8 *list, String_Const_u8 file_name);

#if OS_LINUX
#include <stdio.h>
#endif

function FILE *def_search_fopen(Arena *arena, List_String_Const_u8 *list, char *file_name, char *opt);
function FILE *def_search_normal_fopen(Arena *arena, char *file_name, char *opt);

#endif

// BOTTOM

