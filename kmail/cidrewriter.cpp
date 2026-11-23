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

#include <kurl.h>

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

  // The character references of an attribute value: &amp; &lt; &gt;
  // &quot; &apos; &#N; &#xN; (with their semicolon); others stay.
  QString decodeReferences( const QString & v ) {
    if ( v.find( '&' ) < 0 )
      return v;
    QString out;
    const int n = v.length();
    int last = 0;
    for ( int i = v.find( '&' ); i >= 0 && i < n; i = v.find( '&', i + 1 ) ) {
      int semi = -1;   // within 10 characters: no search through the rest
      for ( int k = i + 1; k < n && k <= i + 10; ++k )
        if ( v[k] == ';' ) { semi = k; break; }
      if ( semi < 0 )
        continue;
      const QString name = v.mid( i + 1, semi - i - 1 );
      QChar c;
      if ( name == "amp" ) c = '&';
      else if ( name == "lt" ) c = '<';
      else if ( name == "gt" ) c = '>';
      else if ( name == "quot" ) c = '"';
      else if ( name == "apos" ) c = '\'';
      else if ( name.startsWith( "#x" ) || name.startsWith( "#X" ) ) {
        bool ok;
        const uint u = name.mid( 2 ).toUInt( &ok, 16 );
        if ( ok && u > 0 && u < 0xD800 ) c = QChar( (ushort)u );
      } else if ( name.startsWith( "#" ) ) {
        bool ok;
        const uint u = name.mid( 1 ).toUInt( &ok, 10 );
        if ( ok && u > 0 && u < 0xD800 ) c = QChar( (ushort)u );
      }
      if ( c.isNull() )
        continue;
      out += v.mid( last, i - last );
      out += c;
      last = semi + 1;
      i = semi;
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
  QString resolveValue( const QString & value, KMail::CidResolver & resolver ) {
    const QString v = value.stripWhiteSpace();
    if ( v.length() <= 4 || !matchAt( v, 0, v.length(), "cid:" ) )
      return QString::null;
    return resolver.urlFor( KURL::decode_string( v.mid( 4 ) ) );
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
          const QString url = plain ? resolveValue( value, resolver ) : QString::null;
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
  if ( html.find( "cid:", 0, false ) < 0 )
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
    bool seenSrc = false, seenBackground = false, seenStyle = false;
    // attributes
    while ( i < n && html[i] != '>' ) {
      if ( isWs( html[i] ) || html[i] == '/' ) { ++i; continue; }
      const int ns = i;
      ++i;   // the first character is part of the name, even '='
      while ( i < n && !isWs( html[i] ) && html[i] != '/' && html[i] != '>' && html[i] != '=' )
        ++i;
      const QString name = html.mid( ns, i - ns ).lower();
      int k = i;
      while ( k < n && isWs( html[k] ) ) ++k;
      if ( k >= n || html[k] != '=' )
        continue;   // no value
      i = k + 1;
      while ( i < n && isWs( html[i] ) ) ++i;
      const int vs = i;   // the value, quotes included
      int cs, ce;         // its characters
      if ( i < n && ( html[i] == '"' || html[i] == '\'' ) ) {
        const QChar q = html[i];
        cs = i + 1;
        ce = html.find( q, cs );
        if ( ce < 0 ) ce = n;
        i = ce < n ? ce + 1 : n;
      } else {
        cs = i;
        while ( i < n && !isWs( html[i] ) && html[i] != '>' ) ++i;
        ce = i;
      }
      const int ve = i;
      // the first of each name counts, as for the browser
      bool url = false, style = false;
      if ( name == "src" ) { url = tag == "img" && !seenSrc; seenSrc = true; }
      else if ( name == "background" ) { url = !seenBackground; seenBackground = true; }
      else if ( name == "style" ) { style = !seenStyle; seenStyle = true; }
      if ( !url && !style )
        continue;
      const QString value = decodeReferences( html.mid( cs, ce - cs ) );
      QString replacement;
      if ( url ) {
        replacement = resolveValue( value, resolver );
      } else {
        bool changed = false;
        const QString css = rewriteCss( value, resolver, changed );
        if ( changed )
          replacement = css;
      }
      if ( replacement.isEmpty() )
        continue;
      out += html.mid( last, vs - last ) + "\"" + escapeAttribute( replacement ) + "\"";
      last = ve;
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
