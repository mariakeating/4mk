/*
 * FSM table generator:
 *  Generate FSM tables as ".c" files from FSM functions.
 *
 * 23.03.2016 (dd.mm.yyyy)
 */

// TOP

/* TODO(allen):

Next Time:
Finish linking from one FSM to the next in the keyword recognizer.

1. Make sure each FSM follows the rules about state types correctly.
2. Make a look up table from final states to resulting token types.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define ArrayCount(a) (sizeof(a)/sizeof(*a))

#include "../4cpp_lexer_types.h"
#include "4cpp_lexer_fsms.h"

static String_And_Flag preprop_strings[] = {
	{"include", CPP_PP_INCLUDE},
	{"INCLUDE", CPP_PP_INCLUDE},
	{"ifndef", CPP_PP_IFNDEF},
	{"IFNDEF", CPP_PP_IFNDEF},
	{"define", CPP_PP_DEFINE},
	{"DEFINE", CPP_PP_DEFINE},
	{"import", CPP_PP_IMPORT},
	{"IMPORT", CPP_PP_IMPORT},
	{"pragma", CPP_PP_PRAGMA},
	{"PRAGMA", CPP_PP_PRAGMA},
	{"undef", CPP_PP_UNDEF},
	{"UNDEF", CPP_PP_UNDEF},
	{"endif", CPP_PP_ENDIF},
	{"ENDIF", CPP_PP_ENDIF},
	{"error", CPP_PP_ERROR},
	{"ERROR", CPP_PP_ERROR},
	{"ifdef", CPP_PP_IFDEF},
	{"IFDEF", CPP_PP_IFDEF},
	{"using", CPP_PP_USING},
	{"USING", CPP_PP_USING},
	{"else", CPP_PP_ELSE},
	{"ELSE", CPP_PP_ELSE},
	{"elif", CPP_PP_ELIF},
	{"ELIF", CPP_PP_ELIF},
	{"line", CPP_PP_LINE},
	{"LINE", CPP_PP_LINE},
	{"if", CPP_PP_IF},
    {"IF", CPP_PP_IF},
};
static String_And_Flag keyword_strings[] = {
    {"true", CPP_TOKEN_BOOLEAN_CONSTANT},
    {"false", CPP_TOKEN_BOOLEAN_CONSTANT},
    
    {"and", CPP_TOKEN_AND},
    {"and_eq", CPP_TOKEN_ANDEQ},
    {"bitand", CPP_TOKEN_BIT_AND},
    {"bitor", CPP_TOKEN_BIT_OR},
    {"or", CPP_TOKEN_OR},
    {"or_eq", CPP_TOKEN_OREQ},
    {"sizeof", CPP_TOKEN_SIZEOF},
    {"alignof", CPP_TOKEN_ALIGNOF},
    {"decltype", CPP_TOKEN_DECLTYPE},
    {"throw", CPP_TOKEN_THROW},
    {"new", CPP_TOKEN_NEW},
    {"delete", CPP_TOKEN_DELETE},
    {"xor", CPP_TOKEN_BIT_XOR},
    {"xor_eq", CPP_TOKEN_XOREQ},
    {"not", CPP_TOKEN_NOT},
    {"not_eq", CPP_TOKEN_NOTEQ},
    {"typeid", CPP_TOKEN_TYPEID},
    {"compl", CPP_TOKEN_BIT_NOT},
    
    {"void", CPP_TOKEN_KEY_TYPE},
    {"bool", CPP_TOKEN_KEY_TYPE},
    {"char", CPP_TOKEN_KEY_TYPE},
    {"int", CPP_TOKEN_KEY_TYPE},
    {"float", CPP_TOKEN_KEY_TYPE},
    {"double", CPP_TOKEN_KEY_TYPE},
    
    {"long", CPP_TOKEN_KEY_MODIFIER},
    {"short", CPP_TOKEN_KEY_MODIFIER},
    {"unsigned", CPP_TOKEN_KEY_MODIFIER},
    
    {"const", CPP_TOKEN_KEY_QUALIFIER},
    {"volatile", CPP_TOKEN_KEY_QUALIFIER},
    
    {"asm", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"break", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"case", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"catch", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"continue", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"default", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"do", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"else", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"for", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"goto", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"if", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"return", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"switch", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"try", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"while", CPP_TOKEN_KEY_CONTROL_FLOW},
    {"static_assert", CPP_TOKEN_KEY_CONTROL_FLOW},
    
    {"const_cast", CPP_TOKEN_KEY_CAST},
    {"dynamic_cast", CPP_TOKEN_KEY_CAST},
    {"reinterpret_cast", CPP_TOKEN_KEY_CAST},
    {"static_cast", CPP_TOKEN_KEY_CAST},
    
    {"class", CPP_TOKEN_KEY_TYPE_DECLARATION},
    {"enum", CPP_TOKEN_KEY_TYPE_DECLARATION},
    {"struct", CPP_TOKEN_KEY_TYPE_DECLARATION},
    {"typedef", CPP_TOKEN_KEY_TYPE_DECLARATION},
    {"union", CPP_TOKEN_KEY_TYPE_DECLARATION},
    {"template", CPP_TOKEN_KEY_TYPE_DECLARATION},
    {"typename", CPP_TOKEN_KEY_TYPE_DECLARATION},
    
    {"friend", CPP_TOKEN_KEY_ACCESS},
    {"namespace", CPP_TOKEN_KEY_ACCESS},
    {"private", CPP_TOKEN_KEY_ACCESS},
    {"protected", CPP_TOKEN_KEY_ACCESS},
    {"public", CPP_TOKEN_KEY_ACCESS},
    {"using", CPP_TOKEN_KEY_ACCESS},
    
    {"extern", CPP_TOKEN_KEY_LINKAGE},
    {"export", CPP_TOKEN_KEY_LINKAGE},
    {"inline", CPP_TOKEN_KEY_LINKAGE},
    {"static", CPP_TOKEN_KEY_LINKAGE},
    {"virtual", CPP_TOKEN_KEY_LINKAGE},
    
    {"alignas", CPP_TOKEN_KEY_OTHER},
    {"explicit", CPP_TOKEN_KEY_OTHER},
    {"noexcept", CPP_TOKEN_KEY_OTHER},
    {"nullptr", CPP_TOKEN_KEY_OTHER},
    {"operator", CPP_TOKEN_KEY_OTHER},
    {"register", CPP_TOKEN_KEY_OTHER},
    {"this", CPP_TOKEN_KEY_OTHER},
    {"thread_local", CPP_TOKEN_KEY_OTHER},
};

struct FSM_State{
    unsigned int transition_rule[256];
    unsigned char override;
};

struct FSM{
    FSM_State *states;
    unsigned short count, max;
    
    FSM_State *term_states;
    unsigned short term_count, term_max;
    
    unsigned char terminal_base;
    
    char *comment;
};

struct FSM_Stack{
    FSM *fsms;
    int count, max;
    
    unsigned char table_transition_state;
    unsigned char final_state;
};

struct Match_Node{
    Match_Node *first_child;
    Match_Node *next_sibling;
    
    int *words;
    int count, max;
    int index;
    
    FSM_State *state;
};

struct Match_Tree{
    Match_Node *nodes;
    int count, max;
};

struct Match_Tree_Stack{
    Match_Tree *trees;
    int count, max;
};

struct Future_FSM{
    Match_Node *source;
};

struct Future_FSM_Stack{
    Future_FSM *futures;
    int count, max;
};

FSM*
get_fsm(FSM_Stack *stack){
    FSM* result = 0;
    assert(stack->count < stack->max);
    result = &stack->fsms[stack->count];
    ++stack->count;
    return(result);
}

Match_Tree*
get_tree(Match_Tree_Stack *stack){
    Match_Tree* result = 0;
    assert(stack->count < stack->max);
    result = &stack->trees[stack->count++];
    return(result);
}

FSM
fsm_init(unsigned short max, unsigned char terminal_base){
    FSM fsm;
    int memsize;
    fsm.max = max;
    fsm.count = 0;
    memsize = sizeof(FSM_State)*fsm.max;
    fsm.states = (FSM_State*)malloc(memsize);
    
    fsm.term_max = max;
    fsm.term_count = 0;
    memsize = sizeof(FSM_State)*fsm.term_max;
    fsm.term_states = (FSM_State*)malloc(memsize);
    
    fsm.comment = 0;
    fsm.terminal_base = terminal_base;
    return(fsm);
}

void
fsm_add_comment(FSM *fsm, char *str){
    int comment_len;
    int str_len;
    char *new_comment;
    
    str_len = (int)strlen(str);
    
    if (fsm->comment != 0){
        comment_len = (int)strlen(fsm->comment);
        new_comment = (char*)malloc(str_len + comment_len + 1);

        memcpy(new_comment, fsm->comment, comment_len);
        memcpy(new_comment + comment_len, str, str_len);
        new_comment[comment_len + str_len] = 0;
        
        free(fsm->comment);
        fsm->comment = new_comment;
    }
    else{
        fsm->comment = (char*)malloc(str_len + 1);
        memcpy(fsm->comment, str, str_len);
        fsm->comment[str_len] = 0;
	}
}

Match_Tree
tree_init(unsigned short max){
    Match_Tree tree;
    int memsize;
    tree.max = max;
    tree.count = 0;
    memsize = sizeof(Match_Node)*tree.max;
    tree.nodes = (Match_Node*)malloc(memsize);
    return(tree);
}

unsigned char
push_future_fsm(Future_FSM_Stack *stack, Match_Node *node){
    unsigned char index = 0;
    Future_FSM *future = 0;
    assert(stack->count < stack->max);
    assert(stack->max < 256);
    index = (unsigned char)(stack->count++);
    future = &stack->futures[index];
    future->source = node;
    return(index);
}

Match_Node*
match_get_node(Match_Tree *tree){
    Match_Node *result;
    assert(tree->count < tree->max);
    result = &tree->nodes[tree->count++];
    return(result);
}

void
match_init_node(Match_Node *node, int match_count){
    *node = {};
    node->words = (int*)malloc(sizeof(int)*match_count);
    node->max = match_count;
}

void
match_copy_init_node(Match_Node *node, Match_Node *source){
    *node = {};
    node->max = source->count;
    node->count = source->count;
    node->words = (int*)malloc(sizeof(int)*source->count);
    node->index = source->index;
    memcpy(node->words, source->words, sizeof(int)*source->count);
}

void
match_add_word(Match_Node *node, int word){
    assert(node->count < node->max);
    node->words[node->count++] = word;
}

FSM_State*
fsm_get_state(FSM *fsm, unsigned int terminal_base){
    FSM_State *result;
    unsigned short i;
    assert(fsm->count < fsm->max);
    result = &fsm->states[fsm->count++];
    for (i = 0; i < 256; ++i){
        result->transition_rule[i] = terminal_base;
	}
    result->override = 0;
    return(result);
}

FSM_State*
fsm_get_state(FSM *fsm){
    FSM_State *result = fsm_get_state(fsm, fsm->terminal_base);
    return(result);
}

FSM_State*
fsm_get_term_state(FSM *fsm, unsigned char override){
    FSM_State *result;
    assert(fsm->term_count < fsm->term_max);
    result = &fsm->term_states[fsm->term_count++];
    result->override = override;
    return(result);
}

unsigned char
fsm_index(FSM *fsm, FSM_State *s){
    unsigned char result;
    result = (unsigned char)(unsigned long long)(s - fsm->states);
    if (s->override){
        result = fsm->terminal_base + s->override;
	}
    return(result);
}

void
fsm_add_transition(FSM_State *state, char c, unsigned char dest){
    state->transition_rule[c] = dest;
}

struct Terminal_Lookup_Table{
    unsigned int state_to_type[60];
    unsigned char type_to_state[CPP_TOKEN_TYPE_COUNT];
    unsigned char state_count;
};

void
process_match_node(String_And_Flag *input, Match_Node *node, Match_Tree *tree, FSM *fsm){
    int next_index = node->index + 1;
    int match_count = node->count;
    FSM_State *this_state = node->state;
    unsigned char terminal_base = fsm->terminal_base;
    
    int i, j, *words = node->words;
    
    String_And_Flag saf;
    int l;
    
    char c;
    Match_Node *next_nodes[256];
    Match_Node *newest_child = 0;
    Match_Node *n;
    
    unsigned char unjunkify = 0;
    
    memset(next_nodes, 0, sizeof(next_nodes));
    
    for (i = 0; i < match_count; ++i){
        j = words[i];
        saf = input[j];
        l = (int)strlen(saf.str);
        
        if (next_index < l){
            c = saf.str[next_index];
            
            if (next_nodes[c] == 0){
                next_nodes[c] = match_get_node(tree);
                match_init_node(next_nodes[c], match_count);
                
                next_nodes[c]->index = next_index;
                next_nodes[c]->state = fsm_get_state(fsm);
                
                if (newest_child == 0){
                    assert(node->first_child == 0);
                    node->first_child = next_nodes[c];
                }
                else{
                    assert(newest_child->next_sibling == 0);
                    newest_child->next_sibling = next_nodes[c];
                }
                newest_child = next_nodes[c];
            }
            
            match_add_word(next_nodes[c], j);
            fsm_add_transition(this_state, c, fsm_index(fsm, next_nodes[c]->state));
        }
        else if (next_index == l){
            assert(unjunkify == 0);
            unjunkify = (unsigned char)saf.flags;
        }
    }
    
    if (unjunkify){
        for (i = 0; i < 256; ++i){
            if (this_state->transition_rule[i] == terminal_base){
                this_state->transition_rule[i] = terminal_base + unjunkify;
            }
        }
    }
    
    for (n = node->first_child; n; n = n->next_sibling){
        process_match_node(input, n, tree, fsm);
    }
}

FSM
generate_pp_directive_fsm(){
    Match_Tree tree;
    FSM fsm;
    Match_Node *root_node;
    FSM_State *root_state;
    int i;
    
    fsm = fsm_init(200, 200);
    tree = tree_init(200);
    
    root_state = fsm_get_state(&fsm);
    
    root_node = match_get_node(&tree);
    match_init_node(root_node, ArrayCount(preprop_strings));
    for (i = 0; i < ArrayCount(preprop_strings); ++i){
        root_node->words[i] = i;
	}
    root_node->count = ArrayCount(preprop_strings);
    root_node->state = root_state;
    root_node->index = -1;
    process_match_node(preprop_strings, root_node, &tree, &fsm);
    
    root_state->transition_rule[' '] = 0;
    root_state->transition_rule['\t'] = 0;
    root_state->transition_rule['\r'] = 0;
    root_state->transition_rule['\v'] = 0;
    root_state->transition_rule['\f'] = 0;
    
    return(fsm);
}

/*

Each state needs a full set of transition rules.  Most transitions should go into a
"not-a-keyword-state".  The exceptions are:
1. When we see an alphanumeric character that is the next character of an actual keyword
 i. May need to transition to a new table at this point.
2. When we have just seen an entire valid keyword, and the next thing we see is not alphanumeric.

*/

