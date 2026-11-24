/*  -*- c++ -*-
    cidrewriter.cpp

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

#include "cidrewriter.h"

#include "htmlentities.h"

#include <kurl.h>

#include <qcstring.h>
#include <qregexp.h>
#include <qvaluelist.h>

namespace {

  // HTML's white space (not every QChar::isSpace())
  inline bool isWs( QChar c ) {
    const ushort u = c.unicode();
    return u == ' ' || u == '\t' || u == '\n' || u == '\r' || u == '\f';
  }

  inline QChar lowerAscii( QChar c ) {
    const ushort u = c.unicode();
    return ( u >= 'A' && u <= 'Z' ) ? QChar( (ushort)( u + 32 ) ) : c;
  }

  // s[i...] starts with @p what (ASCII, lower case), case-insensitively,
  // within [i, end)
  bool matchAt( const QString & s, int i, int end, const char * what ) {
    for ( int k = 0; what[k]; ++k )
      if ( i + k >= end || lowerAscii( s[i + k] ) != QChar( what[k] ) )
        return false;
    return true;
  }

  bool isIdentChar( QChar c ) {
    const ushort u = c.unicode();
    return ( u >= 'a' && u <= 'z' ) || ( u >= 'A' && u <= 'Z' ) || ( u >= '0' && u <= '9' )
           || u == '-' || u == '_' || u >= 0x80 || u == '\\';
  }

  // @p code (at most U+10FFFF) as UTF-16
  void appendCodePoint( QString & out, uint code ) {
    if ( code > 0xffff ) {
      code -= 0x10000;
      out += QChar( (ushort)( 0xd800 + ( code >> 10 ) ) );
      out += QChar( (ushort)( 0xdc00 + ( code & 0x3ff ) ) );
    } else
      out += QChar( (ushort)code );
  }

  // A named character reference (the name, without '&' and ';'): its
  // characters (one or two code points, as UTF-16) by HTML 5's table
  // (htmlentities.h); QString::null if none.  @p legacy: whether a
  // browser also takes it without the semicolon.
  QString namedReference( const QString & name, bool & legacy ) {
    const QCString key = name.latin1();   // (ASCII letters and digits)
    unsigned int low = 0, high = htmlEntityCount;
    while ( low < high ) {
      const unsigned int mid = ( low + high ) / 2;
      const HtmlEntity & e = htmlEntities[mid];
      const int c = qstrcmp( key.data(), htmlEntityNames + e.name );
      if ( c == 0 ) {
        legacy = ( e.first & 0x80000000u ) != 0;
        QString out;
        appendCodePoint( out, e.first & 0x7fffffffu );
        if ( e.second )
          appendCodePoint( out, e.second );
        return out;
      }
      if ( c < 0 )
        high = mid;
      else
        low = mid + 1;
    }
    return QString::null;
  }

  inline bool isAsciiAlnum( QChar c ) {
    const ushort u = c.unicode();
    return ( u >= 'a' && u <= 'z' ) || ( u >= 'A' && u <= 'Z' ) || ( u >= '0' && u <= '9' );
  }

  // what HTML 5 makes of &#128; to &#159; (windows-1252); 0: unchanged
  const ushort windows1252[32] = {
    0x20ac, 0, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017d, 0,
    0, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0, 0x017e, 0x0178
  };

  // The character references of an attribute value, as HTML 5 decodes
  // them there: numeric ones (&#N, &#xN, with or without semicolon;
  // 0, surrogates and beyond U+10FFFF are U+FFFD, 128 to 159 windows-1252),
  // named ones (all of HTML 5's) with their semicolon, the legacy ones also without it
  // unless an alphanumeric character or '=' follows.  Others stay.
  QString decodeReferences( const QString & v ) {
    if ( v.find( '&' ) < 0 )
      return v;
    QString out;
    const int n = v.length();
    int last = 0;   // v[last, i) is not in out yet
    int i = v.find( '&' );
    while ( i >= 0 && i < n ) {
      const int j = i + 1;
      int end = -1;   // after the reference
      uint code = 0;
      QString named;
      if ( j < n && v[j] == '#' ) {
        int k = j + 1;
        const bool hex = k < n && ( v[k] == 'x' || v[k] == 'X' );
        if ( hex )
          ++k;
        const int digits = k;
        for ( ; k < n; ++k ) {
          const ushort u = v[k].unicode();
          int d;
          if ( u >= '0' && u <= '9' ) d = u - '0';
          else if ( hex && u >= 'a' && u <= 'f' ) d = u - 'a' + 10;
          else if ( hex && u >= 'A' && u <= 'F' ) d = u - 'A' + 10;
          else break;
          code = code * ( hex ? 16 : 10 ) + d;
          if ( code > 0x10ffff )
            code = 0x110000;   // (stays beyond: no overflow)
        }
        if ( k > digits ) {
          end = ( k < n && v[k] == ';' ) ? k + 1 : k;
          if ( code == 0 || code > 0x10ffff || ( code >= 0xd800 && code <= 0xdfff ) )
            code = 0xfffd;
          else if ( code >= 0x80 && code <= 0x9f && windows1252[code - 0x80] )
            code = windows1252[code - 0x80];
        }
      } else {
        int k = j;
        while ( k < n && k - j < 32 && isAsciiAlnum( v[k] ) )
          ++k;
        if ( k > j && ( k >= n || !isAsciiAlnum( v[k] ) ) ) {
          bool legacy = false;
          named = namedReference( v.mid( j, k - j ), legacy );
          if ( !named.isNull() ) {
            if ( k < n && v[k] == ';' )
              end = k + 1;
            else if ( legacy && ( k >= n || v[k] != '=' ) )
              end = k;
          }
        }
      }
      if ( end < 0 ) {
        i = v.find( '&', j );   // not a reference: the '&' stays
        continue;
      }
      out += v.mid( last, i - last );
      if ( !named.isNull() )
        out += named;
      else
        appendCodePoint( out, code );
      last = end;
      i = v.find( '&', end );
    }
    return out + v.mid( last );
  }

  QString escapeAttribute( const QString & v ) {
    QString out;
    const int n = v.length();
    int last = 0;
    for ( int i = 0; i < n; ++i ) {
      const char * e = 0;
      switch ( v[i].unicode() ) {
      case '&': e = "&amp;"; break;
      case '"': e = "&quot;"; break;
      case '<': e = "&lt;"; break;
      case '>': e = "&gt;"; break;
      default: break;
      }
      if ( !e )
        continue;
      out += v.mid( last, i - last );
      out += QString::fromLatin1( e );
      last = i + 1;
    }
    return out + v.mid( last );
  }

  // A cid: URL value (white space around it ignored) and what it is
  // shown with; QString::null if it is none or not known.
  QString resolveValue( const QString & value, KMail::CidResolver & resolver, bool shown ) {
    const QString v = value.stripWhiteSpace();
    if ( v.length() <= 4 || !matchAt( v, 0, v.length(), "cid:" ) )
      return QString::null;
    return resolver.urlFor( KURL::decode_string( v.mid( 4 ) ), shown );
  }

  // url(cid:...) of CSS text; comments and strings skipped.
  QString rewriteCss( const QString & css, KMail::CidResolver & resolver, bool & changed ) {
    QString out;
    const int n = css.length();
    int last = 0;
    int i = 0;
    while ( i < n ) {
      const QChar c = css[i];
      if ( c == '/' && i + 1 < n && css[i + 1] == '*' ) {
        const int e = css.find( "*/", i + 2 );
        if ( e < 0 )
          break;
        i = e + 2;
      } else if ( c == '"' || c == '\'' ) {
        int j = i + 1;
        while ( j < n && css[j] != c && css[j] != '\n' )
          j += css[j] == '\\' ? 2 : 1;
        i = j + 1;
      } else if ( ( c == 'u' || c == 'U' ) && matchAt( css, i, n, "url(" )
                  && ( i == 0 || !isIdentChar( css[i - 1] ) ) ) {
        int j = i + 4;
        while ( j < n && isWs( css[j] ) ) ++j;
        QString value;
        bool plain = true;   // no escapes: the value is what it says
        if ( j < n && ( css[j] == '"' || css[j] == '\'' ) ) {
          const QChar q = css[j];
          const int s = ++j;
          while ( j < n && css[j] != q && css[j] != '\n' ) {
            if ( css[j] == '\\' ) { plain = false; ++j; }
            ++j;
          }
          value = css.mid( s, j - s );
          ++j;
        } else {
          const int s = j;
          while ( j < n && css[j] != ')' && !isWs( css[j] ) ) {
            if ( css[j] == '\\' || css[j] == '"' || css[j] == '\'' || css[j] == '(' ) plain = false;
            ++j;
          }
          value = css.mid( s, j - s );
        }
        while ( j < n && isWs( css[j] ) ) ++j;
        if ( j < n && css[j] == ')' ) {
          const QString url = plain ? resolveValue( value, resolver, false ) : QString::null;
          if ( !url.isEmpty() ) {
            out += css.mid( last, i - last ) + "url(\"" + url + "\")";
            last = j + 1;
            changed = true;
          }
          i = j + 1;
        } else
          i = j;
      } else
        ++i;
    }
    return out + css.mid( last );
  }

  bool isRawText( const QString & tag ) {
    return tag == "style" || tag == "script" || tag == "textarea" || tag == "title" || tag == "xmp"
           || tag == "iframe" || tag == "noembed" || tag == "noframes";
  }

} // anon namespace

