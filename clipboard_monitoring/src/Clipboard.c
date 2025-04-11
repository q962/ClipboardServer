#include "Clipboard.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

#ifdef G_OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib-unix.h>
#include <gio/gunixinputstream.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#endif

#include "utils/res-utils.h"

static GdkClipboard* gdkclip = NULL;

#if 1  // gobject definition

struct _Clipboard {
	GObject parent_instance;

	GdkClipboard* gdk_clipboard;
};

G_DEFINE_TYPE( Clipboard, clipboard, g_object_get_type() )

enum {
	PROP_0,
	PROP_push,
	PROP_N,
} PROPS;

static guint SIGNAL_notify = 0;

#if 1  // static function

static void _clipboard_set_text( Clipboard* clipboard, const char* text, gssize size );
static void _clipboard_set_image( Clipboard* clipboard, GBytes* bytes );

#ifdef G_OS_UNIX

static void _clipboard_set_content( Clipboard* clipboard, const char* mimetype, const void* data, gssize size )
{
	GdkPixbufLoader* loader = gdk_pixbuf_loader_new();

	GdkPixbuf*  pixbuf  = NULL;
	GdkTexture* texture = NULL;

	GError* error = NULL;
	if ( gdk_pixbuf_loader_write( loader, data, size, &error ) ) {
		if ( gdk_pixbuf_loader_close( loader, &error ) ) {
			pixbuf  = gdk_pixbuf_loader_get_pixbuf( loader );
			texture = gdk_texture_new_for_pixbuf( pixbuf );
			gdk_clipboard_set_texture( gdkclip, texture );
			g_object_unref( texture );
		}
	}

	if ( error )
		g_error_free( error );
	g_object_unref( loader );
}

static void _clipboard_set_text( Clipboard* clipboard, const char* text, gssize size )
{
	gdk_clipboard_set_text( gdkclip, text );
}

static void _clipboard_set_image( Clipboard* clipboard, GBytes* bytes )
{
	gsize         source_data_size = 0;
	gconstpointer source_data      = g_bytes_get_data( bytes, &source_data_size );

	GdkPixbufLoader* loader = gdk_pixbuf_loader_new();

	GdkPixbuf*  pixbuf  = NULL;
	GdkTexture* texture = NULL;

	GError* error = NULL;
	if ( gdk_pixbuf_loader_write( loader, source_data, source_data_size, &error ) ) {
		if ( gdk_pixbuf_loader_close( loader, &error ) ) {
			pixbuf  = gdk_pixbuf_loader_get_pixbuf( loader );
			texture = gdk_texture_new_for_pixbuf( pixbuf );
			gdk_clipboard_set_texture( gdkclip, texture );
			g_object_unref( texture );
		}
	}

	g_clear_object( &loader );
	g_clear_error( &error );
}

#endif

#ifdef G_OS_WIN32

#include "utils/image-to.h"

static void _clipboard_set_text( Clipboard* clipboard, const char* text, gssize size )
{
	HWND hwnd = g_object_get_data( ( gpointer )clipboard, "clipboard_window" );

	if ( !OpenClipboard( hwnd ) )
		return;

	if ( size < 0 )
		size = strlen( text );

	EmptyClipboard();

	gunichar2* u16 = NULL;

	G_STMT_START
	{
		glong u16_len = 0;
		u16           = g_utf8_to_utf16( text, size, NULL, &u16_len, NULL );
		if ( !u16 )
			break;

		// EOL
		u16_len += 1;

		u16_len *= sizeof( gunichar2 );

		HGLOBAL hGlobal = GlobalAlloc( GMEM_MOVEABLE, u16_len );
		if ( !hGlobal )
			break;

		void* buffer = GlobalLock( hGlobal );
		memcpy( buffer, u16, u16_len );

		GlobalUnlock( hGlobal );

		SetClipboardData( CF_UNICODETEXT, hGlobal );
	}
	G_STMT_END;

	if ( u16 )
		g_free( u16 );

	CloseClipboard();
}

