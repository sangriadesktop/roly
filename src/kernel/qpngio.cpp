/****************************************************************************
** $Id: qpngio.cpp,v 1.8 1998/07/03 00:09:26 hanord Exp $
**
** Implementation of PNG QImage IOHandler
**
** Created : 970521
**
** Copyright (C) 1992-1999 Troll Tech AS.  All rights reserved.
**
** This file is part of Qt Free Edition, version 1.45.
**
** See the file LICENSE included in the distribution for the usage
** and distribution terms, or http://www.troll.no/free-license.html.
**
** IMPORTANT NOTE: You may NOT copy this file or any part of it into
** your own programs or libraries.
**
** Please see http://www.troll.no/pricing.html for information about 
** Qt Professional Edition, which is this same library but with a
** license which allows creation of commercial/proprietary software.
**
*****************************************************************************/

#include "qimage.h"
#include "qiodevice.h"

#include <png.h>

/*
  The following PNG Test Suite (October 1996) images do not load correctly,
  with no apparent reason:

    ct0n0g04.png
    ct1n0g04.png
    ctzn0g04.png
    cm0n0g04.png
    cm7n0g04.png
    cm9n0g04.png

  All others load apparently correctly, and to the minimal QImage equivalent.

  All QImage formats output to reasonably efficient PNG equivalents.  Never
  to greyscale.
*/

static void qt_png_warning(png_structp /*png_ptr*/, png_const_charp message)
{
    static const char *wrongMsg = "Interlace handling should be turned on when using png_read_image";
    if (strncmp(message, wrongMsg, strlen(wrongMsg)) == 0) {
        return;
    }
    warning("libpng warning: %s", message);
}


static
void setup_qt( QImage& image, png_structp png_ptr, png_infop info_ptr )
{
    if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_gAMA) ) {
        double file_gamma;
        png_get_gAMA(png_ptr, info_ptr, &file_gamma);
#if defined(_OS_MAC_)
        // a good guess for Mac systems
	png_set_gamma( png_ptr, 1.7, file_gamma );
#else
        // a good guess for PC monitors in a bright office or a dim room
        png_set_gamma( png_ptr, 2.2, file_gamma );
#endif
    }

    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 0, 0, 0);

#if PNG_LIBPNG_VER > 10257
    png_byte channels;
    channels = png_get_channels(png_ptr, info_ptr);

    png_color* palette;
    int num_palette;
    png_get_PLTE(png_ptr, info_ptr, &palette, &num_palette);

    png_bytep trans_alpha = NULL;
    int num_trans = 0;
    png_color_16p trans_color = NULL;

    png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &num_trans, &trans_color);
#endif

    if ( color_type == PNG_COLOR_TYPE_GRAY ) {
        // Black & White or 8-bit greyscale

#if PNG_LIBPNG_VER > 10257
        if ( bit_depth == 1 && channels == 1 ) {
#else
            if ( bit_depth == 1 && info_ptr->channels == 1 ) {
#endif
            png_set_invert_mono( png_ptr );
            png_read_update_info( png_ptr, info_ptr );
            image.create( width, height, 1, 2, QImage::BigEndian );
            image.setColor( 1, qRgb(0,0,0) );
            image.setColor( 0, qRgb(255,255,255) );
        } else {
            if ( bit_depth == 16 )
                png_set_strip_16(png_ptr);
            else if ( bit_depth < 8 )
                png_set_packing(png_ptr);
            int ncols = bit_depth < 8 ? 1 << bit_depth : 256;
            png_read_update_info(png_ptr, info_ptr);
            image.create(width, height, 8, ncols);
            for (int i=0; i<ncols; i++) {
                int c = i*255/(ncols-1);
                image.setColor( i, qRgb(c,c,c) );
            }
            if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) ) {
#if PNG_LIBPNG_VER > 10257
                int g = trans_color->gray;
#else
                int g = info_ptr->trans_values.gray;
#endif
                if ( bit_depth > 8 ) {
                    // transparency support disabled for now
                } else {
                    image.setAlphaBuffer( TRUE );
                    image.setColor(g, RGB_MASK & image.color(g));
                }
            }
        }
    } else if ( color_type == PNG_COLOR_TYPE_PALETTE
                && png_get_valid(png_ptr, info_ptr, PNG_INFO_PLTE)
                #if PNG_LIBPNG_VER > 10257
                && num_palette <= 256 )
#else
        && info_ptr->num_palette <= 256 )
