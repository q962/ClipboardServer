#include "stdafx.h"

#ifdef G_OS_WIN32
#include "ditto.h"
#endif

#include "clipdata_manager.h"
#include "client_ip.h"
#include "tray.h"
#include "luafuns.h"
#include "utils/res-utils.h"
#include "utils/image-to.h"
#include "clip_data_type.h"
#include "Clipboard.h"

#include <adwaita.h>
#include <locale.h>
#include <stdlib.h>

static GtkApplication* app       = NULL;
static Clipboard*      clipboard = NULL;

static AdwToastOverlay* toast_overlay = NULL;

GSettings* settings = NULL;

static lua_State* LuaS = NULL;

struct ClipI* clip_impl = NULL;

/////////////////////////

#if defined( G_OS_WIN32 )

HMODULE self_module = NULL;

static PROCESS_INFORMATION dns_sd_pi  = {};
static HANDLE              job_handle = NULL;

static void _close_mdns_process()
{
	if ( dns_sd_pi.hProcess ) {
		TerminateProcess( dns_sd_pi.hProcess, 0 );
		memset( &dns_sd_pi, 0, sizeof( dns_sd_pi ) );
	}
}

static void start_mdns( const char* alias, const char* ip, const char* port )
{
	const char* dnssd_path = app_libexec_path( "dnssd", NULL );

	char* server_cmd = g_strdup_printf( "%s register -Name Clipboard-server%s%s::%s:%s -Type _http._tcp -Port %s",
	                                    dnssd_path,
	                                    alias ? "-" : "",
	                                    alias ? alias : "",
	                                    ip,
	                                    port,
	                                    port );

	void* server_cmd_utf16 = g_utf8_to_utf16( server_cmd, -1, NULL, NULL, NULL );

	if ( !job_handle ) {
		job_handle = CreateJobObject( NULL, NULL );

		JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = {};

		limit_info.BasicLimitInformation.LimitFlags =
		  JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;  // job句柄回收时，所有加入job的进程都会强制结束。

		SetInformationJobObject( job_handle, JobObjectExtendedLimitInformation, &limit_info, sizeof( limit_info ) );
	}

	_close_mdns_process();

	STARTUPINFO si = {};

	ZeroMemory( &si, sizeof( si ) );
	si.cb          = sizeof( si );
	si.dwFlags     = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;  // 隐藏窗口

	// 创建进程，lpApplicationName是你要启动的控制台程序的路径
	if ( !CreateProcess( NULL,              // 不使用模块名，使用命令行
	                     server_cmd_utf16,  // 命令行
	                     NULL,              // 默认进程安全性
	                     NULL,              // 默认线程安全性
	                     FALSE,             // 不继承句柄
	                     CREATE_NO_WINDOW,  // 不创建新窗口
	                     NULL,              // 使用父进程的环境块
	                     NULL,              // 使用父进程的驱动目录
	                     &si,               // 指向STARTUPINFO结构的指针
	                     &dns_sd_pi ) )     // 指向PROCESS_INFORMATION结构的指针
	{
		printf( "CreateProcess failed (%d).\n", GetLastError() );
		return;
	}
	AssignProcessToJobObject( job_handle, dns_sd_pi.hProcess );

	g_free( server_cmd );
	g_free( server_cmd_utf16 );
}

#else

static GSubprocess* mdns_process = NULL;

static void _close_mdns_process()
{
	if ( !mdns_process )
		return;

	g_subprocess_force_exit( mdns_process );
	mdns_process = NULL;
}

static void start_mdns( const char* alias, const char* ip, const char* port )
{
	_close_mdns_process();

	const char* dnssd_path = "dnssd";

	char* server_name = g_strdup_printf( "Clipboard-server%s%s::%s:%s",  //
	                                     alias ? "-" : "",
	                                     alias ? alias : "",
	                                     ip,
	                                     port );

	GError* error = NULL;
	mdns_process  = g_subprocess_new( G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                                     &error,
                                     dnssd_path,
                                     "register",
                                     "-Name",
                                     server_name,
                                     "-Type",
                                     "_http._tcp",
                                     "-Port",
                                     port,
                                     NULL );

	if ( error ) {
		g_warning( "%s", error->message );
		g_error_free( error );
	}

	g_free( server_name );
};

#endif

