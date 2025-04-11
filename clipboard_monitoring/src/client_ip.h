#pragma once

#include "stdafx.h"

struct NetworkAdapters {
	char* name;
	char* ip_str;
};

#ifdef G_OS_WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

GPtrArray* get_network_adapters();

#else

GPtrArray* get_network_adapters();

#endif

