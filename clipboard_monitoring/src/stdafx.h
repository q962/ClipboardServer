#pragma once

#include <gtk/gtk.h>

#define GETTEXT_PACKAGE APPID
#include <glib/gi18n.h>

#include <luajit.h>
#include <lauxlib.h>

#ifdef G_OS_WIN32

#include <MagickWand/MagickWand.h>

#include <windows.h>
#include <windowsx.h>
#endif

#include <sqlite3.h>

#include "config.h"