static bool _lua_do( lua_State* L, int ret )
{
	if ( ret != LUA_OK ) {
		printf( "Error: %s\n", lua_tostring( L, -1 ) );
		lua_pop( L, 1 );  // pop error message
		return false;
	}

	return true;
}

static gboolean server_running = FALSE;
static void     _start_server( const char* alias, const char* ip_str, unsigned short port )
{
	char* statements =
	  g_strdup_printf( "require(\"gtkclip\").startServer(\"%s\",\"%s\", \"%hu\")", alias, ip_str, port );
	if ( luaL_dostring( LuaS, statements ) ) {
		const char* error_msg = lua_tostring( LuaS, -1 );
		fprintf( stderr, "Error _start_server: %s\n", error_msg );
	}

	server_running = lua_toboolean( LuaS, -1 );

	g_free( statements );
}

static void _stop_server()
{
#if defined( G_OS_WIN32 )
	CloseHandle( job_handle );
#endif

	_close_mdns_process();
}

/////

static GtkStack*    stack             = NULL;
static GtkWidget*   stack_empty_child = NULL;
static GtkTextView* msg_textView      = NULL;

static void _set_tip( const char* msg )
{
	adw_toast_overlay_add_toast( toast_overlay, adw_toast_new( msg ) );
}

#if 1  // data plan selects widget methods

static void _data_plan_selects_widget_selected_do( guint index )
{
	clip_impl = NULL;

	gtk_stack_set_visible_child( stack, stack_empty_child );

	switch ( index ) {
		// Self-management
		case 0: {
			clip_impl = clipdata_manager_selected_do( stack );
			break;
		}
#ifdef G_OS_WIN32
		// Ditto
		case 1: {
			clip_impl = ditto_selected_do( stack );
			break;
		}
#endif
	}

	if ( clip_impl )
		g_settings_set_uint( settings, "selected-index", index );
}

static void data_plan_selects_widget_selected( GtkDropDown* self, GParamSpec* spec, gpointer user_data )
{
	guint selected_index = gtk_drop_down_get_selected( self );
	_data_plan_selects_widget_selected_do( selected_index );
}

#endif

#if 1  // ip plan selects widget methods

static char*   current_ip_str    = NULL;
static char*   current_alias_str = NULL;
static guint16 current_port      = 0;

static void _try_bind( GtkButton* btn )
{
	int eq_count = 0;

	GtkDropDown* ip_plan_selects_widget = g_object_get_data( ( gpointer )btn, "selects" );
	GtkEntry*    port_widget            = g_object_get_data( ( gpointer )btn, "port_entry" );
	GtkEntry*    alias_widget           = g_object_get_data( ( gpointer )btn, "alias_widget" );

	if ( gtk_entry_get_text_length( port_widget ) == 0 ) {
		gtk_editable_set_text( ( gpointer )port_widget, "0" );
	}

	GtkStringObject* str_obj = gtk_drop_down_get_selected_item( ip_plan_selects_widget );
	if ( !str_obj ) {
		_set_tip( _( "IP not selected" ) );
		return;
	}

	const char* alias_str = gtk_editable_get_text( ( gpointer )alias_widget );
	for ( const char* ch = alias_str; ch && *ch; ch++ ) {
		const char chv = *ch;
		if ( g_ascii_isalnum( chv ) || chv == '-' || chv == '_' )
			continue;

		_set_tip( _( "Only characters allowed in aliases: 0-9a-Z_-" ) );
		return;
	}

	const char* port_str = gtk_editable_get_text( ( gpointer )port_widget );
	long        _port    = strtol( port_str, NULL, 10 );
	guint16     port     = ( guint16 )_port;
	if ( _port < 0 || _port > G_MAXUINT16 ) {
		gtk_editable_set_text( ( gpointer )port_widget, "0" );
		port = 0;
	}

	const char* ip_str  = g_object_get_data( ( gpointer )str_obj, "ip_str" );
	const char* if_name = g_object_get_data( ( gpointer )str_obj, "name" );

	if ( g_strcmp0( ip_str, current_ip_str ) == 0 )
		eq_count += 1;

	if ( g_strcmp0( alias_str, current_alias_str ) == 0 )
		eq_count += 1;

	if ( port != 0 && port == current_port )
		eq_count += 1;

	if ( eq_count == 3 )
		if ( server_running )
			return;

	GtkTextBuffer* buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( msg_textView ) );
	gtk_text_buffer_set_text( buffer, "", -1 );

	g_settings_set_string( settings, "network-interface", if_name );
	g_settings_set_string( settings, "alias", alias_str );
	g_settings_set_uint( settings, "port", port );

	g_free( current_alias_str );
	g_free( current_ip_str );

	current_alias_str = g_strdup( alias_str );
	current_ip_str    = g_strdup( ip_str );
	current_port      = port;

	_start_server( alias_str, ip_str, port );
}