// 从 bmp 缓冲区中创建一个位图句柄
// from https://bbs.csdn.net/topics/270069007
static HBITMAP Helper_CreateBitmapFromBuffer( LPCBYTE pBuffer, DWORD cbSize )
{
	HBITMAP           hBmp = NULL;
	HDC               hdc  = NULL;
	HPALETTE          hPal = NULL, hOldPal = NULL;
	BITMAPFILEHEADER* pbmfHeader = ( BITMAPFILEHEADER* )pBuffer;
	DWORD             dwOffset   = sizeof( BITMAPFILEHEADER );

	BITMAPINFOHEADER* pbmiHeader = ( LPBITMAPINFOHEADER )( pBuffer + dwOffset );
	BITMAPINFO*       pbmInfo    = ( LPBITMAPINFO )( pBuffer + dwOffset );
	// If bmiHeader.biClrUsed is zero we have to infer the number
	// of colors from the number of bits used to specify it.
	int nColors = pbmiHeader->biClrUsed ? pbmiHeader->biClrUsed : 1 << pbmiHeader->biBitCount;

	LPVOID lpDIBBits;

	// File type should be 'BM'
	if ( pbmfHeader->bfType != ( ( WORD )( 'M' << 8 ) | 'B' ) )
		return NULL;
	if ( pbmInfo->bmiHeader.biBitCount > 8 )
		lpDIBBits = ( LPVOID )( ( LPDWORD )( pbmInfo->bmiColors + pbmInfo->bmiHeader.biClrUsed ) +
		                        ( ( pbmInfo->bmiHeader.biCompression == BI_BITFIELDS ) ? 3 : 0 ) );
	else
		lpDIBBits = ( LPVOID )( pbmInfo->bmiColors + nColors );

	hdc = GetDC( NULL );
	// Create the palette
	if ( nColors <= 256 ) {
		UINT        nSize = sizeof( LOGPALETTE ) + ( sizeof( PALETTEENTRY ) * nColors );
		LOGPALETTE* pLP   = ( LOGPALETTE* )malloc( nSize );
		int         i;
		pLP->palVersion    = 0x300;
		pLP->palNumEntries = nColors;

		for ( i = 0; i < nColors; i++ ) {
			pLP->palPalEntry[ i ].peRed   = pbmInfo->bmiColors[ i ].rgbRed;
			pLP->palPalEntry[ i ].peGreen = pbmInfo->bmiColors[ i ].rgbGreen;
			pLP->palPalEntry[ i ].peBlue  = pbmInfo->bmiColors[ i ].rgbBlue;
			pLP->palPalEntry[ i ].peFlags = 0;
		}
		hPal = CreatePalette( pLP );
		free( pLP );
		hOldPal = ( HPALETTE )SelectObject( hdc, hPal );
		RealizePalette( hdc );
	}

	hBmp = CreateDIBitmap( hdc,               // handle to device context
	                       pbmiHeader,        // pointer to bitmap size and format data
	                       CBM_INIT,          // initialization flag
	                       lpDIBBits,         // pointer to initialization data
	                       pbmInfo,           // pointer to bitmap color-format data
	                       DIB_RGB_COLORS );  // color-data usage
	if ( hPal ) {
		SelectObject( hdc, hOldPal );
		DeleteObject( hPal );
	}
	ReleaseDC( NULL, hdc );
	return hBmp;
}

