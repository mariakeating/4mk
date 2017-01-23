/*
4coder development build rule.
*/

// TOP

#include "../4tech_defines.h"
#include "4tech_file_moving.h"

#include <assert.h>
#include <string.h>

#include "../4coder_API/version.h"

#define FSTRING_IMPLEMENTATION
#include "../4coder_lib/4coder_string.h"

//
// reusable
//

#define IS_64BIT

#define BEGIN_TIME_SECTION() uint64_t start = get_time()
#define END_TIME_SECTION(n) uint64_t total = get_time() - start; printf("%-20s: %.2lu.%.6lu\n", (n), total/1000000, total%1000000);

//
// 4coder specific
//

#if defined(IS_WINDOWS)
#define EXE ".exe"
#elif defined(IS_LINUX)
#define EXE ""
#else
#error No EXE format specified for this OS
#endif

#if defined(IS_WINDOWS)
#define PDB ".pdb"
#elif defined(IS_LINUX)
#define PDB ""
#else
#error No EXE format specified for this OS
#endif

#if defined(IS_WINDOWS)
#define DLL ".dll"
#elif defined(IS_LINUX)
#define DLL ".so"
#else
#error No EXE format specified for this OS
#endif

#if defined(IS_WINDOWS)
#define BAT ".bat"
#elif defined(IS_LINUX)
#define BAT ".sh"
#else
#error No EXE format specified for this OS
#endif

static void
swap_ptr(char **A, char **B){
    char *a = *A;
    char *b = *B;
    *A = b;
    *B = a;
}

enum{
    OPTS = 0x1,
    INCLUDES = 0x2,
    LIBS = 0x4,
    ICON = 0x8,
    SHARED_CODE = 0x10,
    DEBUG_INFO = 0x20,
    SUPER = 0x40,
    INTERNAL = 0x80,
    OPTIMIZATION = 0x100,
    KEEP_ASSERT = 0x200,
    SITE_INCLUDES = 0x400,
};


#define BUILD_LINE_MAX 4096
typedef struct Build_Line{
    char build_optionsA[BUILD_LINE_MAX];
    char build_optionsB[BUILD_LINE_MAX];
    char *build_options;
    char *build_options_prev;
    i32 build_max;
} Build_Line;

static void
init_build_line(Build_Line *line){
    line->build_options = line->build_optionsA;
    line->build_options_prev = line->build_optionsB;
    line->build_optionsA[0] = 0;
    line->build_optionsB[0] = 0;
    line->build_max = BUILD_LINE_MAX;
}

#if defined(IS_CL)

#define build_ap(line, str, ...) do{        \
    snprintf(line.build_options,            \
    line.build_max, "%s "str,               \
    line.build_options_prev, __VA_ARGS__);  \
    swap_ptr(&line.build_options,           \
    &line.build_options_prev);              \
}while(0)

#elif defined(IS_GCC)

#define build_ap(line, str, ...) do{                         \
    snprintf(line.build_options, line.build_max, "%s "str,   \
    line.build_options_prev, ##__VA_ARGS__);        \
    swap_ptr(&line.build_options, &line.build_options_prev); \
}while(0)

#endif

#define CL_OPTS                                  \
"-W4 -wd4310 -wd4100 -wd4201 -wd4505 -wd4996 "   \
"-wd4127 -wd4510 -wd4512 -wd4610 -wd4390 "       \
"-wd4611 -WX -GR- -EHa- -nologo -FC"

#define CL_INCLUDES "/I..\\foreign /I..\\foreign\\freetype2"

#define CL_SITE_INCLUDES "/I..\\..\\foreign /I..\\..\\code"

#define CL_LIBS                                  \
"user32.lib winmm.lib gdi32.lib opengl32.lib "   \
"..\\foreign\\freetype.lib"

#define CL_ICON "..\\res\\icon.res"