static void ip_changed_done_cb( GtkButton* btn, gpointer user_data )
{
	_try_bind( btn );
}

#endif

static void update_ui_state( gpointer user_data )
{
	GtkDropDown* data_plan_selects_widget = user_data;

	guint selected_index = g_settings_get_uint( settings, "selected-index" );
	if ( selected_index == gtk_drop_down_get_selected( data_plan_selects_widget ) ) {
		_data_plan_selects_widget_selected_do( selected_index );
	}
	else
		gtk_drop_down_set_selected( data_plan_selects_widget, selected_index );
}

static void _on_text_data( Clipboard* clipboard, const char* mimetype, GBytes* bytes )
{
	if ( !clip_impl || !clip_impl->set )
		return;

	gsize       data_size = 0;
	const void* data      = g_bytes_get_data( bytes, &data_size );
	clip_impl->set( CLIP_DATA_TYPE_STRING, data, data_size );
}

static void _on_image_data( Clipboard* clipboard, const char* mimetype, GBytes* bytes )
{
	if ( !clip_impl || !clip_impl->set )
		return;

	gsize       data_size = 0;
	const void* data      = g_bytes_get_data( bytes, &data_size );
	clip_impl->set( CLIP_DATA_TYPE_IMAGE, data, data_size );
}

static void _on_clip_data( Clipboard*   clipboard,
                           guint        mimetype_count,
                           const char** mimetype_array,
                           GBytes**     bytes_array,
                           gpointer     user_data )
{
	for ( int i = 0; i < mimetype_count; i++ ) {
		const char* mimetype = mimetype_array[ i ];
		GBytes*     bytes    = bytes_array[ i ];

		if ( g_ascii_strncasecmp( mimetype, "image/", 6 ) == 0 ) {
			_on_image_data( clipboard, mimetype, bytes );
			return;
		}
	}

	for ( int i = 0; i < mimetype_count; i++ ) {
		const char* mimetype = mimetype_array[ i ];
		GBytes*     bytes    = bytes_array[ i ];

		if ( g_strcmp0( mimetype, "text/plain;charset=utf-8" ) == 0 ) {
			_on_text_data( clipboard, mimetype, bytes );
			return;
		}
	}
}

static gboolean _win_close( GtkWindow* window, gpointer user_data )
{
	GdkDisplay* display     = gdk_display_get_default();
	GdkSeat*    seat        = gdk_display_get_default_seat( display );
	GList*      device_list = gdk_seat_get_devices( seat, GDK_SEAT_CAPABILITY_KEYBOARD );

	for ( GList* device_list_iter = device_list; device_list_iter; device_list_iter = device_list_iter->next ) {
		GdkDevice*      device = device_list_iter->data;
		GdkModifierType type   = gdk_device_get_modifier_state( device );

		if ( type & GDK_SHIFT_MASK ) {
			g_application_quit( ( gpointer )app );
			break;
		}
	}

	g_list_free( device_list );

	return FALSE;
}