static void _clipboard_set_image( Clipboard* clipboard, GBytes* bytes )
{
	HWND hwnd = g_object_get_data( ( gpointer )clipboard, "clipboard_window" );

	if ( !OpenClipboard( hwnd ) )
		return;

	EmptyClipboard();

	gsize         source_data_size = 0;
	gconstpointer source_data      = g_bytes_get_data( bytes, &source_data_size );

	MagickWand* mgw = NewMagickWand();

	GBytes* bmp_bytes = image_to_x( bytes, "bmp", 100 );

	G_STMT_START
	{
		gsize         size = 0;
		gconstpointer data = g_bytes_get_data( bmp_bytes, &size );

		// set bmp image
		HBITMAP hBitmap = Helper_CreateBitmapFromBuffer( ( LPCBYTE )data, size );
		SetClipboardData( CF_BITMAP, hBitmap );
		DeleteObject( hBitmap );

		/// set source image
		if ( !MagickReadImageBlob( mgw, source_data, source_data_size ) )
			break;

		char* image_format = MagickGetImageFormat( mgw );
		if ( !image_format ) {
			break;
		}

		char* image_format_mime       = g_strdup_printf( "image/%s", image_format );
		char* image_format_mime_lower = g_ascii_strdown( image_format_mime, -1 );

		UINT image_format_code = RegisterClipboardFormatA( image_format_mime_lower );

		g_free( image_format_mime_lower );
		g_free( image_format_mime );

		if ( !image_format_code )
			break;

		HGLOBAL image_hGlobal = GlobalAlloc( GMEM_MOVEABLE, source_data_size );
		if ( !image_hGlobal )
			break;

		void* buffer = GlobalLock( image_hGlobal );
		memcpy( buffer, source_data, ( size_t )source_data_size );

		GlobalUnlock( image_hGlobal );

		SetClipboardData( image_format_code, image_hGlobal );
	}
	G_STMT_END;

	g_bytes_unref( bmp_bytes );

	DestroyMagickWand( mgw );

	CloseClipboard();
}

static char* _get_format_string( UINT format )
{
	int   step_size   = 100;
	int   buffer_size = step_size;
	char* buffer      = g_try_new( char, buffer_size );
	if ( !buffer )
		return NULL;

	int write_size = 0;

	for ( ;; ) {
		write_size = GetClipboardFormatNameA( format, buffer, buffer_size );

		if ( write_size < buffer_size )
			break;

		buffer_size += step_size;
		buffer = g_try_realloc( buffer, buffer_size );
		if ( !buffer )
			break;
	}

	if ( write_size == 0 ) {
		g_free( buffer );
		buffer = NULL;
	}

	return buffer;
}

struct _clipboard_image_data {
	GBytes* bytes;
	char*   format;
};

static void _clipboard_image_data_free( struct _clipboard_image_data* data )
{
	g_return_if_fail( data != NULL );

	g_bytes_unref( data->bytes );
	g_free( data->format );
}

static GBytes* _get_clipboard_data_from_format( UINT format )
{
	HANDLE hClipMemory = GetClipboardData( format );
	if ( !hClipMemory )
		return NULL;

	LPBYTE lpClipMemory = GlobalLock( hClipMemory );
	DWORD  dwLength     = GlobalSize( hClipMemory );

	GBytes* bytes = g_bytes_new( lpClipMemory, dwLength );

	GlobalUnlock( hClipMemory );

	return bytes;
}

