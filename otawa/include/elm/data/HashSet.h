/*
 *	HashSet class interface
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2016, IRIT UPS.
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
#ifndef ELM_DATA_HASHSET_H_
#define ELM_DATA_HASHSET_H_

#include "HashTable.h"
#include <elm/adapter.h>

namespace elm {

template <class T, class M = HashManager<T> >
class HashSet {
	typedef HashTable<T, IdAdapter<T>, M> tab_t;
public:
	typedef HashSet<T, M> self_t;

	inline HashSet(int size = 211): _tab(size) { }
	inline HashSet(M& man, int size = 211): _tab(man, size) { }

	class Iter: public PreIterator<Iter, T> {
	public:
		inline Iter(const HashSet& set): i(set._tab) { }
		inline bool ended(void) const { return i.ended(); }
		inline const T& item(void) const { return i.item(); }
		inline void next(void) { i.next(); }
	private:
		typename tab_t::Iter i;
	};

	inline int count(void) const { return _tab.count(); }
	inline bool contains(const T& val) { return _tab.hasKey(val); }
	template <class C> inline bool containsAll(const C& coll)
		{ for(typename C::Iter i(coll); i; i++) if(!contains(*i)) return false; return true; }
	inline bool isEmpty(void) const { return _tab.isEmpty(); }
	inline Iter items(void) const { return Iter(*this); }

	inline void clear(void) { _tab.clear(); }
	inline void add(const T& val) { _tab.put(val); }
	template <class C> void addAll(const C& coll)
		{ for(typename C::Iter i(coll); i; i++) add(*i); }
	inline void remove(const T& val) { _tab.remove(val); }
	template <class C> void removeAll(const C& coll)
		{ for(typename C::Iter i(coll); i; i++) remove(*i); }
	inline void remove(const Iter& i) { remove(*i); }

	inline void insert(const T& val) { add(val); }

	inline operator bool(void) const { return !isEmpty(); }
	inline Iter operator*(void) const { return items(); }
	static const self_t null;

#	ifdef ELM_STAT
		int minEntry(void) const { return _tab.minEntry(); }
		int maxEntry(void) const { return _tab.maxEntry(); }
		int zeroEntry(void) const { return _tab.zeroEntry(); }
		int size(void) const { return _tab.size(); }
#	endif

private:
	tab_t _tab;
};

template <class T, class M>
const HashSet<T, M> HashSet<T, M>::null(1);

}	// elm

#endif /* ELM_DATA_HASHSET_H_ */