#define RealTerminateBase 65536

int
char_is_alphanumeric(char x){
    int result = 0;
    if ((x >= '0' && x <= '9') ||
        (x >= 'A' && x <= 'Z') ||
        (x >= 'a' && x <= 'z') ||
        x == '_'){
        result = 1;
    }
    return(result);
}

void
process_match_node(String_And_Flag *input, Match_Node *node, Match_Tree *tree, FSM *fsm,
                   Terminal_Lookup_Table *terminal_table, int levels_to_go,
                   Future_FSM_Stack *unfinished_fsms){
    
    int next_index = node->index + 1;
    int match_count = node->count;
    int *words = node->words;
    FSM_State *this_state = node->state;
    
    int word_index = 0;
    int good_transition = 0;
    int len = 0;
    int i = 0;
    
    String_And_Flag saf = {0};
    
    Match_Node *next_nodes[256];
    Match_Node *newest_child = 0;
    Match_Node *n = 0;
    char c = 0;
    
    unsigned char override = 0;
    
    memset(next_nodes, 0, sizeof(next_nodes));

    for (i = 0; i < match_count; ++i){
        word_index = words[i];
        saf = input[word_index];

        len = (int)strlen(saf.str);
        if (next_index < len){
            c = saf.str[next_index];
            
            if (next_nodes[c] == 0){
                next_nodes[c] = match_get_node(tree);
                match_init_node(next_nodes[c], match_count);
                
                next_nodes[c]->index = next_index;
                
                if (levels_to_go == 1){
                    override = push_future_fsm(unfinished_fsms, next_nodes[c]);
                    next_nodes[c]->state = fsm_get_term_state(fsm, override);
                }
                else{
                    next_nodes[c]->state = fsm_get_state(fsm, RealTerminateBase);
                }
                
                if (newest_child == 0){
                    assert(node->first_child == 0);
                    node->first_child = next_nodes[c];
                }
                else{
                    assert(newest_child->next_sibling == 0);
                    newest_child->next_sibling = next_nodes[c];
                }
                newest_child = next_nodes[c];
            }
            
            match_add_word(next_nodes[c], word_index);
            fsm_add_transition(this_state, c, fsm_index(fsm, next_nodes[c]->state));
        }
        else{
            assert(next_index == len);
            assert(good_transition == 0);
            good_transition = terminal_table->type_to_state[saf.flags] + RealTerminateBase;
        }
    }
    
    if (good_transition){
        for (i = 0; i < 256; ++i){
            if (!char_is_alphanumeric((char)i)){
                this_state->transition_rule[i] = good_transition;
            }
        }
    }
    
    if (levels_to_go != 1){
        for (n = node->first_child; n; n = n->next_sibling){
            process_match_node(input, n, tree, fsm, terminal_table, levels_to_go-1, unfinished_fsms);
        }
    }
}

