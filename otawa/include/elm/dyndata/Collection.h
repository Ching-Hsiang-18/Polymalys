/*
 *	$Id$
 *	Collection class interface
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2004-08, IRIT UPS.
 * 
 *	OTAWA is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	OTAWA is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with OTAWA; if not, write to the Free Software 
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef ELM_DYNDATA_COLLECTION_H
#define ELM_DYNDATA_COLLECTION_H

#include "../dyndata/AbstractCollection.h"

namespace elm { namespace dyndata {

// Collection class
template <class T, template <class _> class C>
class Collection: public virtual AbstractCollection<T> {
public:
	inline C<T>& collection(void) { return coll; }
	
	virtual ~Collection(void) { }
	virtual int count(void) { return coll.count(); }
	virtual bool contains(const T& item) const { return coll.contains(item); }
	virtual bool isEmpty(void) const { return coll.isEmpty(); }
	virtual AbstractIter<const T&> *iterator(void) const { return new Iter(coll); }

protected:
	C<T> coll;
	class Iter: public AbstractIter<const T&> {
	public:
		inline Iter(const C<T>& coll): iter(coll) { }
		virtual bool ended(void) const { return iter.ended(); }
		virtual const T& item(void) const { return iter.item(); }
		virtual void next(void) { iter.next(); }
		typename C<T>::Iterator iter;
	};
};

// MutableCollection class
template <class T, template <class _> class C>
class MutableCollection: public Collection<T, C>, public MutableAbstractCollection<T> {
public:
	virtual ~MutableCollection(void) { }
	virtual void clear(void) { Collection<T, C>::coll.clear(); }
	virtual void add(const T& item) { Collection<T, C>::coll.add(item); }
	virtual void addAll(const AbstractCollection<T>& items)
		{ for(Iter<const T&> iter(items.iterator()); iter; iter++) add(iter); }
	virtual void remove(const T& item) { Collection<T, C>::coll.remove(item); }
	virtual void removeAll(const AbstractCollection<T>& items)
		{ for(Iter<const T&> iter(items.iterator()); iter; iter++) remove(iter); }
	virtual void remove(const AbstractIter<const T&>& iter)
		{ Collection<T, C>::coll.remove(
			static_cast<typename Collection<T, C>::Iterator *>(iter.instance())->iter); }
};

template <class T, class I>
class IterInst: public elm::dyndata::AbstractIter<T> {
public:
	inline IterInst(const I& i): _i(i) { }
	virtual ~IterInst(void) { }
	virtual bool ended(void) const { return _i.ended(); }
	virtual T item(void) const { return _i.item(); }
	virtual void next(void) { _i.next(); }
private:
	I _i;
};
template <class T, class I>
inline IterInst<T, I> *iter(const I& i) { return new IterInst<T, I>(i); }

} } // elm::dyndata

#endif	// ELM_DYNDATA_COLLECTION_H
