/*
 *	$Id$
 *	AbstractCollection class interface
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
#ifndef ELM_DYNDATA_ABSTRACTCOLLECTION_H
#define ELM_DYNDATA_ABSTRACTCOLLECTION_H

#include <elm/PreIterator.h>

namespace elm { namespace dyndata {

// IteratorInst class
template <class T>
class AbstractIter {
public:
	virtual ~AbstractIter(void) { }
	virtual bool ended(void) const = 0;
	virtual T item(void) const = 0;
	virtual void next(void) = 0;
};


// Iterator class
template <class T>
class Iter: public PreIterator<Iter<T>, T> {
public:
	inline Iter(AbstractIter<T> *_iter): iter(_iter) { }
	inline AbstractIter<T> *instance(void) const { return iter; }
	inline ~Iter(void) { delete iter; }
	inline bool ended(void) const { return iter->ended(); }
	inline T item(void) const { return iter->item(); }
	inline void next(void) { iter->next(); }
protected:
	AbstractIter<T> *iter;
};


// AbstractCollection class
template <class T>
class AbstractCollection {
public:
	virtual ~AbstractCollection(void) { }
	virtual int count(void) = 0;
	virtual bool contains(const T& item) const = 0;
	virtual bool isEmpty(void) const = 0;
	inline operator bool(void) const { return !isEmpty(); }
	virtual AbstractIter<T> *iterator(void) const = 0;
};

// MutableAbstractCollection class
template <class T>
class MutableAbstractCollection: public virtual AbstractCollection<T> {
public:
	virtual ~MutableAbstractCollection(void) { }
	virtual void clear(void) = 0;
	virtual void add(const T& item) = 0;
	virtual void addAll(const AbstractCollection<T>& items) = 0;
	virtual void remove(const T& item) = 0;
	virtual void removeAll(const AbstractCollection<T>& items) = 0;
	virtual void remove(const AbstractIter<const T&>& iter) = 0;
};

} } // elm::dyndata

#endif	// ELM_DYNDATA_ABSTRACTCOLLECTION_H