FSM_Stack
generate_keyword_fsms(){
    Terminal_Lookup_Table terminal_table;
    Cpp_Token_Type type;

    Future_FSM_Stack unfinished_futures;
    Match_Tree_Stack tree_stack;
    FSM_Stack fsm_stack;
    Match_Tree *tree;
    FSM *fsm;
    Future_FSM *future;
    Match_Node *root_node;
    FSM_State *root_state;
    int i, j;

    memset(terminal_table.type_to_state, 0, sizeof(terminal_table.type_to_state));
    memset(terminal_table.state_to_type, 0, sizeof(terminal_table.state_to_type));

    for (i = 0; i < ArrayCount(keyword_strings); ++i){
        type = (Cpp_Token_Type)keyword_strings[i].flags;
        if (terminal_table.type_to_state[type] == 0){
            terminal_table.type_to_state[type] = terminal_table.state_count;
            terminal_table.state_to_type[terminal_table.state_count] = type;
            ++terminal_table.state_count;
        }
    }

    fsm_stack.max = 255;
    fsm_stack.count = 0;
    fsm_stack.fsms = (FSM*)malloc(sizeof(FSM)*fsm_stack.max);
    fsm_stack.table_transition_state = 26;

    tree_stack.max = 255;
    tree_stack.count = 0;
    tree_stack.trees = (Match_Tree*)malloc(sizeof(Match_Tree)*tree_stack.max);

    unfinished_futures.max = 255;
    unfinished_futures.count = 0;
    unfinished_futures.futures = (Future_FSM*)malloc(sizeof(Future_FSM)*unfinished_futures.max);

    fsm = get_fsm(&fsm_stack);
    tree = get_tree(&tree_stack);

    *fsm = fsm_init(200, fsm_stack.table_transition_state);
    *tree = tree_init(200);

    root_state = fsm_get_state(fsm, RealTerminateBase);
    root_node = match_get_node(tree);
    match_init_node(root_node, ArrayCount(keyword_strings));
    for (i = 0; i < ArrayCount(keyword_strings); ++i){
        root_node->words[i] = i;
    }

    root_node->count = ArrayCount(keyword_strings);
    root_node->state = root_state;
    root_node->index = -1;
    
    push_future_fsm(&unfinished_futures, root_node);
    process_match_node(keyword_strings, root_node, tree, fsm, &terminal_table, 2, &unfinished_futures);

    for (i = 1; i < unfinished_futures.count; ++i){
        future = unfinished_futures.futures + i;

        fsm = get_fsm(&fsm_stack);
        tree = get_tree(&tree_stack);

        assert((int)(fsm - fsm_stack.fsms) == i);

        *fsm = fsm_init(200, fsm_stack.table_transition_state);
        *tree = tree_init(200);

        root_state = fsm_get_state(fsm, RealTerminateBase);
        root_node = match_get_node(tree);
        match_copy_init_node(root_node, future->source);
        root_node->state = root_state;

        for (j = 0; j < root_node->count; ++j){
            char space[1024];
            sprintf(space, "%s\n", keyword_strings[root_node->words[j]].str);
            fsm_add_comment(fsm, space);
        }

        process_match_node(keyword_strings, root_node, tree, fsm, &terminal_table, 12, &unfinished_futures);
    }

    assert(fsm_stack.count < 255);
    fsm_stack.final_state = fsm_stack.table_transition_state + (unsigned char)fsm_stack.count;

    return(fsm_stack);
}