static void activate( GtkApplication* application )
{
	static GtkWindow* win = NULL;

	static int isRunning = 0;
	if ( isRunning ) {
		gtk_window_present( win );
		return;
	}
	isRunning = 1;

#ifdef G_OS_WIN32
	MagickWandGenesis();
#endif

	settings = g_settings_new( APPID );

	GdkDisplay* display = gdk_display_get_default();

	clipboard = clipboard_new();
	g_signal_connect( clipboard, CLIPBOARD_SIGNAL_NOTIFY, G_CALLBACK( _on_clip_data ), NULL );

	GBytes* css_data = g_resources_lookup_data( gres_path_translate( "styles/main.css" ), 0, NULL );
	if ( css_data ) {
		GtkCssProvider* provider = gtk_css_provider_new();
		gtk_css_provider_load_from_string( provider, g_bytes_get_data( css_data, NULL ) );
		gtk_style_context_add_provider_for_display(
		  display, ( gpointer )provider, GTK_STYLE_PROVIDER_PRIORITY_APPLICATION );

		g_bytes_unref( css_data );
	}

	win = ( gpointer )gtk_application_window_new( application );

	g_signal_connect( ( gpointer )win, "close_request", G_CALLBACK( _win_close ), NULL );
	gtk_window_set_hide_on_close( win, TRUE );
	gtk_window_set_title( ( gpointer )win, _( "Clipboard Server Manager" ) );
	gtk_window_set_default_icon_name( APPID );

	GtkBox* win_box = ( gpointer )gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_widget_add_css_class( ( gpointer )win_box, "main-box" );

#if 1  // box1

	GtkBox* box1 = ( gpointer )gtk_box_new( GTK_ORIENTATION_VERTICAL, 10 );
	gtk_widget_add_css_class( ( gpointer )box1, "block" );

	GtkLabel* ip_bind_label = ( gpointer )gtk_label_new( _( "Bind IP Address" ) );
	gtk_widget_set_halign( ( gpointer )ip_bind_label, GTK_ALIGN_START );
	gtk_box_append( box1, ( gpointer )ip_bind_label );

	GtkBox* ip_box = ( gpointer )gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 10 );

	GListStore* ip_store = g_list_store_new( GTK_TYPE_STRING_OBJECT );

	gint       if_index = -1;
	char*      if_name  = g_settings_get_string( settings, "network-interface" );
	GPtrArray* ifs      = get_network_adapters();

	for ( int i = 0; ifs && i < ifs->len; i++ ) {
		struct NetworkAdapters* adap = g_ptr_array_index( ifs, i );

		if ( g_strcmp0( adap->name, if_name ) == 0 )
			if_index = i;

		char*            str     = g_strdup_printf( "%s(%s)", adap->name, adap->ip_str );
		GtkStringObject* str_obj = gtk_string_object_new( str );
		g_object_set_data_full( ( gpointer )str_obj, "name", g_strdup( adap->name ), g_free );
		g_object_set_data_full( ( gpointer )str_obj, "ip_str", g_strdup( adap->ip_str ), g_free );

		g_list_store_append( ip_store, str_obj );

		g_object_unref( str_obj );
		g_free( str );
	}

	if ( ifs )
		g_ptr_array_free( ifs, TRUE );

	GtkDropDown* ip_plan_selects_widget = ( gpointer )gtk_drop_down_new( ( gpointer )ip_store, NULL );
	gtk_widget_set_hexpand( ( gpointer )ip_plan_selects_widget, TRUE );
	gtk_box_append( ip_box, ( gpointer )ip_plan_selects_widget );

	if ( if_index >= 0 )
		gtk_drop_down_set_selected( ip_plan_selects_widget, if_index );

	g_free( if_name );

	char* alias_str = g_settings_get_string( settings, "alias" );

	GtkEntry* alias_widget = ( gpointer )gtk_entry_new();
	gtk_widget_set_tooltip_text( ( gpointer )alias_widget,
	                             _( "Alias (optional) - used when publishing mDNS service" ) );
	gtk_entry_set_placeholder_text( alias_widget, _( "Alias" ) );
	gtk_entry_set_input_purpose( alias_widget, GTK_INPUT_PURPOSE_FREE_FORM );
	gtk_entry_set_max_length( alias_widget, 5 );
	gtk_editable_set_width_chars( ( gpointer )alias_widget, 5 );
	gtk_editable_set_max_width_chars( ( gpointer )alias_widget, 5 );
	gtk_editable_set_text( ( gpointer )alias_widget, alias_str );
	g_free( alias_str );

	gtk_box_append( ip_box, ( gpointer )alias_widget );

	GtkEntry* port_widget = ( gpointer )gtk_entry_new();
	gtk_widget_set_tooltip_text( ( gpointer )port_widget, _( "Random port assignment when set to 0" ) );
	gtk_entry_set_placeholder_text( port_widget, _( "Port" ) );
	gtk_entry_set_input_purpose( port_widget, GTK_INPUT_PURPOSE_NUMBER );
	gtk_entry_set_max_length( port_widget, 5 );
	gtk_editable_set_width_chars( ( gpointer )port_widget, 5 );
	gtk_editable_set_max_width_chars( ( gpointer )port_widget, 5 );

	guint port     = g_settings_get_uint( settings, "port" );
	char* port_str = g_strdup_printf( "%u", port );
	gtk_editable_set_text( ( gpointer )port_widget, port_str );
	g_free( port_str );

	gtk_box_append( ip_box, ( gpointer )port_widget );

	GtkButton* ip_changed_done_btn = ( gpointer )gtk_button_new_from_icon_name( "emblem-default-symbolic" );
	g_signal_connect( ip_changed_done_btn, "clicked", G_CALLBACK( ip_changed_done_cb ), NULL );
	g_object_set_data( ( gpointer )ip_changed_done_btn, "selects", ip_plan_selects_widget );
	g_object_set_data( ( gpointer )ip_changed_done_btn, "alias_widget", alias_widget );
	g_object_set_data( ( gpointer )ip_changed_done_btn, "port_entry", port_widget );

	gtk_box_append( ip_box, ( gpointer )ip_changed_done_btn );

	gtk_box_append( box1, ( gpointer )ip_box );

	msg_textView = ( gpointer )gtk_text_view_new();
	gtk_text_view_set_wrap_mode( msg_textView, GTK_WRAP_NONE );
	gtk_text_view_set_editable( msg_textView, FALSE );
	gtk_widget_add_css_class( GTK_WIDGET( msg_textView ), "app" );
	gtk_widget_set_halign( ( gpointer )msg_textView, GTK_ALIGN_FILL );
	gtk_widget_set_hexpand( ( gpointer )msg_textView, TRUE );
	gtk_widget_set_size_request( GTK_WIDGET( msg_textView ), 60, 30 );

	gtk_box_append( box1, ( gpointer )msg_textView );

	gtk_box_append( win_box, ( gpointer )box1 );

