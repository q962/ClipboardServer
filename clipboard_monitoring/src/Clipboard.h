#pragma once

#include "stdafx.h"

G_BEGIN_DECLS

#if 1  // object definition

G_DECLARE_FINAL_TYPE( Clipboard, clipboard, CLIPBOARD, , GObject )

#define CLIPBOARD_SIGNAL_NOTIFY \
	"clipboard-notify"  // (guint mimetype_count, string[] mimetype_array:r, GBytes*[] data_array:r)

Clipboard* clipboard_new();

void clipboard_set_text( Clipboard* clipboard, const char* text, gssize size );
void clipboard_set_image( Clipboard* clipboard, GBytes* bytes );

#endif

G_END_DECLS
