/* simple hash table for kmail.  inspired by QDict */
/* Author: Ronen Tzur <rtzur@shani.net> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "kmdict.h"
#include "kmglobal.h"
#include <kdebug.h>

#include <limits.h>
#include <string.h>
//-----------------------------------------------------------------------------

KMDict::KMDict( int size )
{
  init( ( int ) KMail::nextPrime( size ) );
  //kdDebug( 5006 ) << "KMMDict::KMDict Size: " << mSize << endl;
}

//-----------------------------------------------------------------------------

KMDict::~KMDict()
{
  clear();
}

//-----------------------------------------------------------------------------

void KMDict::init(int size)
{
  mSize = size;
  mCount = 0;
  mVecs = new KMDictItem *[mSize];
  memset(mVecs, 0, mSize * sizeof(KMDictItem *));
}

//-----------------------------------------------------------------------------

void KMDict::clear()
{
  if (!mVecs)
    return;
  for (int i = 0; i < mSize; i++) {
    KMDictItem *item = mVecs[i];
    while (item) {
      KMDictItem *nextItem = item->next;
      delete item;
      item = nextItem;
    }
  }
  delete [] mVecs;
  mVecs = 0;
  mCount = 0;
}

//-----------------------------------------------------------------------------

void KMDict::replace( long key, KMDictItem *item )
{
  insert( key, item );
  removeFollowing( item, key );           // remove other items with same key
}

//-----------------------------------------------------------------------------


void KMDict::insert( long key, KMDictItem *item )
{
  if ( mCount >= 2 * mSize )
    grow();
  item->key = key;
  int idx = (unsigned long)key % mSize; // insert in
  item->next = mVecs[idx];              // appropriate
  mVecs[idx] = item;                    // column
  mCount++;
}

//-----------------------------------------------------------------------------

void KMDict::grow()
{
  const unsigned long n = KMail::nextPrime( 2 * (unsigned long)mCount );
  if ( n > (unsigned long)INT_MAX || (int)n <= mSize )
    return;
  const int newSize = (int)n;
  KMDictItem **newVecs = new KMDictItem *[newSize]();
  // the last item of each new column, to append to it: the items of a
  // column keep their order, so the newest of equal keys stays in front
  KMDictItem **tails = new KMDictItem *[newSize]();
  for (int i = 0; i < mSize; i++) {
    KMDictItem *item = mVecs[i];
    while (item) {
      KMDictItem *nextItem = item->next;
      int idx = (unsigned long)item->key % newSize;
      item->next = 0;
      if (tails[idx])
        tails[idx]->next = item;
      else
        newVecs[idx] = item;
      tails[idx] = item;
      item = nextItem;
    }
  }
  delete [] tails;
  delete [] mVecs;
  mVecs = newVecs;
  mSize = newSize;
}

//-----------------------------------------------------------------------------

void KMDict::remove(long key)
{
  int idx = (unsigned long)key % mSize;
  KMDictItem *item = mVecs[idx];

  if (item) {
    if (item->key == key) {             // if first in the column
      mVecs[idx] = item->next;
      delete item;
      mCount--;
    } else
      removeFollowing(item, key);       // if deep in the column
  }
}

//-----------------------------------------------------------------------------

void KMDict::removeFollowing(KMDictItem *item, long key)
{
  while (item) {
    KMDictItem *itemNext = item->next;
    if (itemNext && itemNext->key == key) {
      KMDictItem *itemNextNext = itemNext->next;
      delete itemNext;
      mCount--;
      item->next = itemNextNext;
    } else
      item = itemNext;
  }
}

//-----------------------------------------------------------------------------

KMDictItem *KMDict::find(long key)
{
  int idx = (unsigned long)key % mSize;
  KMDictItem *item = mVecs[idx];
  while (item) {
    if (item->key == key)
      break;
    item = item->next;
  }
  return item;
}
