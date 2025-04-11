#pragma once

#include "../stdafx.h"

const char* gres_path_translate( const char* path );

const char* app_exe_path();
const char* app_prefix_path( const char* path, ... );
const char* app_share_path( const char* path, ... );
const char* app_libexec_path( const char* path, ... );
const char* app_data_path_translate( const char* path, ... );
const char* user_data_path_translate( const char* path, ... );

#ifndef LOCALE_PATH
#define LOCALE_PATH app_share_path( "/locale", NULL )
#endif