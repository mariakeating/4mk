/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 03.10.2019
 *
 * System API definition types.
 *
 */

// TOP

#if !defined(FRED_API_DEFINITION_H)
#define FRED_API_DEFINITION_H

struct API_Param{
    API_Param *next;
    String_Const_u8 type_name;
    String_Const_u8 name;
};

struct API_Param_List{
    API_Param *first;
    API_Param *last;
    i32 count;
};

struct API_Call{
    API_Call *next;
    String_Const_u8 name;
    String_Const_u8 return_type;
    String_Const_u8 location_string;
    API_Param_List params;
};

struct API_Definition{
    API_Definition *next;
    
    API_Call *first;
    API_Call *last;
    i32 count;
    
    String_Const_u8 name;
};

struct API_Definition_List{
    API_Definition *first;
    API_Definition *last;
    i32 count;
};

typedef u32 API_Generation_Flag;
enum{
    APIGeneration_NoAPINameOnCallables = 1,
};

typedef u32 API_Check_Flag;
enum{
    APICheck_ReportMissingAPI = 1,
    APICheck_ReportExtraAPI = 2,
    APICheck_ReportMismatchAPI = 4,
};
enum{
    APICheck_ReportAll = APICheck_ReportMissingAPI|APICheck_ReportExtraAPI|APICheck_ReportMismatchAPI,
};

#endif

// BOTTOM

