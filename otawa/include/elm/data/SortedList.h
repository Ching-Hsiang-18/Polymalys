/*
 *	SortedList class interface
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2012, IRIT UPS.
 *
 *	ELM is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	ELM is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with ELM; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef ELM_DATA_SORTEDLIST_H_
#define ELM_DATA_SORTEDLIST_H_

#include "Adapter.h"
#include "List.h"
#include "Manager.h"
#include "util.h"
#include <elm/util/Option.h>

namespace elm {

// SortedList class
template <class T, class M = CompareManager<T> >
class SortedList {
protected:
	typedef List<T, M> list_t;
public:
	typedef T t;
	typedef SortedList<T, M> self_t;

	inline SortedList(void) { }
	inline SortedList(M& man): list(man) { }
	SortedList(const SortedList<T, M> &l): list(l.list) { }
	inline M& manager(void) const { return list.manager(); }

	inline void removeFirst(void) { list.removeFirst(); }
	inline void removeLast(void) { list.removeLast(); }

	// Collection concept
	inline int count (void) const { return list.count(); }

	inline bool contains(const T &item) const { return find(item) != end(); }
	template <class C> bool containsAll(const C& c) const
		{ for(typename C::Iter i(c); i; i++) if(!contains(*i)) return false; return true; }

	inline bool isEmpty(void) const { return list.isEmpty(); }
	inline operator bool(void) const { return !list.isEmpty(); }

	class Iter: public list_t::Iter {
	public:
		inline Iter(void) { }
		inline Iter(const self_t& _list): list_t::Iter(_list.list) { }
	};
	inline Iter items(void) const { return Iter(*this); }
	inline Iter operator*(void) const { return items(); }
	inline operator Iter(void) const { return items(); }
	inline Iter begin(void) const { return items(); }
	inline Iter end(void) const { return Iter(); }


	bool equals(const SortedList<T, M>& l) const {
		Iter i = *this, j = *l;
		for(; i && j; i++, j++)
			if(!list.manager().equals(*i, *j))
				return false;
		return !i && !j;
	}
	inline bool operator==(const SortedList<T, M>& l) const { return equals(l); }
	inline bool operator!=(const SortedList<T, M>& l) const { return !equals(l); }

	bool contains(const SortedList<T, M>& l) const {
		Iter i = *this, j = *l;
		for(; i && j; i++) {
			int cmp = list.manager().compare(*i, *j);
			if(cmp > 0)
				return false;
			if(cmp == 0)
				j++;
		}
		return !j;
	}
	inline bool operator<=(const SortedList<T, M>& l) const { return l.contains(*this); }
	inline bool operator<(const SortedList<T, M>& l) const { return l.contains(*this) && !equals(l); }
	inline bool operator>=(const SortedList<T, M>& l) const { return contains(l); }
	inline bool operator>(const SortedList<T, M>& l) const { return contains(l) && !equals(l); }


	// MutableCollection concept
	inline void clear(void) { list.clear(); }

	void add(const T &value) {
		for(typename list_t::PrecIter current(list); current; current++)
			if(list.manager().compare(value,  *current) < 0) {
				list.addBefore(current, value);
				return;
			}
		list.addLast(value);
	}

	template <template <class _> class CC> inline void addAll (const CC<T> &items)
		{ for(typename CC<T>::Iterator item(items); item; item++) add(item); }
	inline void remove(const T &item) { list.remove(item); }
	template <template <class _> class CC> inline void removeAll(const CC<T> &items)
		{ list.removeAll(items); }
	inline void remove(const Iter &iter) { list.remove(*iter); }
	inline SortedList<T, M>& operator+=(const T& v) { add(v); return *this; }
	inline SortedList<T, M>& operator-=(const T& v) { remove(v); return *this; }

	// List concept
	inline const T& first(void) const { return list.first(); }
	inline const T& last(void) const { return list.last(); }
	Iter find(const T& item, const Iter& iter) const {
		Iter i = iter; for(; i; i++) {
			int cmp = list.manager().compare(item, *i);
			if(cmp > 0) continue; else if(!cmp) return i; else return end();
		}
		return i;
	}
	inline Iter find(const T& item) const { return find(item, begin()); }

	// operators
	inline SortedList<T, M>& operator=(const SortedList<T, M>& sl) { list.copy(sl.list); return *this; }
	inline bool operator&(const T& e) const { return list.contains(e); }
	inline T& operator[](int k) { return list[k]; }
	inline const T& operator[](int k) const { return list[k]; }


protected:
	inline void set(Iter i, const T& val) { list.set(i, val); }
	list_t list;
};

}	// elm

#endif /* ELM_DATA_SORTEDLIST_H_ */