static void
build_cl(u32 flags, char *code_path, char *code_file, char *out_path, char *out_file, char *exports){
    Build_Line line;
    init_build_line(&line);
    
    if (flags & OPTS){
        build_ap(line, CL_OPTS);
    }
    
    if (flags & INCLUDES){
        build_ap(line, CL_INCLUDES);
    }
    
    if (flags & SITE_INCLUDES){
        build_ap(line, CL_SITE_INCLUDES);
    }
    
    if (flags & LIBS){
        build_ap(line, CL_LIBS);
    }
    
    if (flags & ICON){
        build_ap(line, CL_ICON);
    }
    
    if (flags & DEBUG_INFO){
        build_ap(line, "/Zi");
    }
    
    if (flags & OPTIMIZATION){
        build_ap(line, "/O2");
    }
    
    if (flags & SHARED_CODE){
        build_ap(line, "/LD");
    }
    
    if (flags & SUPER){
        build_ap(line, "/DFRED_SUPER");
    }
    
    if (flags & INTERNAL){
        build_ap(line, "/DFRED_INTERNAL");
    }
    
    if (flags & KEEP_ASSERT){
        build_ap(line, "/DFRED_KEEP_ASSERT");
    }
    
    swap_ptr(&line.build_options, &line.build_options_prev);
    
    char link_options[1024];
    if (flags & SHARED_CODE){
        assert(exports);
        snprintf(link_options, sizeof(link_options), "/OPT:REF %s", exports);
    }
    else{
        snprintf(link_options, sizeof(link_options), "/NODEFAULTLIB:library");
    }
    
    Temp_Dir temp = pushdir(out_path);
    systemf("cl %s %s\\%s /Fe%s /link /DEBUG /INCREMENTAL:NO %s", line.build_options, code_path, code_file, out_file, link_options);
    popdir(temp);
}


#define GCC_OPTS                             \
"-Wno-write-strings -D_GNU_SOURCE -fPIC "    \
"-fno-threadsafe-statics -pthread"

#define GCC_INCLUDES "-I../foreign -I../code"

#define GCC_SITE_INCLUDES "-I../../foreign -I../../code"

#define GCC_LIBS                               \
"-L/usr/local/lib -lX11 -lpthread -lm -lrt "   \
"-lGL -ldl -lXfixes -lfreetype -lfontconfig"

static void
build_gcc(u32 flags, char *code_path, char *code_file, char *out_path, char *out_file, char *exports){
    Build_Line line;
    init_build_line(&line);
    
    if (flags & OPTS){
        build_ap(line, GCC_OPTS);
    }
    
    if (flags & INCLUDES){
        // TODO(allen): Abstract this out.
#if defined(IS_LINUX)
        i32 size = 0;
        char freetype_include[512];
        FILE *file = popen("pkg-config --cflags freetype2", "r");
        if (file != 0){
            fgets(freetype_include, sizeof(freetype_include), file);
            size = strlen(freetype_include);
            freetype_include[size-1] = 0;
            pclose(file);
        }
        
        build_ap(line, GCC_INCLUDES" %s", freetype_include);
#endif
    }
    
    if (flags & SITE_INCLUDES){
        build_ap(line, GCC_SITE_INCLUDES);
    }
    
    if (flags & DEBUG_INFO){
        build_ap(line, "-g -O0");
    }
    
    if (flags & OPTIMIZATION){
        build_ap(line, "-O3");
    }
    
    if (flags & SHARED_CODE){
        build_ap(line, "-shared");
    }
    
    if (flags & SUPER){
        build_ap(line, "-DFRED_SUPER");
    }
    
    if (flags & INTERNAL){
        build_ap(line, "-DFRED_INTERNAL");
    }
    
    if (flags & KEEP_ASSERT){
        build_ap(line, "-DFRED_KEEP_ASSERT");
    }
    
    build_ap(line, "%s/%s", code_path, code_file);
    
    if (flags & LIBS){
        build_ap(line, GCC_LIBS);
    }
    
    swap_ptr(&line.build_options, &line.build_options_prev);
    
    Temp_Dir temp = pushdir(out_path);
    systemf("g++ %s -o %s", line.build_options, out_file);
    popdir(temp);
}

static void
build(u32 flags, char *code_path, char *code_file, char *out_path, char *out_file, char *exports){
#if defined(IS_CL)
    build_cl(flags, code_path, code_file, out_path, out_file, exports);
#elif defined(IS_GCC)
    build_gcc(flags, code_path, code_file, out_path, out_file, exports);
#else
#error The build rule for this compiler is not ready
#endif
}

static void
buildsuper(char *code_path, char *out_path, char *filename){
    Temp_Dir temp = pushdir(out_path);
    
#if defined(IS_CL)
    systemf("call \"%s\\buildsuper.bat\" %s", code_path, filename);
    
#elif defined(IS_GCC)
    
    systemf("\"%s/buildsuper.sh\" %s", code_path, filename);
    
#else
#error The build rule for this compiler is not ready
#endif
    
    popdir(temp);
}

