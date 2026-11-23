/*  -*- c++ -*-
    cidrewriter.h

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

#ifndef __KMAIL_CIDREWRITER_H__
#define __KMAIL_CIDREWRITER_H__

#include <qstring.h>

namespace KMail {

  /** Gives the URL a Content-ID is shown with (an image of the message),
      or QString::null: the reference stays as it is. */
  class CidResolver {
  public:
    virtual ~CidResolver() {}
    /** @p shown: the reference shows the image for sure (an <img src>
        or background="" not hidden by its own attributes), so the part
        need not stay an attachment; false for references in CSS (whether
        a declaration shows its image is not known). */
    virtual QString urlFor( const QString & contentId, bool shown ) = 0;
  };

  /**
   * The cid: URLs @p html uses to show an image, replaced by the URLs
   * @p resolver gives: <img src>, background="" of any element, url() of
   * <style> and of style="".  Not cid: in comments, text, links, scripts,
   * other raw-text elements, CSS strings and comments, a repeated
   * attribute (the browser takes the first), <link>ed style sheets.
   *
   * A tokenizer after HTML's rules for what matters here (comments,
   * raw-text elements and their end tags, attribute quoting, character
   * references in attribute values), not a search in the text.  Linear in
   * the length of @p html: no search goes beyond the construct it is in.
   * What it does not recognize stays as it is (an image less, never one
   * more).
   */
  QString rewriteCidReferences( const QString & html, CidResolver & resolver );

} // namespace KMail

#endif // __KMAIL_CIDREWRITER_H__