QString KMail::rewriteCidReferences( const QString & html, CidResolver & resolver ) {
  // cid: may be written with character references (c&#105;d&colon;)
  if ( html.find( "cid", 0, false ) < 0 && html.find( '&' ) < 0 )
    return html;
  QString out;
  const int n = html.length();
  int last = 0;   // html[last, ...) is not in out yet
  int i = 0;
  while ( i < n ) {
    const int lt = html.find( '<', i );
    if ( lt < 0 )
      break;
    i = lt + 1;
    if ( matchAt( html, lt, n, "<!--" ) ) {
      const int e = html.find( "-->", lt + 4 );
      i = e < 0 ? n : e + 3;
      continue;
    }
    if ( i < n && ( html[i] == '!' || html[i] == '?' || html[i] == '/' ) ) {
      const int e = html.find( '>', i );
      i = e < 0 ? n : e + 1;
      continue;
    }
    if ( i >= n || !html[i].isLetter() )
      continue;
    int e = i;
    while ( e < n && !isWs( html[e] ) && html[e] != '/' && html[e] != '>' )
      ++e;
    QString tag = html.mid( i, e - i ).lower();
    if ( tag == "image" )
      tag = "img";   // the HTML parser makes it one
    i = e;
    // the attributes of the tag first (an image's own style can hide it)
    struct Attribute { QString name; int vs, ve, cs, ce; bool value; };
    QValueList<Attribute> attributes;
    while ( i < n && html[i] != '>' ) {
      if ( isWs( html[i] ) || html[i] == '/' ) { ++i; continue; }
      Attribute a;
      const int ns = i;
      ++i;   // the first character is part of the name, even '='
      while ( i < n && !isWs( html[i] ) && html[i] != '/' && html[i] != '>' && html[i] != '=' )
        ++i;
      a.name = html.mid( ns, i - ns ).lower();
      a.value = false;
      a.vs = a.ve = a.cs = a.ce = i;
      int k = i;
      while ( k < n && isWs( html[k] ) ) ++k;
      if ( k < n && html[k] == '=' ) {
        a.value = true;
        i = k + 1;
        while ( i < n && isWs( html[i] ) ) ++i;
        a.vs = i;   // the value, quotes included
        if ( i < n && ( html[i] == '"' || html[i] == '\'' ) ) {
          const QChar q = html[i];
          a.cs = i + 1;
          a.ce = html.find( q, a.cs );
          if ( a.ce < 0 ) a.ce = n;
          i = a.ce < n ? a.ce + 1 : n;
        } else {
          a.cs = i;
          while ( i < n && !isWs( html[i] ) && html[i] != '>' ) ++i;
          a.ce = i;
        }
        a.ve = i;
      }
      attributes.append( a );
    }
    // The first of each name counts, as for the browser (also one
    // without a value).  An image hidden by its own attributes is not
    // shown: its part stays an attachment (references are rewritten all
    // the same).  What else hides it (an ancestor, a style sheet) is not
    // known here: see shown below.
    bool seenSrc = false, seenBackground = false, seenStyle = false, hidden = false;
    const bool backgroundShown = tag == "body" || tag == "table" || tag == "td" || tag == "th";
    const bool showsImage = tag == "img" || backgroundShown;   // only there it matters
    for ( QValueList<Attribute>::ConstIterator it = attributes.begin(); showsImage && it != attributes.end(); ++it ) {
      const QString & name = (*it).name;
      if ( name == "hidden" )
        hidden = true;
      else if ( ( name == "width" || name == "height" ) && (*it).value
                && decodeReferences( html.mid( (*it).cs, (*it).ce - (*it).cs ) ).stripWhiteSpace() == "0" )
        hidden = true;
      else if ( name == "style" && !seenStyle && (*it).value ) {
        static const QRegExp none( "display\\s*:\\s*none" ), invisible( "visibility\\s*:\\s*(hidden|collapse)" ),
                             transparent( "opacity\\s*:\\s*0(\\.0*)?\\s*(;|!|$)" );
        const QString css = decodeReferences( html.mid( (*it).cs, (*it).ce - (*it).cs ) ).lower();
        if ( css.find( none ) >= 0 || css.find( invisible ) >= 0 || css.find( transparent ) >= 0 )
          hidden = true;
      }
      if ( name == "style" )
        seenStyle = true;   // (marked again below)
    }
    seenStyle = false;
    for ( QValueList<Attribute>::ConstIterator it = attributes.begin(); it != attributes.end(); ++it ) {
      const QString & name = (*it).name;
      bool url = false, style = false, shown = false;
      if ( name == "src" ) {
        url = tag == "img" && !seenSrc; seenSrc = true;
        shown = !hidden;
      } else if ( name == "background" ) {
        url = backgroundShown && !seenBackground; seenBackground = true;
        shown = !hidden;
      } else if ( name == "style" ) {
        style = !seenStyle; seenStyle = true;
      }
      if ( ( !url && !style ) || !(*it).value )
        continue;
      const QString value = decodeReferences( html.mid( (*it).cs, (*it).ce - (*it).cs ) );
      QString replacement;
      if ( url ) {
        replacement = resolveValue( value, resolver, shown );
      } else {
        // CSS: whether a declaration shows its image is not known here
        bool changed = false;
        const QString css = rewriteCss( value, resolver, changed );
        if ( changed )
          replacement = css;
      }
      if ( replacement.isEmpty() )
        continue;
      out += html.mid( last, (*it).vs - last ) + "\"" + escapeAttribute( replacement ) + "\"";
      last = (*it).ve;
    }
    if ( i < n )
      ++i;   // '>'
    if ( tag == "plaintext" )
      break;
    if ( isRawText( tag ) ) {
      // up to </tag followed by white space, '/' or '>'
      int end = i;
      for ( ;; ) {
        end = html.find( "</", end );
        if ( end < 0 )
          break;
        const int after = end + 2 + tag.length();
        if ( matchAt( html, end + 2, n, tag.latin1() )
             && ( after >= n || isWs( html[after] ) || html[after] == '/' || html[after] == '>' ) )
          break;
        end += 2;
      }
      if ( end < 0 ) end = n;
      if ( tag == "style" ) {
        bool changed = false;
        const QString css = rewriteCss( html.mid( i, end - i ), resolver, changed );
        if ( changed ) {
          out += html.mid( last, i - last ) + css;
          last = end;
        }
      }
      i = end;
    }
  }
  return out + html.mid( last );
}