#define META_DIR "../meta"
#define BUILD_DIR "../build"

#define SITE_DIR "../site"
#define PACK_DIR "../distributions"
#define PACK_DATA_DIR "../data/dist_files"
#define DATA_DIR "../data/test"

#define PACK_ALPHA_PAR_DIR "../current_dist"
#define PACK_SUPER_PAR_DIR "../current_dist_super"
#define PACK_POWER_PAR_DIR "../current_dist_power"

#define PACK_ALPHA_DIR PACK_ALPHA_PAR_DIR"/4coder"
#define PACK_SUPER_DIR PACK_SUPER_PAR_DIR"/4coder"
#define PACK_POWER_DIR PACK_POWER_PAR_DIR"/power"

#if defined(IS_WINDOWS)
#define PLAT_LAYER "win32_4ed.cpp"
#elif defined(IS_LINUX)
#define PLAT_LAYER "linux_4ed.cpp"
#else
#error No platform layer defined for this OS.
#endif

static void
fsm_generator(char *cdir){
    {
        DECL_STR(file, "meta/fsm_table_generator.cpp");
        DECL_STR(dir, META_DIR);
        
        BEGIN_TIME_SECTION();
        build(OPTS | DEBUG_INFO, cdir, file, dir, "fsmgen", 0);
        END_TIME_SECTION("build fsm generator");
    }
    
    if (prev_error == 0){
        DECL_STR(cmd, META_DIR"/fsmgen");
        BEGIN_TIME_SECTION();
        execute_in_dir(cdir, cmd, 0);
        END_TIME_SECTION("run fsm generator");
    }
}

static void
metagen(char *cdir){
    {
        DECL_STR(file, "meta/4ed_metagen.cpp");
        DECL_STR(dir, META_DIR);
        
        BEGIN_TIME_SECTION();
        build(OPTS | INCLUDES | DEBUG_INFO, cdir, file, dir, "metagen", 0);
        END_TIME_SECTION("build metagen");
    }
    
    if (prev_error == 0){
        DECL_STR(cmd, META_DIR"/metagen");
        BEGIN_TIME_SECTION();
        execute_in_dir(cdir, cmd, 0);
        END_TIME_SECTION("run metagen");
    }
}

enum{
    Custom_Default,
    Custom_Experiments,
    Custom_Casey,
    Custom_ChronalVim,
    Custom_COUNT
};

static void
do_buildsuper(char *cdir, i32 custom_option){
    char space[1024];
    String str = make_fixed_width_string(space);
    
    BEGIN_TIME_SECTION();
    
    switch (custom_option){
        case Custom_Default:
        {
            copy_sc(&str, "../code/4coder_default_bindings.cpp");
        }break;
        
        case Custom_Experiments:
        {
#if defined(IS_WINDOWS)
            copy_sc(&str, "../code/internal_4coder_tests.cpp");
#else
            copy_sc(&str, "../code/power/4coder_experiments.cpp");
#endif
        }break;
        
        case Custom_Casey:
        {
            copy_sc(&str, "../code/power/4coder_casey.cpp");
        }break;
        
        case Custom_ChronalVim:
        {
            copy_sc(&str, "../4vim/4coder_chronal.cpp");
        }break;
    }
    
    terminate_with_null(&str);
    
    DECL_STR(dir, BUILD_DIR);
    buildsuper(cdir, dir, str.str);
    
    END_TIME_SECTION("build custom");
}

static void
build_main(char *cdir, u32 flags){
    DECL_STR(dir, BUILD_DIR);
    {
        DECL_STR(file, "4ed_app_target.cpp");
        BEGIN_TIME_SECTION();
        build(OPTS | INCLUDES | SHARED_CODE | flags, cdir, file, dir, "4ed_app"DLL, "/EXPORT:app_get_functions");
        END_TIME_SECTION("build 4ed_app");
    }
    
    {
        BEGIN_TIME_SECTION();
        build(OPTS | INCLUDES | LIBS | ICON | flags, cdir, PLAT_LAYER, dir, "4ed", 0);
        END_TIME_SECTION("build 4ed");
    }
}

