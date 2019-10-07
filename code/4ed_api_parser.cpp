/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 03.10.2019
 *
 * Parser that extracts an API from C++ source code.
 *
 */

// TOP

/*
function:
api ( <identifier> ) function <identifier> {*} <identifier> ( <param_list> )

param_list:
void |
<identifier> {*} <identifier> [, <identifier> {*} <identifier>]

anything_else:
***

api_source:
{function|anything_else} EOF
*/

function b32
api_parse__match(Token_Iterator *it, Token_Cpp_Kind sub_kind){
    b32 match = false;
    Token *token = token_it_read(it);
    if (token->sub_kind == sub_kind){
        if (token_it_inc(it)){
            match = true;
        }
    }
    return(match);
}

function b32
api_parse__match_identifier(Token_Iterator *it, String_Const_u8 source, String_Const_u8 *lexeme){
    b32 match = false;
    Token *token = token_it_read(it);
    if (token->kind == TokenBaseKind_Identifier ||
        token->kind == TokenBaseKind_Keyword){
        if (token_it_inc(it)){
            *lexeme = string_substring(source, Ii64(token));
            match = true;
        }
    }
    return(match);
}

function b32
api_parse__match_identifier(Token_Iterator *it, String_Const_u8 source, char *lexeme){
    b32 match = false;
    Token *token = token_it_read(it);
    if ((token->kind == TokenBaseKind_Identifier ||
         token->kind == TokenBaseKind_Keyword) &&
        string_match(SCu8(lexeme), string_substring(source, Ii64(token)))){
        if (token_it_inc(it)){
            match = true;
        }
    }
    return(match);
}

function String_Const_u8
api_parse__type_name_with_stars(Arena *arena, String_Const_u8 type, i32 star_counter){
    if (star_counter > 0){
        i32 type_full_size = type.size + star_counter;
        u8 *type_buffer = push_array(arena, u8, type_full_size + 1);
        block_copy(type_buffer, type.str, type.size);
        block_fill_u8(type_buffer + type.size, star_counter, (u8)'*');
        type_buffer[type_full_size] = 0;
        type = SCu8(type_buffer, type_full_size);
    }
    return(type);
}

function void
api_parse_add_param(Arena *arena, API_Param_List *list, String_Const_u8 type, i32 star_counter, String_Const_u8 name){
    type = api_parse__type_name_with_stars(arena, type, star_counter);
    API_Param *param = push_array(arena, API_Param, 1);
    sll_queue_push(list->first, list->last, param);
    list->count += 1;
    param->type_name = type;
    param->name = name;
}

function void
api_parse_add_function(Arena *arena, API_Definition_List *list,
                       String_Const_u8 api_name, String_Const_u8 func_name,
                       String_Const_u8 type, i32 star_counter, API_Param_List param_list,
                       String_Const_u8 location){
    API_Definition *api = api_get_api(arena, list, api_name);
    type = api_parse__type_name_with_stars(arena, type, star_counter);
    API_Call *call = api_call_with_location(arena, api, func_name, type, location);
    api_set_param_list(call, param_list);
}

function String_Const_u8
api_parse_location(Arena *arena, String_Const_u8 source_name, String_Const_u8 source, u8 *pos){
    i32 line_number = 1;
    i32 col_number = 1;
    if (source.str <= pos && pos < source.str + source.size){
        for (u8 *ptr = source.str;;){
            if (ptr == pos){
                break;
            }
            if (*ptr == '\n'){
                line_number += 1;
                col_number = 1;
            }
            else{
                col_number += 1;
            }
            ptr += 1;
        }
    }
    return(push_u8_stringf(arena, "%.*s:%d:%d:", string_expand(source_name), line_number, col_number));
}

function void
api_parse_source_add_to_list(Arena *arena, String_Const_u8 source_name, String_Const_u8 source,
                             API_Definition_List *list){
    Token_List token_list = lex_full_input_cpp(arena, source);
    Token_Iterator token_it = token_iterator(token_iterator(0, &token_list));
    
    for (;;){
        Token *token = token_it_read(&token_it);
        if (token->sub_kind == TokenCppKind_EOF){
            break;
        }
        
        if (api_parse__match_identifier(&token_it, source, "api")){
            String_Const_u8 api_name = {};
            String_Const_u8 ret_type = {};
            i32 ret_type_star_counter = 0;
            String_Const_u8 func_name = {};
            API_Param_List param_list = {};
            
            b32 success = false;
            if (api_parse__match(&token_it, TokenCppKind_ParenOp)){
                if (api_parse__match_identifier(&token_it, source, &api_name)){
                    if (api_parse__match(&token_it, TokenCppKind_ParenCl)){
                        if (api_parse__match_identifier(&token_it, source, "function")){
                            if (api_parse__match_identifier(&token_it, source, &ret_type)){
                                for (;api_parse__match(&token_it, TokenCppKind_Star);){
                                    ret_type_star_counter += 1;
                                }
                                if (api_parse__match_identifier(&token_it, source, &func_name)){
                                    if (api_parse__match(&token_it, TokenCppKind_ParenOp)){
                                        b32 param_list_success = false;
                                        if (api_parse__match_identifier(&token_it, source, "void")){
                                            param_list_success = true;
                                        }
                                        else{
                                            for (;;){
                                                String_Const_u8 type = {};
                                                i32 star_counter = 0;
                                                String_Const_u8 name = {};
                                                if (api_parse__match_identifier(&token_it, source, &type)){
                                                    for (;api_parse__match(&token_it, TokenCppKind_Star);){
                                                        star_counter += 1;
                                                    }
                                                    if (api_parse__match_identifier(&token_it, source, &name)){
                                                        param_list_success = true;
                                                    }
                                                    else{
                                                        break;
                                                    }
                                                }
                                                else{
                                                    break;
                                                }
                                                if (param_list_success){
                                                    api_parse_add_param(arena, &param_list, type, star_counter, name);
                                                }
                                                if (api_parse__match(&token_it, TokenCppKind_Comma)){
                                                    param_list_success = false;
                                                }
                                                else{
                                                    break;
                                                }
                                            }
                                        }
                                        if (param_list_success){
                                            if (api_parse__match(&token_it, TokenCppKind_ParenCl)){
                                                success = true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            if (success){
                String_Const_u8 location = api_parse_location(arena, source_name, source, func_name.str);
                api_parse_add_function(arena, list, api_name, func_name,
                                       ret_type, ret_type_star_counter, param_list,
                                       location);
            }
        }
        else{
            if (!token_it_inc(&token_it)){
                break;
            }
        }
    }
}

function API_Definition_List
api_parse_source(Arena *arena, String_Const_u8 source_name, String_Const_u8 source){
    API_Definition_List list = {};
    api_parse_source_add_to_list(arena, source_name, source, &list);
    return(list);
}

// BOTTOM

