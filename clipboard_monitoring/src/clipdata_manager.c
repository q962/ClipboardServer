#include "clipdata_manager.h"
#include "utils/res-utils.h"

extern GSettings* settings;

static sqlite3*      db          = NULL;
static struct ClipI  impl        = {};
static sqlite3_stmt* data_stmt   = NULL;
static sqlite3_stmt* insert_stmt = NULL;

GtkBox* main_box = NULL;

static void ClipDataItem_free( gpointer user_data )
{
	struct ClipDataItem* item = user_data;
	g_free( item->data );
	g_free( item );
}

static int _clipdata_get( guint limit, guint count, struct ClipData* data )
{
	if ( !db || !data_stmt )
		return 0;

	if ( count == 0 )
		return 0;

	if ( SQLITE_OK != sqlite3_bind_int( data_stmt, 1, limit ) ||
	     SQLITE_OK != sqlite3_bind_int( data_stmt, 2, count ) ) {
		g_warning( "sqlite error: %s", sqlite3_errmsg( db ) );

		return 0;
	}

	GArray*    ids       = g_array_new( TRUE, TRUE, sizeof( gsize ) );
	GPtrArray* itemsdata = g_ptr_array_new();
	g_ptr_array_set_free_func( itemsdata, ClipDataItem_free );

	data->ids  = ids;
	data->data = itemsdata;

	int ret = 0;
	while ( ( ret = sqlite3_step( data_stmt ) ) == SQLITE_ROW ) {
		sqlite3_int64 id   = sqlite3_column_int64( data_stmt, 0 );
		sqlite3_int64 type = sqlite3_column_int64( data_stmt, 1 );
		gconstpointer data = sqlite3_column_blob( data_stmt, 2 );
		gsize         size = sqlite3_column_bytes( data_stmt, 2 );

		g_array_append_val( ids, id );

		struct ClipDataItem* item = g_new( struct ClipDataItem, 1 );
		item->type                = type;
		item->data                = g_memdup2( data, size );
		item->len                 = size;

		g_ptr_array_add( itemsdata, item );
	}

	sqlite3_clear_bindings( data_stmt );
	sqlite3_reset( data_stmt );

	return 0;
}

static int _clipdata_set( enum ClipDataType type, gconstpointer data, gsize data_size )
{
	if ( !db || !insert_stmt )
		return 0;

	static char* last_md5 = NULL;

	char* md5 = g_compute_checksum_for_data( G_CHECKSUM_MD5, data, data_size );

	if ( g_strcmp0( last_md5, md5 ) == 0 ) {
		g_free( md5 );
		return 0;
	}

	g_free( last_md5 );
	last_md5 = md5;

	sqlite3_clear_bindings( insert_stmt );
	sqlite3_reset( insert_stmt );

	if ( SQLITE_OK != sqlite3_bind_int( insert_stmt, 1, type ) ||
	     SQLITE_OK != sqlite3_bind_blob( insert_stmt, 2, data, data_size, NULL ) ) {
		g_warning( "sqlite error: %s", sqlite3_errmsg( db ) );
		return 0;
	}
	if ( sqlite3_step( insert_stmt ) != SQLITE_DONE ) {
		g_warning( "sqlite error: %s", sqlite3_errmsg( db ) );
	}

	sqlite3_clear_bindings( data_stmt );
	sqlite3_reset( data_stmt );

	return 0;
}

static void init_stmt( sqlite3* db )
{
	if ( SQLITE_OK != sqlite3_prepare_v2( db,
	                                      "SELECT id, type, data FROM list "
	                                      "ORDER BY id DESC "
	                                      "LIMIT ?, ? ",
	                                      -1,
	                                      &data_stmt,
	                                      NULL ) ) {
		goto fail;
	}

	if ( SQLITE_OK !=
	     sqlite3_prepare_v2( db, "INSERT INTO list('type', 'data') VALUES(?, ?)", -1, &insert_stmt, NULL ) ) {
		goto fail;
	}

	return;

fail:
	g_warning( "sqlite error: %s", sqlite3_errmsg( db ) );

	return;
}

