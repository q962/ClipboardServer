#include "image-to.h"

GBytes* image_to_x( GBytes* bytes, const char* format, int quality )
{
	MagickWand* mgw = NewMagickWand();

	if ( quality < 0 )
		quality = 90;

	int retval_count = 0;

	gsize         size = 0;
	gconstpointer data = g_bytes_get_data( bytes, &size );

	bytes = NULL;

	G_STMT_START
	{
		if ( !MagickReadImageBlob( mgw, data, size )          //
		     || !MagickSetImageFormat( mgw, format )          //
		     || !MagickSetImageCompressionQuality( mgw, quality )  //
		) {
			ExceptionType severity;
			char*         err = MagickGetException( mgw, &severity );
			fprintf( stderr, "ImageMagickReadImageBlob Error: [%s]\n", err );
			MagickRelinquishMemory( err );
			break;
		}

		size_t   jpeg_size   = 0;
		gpointer jpeg_buffer = MagickGetImageBlob( mgw, &jpeg_size );
		if ( !jpeg_buffer ) {
			break;
		}

		bytes = g_bytes_new( jpeg_buffer, jpeg_size );

		MagickRelinquishMemory( jpeg_buffer );
	}
	G_STMT_END;

	DestroyMagickWand( mgw );

	return bytes;
}