static GArray* _get_images_from_win_clipboard()
{
	UINT uFormat = EnumClipboardFormats( 0 );

	GArray* image_array = g_array_new( TRUE, TRUE, sizeof( struct _clipboard_image_data ) );
	g_array_set_clear_func( image_array, ( GDestroyNotify )_clipboard_image_data_free );

	gboolean has_bmp = FALSE;

	for ( UINT uFormat = EnumClipboardFormats( 0 ); uFormat; uFormat = EnumClipboardFormats( uFormat ) ) {
		char* format_string = _get_format_string( uFormat );
		if ( !format_string )
			continue;

		if ( g_ascii_strncasecmp( format_string, "image/bmp", 9 ) == 0 )
			has_bmp = TRUE;

		if ( g_ascii_strncasecmp( format_string, "image/", 6 ) == 0 ) {
			struct _clipboard_image_data data = {};

			GBytes* foramt_data = _get_clipboard_data_from_format( uFormat );

			data.bytes  = foramt_data;
			data.format = g_strdup( format_string );

			g_array_append_val( image_array, data );
		}

		g_free( format_string );
	}

	if ( has_bmp )
		return image_array;

	do {
		if ( !IsClipboardFormatAvailable( CF_DIB ) )
			break;

		HANDLE hClipMemory = GetClipboardData( CF_DIB );
		if ( !hClipMemory )
			break;

		LPBYTE lpClipMemory = GlobalLock( hClipMemory );
		DWORD  dwLength     = GlobalSize( hClipMemory );

		BITMAPFILEHEADER bmfh;
		bmfh.bfType      = 0x4D42;  // "BM"
		bmfh.bfSize      = GUINT32_TO_LE( sizeof( BITMAPFILEHEADER ) + dwLength );
		bmfh.bfReserved1 = 0;
		bmfh.bfReserved2 = 0;
		bmfh.bfOffBits   = GUINT32_TO_LE( sizeof( BITMAPFILEHEADER ) + GUINT32_FROM_LE( *lpClipMemory ) );

		void* bmp_buffer = g_new( guchar, sizeof( BITMAPFILEHEADER ) + dwLength );
		memcpy( bmp_buffer, ( guchar* )&bmfh, sizeof( BITMAPFILEHEADER ) );
		memcpy( bmp_buffer + sizeof( BITMAPFILEHEADER ), lpClipMemory, dwLength );

		GBytes* bytes = g_bytes_new_take( bmp_buffer, sizeof( BITMAPFILEHEADER ) + dwLength );

		struct _clipboard_image_data data = {};

		data.bytes  = bytes;
		data.format = g_strdup( "image/bmp" );

		g_array_append_val( image_array, data );

		GlobalUnlock( hClipMemory );

	} while ( 0 );

	if ( image_array->len == 0 ) {
		g_array_free( image_array, TRUE );
		image_array = NULL;
	}

	return image_array;
}

static GBytes* _get_text_from_win_clipboard()
{
	if ( !IsClipboardFormatAvailable( CF_UNICODETEXT ) )
		return NULL;

	HANDLE hClipMemory = GetClipboardData( CF_UNICODETEXT );
	if ( !hClipMemory )
		return NULL;

	LPBYTE lpClipMemory = GlobalLock( hClipMemory );
	DWORD  dwLength     = GlobalSize( hClipMemory );

	GBytes* bytes = NULL;

	glong str_len = 0;
	char* str     = g_utf16_to_utf8( ( gunichar2* )lpClipMemory, dwLength, NULL, &str_len, NULL );
	if ( str )
		bytes = g_bytes_new_take( str, str_len );

	GlobalUnlock( hClipMemory );

	return bytes;
}

static void _signal_emit( gpointer arg )
{
	struct emit_arg {
		Clipboard* clipboard;
		GPtrArray* mimetype_array;
		GPtrArray* data_array;
	}* emit_arg = arg;

	g_signal_emit( ( gpointer )emit_arg->clipboard,
	               SIGNAL_notify,
	               0,
	               emit_arg->mimetype_array->len,
	               emit_arg->mimetype_array->pdata,
	               emit_arg->data_array->pdata );

	g_ptr_array_unref( emit_arg->mimetype_array );
	g_ptr_array_unref( emit_arg->data_array );
	g_free( arg );
}