#endif

#if 1  // box2

	GtkBox* box2 = ( gpointer )gtk_box_new( GTK_ORIENTATION_VERTICAL, 10 );
	gtk_widget_add_css_class( ( gpointer )box2, "block" );

	GtkLabel* data_source_label = ( gpointer )gtk_label_new( _( "Data Source" ) );
	gtk_widget_set_halign( ( gpointer )data_source_label, GTK_ALIGN_START );
	gtk_box_append( box2, ( gpointer )data_source_label );

	static const char* db_plan[] = { NULL, NULL, NULL };
	db_plan[ 0 ]                 = _( "Self-managed" );
#ifdef G_OS_WIN32
	db_plan[ 1 ] = "Ditto";
#endif

	GtkDropDown* data_plan_selects_widget = ( gpointer )gtk_drop_down_new_from_strings( db_plan );
	g_signal_connect(
	  data_plan_selects_widget, "notify::selected", G_CALLBACK( data_plan_selects_widget_selected ), NULL );
	gtk_box_append( box2, ( gpointer )data_plan_selects_widget );

	g_idle_add_once( update_ui_state, data_plan_selects_widget );

	stack = ( gpointer )gtk_stack_new();
	gtk_stack_set_vhomogeneous( stack, FALSE );
	gtk_box_append( box2, ( gpointer )stack );

	stack_empty_child = ( gpointer )gtk_box_new( GTK_ORIENTATION_VERTICAL, 10 );
	gtk_stack_add_child( stack, stack_empty_child );

	gtk_box_append( win_box, ( gpointer )box2 );

#endif

	toast_overlay = ( gpointer )adw_toast_overlay_new();
	adw_toast_overlay_set_child( ( gpointer )toast_overlay, ( gpointer )win_box );
	gtk_window_set_child( win, ( gpointer )toast_overlay );

	gboolean is_first_run = g_settings_get_boolean( settings, "first-run" );
	if ( is_first_run )
		gtk_window_present( win );

	g_settings_set_boolean( settings, "first-run", FALSE );

	tray_init( win );

	if ( if_index >= 0 )
		_try_bind( ip_changed_done_btn );
}

static gint _handle_command_line( GtkApplication* application, GApplicationCommandLine* command_line )
{
	GVariantDict* options = g_application_command_line_get_options_dict( ( gpointer )command_line );

	static gboolean is_quit = FALSE;
	if ( is_quit )
		return 0;

	if ( g_variant_dict_lookup( options, "quit", "b", &is_quit ) ) {
		if ( is_quit ) {
			g_application_quit( ( gpointer )application );
			return 0;
		}
	}

	g_application_activate( ( gpointer )application );

	return 0;
}

