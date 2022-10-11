/*
 *	ListSet class interface
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2017, IRIT UPS.
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
#ifndef ELM_DATA_LISTMAP_H_
#define ELM_DATA_LISTMAP_H_

#include "SortedList.h"

namespace elm {

template <class K, class T, class KC = Comparator<K>, class TE = Equiv<K>, class A = DefaultAllocator>
class ListMapManager {
public:
	inline ListMapManager(KC& kcmp = Single<KC>::_, TE& tequ = Single<TE>::_, A& alloc = DefaultAllocator::DEFAULT)
		: _kcmp(kcmp), _tequ(tequ), _alloc(alloc) { }

	inline void *allocate(t::size size) { return _alloc.allocate(size); }
	inline void free(t::ptr p) { _alloc.free(p); }

	inline int compareKey(const K& k1, const K& k2) { return _kcmp.doCompare(k1, k2); }
	inline bool equalsValue(const T& v1, const T& v2) { return _tequ.isEqual(v1, v2); }

	// for compatibility with CompareManager
	typedef Pair<K, T> pair_t;
	inline int compare(const pair_t& p1, const pair_t& p2) { return _kcmp.doCompare(p1.fst, p2.fst); }
	inline bool equals(const pair_t& p1, const pair_t& p2) { return _kcmp.doCompare(p1.fst, p2.fst) == 0; }

private:
	KC& _kcmp;
	TE& _tequ;
	A& _alloc;
};

template <class K, class T, class M = ListMapManager<K, T> >
class ListMap: private SortedList<typename M::pair_t, M> {
	typedef SortedList<typename M::pair_t, M> base_t;
public:
	typedef ListMap<K, T, M> self_t;

	inline ListMap(M& man = Single<M>::_): base_t(man) { }
	inline ListMap(const ListMap<K, T, M>& l): base_t::type_t(l) { }
	inline M& manager(void) const { return base_t::manager(); }

	typedef typename base_t::Iter PairIter;

	class Iter: public PreIterator<Iter, T> {
	public:
		inline Iter(const ListMap<K,T, M>& l): i(l) { }
		inline bool ended(void) const { return i.ended(); }
		inline const T& item(void) const { return (*i).snd; }
		inline void next(void) { i.next(); }
	private:
		PairIter i;
	};

	class KeyIter: public PreIterator<KeyIter, T> {
	public:
		inline KeyIter(void) { }
		inline KeyIter(const ListMap<K,T, M>& l): i(l) { }
		inline bool ended(void) const { return i.ended(); }
		inline const K& item(void) const { return (*i).fst; }
		inline void next(void) { i.next(); }
	private:
		PairIter i;
	};

	// Collection concept
	inline Iter begin(void) const { return Iter(*this); }
	inline Iter end(void) const { return Iter(); }
	inline int count(void) const { return base_t::count(); }
	inline bool contains(const T& v) const
		{ for(Iter i = begin(); i; i++) if(manager().equals(*i, v)) return true; return false; }
	template <class C> inline bool contains(const C& c) const
		{ for(typename C::Iter i = c.items(); i; i++) if(!contains(*i)) return false; return true; }
	inline bool isEmpty(void) const { return base_t::isEmpty(); }
	inline operator bool(void) const { return !isEmpty(); }
	inline Iter items(void) const { return begin(); }
	inline Iter operator*(void) const { return begin(); }
	inline operator Iter(void) const { return begin(); }
	inline bool equals(const self_t& m) const { return base_t::equals(m); }
	inline bool operator==(const self_t& m) const { return equals(m); }
	inline bool operator!=(const self_t& m) const { return !equals(m); }
	inline bool contains(const self_t& m) const { return base_t::contains(m); }
	inline bool operator<=(const self_t& m) const { return m.contains(*this); }
	inline bool operator<(const self_t& m) const { return m.contains(*this) && !equals(m); }
	inline bool operator>=(const self_t& m) const { return contains(m); }
	inline bool operator>(const self_t& m) const { return contains(m) && !equals(m); }

	// Map concept
	inline Option<T> get(const K &k) const
		{ PairIter i = lookup(k); if(i) return some((*i).snd); else return none; }
	inline const T &get(const K &k, const T &d) const
		{ PairIter i = lookup(k); if(i) return (*i).snd; else return d; }
	inline bool hasKey(const K &k) const { return lookup(k); }
	inline KeyIter keys(void) const { return KeyIter(*this); }
	inline KeyIter keys_end(void) const { return KeyIter(); }
	inline PairIter pairs(void) const { return PairIter(*this); }
	inline PairIter pairs_end(void) const { return PairIter(); }

	// MutableMap concept
	void put(const K& k, const T& v) {
		PairIter i = lookup(k);
		if(i)
			base_t::set(i, pair(k, v));
		else
			base_t::add(pair(k, v));
	}
	inline void remove(const Iter& i) { base_t::remove(*i); }
	inline void remove(const K& k)
		{ PairIter i = lookup(k); if(i) base_t::remove(*i); }

private:
	PairIter lookup(const K& k) const {
		for(PairIter i = base_t::begin(); i; i++) {
			int cmp = base_t::manager().compareKey(k, (*i).fst);
			if(cmp == 0)
				return i;
			else if(cmp < 0)
				break;
		}
		return base_t::end();
	}
};

template <class K, class T, class M>
inline bool operator<=(const T& v, const ListMap<K, T, M>& m) { return m.contains(v); }

}	// elm

#endif /* ELM_DATA_LISTMAP_H_ */