static LRESULT CALLBACK _clipboard_window_proc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam )
{
	LRESULT retval = 0;

	static HWND hwndNextViewer;

	switch ( message ) {
		case WM_CHANGECBCHAIN: {
			if ( ( HWND )wparam == hwndNextViewer )
				hwndNextViewer = ( HWND )lparam;

			else if ( hwndNextViewer != NULL )
				SendMessage( hwndNextViewer, message, wparam, lparam );
		} break;

		case WM_DESTROY: {
			ChangeClipboardChain( hwnd, hwndNextViewer );
			PostQuitMessage( 0 );
		} break;
		case WM_CREATE: {
			hwndNextViewer = SetClipboardViewer( hwnd );

		} break;
		case WM_DRAWCLIPBOARD: {
			Clipboard* clipboard = ( void* )GetWindowLongPtr( hwnd, GWLP_USERDATA );
			if ( !clipboard )
				break;

			if ( !OpenClipboard( hwnd ) )
				break;

			GArray* images = NULL;
			GBytes* text   = NULL;

			// char*
			GPtrArray* mimetype_array = g_ptr_array_new();
			// GBytes*
			GPtrArray* data_array = g_ptr_array_new();

			do {
				images = _get_images_from_win_clipboard();
				if ( images && images->len > 0 ) {
					for ( int i = 0; i < images->len; i++ ) {
						struct _clipboard_image_data* data = &g_array_index( images, struct _clipboard_image_data, i );

						g_ptr_array_add( mimetype_array, g_strdup( data->format ) );
						g_ptr_array_add( data_array, g_bytes_ref( data->bytes ) );
						g_ptr_array_set_free_func( mimetype_array, ( GDestroyNotify )g_free );
						g_ptr_array_set_free_func( data_array, ( GDestroyNotify )g_bytes_unref );
					}
				}

				text = _get_text_from_win_clipboard();
				if ( text ) {
					g_ptr_array_add( mimetype_array, "text/plain;charset=utf-8" );
					g_ptr_array_add( data_array, text );
					g_ptr_array_set_free_func( data_array, ( GDestroyNotify )g_bytes_unref );
				}
			} while ( 0 );

			CloseClipboard();

			struct emit_arg {
				Clipboard* clipboard;
				GPtrArray* mimetype_array;
				GPtrArray* data_array;
			}* emit_arg              = g_new( struct emit_arg, 1 );
			emit_arg->clipboard      = clipboard;
			emit_arg->mimetype_array = mimetype_array;
			emit_arg->data_array     = data_array;

			g_idle_add_once( _signal_emit, emit_arg );

			if ( images )
				g_array_free( images, TRUE );

			SendMessage( hwndNextViewer, message, wparam, lparam );

		} break;

		default: return DefWindowProcW( hwnd, message, wparam, lparam );
	}

	return retval;
}

static gpointer _create_clipboard_window_thread( Clipboard* clipboard )
{
	WNDCLASS wclass = { 0 };
	ATOM     klass;

	wclass.lpszClassName = L"ClipboardNotification";
	wclass.lpfnWndProc   = _clipboard_window_proc;
	wclass.hInstance     = NULL;
	wclass.cbWndExtra    = sizeof( void* );

	klass = RegisterClass( &wclass );
	if ( !klass )
		return NULL;

	HWND clipboard_window =
	  CreateWindow( MAKEINTRESOURCE( klass ), NULL, WS_POPUP, 0, 0, 0, 0, NULL, NULL, NULL, NULL );

	SetWindowLongPtr( clipboard_window, GWLP_USERDATA, ( LONG_PTR )clipboard );

	g_object_set_data( ( gpointer )clipboard, "clipboard_window", clipboard_window );

	MSG msg;
	while ( GetMessage( &msg, NULL, 0, 0 ) ) {
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}

	return NULL;
}

static void _create_clipboard_window( Clipboard* clipboard )
{
	g_thread_new( NULL, ( GThreadFunc )_create_clipboard_window_thread, clipboard );
}

#endif

struct _clipboard_read_data {
	char*       text;
	GdkTexture* texture;
	int         count;

	Clipboard* clipboard;
};

static void _read_finish( struct _clipboard_read_data* data, GError* error )
{
	if ( data->count != 2 )
		return;

	if ( !g_error_matches( error, G_IO_ERROR, G_IO_ERROR_CANCELLED ) ) {
		GPtrArray* mimetype_array = g_ptr_array_new();
		GPtrArray* data_array     = g_ptr_array_new();

		if ( data->texture ) {
			GBytes* bytes = gdk_texture_save_to_png_bytes( data->texture );
			g_clear_object( &data->texture );

			g_ptr_array_add( mimetype_array, "image/png" );
			g_ptr_array_add( data_array, bytes );
			g_ptr_array_set_free_func( data_array, ( GDestroyNotify )g_bytes_unref );
		}

		if ( data->text ) {
			g_ptr_array_add( mimetype_array, "text/plain;charset=utf-8" );
			GBytes* bytes = g_bytes_new_take( data->text, strlen( data->text ) );
			g_ptr_array_add( data_array, bytes );
			g_ptr_array_set_free_func( data_array, ( GDestroyNotify )g_bytes_unref );
		}

		g_signal_emit(
		  data->clipboard, SIGNAL_notify, 0, mimetype_array->len, mimetype_array->pdata, data_array->pdata );
		g_ptr_array_unref( mimetype_array );
		g_ptr_array_unref( data_array );
	}
	else {
		if ( data->texture )
			g_clear_object( &data->texture );

		if ( data->text )
			g_free( data->text );
	}

	g_clear_object( &data->clipboard );

	g_free( data );
}

