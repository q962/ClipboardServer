#include "client_ip.h"

static void network_adapters_free( gpointer data )
{
	struct NetworkAdapters* adap = data;

	g_free( adap->name );
	g_free( adap->ip_str );

	g_free( data );
}

#ifdef G_OS_WIN32

GPtrArray* get_network_adapters()
{
	// 初始化 Winsock
	WSADATA wsaData;
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 ) {
		fprintf( stderr, "WSAStartup failed.\n" );
		return NULL;
	}

	int                   ret              = 0;
	int                   buf_len_base     = 15000;
	ULONG                 outBufLen        = 0;
	PIP_ADAPTER_ADDRESSES adapterAddresses = NULL;

	do {
		outBufLen = outBufLen + buf_len_base;

		adapterAddresses = ( IP_ADAPTER_ADDRESSES* )malloc( outBufLen );
		ret              = GetAdaptersAddresses( AF_INET, 0, NULL, adapterAddresses, &outBufLen );

		if ( ret == ERROR_BUFFER_OVERFLOW ) {
			free( adapterAddresses );
			continue;
		}

		if ( ret != ERROR_SUCCESS ) {
			fprintf( stderr, "GetAdaptersAddresses failed. %d\n", ret );
			free( adapterAddresses );
			WSACleanup();
			return NULL;
		}
		else
			break;
	} while ( TRUE );

	char defaultIp[ INET_ADDRSTRLEN ];

	GPtrArray* adapters = g_ptr_array_new();
	g_ptr_array_set_free_func( adapters, network_adapters_free );

	PIP_ADAPTER_ADDRESSES adapter = adapterAddresses;
	while ( adapter != NULL ) {
		if ( adapter->OperStatus == IfOperStatusUp && adapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK ) {
			PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress;
			while ( addr != NULL ) {
				if ( addr->Address.lpSockaddr->sa_family == AF_INET ) {
					struct sockaddr_in* ipAddr = ( struct sockaddr_in* )addr->Address.lpSockaddr;

					if ( !adapter->FriendlyName )
						continue;

					char* ipStr = inet_ntoa( ipAddr->sin_addr );

					struct NetworkAdapters* adap = g_new( struct NetworkAdapters, 1 );

					adap->ip_str = g_strdup( ipStr );
					adap->name   = g_utf16_to_utf8( adapter->FriendlyName, -1, NULL, NULL, NULL );

					g_ptr_array_add( adapters, adap );
				}

				addr = addr->Next;
			}
		}
		adapter = adapter->Next;
	}

	free( adapterAddresses );
	WSACleanup();

	return adapters;
}

#else

#include <arpa/inet.h>
#include <ifaddrs.h>

GPtrArray* get_network_adapters()
{
	GPtrArray* adapters = g_ptr_array_new();
	g_ptr_array_set_free_func( adapters, network_adapters_free );

	struct ifaddrs* ifAddrs = NULL;
	struct ifaddrs* ifa     = NULL;
	getifaddrs( &ifAddrs );

	char address_buffer[ INET_ADDRSTRLEN ];
	for ( ifa = ifAddrs; ifa != NULL; ifa = ifa->ifa_next ) {
		if ( ifa->ifa_addr == NULL )
			continue;

		if ( ifa->ifa_addr->sa_family == AF_INET ) {
			void* addr = &( ( struct sockaddr_in* )ifa->ifa_addr )->sin_addr;
			inet_ntop( AF_INET, addr, address_buffer, INET_ADDRSTRLEN );

			struct NetworkAdapters* adap = g_new( struct NetworkAdapters, 1 );

			adap->name   = g_strdup( ifa->ifa_name );
			adap->ip_str = g_strdup( address_buffer );

			g_ptr_array_add( adapters, adap );
		}
	}

	// 释放资源
	if ( ifAddrs != NULL )
		freeifaddrs( ifAddrs );

	return adapters;
}

#endif