static void
standard_build(char *cdir, u32 flags){
    //run_update("4coder_string.h");
    //run_update("4cpp/4cpp_lexer.h 4cpp/4cpp_lexer.h 4cpp/4cpp_lexer.h");
    fsm_generator(cdir);
    metagen(cdir);
    do_buildsuper(cdir, Custom_Experiments);
    //do_buildsuper(cdir, Custom_ChronalVim);
    build_main(cdir, flags);
}

static void
site_build(char *cdir, u32 flags){
    {
        DECL_STR(file, "site/sitegen.cpp");
        DECL_STR(dir, BUILD_DIR"/site");
        BEGIN_TIME_SECTION();
        build(OPTS | SITE_INCLUDES | flags, cdir, file, dir, "sitegen", 0);
        END_TIME_SECTION("build sitegen");
    }
    
    {
        BEGIN_TIME_SECTION();
        
        DECL_STR(cmd, "../build/site/sitegen . ../site_resources site/source_material ../site");
        systemf("%s", cmd);
        
        END_TIME_SECTION("run sitegen");
    }
}

static void
get_4coder_dist_name(String *zip_file, i32 OS_specific, char *tier, char *ext){
    zip_file->size = 0;
    
    append_sc(zip_file, PACK_DIR"/");
    append_sc(zip_file, tier);
    append_sc(zip_file, "/4coder-");
    
    if (OS_specific){
#if defined(IS_WINDOWS)
        append_sc(zip_file, "win-");
#elif defined(IS_LINUX) && defined(IS_64BIT)
        append_sc(zip_file, "linux-64-");
#else
#error No OS string for zips on this OS
#endif
    }
    
    append_sc         (zip_file, "alpha");
    append_sc         (zip_file, "-");
    append_int_to_str (zip_file, MAJOR);
    append_sc         (zip_file, "-");
    append_int_to_str (zip_file, MINOR);
    append_sc         (zip_file, "-");
    append_int_to_str (zip_file, PATCH);
    if (!match_cc(tier, "alpha")){
        append_sc     (zip_file, "-");
        append_sc     (zip_file, tier);
    }
    append_sc         (zip_file, ".");
    append_sc         (zip_file, ext);
    terminate_with_null(zip_file);
}