static void set_share_path()
{
	const char* XDG_DATA_DIRS = g_getenv( "XDG_DATA_DIRS" );

	const char* app_prefix = app_share_path( NULL );

	char* new_dirs = g_strdup_printf( "%s%s%s",
	                                  XDG_DATA_DIRS ? XDG_DATA_DIRS : "",
	                                  app_prefix ? G_SEARCHPATH_SEPARATOR_S : "",
	                                  app_prefix ? app_prefix : "" );

	g_setenv( "XDG_DATA_DIRS", new_dirs, TRUE );
	g_free( new_dirs );
}

static gpointer _main( gpointer user_data )
{
	GStrv args = user_data;

	set_share_path();

	setlocale( LC_ALL, "" );
	bindtextdomain( GETTEXT_PACKAGE, LOCALE_PATH );
	bind_textdomain_codeset( GETTEXT_PACKAGE, "UTF-8" );
	textdomain( GETTEXT_PACKAGE );

	void gresources_register_resource( void );
	gresources_register_resource();

	g_setenv( "PANGOCAIRO_BACKEND", "fc", TRUE );
#ifdef G_OS_WIN32
	g_setenv( "GSK_RENDERER", "cairo", TRUE );
#endif

	adw_init();

	app = gtk_application_new( APPID, G_APPLICATION_HANDLES_COMMAND_LINE );

	GOptionEntry entries[] = { { "quit", 'q', G_OPTION_FLAG_NONE, G_OPTION_ARG_NONE, NULL, "application quit", NULL },
		                       G_OPTION_ENTRY_NULL };
	g_application_add_main_option_entries( ( gpointer )app, entries );

	g_signal_connect( app, "activate", G_CALLBACK( activate ), NULL );
	g_signal_connect( app, "command_line", G_CALLBACK( _handle_command_line ), NULL );

	int argc = 0;
	for ( char** _args = args; *_args; _args++ )
		argc++;

	g_application_run( ( gpointer )app, argc, args );

	g_strfreev( args );
	g_object_unref( app );

#ifdef G_OS_WIN32
	MagickWandTerminus();
#endif

	_stop_server();

	exit( EXIT_SUCCESS );

	return NULL;
}

static GThread* gtk_thread = NULL;
void            _main_wrapper( GStrv args )
{
	if ( gtk_thread )
		return;

	gtk_thread = g_thread_new( NULL, _main, args );
}

#if 1  // luafun api

static int _push_image( lua_State* L )
{
	size_t      buffer_size = 0;
	const char* str         = lua_tolstring( L, 1, &buffer_size );

	GBytes* image_bytes = g_bytes_new_static( str, buffer_size );

	clipboard_set_image( clipboard, image_bytes );

	g_bytes_unref( image_bytes );

	return 0;
}

static int _push_text( lua_State* L )
{
	size_t      buffer_size = 0;
	const char* str         = lua_tolstring( L, 1, &buffer_size );

	clipboard_set_text( clipboard, str, buffer_size );

	return 0;
}

// 返回二进制内容
static int _get_clips( lua_State* L )
{
	if ( !clip_impl || !clip_impl->get )
		return 0;

	int limit = luaL_checkinteger( L, 1 );
	int count = luaL_checkinteger( L, 2 );

	lua_settop( L, 0 );

	if ( count == 0 )
		return 0;

	struct ClipData data = {};
	clip_impl->get( limit, count, &data );

	if ( !data.ids || !data.data ) {
		return 0;
	}

	if ( data.data->len == 0 ) {
		g_array_free( data.ids, TRUE );
		g_ptr_array_free( data.data, TRUE );
		return 0;
	}

	// [ items, ids ]
	lua_newtable( L );

	size_t image_buffers_size = 0;

	// datas
	lua_newtable( L );
	for ( int i = 0; i < data.data->len; i++ ) {
		struct ClipDataItem* item = g_ptr_array_index( data.data, i );
		if ( item->type == CLIP_DATA_TYPE_STRING ) {
			lua_pushlstring( L, item->data, item->len );
		}
		else {
			lua_pushinteger( L, item->len );
			image_buffers_size += item->len;
		}

		lua_rawseti( L, -2, i + 1 );
	}
	lua_rawseti( L, -2, 1 );

	// ids
	lua_newtable( L );
	for ( int i = 0; i < data.ids->len; i++ ) {
		gsize id = g_array_index( data.ids, gsize, i );
		lua_pushinteger( L, id );
		lua_rawseti( L, -2, i + 1 );
	}
	lua_rawseti( L, -2, 2 );

	size_t   image_buffers_write_size = 0;
	gpointer image_buffers            = g_try_malloc( image_buffers_size );
	if ( image_buffers_size > 0 ) {
		for ( int i = 0; i < data.data->len; i++ ) {
			struct ClipDataItem* d = g_ptr_array_index( data.data, i );
			if ( d->type != CLIP_DATA_TYPE_IMAGE )
				continue;

			memmove( image_buffers + image_buffers_write_size, d->data, d->len );
			image_buffers_write_size += d->len;
		}

		lua_pushlstring( L, image_buffers, image_buffers_size );
	}

	g_array_free( data.ids, TRUE );
	g_ptr_array_free( data.data, TRUE );

	return image_buffers_size > 0 ? 2 : 1;
}

