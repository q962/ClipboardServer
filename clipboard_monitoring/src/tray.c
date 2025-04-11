#include "tray.h"
#include "utils/res-utils.h"

#ifdef G_OS_WIN32
#include <gdk/win32/gdkwin32.h>
#include <wchar.h>
#include <MinHook.h>

static GtkWindow* _popup_window = NULL;

static HWND _get_widget_hwnd( GtkWidget* widget )
{
	if ( widget == NULL )
		return NULL;

	GtkNative*  native  = gtk_widget_get_native( widget );
	GdkSurface* surface = gtk_native_get_surface( native );

	if ( !surface )
		return NULL;

	HWND hwnd = gdk_win32_surface_get_handle( ( gpointer )surface );

	return hwnd;
}

static GdkWin32MessageFilterReturn _WindowProcedure( GdkWin32Display* display,
                                                     MSG*             message,
                                                     int*             return_value,
                                                     gpointer         user_data )
{
	GtkWindow* window = user_data;

	switch ( message->message ) {
		case WM_APP + 1: {
			switch ( message->lParam ) {
				case WM_LBUTTONDBLCLK: {
					gtk_window_present( window );
				} break;
				case WM_RBUTTONUP: {
					gtk_window_present( _popup_window );

					POINT pt;
					GetCursorPos( &pt );
					HWND hWnd = _get_widget_hwnd( ( gpointer )_popup_window );

					SetWindowLong( hWnd, GWL_EXSTYLE, GetWindowLong( hWnd, GWL_EXSTYLE ) | WS_EX_TOOLWINDOW );
					SetWindowPos( hWnd, HWND_TOPMOST, pt.x, pt.y, 0, 0, SWP_NOSIZE );

				} break;
			}
			break;
		}
		default: break;
	}

	return GDK_WIN32_MESSAGE_FILTER_CONTINUE;
}

extern HMODULE self_module;

static void _remove_icon( gpointer user_data )
{
	GtkWindow* window = user_data;

	NOTIFYICONDATA* nid = g_object_get_data( ( gpointer )window, "_icon" );

	Shell_NotifyIcon( NIM_DELETE, nid );

	g_free( nid );
}

static void _init_icon( GtkWindow* window )
{
	HWND hwnd = _get_widget_hwnd( ( gpointer )window );

	HICON hicon = LoadIcon( self_module, TEXT( "IDI_ICON1" ) );

	NOTIFYICONDATA* nid   = g_new0( NOTIFYICONDATA, 1 );
	nid->cbSize           = sizeof( NOTIFYICONDATA );
	nid->hWnd             = hwnd;
	nid->uID              = 1;  // 图标ID
	nid->uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid->hIcon            = hicon;
	nid->uCallbackMessage = WM_APP + 1;

	void* utf16 = g_utf8_to_utf16( _( "Clipboard Server Manager" ), -1, NULL, NULL, NULL );
	wcscpy_s( nid->szTip, sizeof( nid->szTip ), utf16 );
	g_free( utf16 );

	g_object_set_data( ( gpointer )window, "_icon", nid );

	Shell_NotifyIcon( NIM_ADD, nid );
}

static void _popup_state( GdkToplevel* toplevel, GParamSpec* property_id, GtkWindow* window )
{
	GdkToplevelState state = gdk_toplevel_get_state( toplevel );

	if ( gtk_widget_get_mapped( ( gpointer )window ) == FALSE )
		return;

	if ( state & GDK_TOPLEVEL_STATE_BELOW || state == 0 ) {
		gtk_widget_set_visible( ( gpointer )window, FALSE );
	}
}

static void _on_popup_realize( GtkWindow* popover, gpointer user_data )
{
	GtkNative*  native  = gtk_widget_get_native( ( gpointer )popover );
	GdkSurface* surface = gtk_native_get_surface( native );

	g_signal_connect( ( gpointer )surface, "notify::state", G_CALLBACK( _popup_state ), popover );
}

static void _create_popup( GtkWindow* window )
{
	GtkWindow* win = ( gpointer )gtk_window_new();
	_popup_window  = win;

	gtk_window_set_hide_on_close( win, TRUE );
	gtk_window_set_decorated( win, FALSE );
	gtk_widget_add_css_class( ( gpointer )win, "popup-win" );

	GtkButton* button = ( GtkButton* )gtk_button_new_with_label( _( "Quit" ) );

	g_signal_connect_swapped( ( gpointer )button, "clicked", G_CALLBACK( _remove_icon ), window );
	g_signal_connect_swapped(
	  ( gpointer )button, "clicked", G_CALLBACK( g_application_quit ), g_application_get_default() );
	gtk_window_set_child( win, ( gpointer )button );

	g_signal_connect( ( gpointer )win, "realize", G_CALLBACK( _on_popup_realize ), button );
}

#if 1  // hook

static LONG ( *old_SetWindowLongW )( HWND hWnd, int nIndex, LONG dwNewLong ) = NULL;
static LONG SetWindowLongWWrapper( HWND hWnd, int nIndex, LONG dwNewLong )
{
	if ( hWnd == _get_widget_hwnd( ( gpointer )_popup_window ) && nIndex == GWL_EXSTYLE ) {
		dwNewLong |= WS_EX_TOOLWINDOW;
	}

	return old_SetWindowLongW( hWnd, nIndex, dwNewLong );
}

static LONG ( *old_SetWindowLongA )( HWND hWnd, int nIndex, LONG dwNewLong ) = NULL;
static LONG SetWindowLongAWrapper( HWND hWnd, int nIndex, LONG dwNewLong )
{
	if ( hWnd == _get_widget_hwnd( ( gpointer )_popup_window ) && nIndex == GWL_EXSTYLE ) {
		dwNewLong |= WS_EX_TOOLWINDOW;
	}

	return old_SetWindowLongA( hWnd, nIndex, dwNewLong );
}

static WINBOOL ( *old_SetWindowPos )( HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags );
static WINBOOL SetWindowPosWrapper( HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags )
{
	if ( ( hWnd == _get_widget_hwnd( ( gpointer )_popup_window ) ) ) {
		hWndInsertAfter = HWND_TOPMOST;
		uFlags &= ~SWP_NOZORDER;
		uFlags &= ~SWP_NOACTIVATE;
	}

	return old_SetWindowPos( hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags );
}

#endif

void tray_init( GtkWindow* window )
{
	_create_popup( window );

#if 1  // hook
	MH_Initialize();

	MH_CreateHook( &SetWindowLongW, &SetWindowLongWWrapper, ( LPVOID* )&old_SetWindowLongW );
	MH_EnableHook( &SetWindowLongW );

	MH_CreateHook( &SetWindowLongA, &SetWindowLongAWrapper, ( LPVOID* )&old_SetWindowLongA );
	MH_EnableHook( &SetWindowLongA );

	MH_CreateHook( &SetWindowPos, &SetWindowPosWrapper, ( LPVOID* )&old_SetWindowPos );
	MH_EnableHook( &SetWindowPos );
#endif

	_init_icon( window );

	GdkDisplay* display = gdk_display_get_default();
	gdk_win32_display_add_filter( display, _WindowProcedure, window );
}

#else

void tray_init( GtkWindow* window ) {}

#endif