static void _read_texture_finish( GObject* object, GAsyncResult* result, gpointer user_data )
{
	struct _clipboard_read_data* data = user_data;
	data->count++;

	GError* error = NULL;

	data->texture = gdk_clipboard_read_texture_finish( ( gpointer )object, result, &error );

	_read_finish( data, error );

	g_clear_error( &error );
}

static void _read_text_finish( GObject* object, GAsyncResult* result, gpointer user_data )
{
	struct _clipboard_read_data* data = user_data;
	data->count++;

	GError* error = NULL;

	data->text = gdk_clipboard_read_text_finish( ( gpointer )object, result, &error );

	_read_finish( data, error );

	g_clear_error( &error );
}

static void _clipboard_owner_changd( Clipboard* clipboard )
{
	static GCancellable* cancellable = NULL;

	if ( cancellable ) {
		g_cancellable_cancel( cancellable );
		g_clear_object( &cancellable );
	}
	cancellable = g_cancellable_new();

	struct _clipboard_read_data* data = g_new0( struct _clipboard_read_data, 1 );
	data->clipboard                   = g_object_ref( clipboard );

	gdk_clipboard_read_text_async( gdkclip, cancellable, _read_text_finish, data );
	gdk_clipboard_read_texture_async( gdkclip, cancellable, _read_texture_finish, data );
}

#endif

#if 1  // base class virtual function

static void clipboard_dispose( GObject* object )
{
	Clipboard* self = ( Clipboard* )object;

	G_OBJECT_CLASS( clipboard_parent_class )->dispose( object );
}

static void clipboard_finalize( GObject* object )
{
	Clipboard* self = ( Clipboard* )object;

	G_OBJECT_CLASS( clipboard_parent_class )->finalize( object );
}

static void clipboard_init( Clipboard* self )
{
	Clipboard* clipboard = self;

	GdkDisplay* display = gdk_display_get_default();
	gdkclip             = gdk_display_get_clipboard( display );

	g_signal_connect_swapped( gdkclip, "changed", G_CALLBACK( _clipboard_owner_changd ), clipboard );

#ifdef G_OS_WIN32

	_create_clipboard_window( self );

#endif
}

static void clipboard_class_init( ClipboardClass* klass )
{
	GObjectClass* base_class   = ( GObjectClass* )klass;
	GObjectClass* parent_class = ( GObjectClass* )klass;

	base_class->dispose  = clipboard_dispose;
	base_class->finalize = clipboard_finalize;

	SIGNAL_notify = g_signal_new_class_handler( CLIPBOARD_SIGNAL_NOTIFY,
	                                            clipboard_get_type(),
	                                            G_SIGNAL_RUN_LAST,
	                                            NULL,
	                                            NULL,
	                                            NULL,
	                                            NULL,
	                                            G_TYPE_NONE,
	                                            3,
	                                            G_TYPE_UINT,
	                                            G_TYPE_POINTER,
	                                            G_TYPE_POINTER );
}

#endif

#if 1  // public function

Clipboard* clipboard_new()
{
	return g_object_new( clipboard_get_type(), NULL );
}

void clipboard_set_text( Clipboard* clipboard, const char* text, gssize size )
{
	g_return_if_fail( CLIPBOARD_IS_( clipboard ) );
	g_return_if_fail( text != NULL );

	if ( size < 0 )
		size = strlen( text );

	_clipboard_set_text( clipboard, text, size );
}

void clipboard_set_image( Clipboard* clipboard, GBytes* bytes )
{
	g_return_if_fail( CLIPBOARD_IS_( clipboard ) );
	g_return_if_fail( bytes != NULL );

	_clipboard_set_image( clipboard, bytes );
}

#endif

#endif