#endif
    {
        // 1-bit and 8-bit color
        if ( bit_depth != 1 )
            png_set_packing( png_ptr );
        png_read_update_info( png_ptr, info_ptr );
        png_get_IHDR(png_ptr, info_ptr,
                     &width, &height, &bit_depth, &color_type, 0, 0, 0);
#if PNG_LIBPNG_VER > 10257
        image.create(width, height, bit_depth, num_palette,
#else
                image.create(width, height, bit_depth, info_ptr->num_palette,
#endif
                     QImage::BigEndian);
        int i = 0;
        if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) ) {
            image.setAlphaBuffer( TRUE );
#if PNG_LIBPNG_VER > 10257
            while ( i < num_trans ) {
#else
                while ( i < info_ptr->num_trans ) {
#endif
                image.setColor(i, qRgb(
#if PNG_LIBPNG_VER > 10257
                                       palette[i].red,
                                       palette[i].green,
                                       palette[i].blue
                                       //trans_alpha[i]
#else
                                       info_ptr->palette[i].red,
		    info_ptr->palette[i].green,
		    info_ptr->palette[i].blue,
		    info_ptr->trans[i]
#endif
                               )
                );
                i++;
            }
        }
#if PNG_LIBPNG_VER > 10257
        while ( i < num_palette ) {
#else
            while ( i < info_ptr->num_palette ) {
#endif
            image.setColor(i, qRgb(
#if PNG_LIBPNG_VER > 10257
                                   palette[i].red,
                                   palette[i].green,
                                   palette[i].blue
#else
                                   info_ptr->palette[i].red,
		info_ptr->palette[i].green,
		info_ptr->palette[i].blue,
#endif
                                   //0xff
                           )
            );
            i++;
        }
    } else {
        // 32-bit
        if ( bit_depth == 16 )
            png_set_strip_16(png_ptr);

        png_set_expand(png_ptr);

        if ( color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
            png_set_gray_to_rgb(png_ptr);

        image.create(width, height, 32);

        // Only add filler if no alpha, or we can get 5 channel data.
        if (!(color_type & PNG_COLOR_MASK_ALPHA)
            && !png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
            png_set_filler(png_ptr, 0xff,
                           QImage::systemByteOrder() == QImage::BigEndian ?
                           PNG_FILLER_BEFORE : PNG_FILLER_AFTER);
            // We want 4 bytes, but it isn't an alpha channel
        } else {
            image.setAlphaBuffer(TRUE);
        }

        if ( QImage::systemByteOrder() == QImage::BigEndian ) {
            png_set_swap_alpha(png_ptr);
        }

        png_read_update_info(png_ptr, info_ptr);
    }

    // Qt==ARGB==Big(ARGB)==Little(BGRA)
    if ( QImage::systemByteOrder() == QImage::LittleEndian ) {
        png_set_bgr(png_ptr);
    }
}


static
void iod_read_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
    QImageIO* iio = (QImageIO*)png_get_io_ptr(png_ptr);
    QIODevice* in = iio->ioDevice();

    while (length) {
	int nr = in->readBlock((char*)data, length);
	if (nr <= 0) {
	    png_error(png_ptr, "Read Error");
	    return;
	}
	length -= nr;
    }
}

static
void iod_write_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
    QImageIO* iio = (QImageIO*)png_get_io_ptr(png_ptr);
    QIODevice* out = iio->ioDevice();

    uint nr = out->writeBlock((char*)data, length);
    if (nr != length) {
	png_error(png_ptr, "Write Error");
	return;
    }
}

static
void read_png_image(QImageIO* iio)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_infop end_info;
    png_bytep* row_pointers;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);
    if (!png_ptr) {
        iio->setStatus(-1);
        return;
    }

    png_set_error_fn(png_ptr, 0, 0, &qt_png_warning);

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, 0, 0);
        iio->setStatus(-2);
        return;
    }

    end_info = png_create_info_struct(png_ptr);
    if (!end_info) {
        png_destroy_read_struct(&png_ptr, &info_ptr, 0);
        iio->setStatus(-3);
        return;
    }

#if PNG_LIBPNG_VER > 10257
    if (setjmp(png_jmpbuf(png_ptr))) {
#else
        if (setjmp(png_ptr->jmpbuf)) {
#endif
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        iio->setStatus(-4);
        return;
    }

    png_set_read_fn(png_ptr, (void*)iio, iod_read_fn);
    png_read_info(png_ptr, info_ptr);

    QImage image;
    setup_qt(image, png_ptr, info_ptr);

    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 0, 0, 0);

    uchar** jt = image.jumpTable();
    row_pointers=new png_bytep[height];

    for (uint y=0; y<height; y++) {
        row_pointers[y]=jt[y];
    }

    png_read_image(png_ptr, row_pointers);

#if 0 // libpng takes care of this.
    png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)
    if (image.depth()==32 && png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) {
	QRgb trans = 0xFF000000 | qRgb(
	      (info_ptr->trans_values.red << 8 >> bit_depth)&0xff,
	      (info_ptr->trans_values.green << 8 >> bit_depth)&0xff,
	      (info_ptr->trans_values.blue << 8 >> bit_depth)&0xff);
	for (uint y=0; y<height; y++) {
	    for (uint x=0; x<info_ptr->width; x++) {
		if (((uint**)jt)[y][x] == trans) {
		    ((uint**)jt)[y][x] &= 0x00FFFFFF;
		} else {
		}
	    }
	}
    }
#endif

    //image.setDotsPerMeterX(png_get_x_pixels_per_meter(png_ptr,info_ptr));
    //image.setDotsPerMeterY(png_get_y_pixels_per_meter(png_ptr,info_ptr));

    delete [] row_pointers;

    iio->setImage(image);

    png_read_end(png_ptr, end_info);
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

    iio->setStatus(0);
}