Whitespace_FSM
whitespace_skip_fsm(Whitespace_FSM wfsm, char c){
    if (wfsm.pp_state != LSPP_default){
        if (c == '\n') wfsm.pp_state = LSPP_default;
    }
    if (!(c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v')){
        wfsm.white_done = 1;
    }
    return(wfsm);
}

Lex_FSM
int_fsm(Lex_FSM fsm, char c){
    switch (fsm.int_state){
        case LSINT_default:
        switch (c){
            case 'u': case 'U': fsm.int_state = LSINT_u; break;
            case 'l': fsm.int_state = LSINT_l; break;
            case 'L': fsm.int_state = LSINT_L; break;
            default: fsm.emit_token = 1; break;
        }
        break;

        case LSINT_u:
        switch (c){
            case 'l': fsm.int_state = LSINT_ul; break;
            case 'L': fsm.int_state = LSINT_uL; break;
            default: fsm.emit_token = 1; break;
        }
        break;

        case LSINT_l:
        switch (c){
            case 'l': fsm.int_state = LSINT_ll; break;
            case 'U': case 'u': fsm.int_state = LSINT_extra; break;
            default: fsm.emit_token = 1; break;
        }
        break;

        case LSINT_L:
        switch (c){
            case 'L': fsm.int_state = LSINT_ll; break;
            case 'U': case 'u': fsm.int_state = LSINT_extra; break;
            default: fsm.emit_token = 1; break;
        }
        break;

        case LSINT_ul:
        switch (c){
            case 'l': fsm.int_state = LSINT_extra; break;
            default: fsm.emit_token = 1; break;
        }
        break;

        case LSINT_uL:
        switch (c){
            case 'L': fsm.int_state = LSINT_extra; break;
            default: fsm.emit_token = 1; break;
        }
        break;

        case LSINT_ll:
        switch (c){
            case 'u': case 'U': fsm.int_state = LSINT_extra; break;
            default: fsm.emit_token = 1; break;
        }
        break;

        case LSINT_extra:
        fsm.emit_token = 1;
        break;
    }
    return(fsm);
}

Lex_FSM
main_fsm(Lex_FSM fsm, unsigned char pp_state, unsigned char c){
    if (c == 0) fsm.emit_token = 1;
    else
        switch (pp_state){
        case LSPP_error:
        fsm.state = LS_error_message;
        if (c == '\n') fsm.emit_token = 1;
        break;

        case LSPP_include:
        switch (fsm.state){
            case LSINC_default:
            switch (c){
                case '"': fsm.state = LSINC_quotes; break;
                case '<': fsm.state = LSINC_pointy; break;
                default: fsm.state = LSINC_junk; break;
            }
            break;

            case LSINC_quotes:
            if (c == '"') fsm.emit_token = 1;
            break;

            case LSINC_pointy:
            if (c == '>') fsm.emit_token = 1;
            break;

            case LSINC_junk:
            if (c == '\n') fsm.emit_token = 1;
            break;
        }
        break;

        default:
        switch (fsm.state){
            case LS_default:
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'){
                fsm.state = LS_identifier;
                fsm.emit_token = 1;
            }
            else if (c >= '1' && c <= '9'){
                fsm.state = LS_number;
            }
            else if (c == '0'){
                fsm.state = LS_number0;
            }
            else switch (c){
                case '\'': fsm.state = LS_char; break;
                case '"': fsm.state = LS_string; break;

                case '/': fsm.state = LS_comment_pre; break;

                case '.': fsm.state = LS_dot; break;

                case '<': fsm.state = LS_less; break;
                case '>': fsm.state = LS_more; break;

                case '-': fsm.state = LS_minus; break;

                case '&': fsm.state = LS_and; break;
                case '|': fsm.state = LS_or; break;

                case '+': fsm.state = LS_plus; break;

                case ':': fsm.state = LS_colon; break;

                case '*': fsm.state = LS_star; break;

                case '%': fsm.state = LS_modulo; break;
                case '^': fsm.state = LS_caret; break;

                case '=': fsm.state = LS_eq; break;
                case '!': fsm.state = LS_bang; break;

                case '#':
                if (pp_state == LSPP_default){
                    fsm.state = LS_pp;
                    fsm.emit_token = 1;
                }
                else{
                    fsm.state = LS_pound;
                }
                break;

#define OperCase(op,type) case op: fsm.emit_token = 1; break;
                OperCase('{', CPP_TOKEN_BRACE_OPEN);
                OperCase('}', CPP_TOKEN_BRACE_CLOSE);

                OperCase('[', CPP_TOKEN_BRACKET_OPEN);
                OperCase(']', CPP_TOKEN_BRACKET_CLOSE);

                OperCase('(', CPP_TOKEN_PARENTHESE_OPEN);
                OperCase(')', CPP_TOKEN_PARENTHESE_CLOSE);

                OperCase('~', CPP_TOKEN_TILDE);
                OperCase(',', CPP_TOKEN_COMMA);
                OperCase(';', CPP_TOKEN_SEMICOLON);
                OperCase('?', CPP_TOKEN_TERNARY_QMARK);

                OperCase('@', CPP_TOKEN_JUNK);
                OperCase('$', CPP_TOKEN_JUNK);
                OperCase('\\', CPP_TOKEN_JUNK);
#undef OperCase
            }
            break;

#if 0
            case LS_identifier:
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')){
                fsm.emit_token = 1;
            }
            break;
#endif

            case LS_pound:
            switch (c){
                case '#': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_pp:break;

            case LS_char:
            case LS_char_multiline:
            switch(c){
                case '\'': fsm.emit_token = 1; break;
                case '\\': fsm.state = LS_char_slashed; break;
            }
            break;

            case LS_char_slashed:
            switch (c){
                case '\r': case '\f': case '\v': break;
                case '\n': fsm.state = LS_char_multiline; break;
                default: fsm.state = LS_char; break;
            }
            break;

            case LS_string:
            case LS_string_multiline:
            switch(c){
                case '\"': fsm.emit_token = 1; break;
                case '\\': fsm.state = LS_string_slashed; break;
            }
            break;

            case LS_string_slashed:
            switch (c){
                case '\r': case '\f': case '\v': break;
                case '\n': fsm.state = LS_string_multiline; break;
                default: fsm.state = LS_string; break;
            }
            break;

            case LS_number:
            if (c >= '0' && c <= '9'){
                fsm.state = LS_number;
            }
            else{
                switch (c){
                    case '.': fsm.state = LS_float; break;
                    default: fsm.emit_token = 1; break;
                }
            }
            break;

            case LS_number0:
            if (c >= '0' && c <= '9'){
                fsm.state = LS_number;
            }
            else if (c == 'x'){
                fsm.state = LS_hex;
            }
            else if (c == '.'){
                fsm.state = LS_float;
            }
            else{
                fsm.emit_token = 1;
            }
            break;

            case LS_float:
            if (!(c >= '0' && c <= '9')){
                switch (c){
                    case 'e': fsm.state = LS_crazy_float0; break;
                    default: fsm.emit_token = 1; break;
                }
            }
            break;

            case LS_crazy_float0:
            {
                if ((c >= '0' && c <= '9') || c == '-'){
                    fsm.state = LS_crazy_float1;
                }
                else{
                    fsm.emit_token = 1;
                }
            }
            break;

            case LS_crazy_float1:
            {
                if (!(c >= '0' && c <= '9')){
                    fsm.emit_token = 1;
                }
            }
            break;

            case LS_hex:
            if (!(c >= '0' && c <= '9' || c >= 'a' && c <= 'f' || c >= 'A' && c <= 'F')){
                fsm.emit_token = 1;
            }
            break;

            case LS_dot:
            if (c >= '0' && c <= '9'){
                fsm.state = LS_float;
            }
            else
                switch (c){
                case '.': fsm.state = LS_ellipsis; break;
                case '*': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_ellipsis: fsm.emit_token = 1; break;

            case LS_less:
            switch (c){
                case '<': fsm.state = LS_less_less; break;
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_less_less:
            switch (c){
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_more:
            switch (c){
                case '>': fsm.state = LS_more_more; break;
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_more_more:
            switch (c){
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_comment_pre:
            switch (c){
                case '/': fsm.state = LS_comment; break;
                case '*': fsm.state = LS_comment_block; break;
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_comment:
            switch (c){
                case '\\': fsm.state = LS_comment_slashed; break;
                case '\n': fsm.emit_token = 1; break;
            }
            break;

            case LS_comment_slashed:
            switch (c){
                case '\r': case '\f': case '\v': break;
                default: fsm.state = LS_comment; break;
            }
            break;

            case LS_comment_block:
            switch (c){
                case '*': fsm.state = LS_comment_block_ending; break;
            }
            break;

            case LS_comment_block_ending:
            switch (c){
                case '*': fsm.state = LS_comment_block_ending; break;
                case '/': fsm.emit_token = 1; break;
                default: fsm.state = LS_comment_block; break;
            }
            break;

            case LS_minus:
            switch (c){
                case '>': fsm.state = LS_arrow; break;
                case '-': fsm.emit_token = 1; break;
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_arrow:
            switch (c){
                case '*': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_and:
            switch (c){
                case '&': fsm.emit_token = 1; break;
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_or:
            switch (c){
                case '|': fsm.emit_token = 1; break;
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_plus:
            switch (c){
                case '+': fsm.emit_token = 1; break;
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_colon:
            switch (c){
                case ':': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_star:
            switch (c){
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_modulo:
            switch (c){
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_caret:
            switch (c){
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_eq:
            switch (c){
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;

            case LS_bang:
            switch (c){
                case '=': fsm.emit_token = 1; break;
                default: fsm.emit_token = 1; break;
            }
            break;
        }
        break;
    }
    return(fsm);
}

void
begin_table(FILE *file, char *type, char *group_name, char *table_name){
    fprintf(file, "unsigned %s %s_%s[] = {\n", type, group_name, table_name);
}

void
begin_table(FILE *file, char *type, char *table_name){
    fprintf(file, "unsigned %s %s[] = {\n", type, table_name);
}

void
begin_ptr_table(FILE *file, char *type, char *table_name){
    fprintf(file, "unsigned %s * %s[] = {\n", type, table_name);
}

void
do_table_item(FILE *file, unsigned short item){
    fprintf(file, "%2d,", (int)item);
}

void
do_table_item_direct(FILE *file, char *item, char *tail){
    fprintf(file, "%s%s,", item, tail);
}

void
end_row(FILE *file){
    fprintf(file, "\n");
}

void
end_table(FILE *file){
    fprintf(file, "};\n\n");
}

struct FSM_Tables{
    unsigned char *full_transition_table;
    unsigned char *marks;
    unsigned char *eq_class;
    unsigned char *eq_class_rep;
    unsigned char *reduced_transition_table;

    unsigned char eq_class_counter;
    unsigned short state_count;
};

void
allocate_full_tables(FSM_Tables *table, unsigned char state_count){
    table->full_transition_table = (unsigned char*)malloc(state_count * 256);
    table->marks = (unsigned char*)malloc(state_count * 256);
    table->eq_class = (unsigned char*)malloc(state_count * 256);
    table->eq_class_rep = (unsigned char*)malloc(state_count * 256);
    table->state_count = state_count;
    memset(table->marks, 0, 256);
}

void
do_table_reduction(FSM_Tables *table, unsigned short state_count){
    {
        table->eq_class_counter = 0;
        unsigned char *c_line = table->full_transition_table;
        for (unsigned short c = 0; c < 256; ++c){
            if (table->marks[c] == 0){
                table->eq_class[c] = table->eq_class_counter;
                table->eq_class_rep[table->eq_class_counter] = (unsigned char)c;
                unsigned char *c2_line = c_line + state_count;
                for (unsigned short c2 = c + 1; c2 < 256; ++c2){
                    if (memcmp(c_line, c2_line, state_count) == 0){
                        table->marks[c2] = 1;
                        table->eq_class[c2] = table->eq_class_counter;
                    }
                    c2_line += state_count;
                }
                ++table->eq_class_counter;
            }
            c_line += state_count;
        }
    }

    table->reduced_transition_table = (unsigned char*)malloc(state_count * table->eq_class_counter);
    {
        unsigned char *r_line = table->reduced_transition_table;
        for (unsigned short eq = 0; eq < table->eq_class_counter; ++eq){
            unsigned char *u_line = table->full_transition_table + state_count * table->eq_class_rep[eq];
            memcpy(r_line, u_line, state_count);
            r_line += state_count;
        }
    }
}

FSM_Tables
generate_whitespace_skip_table(){
    unsigned char state_count = LSPP_count;
    FSM_Tables table;
    allocate_full_tables(&table, state_count);

    int i = 0;
    Whitespace_FSM wfsm = {0};
    Whitespace_FSM new_wfsm;
    for (unsigned short c = 0; c < 256; ++c){
        for (unsigned char state = 0; state < state_count; ++state){
            wfsm.pp_state = state;
            wfsm.white_done = 0;
            new_wfsm = whitespace_skip_fsm(wfsm, (unsigned char)c);
            table.full_transition_table[i++] = new_wfsm.pp_state + state_count*new_wfsm.white_done;
        }
    }

    do_table_reduction(&table, state_count);

    return(table);
}

FSM_Tables
generate_int_table(){
    unsigned char state_count = LSINT_count;
    FSM_Tables table;
    allocate_full_tables(&table, state_count);

    int i = 0;
    Lex_FSM fsm = {0};
    Lex_FSM new_fsm;
    for (unsigned short c = 0; c < 256; ++c){
        for (unsigned char state = 0; state < state_count; ++state){
            fsm.int_state = state;
            fsm.emit_token = 0;
            new_fsm = int_fsm(fsm, (unsigned char)c);
            table.full_transition_table[i++] = new_fsm.int_state + state_count*new_fsm.emit_token;
        }
    }

    do_table_reduction(&table, state_count);

    return(table);
}

FSM_Tables
generate_fsm_table(unsigned char pp_state){
    unsigned char state_count = LS_count;
    FSM_Tables table;
    allocate_full_tables(&table, state_count);

    int i = 0;
    Lex_FSM fsm = {0};
    Lex_FSM new_fsm;
    for (unsigned short c = 0; c < 256; ++c){
        for (unsigned char state = 0; state < state_count; ++state){
            fsm.state = state;
            fsm.emit_token = 0;
            new_fsm = main_fsm(fsm, pp_state, (unsigned char)c);
            table.full_transition_table[i++] = new_fsm.state + state_count*new_fsm.emit_token;
        }
    }

    do_table_reduction(&table, state_count);

    return(table);
}

void
render_fsm_table(FILE *file, FSM_Tables tables, char *group_name){
    begin_table(file, "short", group_name, "eq_classes");
    for (unsigned short c = 0; c < 256; ++c){
        do_table_item(file, tables.eq_class[c]*tables.state_count);
    }
    end_row(file);
    end_table(file);

    fprintf(file, "const int num_%s_eq_classes = %d;\n\n", group_name, tables.eq_class_counter);

    int i = 0;
    begin_table(file, "char", group_name, "table");
    for (unsigned short c = 0; c < tables.eq_class_counter; ++c){
        for (unsigned char state = 0; state < tables.state_count; ++state){
            do_table_item(file, tables.reduced_transition_table[i++]);
        }
        end_row(file);
    }
    end_table(file);
}

void
render_variable(FILE *file, char *type, char *variable, unsigned int x){
    fprintf(file, "%s %s = %d;\n\n", type, variable, x);
}

void
render_comment(FILE *file, char *comment){
    fprintf(file, "/*\n%s*/\n", comment);
}

struct PP_Names{
    unsigned char pp_state;
    char *name;
};

PP_Names pp_names[] = {
    {LSPP_default, "main_fsm"},
    {LSPP_include, "pp_include_fsm"},
    {LSPP_macro_identifier, "pp_macro_fsm"},
    {LSPP_identifier, "pp_identifier_fsm"},
    {LSPP_body_if, "pp_body_if_fsm"},
    {LSPP_body, "pp_body_fsm"},
    {LSPP_number, "pp_number_fsm"},
    {LSPP_error, "pp_error_fsm"},
    {LSPP_junk, "pp_junk_fsm"},
};

FSM_Tables
generate_table_from_abstract_fsm(FSM fsm, unsigned char real_term_base){
    unsigned char state_count = (unsigned char)fsm.count;
    FSM_Tables table;

    allocate_full_tables(&table, state_count);

    int i = 0;
    unsigned int new_state;
    for (unsigned short c = 0; c < 256; ++c){
        for (unsigned char state = 0; state < state_count; ++state){
            new_state = fsm.states[state].transition_rule[c];
            if (new_state >= RealTerminateBase){
                new_state = new_state - RealTerminateBase + real_term_base;
            }
            table.full_transition_table[i++] = (unsigned char)new_state;
        }
    }

    do_table_reduction(&table, state_count);
    
    return(table);
}

int
main(){
    FILE *file;
    file = fopen("4cpp_lexer_tables.c", "wb");
    
    FSM_Tables wtables = generate_whitespace_skip_table();
    render_fsm_table(file, wtables, "whitespace_fsm");
    
    FSM_Tables itables = generate_int_table();
    render_fsm_table(file, itables, "int_fsm");
    
    begin_table(file, "char", "multiline_state_table");
    for (unsigned char state = 0; state < LS_count; ++state){
        do_table_item(file, (state == LS_string_multiline || state == LS_char_multiline));
    }
    end_row(file);
    end_table(file);
    
    for (int i = 0; i < ArrayCount(pp_names); ++i){
        assert(i == pp_names[i].pp_state);
        FSM_Tables tables = generate_fsm_table(pp_names[i].pp_state);
        render_fsm_table(file, tables, pp_names[i].name);
    }
    
    begin_ptr_table(file, "short", "get_eq_classes");
    for (int i = 0; i < ArrayCount(pp_names); ++i){
        do_table_item_direct(file, pp_names[i].name, "_eq_classes");
        end_row(file);
    }
    end_table(file);
    
    begin_ptr_table(file, "char", "get_table");
    for (int i = 0; i < ArrayCount(pp_names); ++i){
        do_table_item_direct(file, pp_names[i].name, "_table");
        end_row(file);
    }
    end_table(file);
    
    FSM pp_directive_fsm = generate_pp_directive_fsm();
    FSM_Tables pp_directive_tables = generate_table_from_abstract_fsm(pp_directive_fsm, 0);
    
    render_fsm_table(file, pp_directive_tables, "pp_directive");
    render_variable(file, "unsigned char", "LSDIR_default", 0);
    render_variable(file, "unsigned char", "LSDIR_count", pp_directive_fsm.count);
    render_variable(file, "unsigned char", "pp_directive_terminal_base", pp_directive_fsm.terminal_base);

    FSM_Stack keyword_fsms = generate_keyword_fsms();
    
    char name[1024];
    for (int i = 0; i < keyword_fsms.count; ++i){
        FSM_Tables partial_keywords_table =
            generate_table_from_abstract_fsm(keyword_fsms.fsms[i], keyword_fsms.final_state);
        if (keyword_fsms.fsms[i].comment){
            render_comment(file, keyword_fsms.fsms[i].comment);
        }
        
        sprintf(name, "keyword_part_%d_table", i);
        render_fsm_table(file, partial_keywords_table, name);
	}
    
    begin_ptr_table(file, "short", "key_eq_class_tables");
    for (int i = 0; i < keyword_fsms.count; ++i){
        sprintf(name, "keyword_part_%d_table_eq_classes", i);
        do_table_item_direct(file, name, "");
        end_row(file);
    }
    end_table(file);
    
    begin_ptr_table(file, "char", "key_tables");
    for (int i = 0; i < keyword_fsms.count; ++i){
        sprintf(name, "keyword_part_%d_table_table", i);
        do_table_item_direct(file, name, "");
        end_row(file);
    }
    end_table(file);
    
    fprintf(file, "#define LSKEY_table_transition %d\n", (int)(keyword_fsms.table_transition_state));
    fprintf(file, "#define LSKEY_totally_finished %d\n", (int)(keyword_fsms.final_state));
    
    fclose(file);
    return(0);
}

// BOTTOM


