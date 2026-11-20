/* trustitem.cpp - wraps a gpgme trust item
   Copyright (C) 2003 Klarälvdalens Datakonsult AB

   This file is part of GPGME.
 
   GPGME is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   GPGME is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GPGME; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gpgmepp/trustitem.h>

#include <gpgme.h>

#include <cassert>

#ifdef HAVE_GPGME_OP_TRUSTLIST
#define TRUST_ITEM_REF( item ) gpgme_trust_item_ref( item )
#define TRUST_ITEM_UNREF( item ) gpgme_trust_item_unref( item )
#define TRUST_ITEM_FIELD( item, field, null ) ( (item) ? (item)->field : (null) )
#else
// gpgme 2 has no trust items: Context never makes one, all are null
#define TRUST_ITEM_REF( item ) ( (void)(item) )
#define TRUST_ITEM_UNREF( item ) ( (void)(item) )
#define TRUST_ITEM_FIELD( item, field, null ) ( (void)(item), (null) )
#endif

namespace GpgME {

  struct TrustItem::Private  {
    Private( gpgme_trust_item_t aItem )
      : item( aItem )
    {
    }

    gpgme_trust_item_t item;
  };

  TrustItem::TrustItem( gpgme_trust_item_t item ) {
    d = new Private( item );
    if ( d->item )
      TRUST_ITEM_REF( d->item );
  }

  TrustItem::TrustItem( const TrustItem & other ) {
    d = new Private( other.d->item );
    if ( d->item )
      TRUST_ITEM_REF( d->item );
  }

  const TrustItem & TrustItem::operator=( const TrustItem & other ) {
    if ( &other == this ) return *this;

    if ( other.d->item )
      TRUST_ITEM_REF( other.d->item );
    if ( d->item )
      TRUST_ITEM_UNREF( d->item );
    *d = *other.d;
    return *this;
  }

  TrustItem::~TrustItem() {
    if ( d->item )
      TRUST_ITEM_UNREF( d->item );
    delete d; d = 0;
  }

  bool TrustItem::isNull() const {
    return !d || !d->item;
  }

  gpgme_trust_item_t TrustItem::impl() const {
    return d->item;
  }


  const char * TrustItem::keyID() const {
    return TRUST_ITEM_FIELD( d->item, keyid, (const char *)0 );
  }

  const char * TrustItem::userID() const {
    return TRUST_ITEM_FIELD( d->item, name, (const char *)0 );
  }

  const char * TrustItem::ownerTrustAsString() const {
    return TRUST_ITEM_FIELD( d->item, owner_trust, (const char *)0 );
  }

  const char * TrustItem::validityAsString() const {
    return TRUST_ITEM_FIELD( d->item, validity, (const char *)0 );
  }

  int TrustItem::trustLevel() const {
    return TRUST_ITEM_FIELD( d->item, level, 0 );
  }

  TrustItem::Type TrustItem::type() const {
    if ( !d->item )
      return Unknown;
    else 
      return
	TRUST_ITEM_FIELD( d->item, type, 0 ) == 1 ? Key :
	TRUST_ITEM_FIELD( d->item, type, 0 ) == 2 ? UserID :
	Unknown ;
  }

} // namespace GpgME