static
void write_png_image(QImageIO* iio)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_bytep* row_pointers;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,0,0,0);
    if (!png_ptr) {
	iio->setStatus(-1);
	return;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
	png_destroy_write_struct(&png_ptr, 0);
	iio->setStatus(-2);
	return;
    }

#if PNG_LIBPNG_VER > 10257
        if (setjmp(png_jmpbuf(png_ptr))) {
#else
        if (setjmp(png_ptr->jmpbuf)) {
#endif
	png_destroy_write_struct(&png_ptr, &info_ptr);
	iio->setStatus(-4);
	return;
    }

    png_set_write_fn(png_ptr, (void*)iio, iod_write_fn, 0);

    const QImage& image = iio->image();

#if PNG_LIBPNG_VER > 10257
#else
    info_ptr->channels =
	(image.depth() == 32)
	    ? (image.hasAlphaBuffer() ? 4 : 3)
	    : 1;
#endif

    png_set_IHDR(png_ptr, info_ptr, image.width(), image.height(),
	image.depth() == 1 ? 1 : 8 /* per channel */,
	image.depth() == 32
	    ? image.hasAlphaBuffer()
		? PNG_COLOR_TYPE_RGB_ALPHA
		: PNG_COLOR_TYPE_RGB
	    : PNG_COLOR_TYPE_PALETTE, 0, 0, 0);


#if PNG_LIBPNG_VER > 10257
    png_color_8 sig_bit;
    sig_bit.red = 8;
    sig_bit.green = 8;
    sig_bit.blue = 8;
    png_set_sBIT(png_ptr, info_ptr, &sig_bit);
#else
    //png_set_sBIT(png_ptr, info_ptr, 8);
    info_ptr->sig_bit.red = 8;
    info_ptr->sig_bit.green = 8;
    info_ptr->sig_bit.blue = 8;
#endif

#if 0 // libpng takes care of this.
    if (image.depth() == 1 && image.bitOrder() == QImage::BigEndian)
       png_set_packswap(png_ptr);
#endif

    png_colorp palette = 0;
    png_bytep copy_trans = 0;
    if (image.numColors()) {
        // Paletted
        int num_palette = image.numColors();
        palette = new png_color[num_palette];
        png_set_PLTE(png_ptr, info_ptr, palette, num_palette);
        int* trans = new int[num_palette];
        int num_trans = 0;
        for (int i=0; i<num_palette; i++) {
            QRgb rgb=image.color(i);
#if PNG_LIBPNG_VER > 10257
            palette[i].red = qRed(rgb);
            palette[i].green = qGreen(rgb);
            palette[i].blue = qBlue(rgb);
#else
            info_ptr->palette[i].red = qRed(rgb);
	    info_ptr->palette[i].green = qGreen(rgb);
	    info_ptr->palette[i].blue = qBlue(rgb);
#endif
            if (image.hasAlphaBuffer()) {
                trans[i] = rgb >> 24;
                if (trans[i] < 255) {
                    num_trans = i+1;
                }
            }
        }
        if (num_trans) {
            copy_trans = new png_byte[num_trans];
            for (int i=0; i<num_trans; i++)
                copy_trans[i] = trans[i];
            png_set_tRNS(png_ptr, info_ptr, copy_trans, num_trans, 0);
        }
#if PNG_LIBPNG_VER > 10257
        delete[] trans;
#else
        delete trans;
#endif
    }

    if ( image.hasAlphaBuffer() ) {
#if PNG_LIBPNG_VER > 10257
        png_color_8 sig_bit;
        sig_bit.red = 8;
        sig_bit.green = 8;
        sig_bit.blue = 8;
        png_set_sBIT(png_ptr, info_ptr, &sig_bit);
#else
        info_ptr->sig_bit.alpha = 8;
#endif
    }

    if ( QImage::systemByteOrder() == QImage::BigEndian ) {
	png_set_bgr(png_ptr);
	png_set_swap_alpha(png_ptr);
    }

    png_write_info(png_ptr, info_ptr);

    if ( image.depth() != 1 )
	png_set_packing(png_ptr);

    if ( image.depth() == 32 && !image.hasAlphaBuffer() )
	png_set_filler(png_ptr, 0,
	    QImage::systemByteOrder() == QImage::BigEndian ?
		PNG_FILLER_BEFORE : PNG_FILLER_AFTER);

    png_uint_32 width;
    png_uint_32 height;
    int bit_depth;
    int color_type;
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
                 0, 0, 0);

    uchar** jt = image.jumpTable();
    row_pointers=new png_bytep[height];
    uint y;
    for (y=0; y<height; y++) {
        row_pointers[y]=jt[y];
    }
    png_write_image(png_ptr, row_pointers);
    delete row_pointers;

    png_write_end(png_ptr, info_ptr);

    if ( palette )
        delete [] palette;
    if ( copy_trans )
        delete [] copy_trans;

    png_destroy_write_struct(&png_ptr, &info_ptr);

    iio->setStatus(0);
}


void qInitPngIO()
{
    QImageIO::defineIOHandler("PNG", "^.PNG\r", 0, read_png_image, write_png_image);
}