struct start_mdns_arg {
	char* alias;
	char* ip_str;
	char* port;
};
static void _start_mdns( void* user_data )
{
	struct start_mdns_arg* arg = user_data;

	start_mdns( arg->alias, arg->ip_str, arg->port );

	char* str = g_strdup_printf( _( "Web Service: http://%s:%s/web" ), arg->ip_str, arg->port );

	GtkTextBuffer* buffer = gtk_text_view_get_buffer( GTK_TEXT_VIEW( msg_textView ) );
	gtk_text_buffer_set_text( buffer, str, -1 );

	current_port = ( guint16 )strtoul( arg->port, NULL, 10 );

	g_free( str );

	g_free( arg->alias );
	g_free( arg->ip_str );
	g_free( arg->port );
	g_free( arg );
}

static int _set_port( lua_State* L )
{
	const char* port   = lua_tostring( L, 1 );
	const char* ip_str = lua_tostring( L, 2 );
	const char* alias  = lua_tostring( L, 3 );

	struct start_mdns_arg* arg = g_new0( struct start_mdns_arg, 1 );
	if ( alias && alias[ 0 ] != '\0' )
		arg->alias = g_strdup( alias );
	arg->ip_str = g_strdup( ip_str );
	arg->port   = g_strdup( port );
	g_idle_add_once( _start_mdns, arg );

	return 0;
}

static int _error( lua_State* L )
{
	const char* error_msg  = lua_tostring( L, 1 );
	const char* error_code = lua_tostring( L, 2 );

	char* str = g_strdup_printf( _( "Error: %s:%s" ), error_msg, error_code );
	_set_tip( str );
	g_free( str );

	return 0;
}

static int _web_root( lua_State* L )
{
#ifdef DEBUG
	lua_pushstring( L, g_getenv( "WEBROOT" ) );
#else
	lua_pushstring( L, app_data_path_translate( "web", NULL ) );
#endif

	return 1;
}

#endif

G_MODULE_EXPORT
int luaopen_gtkclip( lua_State* L )
{
	LuaS = L;

	load_luafuns();

	GStrvBuilder* args_builder = g_strv_builder_new();

	lua_getglobal( L, "args" );
	lua_pushnil( L );
	while ( lua_next( L, -2 ) != 0 ) {
		const char* val = lua_tostring( L, -1 );
		g_strv_builder_add( args_builder, val );

		lua_pop( L, 1 );
	}
	lua_pop( L, 1 );

	GStrv args = g_strv_builder_unref_to_strv( args_builder );
	_main_wrapper( args );

	lua_newtable( L );

	lua_pushstring( L, "push_text" );
	lua_pushcfunction( L, _push_text );
	lua_settable( L, -3 );

	lua_pushstring( L, "push_image" );
	lua_pushcfunction( L, _push_image );
	lua_settable( L, -3 );

	lua_pushstring( L, "get_clips" );
	lua_pushcfunction( L, _get_clips );
	lua_settable( L, -3 );

	lua_pushstring( L, "port" );
	lua_pushcfunction( L, _set_port );
	lua_settable( L, -3 );

	lua_pushstring( L, "error" );
	lua_pushcfunction( L, _error );
	lua_settable( L, -3 );

	lua_pushstring( L, "web_root" );
	lua_pushcfunction( L, _web_root );
	lua_settable( L, -3 );

	return 1;
}

#if defined( G_OS_WIN32 )

BOOL APIENTRY DllMain( HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved )
{
	self_module = hModule;
	return TRUE;
}

#endif