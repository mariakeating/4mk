/*
 * Mr. 4th Dimention - Allen Webster
 *
 * 13.11.2015
 *
 * Application layer build target
 *
 */

// TOP
 
#define VERSION_NUMBER "alpha 3.4.4"

#ifdef FRED_SUPER
#define VERSION_TYPE " super!"
#else
#define VERSION_TYPE ""
#endif

#define VERSION VERSION_NUMBER VERSION_TYPE

#include "4ed_config.h"

#define BUFFER_EXPERIMENT_SCALPEL 0

#include "4ed_meta.h"

#include "4cpp_types.h"
#define FCPP_STRING_IMPLEMENTATION
#include "4coder_string.h"

#include "4ed_mem.cpp"
#include "4ed_math.cpp"

#include "4coder_custom.h"
#include "4ed_system.h"
#include "4ed_rendering.h"
#include "4ed.h"

#include "4ed_internal.h"

#include "4tech_table.cpp"

#define FCPP_LEXER_IMPLEMENTATION
#include "4cpp_lexer.h"

#include "4ed_exchange.cpp"
#include "4ed_font_set.cpp"
#include "4ed_rendering_helper.cpp"
#include "4ed_command.cpp"
#include "4ed_layout.cpp"
#include "4ed_style.cpp"
#include "4ed_file.cpp"
#include "4ed_gui.cpp"
#include "4ed_delay.cpp"
#include "4ed_file_view.cpp"
#include "4ed_color_view.cpp"
#include "4ed_interactive_view.cpp"
#include "4ed_menu_view.cpp"
#include "4ed_app_settings.h"
#include "4ed_config_view.cpp"
#include "4ed.cpp"

// BOTTOM
