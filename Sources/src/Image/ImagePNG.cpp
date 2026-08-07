#include "StdAfx.h"

#include "ImagePNG.h"

#include "../libpng/png.h"
enum EBMMTypes
{
	BMM_NO_TYPE,
	BMM_PALETTED,
	BMM_TRUE_32,
	BMM_GRAY_8
};
bool NImage::RecognizeFormatPNG( IDataStream *pStream )
{
	BYTE signature[8];
	int nCounter = pStream->Read( signature, 8 );
	pStream->Seek( -nCounter, STREAM_SEEK_CUR );
	if ( nCounter != 8 )
		return false;
	return png_check_sig( signature, 8 ) != 0;
}
void PNGReadFunction( png_structp png_ptr, png_bytep data, png_size_t length )
{
	IDataStream *pStream = reinterpret_cast<IDataStream*>( png_ptr->io_ptr );
	int check = pStream->Read( data, length );
	if ( check != length )
		png_error(png_ptr, "Read Error");
}
void PNGWriteFunction( png_structp png_ptr, png_bytep data, png_size_t length )
{
	IDataStream *pStream = reinterpret_cast<IDataStream*>( png_ptr->io_ptr );
	int check = pStream->Write( data, length );
	if ( check != length )
		png_error(png_ptr, "Write Error");
}
void PNGFlushFunction( png_structp png_ptr )
{
}
CImage* NImage::LoadImagePNG( IDataStream *pStream )
{
	png_struct *png = 0;
	png_info *info = 0;
	png_bytep	*row_pointers = 0;
  png = png_create_read_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
	if ( png == 0 )
		return 0;
  if ( setjmp(png->jmpbuf) )
	{
    if ( info )
		{
		  for ( png_uint_32 i=0; i<info->height; i++ )
			{
    		if ( row_pointers[i] )
					free( row_pointers[i] );
			}
		}
		if ( row_pointers )
		{
			free( row_pointers );
			row_pointers = 0;
		}
	}
  info = png_create_info_struct( png );
	png_set_read_fn( png, pStream, PNGReadFunction );
	png_read_info( png, info );
	DWORD dwWidth = info->width;
	DWORD dwHeight = info->height;
	std::vector<DWORD> image( static_cast<size_t>(dwWidth) * static_cast<size_t>(dwHeight) );


	if ( ( info->color_type == PNG_COLOR_TYPE_PALETTE && info->bit_depth < 8 ) ||
		   ( info->color_type == PNG_COLOR_TYPE_GRAY && info->bit_depth < 8 ) ||
		   ( info->valid & PNG_INFO_tRNS ) )
		png_set_expand( png );

	int nNumPasses = 1;
	if ( info->interlace_type )
		nNumPasses = png_set_interlace_handling( png );

	if ( info->bit_depth == 16 )
		png_set_swap( png );

	png_read_update_info( png, info );
	int bmtype = BMM_NO_TYPE;
	if ( info->bit_depth != 1 )
	{
		switch( info->color_type )
		{
			case PNG_COLOR_TYPE_PALETTE:
				bmtype = BMM_PALETTED;
				break;
			case PNG_COLOR_TYPE_RGB:
			case PNG_COLOR_TYPE_RGB_ALPHA:
				switch( info->bit_depth )
				{
					case 2:
					case 4:
					case 16:
						break;
					case 8:
						bmtype = BMM_TRUE_32;  // zero alpha for those that don't have it
						break;
				}
				break;
			case PNG_COLOR_TYPE_GRAY_ALPHA:
			case PNG_COLOR_TYPE_GRAY:
				switch( info->bit_depth )
				{
					case 2:
					case 4:
					case 16:
						break;
					case 8:
						bmtype = BMM_GRAY_8;
						break;
				}
				break;
		}
	}
	if ( bmtype == BMM_NO_TYPE )
	{
    png_destroy_read_struct( &png, &info, 0 );
		return nullptr;
	}
	row_pointers = (png_bytep*)malloc( info->height * sizeof(png_bytep) );
	for ( png_uint_32 i=0; i<info->height; i++ )
		row_pointers[i] = (png_bytep)malloc( info->rowbytes );
	png_read_image( png, row_pointers );
	switch( bmtype )
	{
		case BMM_PALETTED:
			{
				if ( info->bit_depth == 8 )
				{
					for ( png_uint_32 iy=0; iy<info->height; iy++ )
					{
						for ( png_uint_32 ix=0; ix<info->width; ix++  )
						{
							DWORD dwColor = 0xFF000000 |
															( DWORD(png->palette[row_pointers[iy][ix]].red) << 16 ) |
															( DWORD(png->palette[row_pointers[iy][ix]].green) << 8 ) |
															( DWORD(png->palette[row_pointers[iy][ix]].blue) );
							image[iy*info->width + ix] = dwColor;
						}
					}
				}
			}
			break;
		case BMM_TRUE_32:
			{
				DWORD r, g, b, a;
				for ( png_uint_32 iy = 0; iy < info->height; iy++ )
				{
					for ( png_uint_32 ix = 0; ix < info->rowbytes; )
					{
						r = row_pointers[iy][ix++];
						g = row_pointers[iy][ix++];
						b = row_pointers[iy][ix++];
						a = ( info->channels == 4 ? row_pointers[iy][ix++] : 255 );
						image[iy*info->width + (ix/info->channels - 1)] = (a << 24) | (r << 16) | (g << 8) | b;
					}
				}
			}
			break;
		case BMM_GRAY_8:
			{
				DWORD color, alpha;
				for ( png_uint_32 iy = 0; iy < info->height; iy++ )
				{
					for ( png_uint_32 ix = 0; ix < info->rowbytes;  )
					{
						color = row_pointers[iy][ix++];
						alpha = info->channels == 2 ? row_pointers[iy][ix++] : 255;
						image[iy*info->width + (ix/info->channels - 1)] = (alpha << 24) | (color << 16) | (color << 8) | color;
					}
				}
			}
			break;
	}

	png_read_end( png, info );

	for ( png_uint_32 i=0; i<info->height; i++ )
		free( row_pointers[i] );
	free( row_pointers );
  png_destroy_read_struct( &png, &info, 0 );

	return new CImage( dwWidth, dwHeight, image );
}
bool NImage::SaveImageAsPNG( IDataStream *pStream, const IImage *pImage )
{
	png_struct *png = 0;
	png_info *info = 0;
	png_bytep	*row_pointers = 0;

  png = png_create_write_struct( PNG_LIBPNG_VER_STRING, 0, 0, 0 );
	if ( png == 0 )
		return false;
  if ( setjmp(png->jmpbuf) ) 
	{
		if ( info )
		{
			for ( png_uint_32 i=0; i<info->height; i++ )
				if ( row_pointers[i] ) 
					free( row_pointers[i] );
		}
		if ( row_pointers ) 
		{
			free( row_pointers );
			row_pointers = 0;
		}

		png_destroy_write_struct( &png, &info );

		return false;
  }
  info = png_create_info_struct( png );

	png_set_write_fn( png, pStream, PNGWriteFunction, PNGFlushFunction );

	info->color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	info->channels = 4;
	info->width = pImage->GetSizeX();
	info->height = pImage->GetSizeY();
	info->gamma = 1.0f;
	info->interlace_type = 1;
	info->bit_depth = 8;

	info->rowbytes = info->width * info->channels * info->bit_depth / 8;

	row_pointers = (png_bytep*)malloc( info->height * sizeof(png_bytep) );
	for ( png_uint_32 i=0; i<info->height; i++ )
		row_pointers[i] = (png_bytep)malloc( info->rowbytes );
	const SColor *pColors = pImage->GetLFB();
	for ( int iy=0; iy<info->height; ++iy )
	{
		for ( int ix=0; ix<info->rowbytes; )
		{
			SColor color = pColors[iy*pImage->GetSizeX() + ix/4];
			row_pointers[iy][ix++] = color.r;
			row_pointers[iy][ix++] = color.g;
			row_pointers[iy][ix++] = color.b;
			row_pointers[iy][ix++] = color.a;
		}
	}

	png_write_info( png, info );

	png_set_swap( png );

	png_write_image( png, row_pointers );

	png_write_end( png, info );

 	for ( png_uint_32 i=0; i<info->height; ++i )
		free(row_pointers[i]);
	free( row_pointers );

  png_destroy_write_struct( &png, &info );

  return true;
}