static void init_terigger( sqlite3* db )
{
	static const char* stmt_str = "DROP TRIGGER IF EXISTS list_auto_delete;"
	                              "CREATE TRIGGER list_auto_delete AFTER INSERT "
	                              "ON list "
	                              "BEGIN "
	                              "  DELETE FROM list WHERE "
	                              "    id <= (SELECT id FROM list ORDER BY id DESC LIMIT %u, 1);"
	                              "END;";

	guint max_count = g_settings_get_uint( settings, "clipdata-max-count" );
	if ( max_count == 0 ) {
		sqlite3_exec( db, "DROP TRIGGER IF EXISTS list_auto_delete;", NULL, NULL, NULL );
		return;
	}

	char* sql_str = g_strdup_printf( stmt_str, max_count );
	if ( sqlite3_exec( db, sql_str, NULL, NULL, NULL ) != SQLITE_OK ) {
		g_warning( "sqlite error: %s", sqlite3_errmsg( db ) );
	}
	g_free( sql_str );
}

static void done_btn_cb( GtkEntry* entry )
{
	const char* buffer = gtk_editable_get_text( ( gpointer )entry );
	if ( !buffer || buffer[ 0 ] == '\0' )
		return;

	guint count_max = 0;
	if ( 1 != sscanf( buffer, "%u", &count_max ) ) {
		return;
	}

	g_settings_set_uint( settings, "clipdata-max-count", count_max );
}

struct ClipI* clipdata_manager_selected_do( GtkStack* stack )
{
	if ( g_once_init_enter_pointer( ( gpointer* )&db ) ) {
		GFile* file = g_file_new_build_filename( user_data_path_translate( "/data", NULL ), NULL );
		g_file_make_directory( file, NULL, NULL );
		g_object_unref( file );

		char* db_path = g_strdup( user_data_path_translate( "/data/clipdata.db", NULL ) );

		sqlite3* _db = NULL;
		if ( SQLITE_OK != sqlite3_open( db_path, &_db ) ) {
			g_free( db_path );
			return NULL;
		}
		g_free( db_path );

		impl.get = _clipdata_get;
		impl.set = _clipdata_set;

		if ( SQLITE_OK !=
		     sqlite3_exec(
		       _db,
		       "CREATE TABLE IF NOT EXISTS list (id INTEGER PRIMARY KEY AUTOINCREMENT, type INTEGER, data BLOB);",
		       NULL,
		       0,
		       NULL ) )  //
		{
			g_warning( "sqlite error: %s", sqlite3_errmsg( _db ) );
		}

		init_stmt( _db );

		init_terigger( _db );
		g_signal_connect_swapped( settings, "changed::clipdata-max-count", G_CALLBACK( init_terigger ), _db );

		///////////////////////////

		GtkBox* box = ( gpointer )gtk_box_new( GTK_ORIENTATION_VERTICAL, 10 );
		gtk_stack_add_child( stack, ( gpointer )box );

		GtkBox* b1 = ( gpointer )gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 10 );
		gtk_box_append( box, ( gpointer )b1 );

		gtk_box_append( b1, ( gpointer )gtk_label_new( _( "Row Limit:" ) ) );
		GtkEntry* count_entry = ( gpointer )gtk_entry_new();
		gtk_entry_set_input_purpose( count_entry, GTK_INPUT_PURPOSE_NUMBER );
		gtk_box_append( b1, ( gpointer )count_entry );
		gtk_widget_set_hexpand( ( gpointer )count_entry, TRUE );
		guint count_max     = g_settings_get_uint( settings, "clipdata-max-count" );
		char* count_max_str = g_strdup_printf( "%u", count_max );
		gtk_editable_set_text( ( gpointer )count_entry, count_max_str );
		g_free( count_max_str );

		// 监听事件，防抖动

		GtkButton* done_btn = ( gpointer )gtk_button_new_from_icon_name( "emblem-default-symbolic" );
		g_signal_connect_swapped( done_btn, "clicked", G_CALLBACK( done_btn_cb ), count_entry );

		gtk_box_append( b1, ( gpointer )done_btn );

		main_box = box;

		g_once_init_leave_pointer( ( gpointer* )&db, _db );
	}

	gtk_stack_set_visible_child( stack, ( gpointer )main_box );

	return &impl;
}
