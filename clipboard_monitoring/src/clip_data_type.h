#pragma once

#include "stdafx.h"

enum ClipDataType {
	CLIP_DATA_TYPE_0,
	CLIP_DATA_TYPE_STRING,
	CLIP_DATA_TYPE_IMAGE,
	CLIP_DATA_TYPE_N
};

struct ClipDataItem {
	enum ClipDataType type;
	gpointer          data;
	gsize             len;
};

struct ClipData {
	GArray*    ids;   // tpye gsize
	GPtrArray* data;  // type struct ClipDataItem
};

struct ClipI {
	int ( *get )( guint limit, guint count, struct ClipData* data );
	int ( *set )( enum ClipDataType, gconstpointer data, gsize data_size );
};
