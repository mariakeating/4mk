/*
4coder_search.h - Types that are used in the search accross all buffers procedures.
*/

// TOP

#if !defined(FCODER_SEARCH_H)
#define FCODER_SEARCH_H

typedef i32  Seek_Potential_Match_Direction;
enum{
    SeekPotentialMatch_Forward = 0,
    SeekPotentialMatch_Backward = 1,
};

enum{
    FindResult_None,
    FindResult_FoundMatch,
    FindResult_PastEnd,
};

typedef i32 Search_Range_Type;
enum{
    SearchRange_FrontToBack = 0,
    SearchRange_BackToFront = 1,
    SearchRange_Wave = 2,
};

typedef u32 Search_Range_Flag;
enum{
    SearchFlag_MatchWholeWord  = 0x0000,
    SearchFlag_MatchWordPrefix = 0x0001,
    SearchFlag_MatchSubstring  = 0x0002,
    SearchFlag_MatchMask       = 0x00FF,
    SearchFlag_CaseInsensitive = 0x0100,
};

struct Search_Range{
    i32 type;
    u32 flags;
    i32 buffer;
    i32 start;
    i32 size;
    i32 mid_start;
    i32 mid_size;
};

struct Search_Set{
    Search_Range *ranges;
    i32 count;
    i32 max;
};

struct Search_Key{
    char *base;
    i32 base_size;
    String words[16];
    i32 count;
    i32 min_size;
};

struct Search_Iter{
    Search_Key key;
    i32 pos;
    i32 back_pos;
    i32 i;
    i32 range_initialized;
};

struct Search_Match{
    Buffer_Summary buffer;
    i32 start;
    i32 end;
    i32 match_word_index;
    i32 found_match;
};

struct Word_Complete_State{
    Search_Set set;
    Search_Iter iter;
    Table hits;
    String_Space str;
    i32 word_start;
    i32 word_end;
    i32 initialized;
};

#endif

// BOTOTM

