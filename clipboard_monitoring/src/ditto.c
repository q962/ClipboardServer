#include "ditto.h"

#include <MagickWand/MagickWand.h>

extern GtkApplication* app;
extern GdkClipboard*   clip;
extern GSettings*      settings;

static char*         ditto_db_path    = NULL;
static sqlite3*      ditto_db         = NULL;
static sqlite3_stmt* ditto_image_stmt = NULL;
static sqlite3_stmt* ditto_stmt       = NULL;
static gboolean      ditto_is_ok      = false;
static GtkLabel*     msg_label        = NULL;

struct ClipI impl = {};

static void ClipDataItem_free( gpointer user_data )
{
	struct ClipDataItem* item = user_data;
	g_free( item->data );
	g_free( item );
}

static int _get_clips_from_ditto( guint limit, guint count, struct ClipData* data )
{
	if ( !ditto_db )
		return 0;

	if ( !ditto_image_stmt || !ditto_stmt ) {
		return 0;
	}

	if ( count == 0 )
		return 0;

	if ( SQLITE_OK != sqlite3_bind_int( ditto_stmt, 1, limit ) ) {
		g_warning( "ditto_stmt bind error: %s", sqlite3_errmsg( ditto_db ) );
		gtk_label_set_label( msg_label, sqlite3_errmsg( ditto_db ) );

		return 0;
	}
	if ( SQLITE_OK != sqlite3_bind_int( ditto_stmt, 2, count ) ) {
		g_warning( "ditto_stmt bind error: %s", sqlite3_errmsg( ditto_db ) );
		gtk_label_set_label( msg_label, sqlite3_errmsg( ditto_db ) );
		return 0;
	}

	GArray*    ids       = g_array_new( TRUE, TRUE, sizeof( gsize ) );
	GPtrArray* itemsdata = g_ptr_array_new();
	g_ptr_array_set_free_func( itemsdata, ClipDataItem_free );

	data->ids  = ids;
	data->data = itemsdata;

	int ret = 0;
	while ( ( ret = sqlite3_step( ditto_stmt ) ) == SQLITE_ROW ) {
		sqlite3_int64 id   = sqlite3_column_int64( ditto_stmt, 0 );
		const char*   text = ( char* )sqlite3_column_text( ditto_stmt, 1 );

		g_array_append_val( ids, id );

		struct ClipDataItem* item = g_new( struct ClipDataItem, 1 );

		do {
			// image
			if ( g_strcmp0( text, "CF_DIB" ) == 0 ) {
				if ( SQLITE_OK == sqlite3_bind_int64( ditto_image_stmt, 1, id ) ) {
					int img_ret = sqlite3_step( ditto_image_stmt );
					if ( img_ret == SQLITE_ROW ) {
						const void* image_blob      = sqlite3_column_blob( ditto_image_stmt, 0 );
						int         image_blob_size = sqlite3_column_bytes( ditto_image_stmt, 0 );

						gpointer buffer = g_try_malloc( image_blob_size + 14 );

						*( ( uint8_t* )( buffer + 0 ) )          = 0x42;
						*( ( uint8_t* )( buffer + 1 ) )          = 0x4d;
						*( ( uint32_t* )( buffer + 2 ) )         = GUINT32_TO_LE( image_blob_size + 14 );
						*( ( uint32_t* )( buffer + 2 + 4 ) )     = 0;
						*( ( uint32_t* )( buffer + 2 + 4 + 4 ) ) = GUINT32_TO_LE( 0x36 );

						memmove( buffer + 14, image_blob, image_blob_size );

						image_blob_size += 14;

						MagickWand* mgw = NewMagickWand();
						if ( mgw && MagickReadImageBlob( mgw, buffer, image_blob_size )  //
						     && MagickSetImageFormat( mgw, "JPEG" )                      //
						     && MagickSetImageCompressionQuality( mgw, 70 )              //
						) {
							size_t   jpeg_size       = 0;
							gpointer jpeg_buffer     = MagickGetImageBlob( mgw, &jpeg_size );
							gpointer jpeg_buffer_dup = g_memdup2( jpeg_buffer, jpeg_size );

							g_free( buffer );

							buffer          = jpeg_buffer_dup;
							image_blob_size = jpeg_size;

							MagickRelinquishMemory( jpeg_buffer );
							DestroyMagickWand( mgw );
						}

						item->type = CLIP_DATA_TYPE_IMAGE;
						item->data = buffer;
						item->len  = image_blob_size;
						break;
					}
				}
				else {
					g_warning( "ditto_image_stmt bind error: %s", sqlite3_errmsg( ditto_db ) );
					sqlite3_reset( ditto_image_stmt );
				}
			}

			item->type = CLIP_DATA_TYPE_STRING;
			item->data = g_strdup( text );
			item->len  = strlen( text );
		} while ( 0 );

		g_ptr_array_add( itemsdata, item );
	}

	sqlite3_clear_bindings( ditto_image_stmt );
	sqlite3_reset( ditto_image_stmt );

	sqlite3_clear_bindings( ditto_stmt );
	sqlite3_reset( ditto_stmt );

	return 0;
}

