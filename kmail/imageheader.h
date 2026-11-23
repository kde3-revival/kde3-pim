/*  -*- c++ -*-
    imageheader.h

    This file is part of KMail, the KDE mail client.
    Copyright (c) 2026 The kde3-revival authors

    KMail is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License, version 2, as
    published by the Free Software Foundation.

    KMail is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef __KMAIL_IMAGEHEADER_H__
#define __KMAIL_IMAGEHEADER_H__

#include <qcstring.h>

namespace KMail {

  /**
   * The bitmap Qt's decoder allocates for the PNG, JPEG, GIF or BMP image
   * @p data, read from its headers before anything is decoded: @p width x
   * @p height pixels of @p bytesPerPixel bytes (what the decoder can hold
   * at once, its own buffers included).  The dimensions are the ones the
   * decoder really uses: for a GIF the frames can replace the logical
   * screen (Qt's QGIFFormat), for a BMP the header kind decides their size.
   * False if they cannot be read, and for a GIF whose later frame would
   * make Qt's decoder write beyond the bitmap of the first one.
   */
  bool decoderBitmapSize( const QByteArray & data, unsigned int & width, unsigned int & height,
                          unsigned int & bytesPerPixel );

} // namespace KMail

#endif // __KMAIL_IMAGEHEADER_H__