static void
package(char *cdir){
    char str_space[1024];
    String str = make_fixed_width_string(str_space), str2 = {0};
    
    // NOTE(allen): meta
    fsm_generator(cdir);
    metagen(cdir);
    
    // NOTE(allen): alpha
    build_main(cdir, OPTIMIZATION | KEEP_ASSERT | DEBUG_INFO);
    
    DECL_STR(build_dir, BUILD_DIR);
    DECL_STR(site_dir, SITE_DIR);
    DECL_STR(pack_dir, PACK_DIR);
    DECL_STR(pack_data_dir, PACK_DATA_DIR);
    DECL_STR(data_dir, DATA_DIR);
    
    DECL_STR(pack_alpha_par_dir, PACK_ALPHA_PAR_DIR);
    DECL_STR(pack_super_par_dir, PACK_SUPER_PAR_DIR);
    DECL_STR(pack_power_par_dir, PACK_POWER_PAR_DIR);
    
    DECL_STR(pack_alpha_dir, PACK_ALPHA_DIR);
    DECL_STR(pack_super_dir, PACK_SUPER_DIR);
    DECL_STR(pack_power_dir, PACK_POWER_DIR);
    
    clear_folder(pack_alpha_par_dir);
    make_folder_if_missing(pack_alpha_dir, "3rdparty");
    make_folder_if_missing(pack_dir, "alpha");
    copy_file(build_dir, "4ed"EXE, pack_alpha_dir, 0, 0);
    //ONLY_WINDOWS(copy_file(build_dir, "4ed"PDB, pack_alpha_dir, 0, 0));
    copy_file(build_dir, "4ed_app"DLL, pack_alpha_dir, 0, 0);
    //ONLY_WINDOWS(copy_file(build_dir, "4ed_app"PDB, pack_alpha_dir, 0, 0));
    copy_all (pack_data_dir, "*", pack_alpha_dir);
    copy_file(0, "README.txt", pack_alpha_dir, 0, 0);
    copy_file(0, "TODO.txt", pack_alpha_dir, 0, 0);
    copy_file(data_dir, "release-config.4coder", pack_alpha_dir, 0, "config.4coder");
    
    get_4coder_dist_name(&str, 1, "alpha", "zip");
    zip(pack_alpha_par_dir, "4coder", str.str);
    
    // NOTE(allen): super
    build_main(cdir, OPTIMIZATION | KEEP_ASSERT | DEBUG_INFO | SUPER);
    do_buildsuper(cdir, Custom_Default);
    
    clear_folder(pack_super_par_dir);
    make_folder_if_missing(pack_super_dir, "3rdparty");
    make_folder_if_missing(pack_dir, "super");
    make_folder_if_missing(pack_dir, "super-docs");
    
    copy_file(build_dir, "4ed"EXE, pack_super_dir, 0, 0);
    //ONLY_WINDOWS(copy_file(build_dir, "4ed"PDB, pack_super_dir, 0, 0));
    copy_file(build_dir, "4ed_app"DLL, pack_super_dir, 0, 0);
    //ONLY_WINDOWS(copy_file(build_dir, "4ed_app"PDB, pack_super_dir, 0, 0));
    copy_file(build_dir, "custom_4coder"DLL, pack_super_dir, 0, 0);
    
    copy_all (pack_data_dir, "*", pack_super_dir);
    copy_file(0, "README.txt", pack_super_dir, 0, 0);
    copy_file(0, "TODO.txt", pack_super_dir, 0, 0);
    copy_file(data_dir, "release-config.4coder", pack_super_dir, 0, "config.4coder");
    
    copy_all(0, "4coder_*", pack_super_dir);
    
    copy_file(0, "buildsuper"BAT, pack_super_dir, 0, 0);
    
    DECL_STR(custom_dir, "4coder_API");
    DECL_STR(custom_helper_dir, "4coder_helper");
    DECL_STR(custom_lib_dir, "4coder_lib");
    DECL_STR(fcpp_dir, "4cpp");
    
    char *dir_array[] = {
        custom_dir,
        custom_helper_dir,
        custom_lib_dir,
        fcpp_dir,
    };
    i32 dir_count = ArrayCount(dir_array);
    
    for (i32 i = 0; i < dir_count; ++i){
        char *d = dir_array[i];
        make_folder_if_missing(pack_super_dir, d);
        
        char space[256];
        String str = make_fixed_width_string(space);
        append_sc(&str, pack_super_dir);
        append_s_char(&str, platform_correct_slash);
        append_sc(&str, d);
        terminate_with_null(&str);
        
        copy_all(d, "*", str.str);
    }
    
    get_4coder_dist_name(&str, 0, "API", "html");
    str2 = front_of_directory(str);
    copy_file(site_dir, "custom_docs.html", pack_dir, "super-docs", str2.str);
    
    get_4coder_dist_name(&str, 1, "super", "zip");
    zip(pack_super_par_dir, "4coder", str.str);
    
    // NOTE(allen): power
    clear_folder(pack_power_par_dir);
    make_folder_if_missing(pack_power_dir, 0);
    make_folder_if_missing(pack_dir, "power");
    copy_all("power", "*", pack_power_dir);
    
    get_4coder_dist_name(&str, 0, "power", "zip");
    zip(pack_power_par_dir, "power", str.str);
}

#if defined(DEV_BUILD)

int main(int argc, char **argv){
    init_time_system();
    
    char cdir[256];
    
    BEGIN_TIME_SECTION();
    i32 n = get_current_directory(cdir, sizeof(cdir));
    assert(n < sizeof(cdir));
    END_TIME_SECTION("current directory");
    
    standard_build(cdir, DEBUG_INFO | SUPER | INTERNAL);
    
    return(error_state);
}

#elif defined(PACKAGE)

int main(int argc, char **argv){
    init_time_system();
    
    char cdir[256];
    
    BEGIN_TIME_SECTION();
    i32 n = get_current_directory(cdir, sizeof(cdir));
    assert(n < sizeof(cdir));
    END_TIME_SECTION("current directory");
    
    package(cdir);
    
    return(error_state);
}

#elif defined(SITE_BUILD)

int main(int argc, char **argv){
    init_time_system();
    
    char cdir[256];
    
    BEGIN_TIME_SECTION();
    i32 n = get_current_directory(cdir, sizeof(cdir));
    assert(n < sizeof(cdir));
    END_TIME_SECTION("current directory");
    
    site_build(cdir, DEBUG_INFO);
    
    return(error_state);
}

#else
#error No build type specified
#endif

#define FTECH_FILE_MOVING_IMPLEMENTATION
#include "4tech_file_moving.h"

// BOTTOM