static gboolean create_ditto_sql_stmt()
{
	if ( ditto_image_stmt )
		sqlite3_finalize( ditto_image_stmt ), ditto_image_stmt = NULL;
	if ( ditto_stmt )
		sqlite3_finalize( ditto_stmt ), ditto_stmt = NULL;
	if ( ditto_db )
		sqlite3_close( ditto_db ), ditto_db = NULL;

	ditto_is_ok = FALSE;
	gtk_label_set_label( msg_label, NULL );

	if ( !ditto_db_path ) {
		return FALSE;
	}

	int ret = sqlite3_open_v2( ditto_db_path, &ditto_db, SQLITE_OPEN_READONLY, NULL ) ||
	          sqlite3_exec( ditto_db,
	                        "select lID, mText as text from Main "
	                        "order by clipOrder desc "
	                        "limit 0, 1 ",
	                        NULL,
	                        NULL,
	                        NULL );
	if ( SQLITE_OK != ret )  //
	{
		g_settings_set_string( settings, "ditto-db-path", "" );

		g_warning( "%s", sqlite3_errstr( ret ) );

		gtk_label_set_label( msg_label, sqlite3_errstr( ret ) );

		return FALSE;
	}

	if ( SQLITE_OK != sqlite3_prepare_v2( ditto_db,
	                                      "select ooData from Data "
	                                      "where lParentId == ? ",
	                                      -1,
	                                      &ditto_image_stmt,
	                                      NULL ) ) {
		g_warning( "ditto_image_stmt error: %s", sqlite3_errmsg( ditto_db ) );

		gtk_label_set_label( msg_label, sqlite3_errmsg( ditto_db ) );

		return FALSE;
	}
	if ( SQLITE_OK != sqlite3_prepare_v2( ditto_db,
	                                      "select lID, mText as text from Main "
	                                      "order by clipOrder desc "
	                                      "limit ?, ? ",
	                                      -1,
	                                      &ditto_stmt,
	                                      NULL ) ) {
		g_warning( "ditto_stmt error: %s", sqlite3_errmsg( ditto_db ) );

		gtk_label_set_label( msg_label, sqlite3_errmsg( ditto_db ) );
	}

	ditto_is_ok = TRUE;
	return TRUE;
}

static void _choose_ditto_db_file_done( GObject* source_object, GAsyncResult* res, gpointer data )
{
	GtkFileDialog* file_dialog = ( gpointer )source_object;
	GtkLabel*      db_label    = g_object_get_data( ( gpointer )file_dialog, "db_label" );

	GError* error = NULL;
	GFile*  file  = gtk_file_dialog_open_finish( file_dialog, res, &error );
	if ( error ) {
		g_warning( "%s", error->message );
		gtk_label_set_label( msg_label, error->message );

		return;
	};

	char* db_path = g_file_get_path( file );
	if ( db_path ) {
		if ( g_strcmp0( db_path, ditto_db_path ) != 0 ) {
			g_set_str( &ditto_db_path, db_path );
			g_settings_set_string( settings, "ditto-db-path", ditto_db_path );

			gtk_label_set_label( ( gpointer )db_label, ditto_db_path );

			// restart
			if ( create_ditto_sql_stmt() ) {
				// 只有当成功的时候，才会记录当前使用的格式
				impl.get = _get_clips_from_ditto;
			}
			else {
				impl.get = NULL;
			}
		}
	}
	g_object_unref( file );
}

static void _choose_ditto_db_file( GtkButton* self, gpointer user_data )
{
	GtkFileDialog* file_dialog = user_data;

	gtk_file_dialog_open( file_dialog, NULL, NULL, _choose_ditto_db_file_done, NULL );
}

struct ClipI* ditto_selected_do( GtkStack* stack )
{
	static GtkBox*   box      = NULL;
	static GtkLabel* db_label = NULL;

	if ( !ditto_db_path ) {
		ditto_db_path = g_settings_get_string( settings, "ditto-db-path" );
		if ( strnlen( ditto_db_path, 1 ) == 0 ) {
			ditto_db_path = NULL;
		}

		impl.get = _get_clips_from_ditto;
		impl.set = NULL;
	}

	if ( !box ) {
		box = ( gpointer )gtk_box_new( GTK_ORIENTATION_VERTICAL, 10 );
		gtk_stack_add_child( stack, ( gpointer )box );

		db_label = ( gpointer )gtk_label_new( NULL );
		gtk_label_set_label( ( gpointer )db_label, ditto_db_path );

		gtk_box_append( box, ( gpointer )db_label );

		GtkFileFilter* file_filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern( file_filter, "*.db" );
		GtkFileDialog* file_dialog = gtk_file_dialog_new();
		gtk_file_dialog_set_default_filter( file_dialog, file_filter );
		g_object_unref( file_filter );

		g_object_set_data( ( gpointer )file_dialog, "db_label", db_label );

		GtkButton* open_file_btn = ( gpointer )gtk_button_new_with_label( _( "Open" ) );
		g_signal_connect( open_file_btn, "clicked", G_CALLBACK( _choose_ditto_db_file ), file_dialog );
		gtk_box_append( box, ( gpointer )open_file_btn );

		GtkLabel* tip = ( gpointer )gtk_label_new( _( "Select the right Ditto.ditto_db path to enable it." ) );
		gtk_box_append( box, ( gpointer )tip );

		g_object_weak_ref( ( gpointer )open_file_btn, ( GWeakNotify )g_object_unref, file_dialog );

		msg_label = ( gpointer )gtk_label_new( NULL );
		gtk_box_append( box, ( gpointer )msg_label );

		create_ditto_sql_stmt();
	}

	gtk_stack_set_visible_child( stack, ( gpointer )box );

	if ( ditto_is_ok )
		return &impl;

	return NULL;
}
