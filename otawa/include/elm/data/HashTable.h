/*
 *	HashTable class interface
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
#ifndef ELM_DATA_HASHTABLE_H_
#define ELM_DATA_HASHTABLE_H_

#include "Manager.h"
#include <elm/adapter.h>
#include <elm/array.h>

namespace elm {

template <class T, class A = IdAdapter<T>, class M = HashManager<typename A::key_t> >
class HashTable {
public:
	typedef HashTable<T, A, M> self_t;
	typedef typename A::key_t key_t;
	typedef typename A::val_t val_t;
	typedef typename A::data_t data_t;

private:
	typedef struct node_t {
		inline node_t(const data_t& d): next(0), data(d)  { }
		node_t *next;
		data_t data;
	} node_t;

	node_t *find(const key_t& key) const {
		int i = _man.hash.computeHash(key) % _size;
		for(node_t *node = _tab[i], *prev = 0; node; prev = node, node = node->next)
			if(_man.hash.isEqual(A::key(node->data), key)) {
				if(prev) { prev->next = node->next; node->next = _tab[i]; _tab[i] = node; }
				return node;
			}
		return 0;
	}

	node_t *make(const data_t& data, M& man)
		{ int i = _man.hash.computeHash(A::key(data)) % _size;
		node_t *node = new (man.alloc.allocate(sizeof(node_t))) node_t(data);
		node->next = _tab[i]; _tab[i] = node; return node; }

	struct InternIterator {
		inline InternIterator(const self_t& _htab): htab(&_htab) { i = 0; step(); }
		inline bool ended(void) const { return i >= htab->_size; }
		inline void next(void) { node = node->next; if(!node) { i++; step(); }  }
	protected:
		node_t *node;
	private:
		inline void step(void) { for(; i < htab->_size; i++) if(htab->_tab[i]) { node = htab->_tab[i]; break; } }
		const self_t *htab;
		int i;
	};

public:

	HashTable(int _size = 211): _man(Single<M>::_), _size(_size), _tab(new node_t *[_size])
		{ array::fast<node_t*>::clear(_tab, _size); }
	HashTable(M& man, int _size = 211): _man(man), _size(_size), _tab(new node_t *[_size])
		{ array::fast<node_t*>::clear(_tab, _size); }
	HashTable(const self_t& h): _man(h._man), _size(h._size), _tab(new node_t *[h._size])
		{ array::fast<node_t*>::clear(_tab, _size); putAll(h); }
	~HashTable(void)
		{ clear(); delete [] _tab; }

	bool isEmpty(void) const
		{ for(int i = 0; i < _size; i++) if(_tab[i]) return false; return true; }
	int count(void) const
	 	{ int cnt = 0; for(int i = 0; i < _size; i++) for(node_t *cur = _tab[i]; cur; cur = cur->next) cnt++; return cnt; }

	inline const data_t *get(const key_t& key) const
		{ node_t *node = find(key); return node ? &node->data : 0; }
	inline bool hasKey(const key_t& key) const
	 	{ node_t *node = find(key); return node != 0; }
	inline bool exists(const key_t& key) const { return hasKey(key); }

	void put(const data_t& data)
		{ node_t *node = find(A::key(data)); if(node) node->data = data; else add(data); }
	void add(const data_t& data) { make(data, _man); }
	void putAll(const self_t& m) { for(typename self_t::Iter i(m); i; i++) put(*i); }

	void remove(const key_t& key) {
		int i = _man.hash.computeHash(key) % _size;
		for(node_t *node = _tab[i], *prev = 0; node; prev = node, node = node->next)
			if(_man.hash.isEqual(A::key(node->data), key)) {
				if(prev)
					prev->next = node->next;
				else
					_tab[i] = node->next;
				_man.alloc.free(node);
				break;
			}
	}

	void clear(void) {
		for(int i = 0; i < _size; i++) {
			for(node_t *cur = _tab[i], *next; cur; cur = next) { next = cur->next; _man.alloc.free(cur); }
			_tab[i] = 0;
		}
	}

	class Iter: public InternIterator, public PreIterator<Iter, T> {
	public:
		inline Iter(const self_t& htab): InternIterator(htab) { };
		inline const data_t& item(void) const { return this->node->data; }
		inline const key_t& key(void) const { return A::key(this->node->data); };
	};

	inline self_t& operator=(const self_t& map) { clear(); putAll(map); return *this; }

#	ifdef ELM_STAT
		int minEntry(void) const { int m = count(0); for(int i = 1; i < _size; i++) m = min(m, count(i)); return m; }
		int maxEntry(void) const { int m = count(0); for(int i = 1; i < _size; i++) m = max(m, count(i)); return m; }
		int zeroEntry(void) const { int c = 0; for(int i = 0; i < _size; i++) if(count(i) == 0) c++; return c; }
		int size(void) const { return _size; }
#	endif

private:
#	ifdef ELM_STAT
		int count(int i) const { int c = 0; for(node_t *n = _tab[i]; n; n = n->next) c++; return c; }
#	endif

	M& _man;
	int _size;
	node_t **_tab;
};

}	// otawa

#endif /* ELM_DATA_HASHTABLE_H_ */
