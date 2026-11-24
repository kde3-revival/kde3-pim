/*  -*- c++ -*-
    imageheader.cpp

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

#include "imageheader.h"

namespace {

  inline unsigned int be16( const QByteArray & d, unsigned int i ) { return ( (uchar)d[i] << 8 ) | (uchar)d[i + 1]; }
  inline unsigned int be32( const QByteArray & d, unsigned int i ) { return ( be16( d, i ) << 16 ) | be16( d, i + 2 ); }
  inline unsigned int le16( const QByteArray & d, unsigned int i ) { return (uchar)d[i] | ( (uchar)d[i + 1] << 8 ); }
  inline unsigned int le32( const QByteArray & d, unsigned int i ) { return le16( d, i ) | ( le16( d, i + 2 ) << 16 ); }

  inline int maxInt( int a, int b ) { return a > b ? a : b; }

  // GIF sub-blocks from d[i] up to their terminator (size 0); false at the
  // end of the data
  bool skipSubBlocks( const QByteArray & d, unsigned int & i ) {
    const unsigned int n = d.size();
    for ( ;; ) {
      if ( i >= n )
        return false;
      const unsigned int size = (uchar)d[i++];
      if ( size == 0 )
        return true;
      i += size;
    }
  }

  // A GIF, block by block, the way Qt's QGIFFormat::decode() consumes it
  // (its quirks included: an extension's first sub-block is skipped by its
  // size byte even when that is 0).  The bitmap is created at the first
  // image descriptor, with the logical screen size, or, where the decoder
  // does not believe that one (0, or ten times the frame and more than
  // 2000), with the frame's offset plus its size.  A later descriptor can
  // change that size again without a new bitmap: beyond the first one the
  // decoder writes outside it, such a GIF is refused.  The walk ends where
  // the decoder stops (trailer, unknown block, LZW size beyond 12, end of
  // the data); it is linear, every step consumes data.
  bool gifBitmapSize( const QByteArray & d, unsigned int & width, unsigned int & height ) {
    const unsigned int n = d.size();
    if ( n < 13 || d[3] != '8' || ( d[4] != '7' && d[4] != '9' ) || d[5] != 'a' )
      return false;   // not one Qt decodes as GIF
    int sw = le16( d, 6 ), sh = le16( d, 8 );
    unsigned int i = 13;
    if ( (uchar)d[10] & 0x80 )
      i += 3 * ( 2 << ( (uchar)d[10] & 7 ) );
    bool created = false;
    int createdWidth = 0, createdHeight = 0;
    while ( i < n ) {
      const char introducer = d[i++];
      if ( introducer == ',' ) {
        if ( i + 9 > n )
          break;
        const int left = le16( d, i ), top = le16( d, i + 2 );
        const int w = le16( d, i + 4 ), h = le16( d, i + 6 );
        const uchar flags = d[i + 8];
        i += 9;
        if ( sw / 10 > maxInt( w, 200 ) )
          sw = -1;
        if ( sh / 10 > maxInt( h, 200 ) )
          sh = -1;
        if ( sw <= 0 )
          sw = left + w;
        if ( sh <= 0 )
          sh = top + h;
        if ( !created ) {
          created = true;
          createdWidth = sw;
          createdHeight = sh;
        } else if ( sw > createdWidth || sh > createdHeight )
          return false;   // the decoder would write beyond its bitmap
        if ( flags & 0x80 )
          i += 3 * ( 2 << ( flags & 7 ) );
        if ( i >= n || (uchar)d[i++] > 12 )
          break;   // the LZW code size: beyond 12 the decoder stops
        if ( !skipSubBlocks( d, i ) )
          break;
      } else if ( introducer == '!' ) {
        if ( i >= n )
          break;
        const uchar label = d[i++];
        if ( label == 0xf9 || label == 0xff ) {
          // graphic control, application: the first sub-block by its size
          if ( i >= n )
            break;
          i += 1 + (uchar)d[i];
        }
        if ( !skipSubBlocks( d, i ) )
          break;
      } else
        break;   // the trailer, or a block the decoder does not know
    }
    if ( !created )
      return false;
    width = createdWidth;
    height = createdHeight;
    return true;
  }

} // anon namespace

bool KMail::decoderBitmapSize( const QByteArray & d, unsigned int & width, unsigned int & height,
                               unsigned int & bytesPerPixel, unsigned int * tableBytes ) {
  const unsigned int n = d.size();
  bytesPerPixel = 4;   // 32-bit bitmaps (or less)
  if ( tableBytes )
    *tableBytes = 0;
  if ( n >= 24 && (uchar)d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G'
       && d[12] == 'I' && d[13] == 'H' && d[14] == 'D' && d[15] == 'R' ) {
    width = be32( d, 16 );
    height = be32( d, 20 );
    return true;
  }
  if ( n >= 6 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F' ) {
    // the bitmap and, for frames restored after display, a backing store
    // of up to its size
    bytesPerPixel = 8;
    return gifBitmapSize( d, width, height );
  }
  if ( n >= 22 && d[0] == 'B' && d[1] == 'M' ) {
    // as Qt's read_dib(): a header of 40 bytes and more (Windows, OS/2 2,
    // their extensions) has 32-bit dimensions, a shorter one (the 12-byte
    // core header of Windows 2 and OS/2 1) 16-bit ones; both signed
    int w, h, bits;
    unsigned int colorsUsed = 0;   // none in a core header
    if ( (int)le32( d, 14 ) >= 40 ) {
      if ( n < 54 )
        return false;   // Qt reads the whole header (or stops)
      w = (int)le32( d, 18 );
      h = (int)le32( d, 22 );
      bits = le16( d, 28 );
      colorsUsed = le32( d, 46 );
    } else {
      if ( n < 26 )
        return false;
      w = (short)le16( d, 18 );
      h = (short)le16( d, 20 );
      bits = le16( d, 24 );
    }
    // The colour table Qt allocates before reading it: biClrUsed
    // entries (of 4 bytes), 2^bits without one, none for 16 bits and
    // more.  Qt (since the kde3-qt image bounds) refuses more than 256
    // and so does this; an older Qt allocates any number, even for a
    // 58-byte file: refused before Qt sees it.  A bit depth Qt does not
    // read (only 1, 4, 8, 16, 24 and 32) is refused too, as Qt does.
    unsigned int colors;
    if ( bits == 16 || bits == 24 || bits == 32 )
      colors = 0;
    else if ( bits == 1 || bits == 4 || bits == 8 )
      colors = colorsUsed ? colorsUsed : 1u << bits;
    else
      return false;
    if ( colors > 256 )
      return false;
    if ( tableBytes )
      *tableBytes = colors * 4;
    width = w < 0 ? -(unsigned int)w : w;
    height = h < 0 ? -(unsigned int)h : h;
    return true;
  }
  if ( n >= 4 && (uchar)d[0] == 0xFF && (uchar)d[1] == 0xD8 ) {
    // the frame header (SOFn), segment by segment
    unsigned int i = 2;
    while ( i + 9 < n ) {
      if ( (uchar)d[i] != 0xFF )
        return false;
      const uchar marker = d[i + 1];
      if ( marker == 0xFF ) { ++i; continue; }
      if ( marker == 0xD8 || marker == 0x01 || ( marker >= 0xD0 && marker <= 0xD7 ) ) { i += 2; continue; }
      const unsigned int length = be16( d, i + 2 );
      if ( marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC ) {
        height = be16( d, i + 5 );
        width = be16( d, i + 7 );
        return true;
      }
      if ( length < 2 )
        return false;
      i += 2 + length;
    }
  }
  return false;
}
