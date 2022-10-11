/*
 *	HashMap class interface
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
#ifndef ELM_DATA_HASHMAP_H_
#define ELM_DATA_HASHMAP_H_

#include "HashTable.h"
#include <elm/delegate.h>

namespace elm {

template <class K, class T, class M = HashManager<K> >
class HashMap {
	typedef HashTable<Pair<K, T>, PairAdapter<K, T>, M> tab_t;
public:
	typedef HashMap<K, T, M> self_t;
	typedef typename tab_t::key_t key_t;
	typedef typename tab_t::val_t val_t;

	inline HashMap(int _size = 211): _tab(_size) { }
	inline HashMap(M& man, int _size = 211): _tab(man, _size) { }
	inline HashMap(const self_t& h): _tab(h._tab) { }

	inline bool isEmpty(void) const { return _tab.isEmpty(); }
	inline int count(void) const { return _tab.count(); }

	inline Option<T> get(const K& key) const
		{ const typename tab_t::data_t *r = _tab.get(key); if(r) return some((*r).snd); else return none; }
	inline const T& get(const K& key, const T& def) const
		{ const typename tab_t::data_t *r = _tab.get(key); if(r) return (*r).snd; else return def; }
	inline bool hasKey(const K& key) const { return _tab.hasKey(key); }
	inline bool exists(const K& key) const { return hasKey(key); }

	inline void put(const K& key, const T& val) { _tab.put(pair(key, val)); }
	inline void add(const K& key, const T& val) { _tab.add(pair(key, val)); }
	inline void remove(const K& key) { _tab.remove(key); }
	inline void clear(void) { _tab.clear(); }

	class Iter: public PreIterator<Iter, T> {
	public:
		inline Iter(const self_t& htab): i(htab._tab) { };
		inline bool ended(void) const { return i.ended(); }
		inline const T& item(void) const { return i.item().snd; }
		inline void next(void) { i.next(); }
		inline const K& key(void) const { return i.item().fst; }
	private:
		typename tab_t::Iter i;
	};

	class KeyIter: public PreIterator<KeyIter, T> {
	public:
		inline KeyIter(const self_t& htab): i(htab._tab) { };
		inline bool ended(void) const { return i.ended(); }
		inline const K& item(void) const { return i.item().fst; }
		inline void next(void) { i.next(); }
	private:
		typename tab_t::Iter i;
	};

	class PairIter: public PreIterator<PairIter, Pair<K, T> > {
	public:
		inline PairIter(const self_t& htab): i(htab._tab) { };
		inline bool ended(void) const { return i.ended(); }
		inline const Pair<K, T>& item(void) const { return i.item(); }
		inline void next(void) { i.next(); }
	private:
		typename tab_t::Iter i;
	};

	inline bool contains(const T& item)
		{ for(Iter i(*this); i; i++) if(*i == item) return true; return false; }
	template <class C>
	inline bool containsAll(const C& c)
		{ for(typename C::Iter i(c); c; i++) if(!contains(*i)) return false; return true; }
	inline void remove(Iter& i) const { const K& key = i.key(); i++; remove(i.key()); }

	inline Iter operator*(void) const { return Iter(*this); }
	inline operator bool(void) const { return !isEmpty(); }
	inline const T& operator[](const K& key) const { const typename tab_t::data_t *r = _tab.get(key); ASSERT(r); return (*r).snd; }
	inline StrictMapDelegate<self_t> operator[](const K& key) { return StrictMapDelegate<self_t>(*this, key); }
	inline const T& operator[](const Iter& i) const { const typename tab_t::data_t *r = _tab.get(i.key()); ASSERT(r); return (*r).snd; }
	inline StrictMapDelegate<self_t> operator[](const Iter& i) { return StrictMapDelegate<self_t>(*this, i.key()); }

#	ifdef ELM_STAT
		int minEntry(void) const { return _tab.minEntry(); }
		int maxEntry(void) const { return _tab.maxEntry(); }
		int zeroEntry(void) const { return _tab.zeroEntry(); }
		int size(void) const { return _tab.size(); }
#	endif

private:
	tab_t _tab;
};

}	// otawa

#endif /* ELM_DATA_HASHTABLE_H_ */
