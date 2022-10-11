/*
 *	Array classes interface
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
#ifndef ELM_ARRAY_TABLE_H_
#define ELM_ARRAY_TABLE_H_

#include <elm/assert.h>
#include <elm/PreIterator.h>
#include <elm/array.h>
#include <elm/compare.h>

namespace elm {

template <class T>
class Array {
public:
	typedef T t;

	inline Array(void): cnt(0), buf(0) { }
	inline Array(int count, T *buffer): cnt(count), buf(buffer) { }
	static const Array<T> null;

	inline int count(void) const { return cnt; }
	inline const T *buffer(void) const { return buf; }
	inline T *buffer(void) { return buf; }
	inline int size(void) const { return count(); }

	class Iter: public PreIterator<Iter, T> {
	public:
		inline Iter(void): p(0), t(0) { }
		inline Iter(const Array<T>& table): p(table.buffer()), t(table.buffer() + table.count()) { }
		inline bool ended(void) const { return p >= t; }
		inline const T& item(void) const { ASSERT(p < t); return *p; }
		inline void next(void) { p++; }
	private:
		const T *p, *t;
	};
	inline Iter items(void) const { return Iter(*this); }
	inline void set(const Iter& i, const T& val) { ASSERT(buf <= i.p && i.p < buf + cnt); *i.p = val; }

	inline void set(int count, T *buffer) { cnt = count; buf = buffer; }
	inline void set(const Array<T>& t) { cnt = t.cnt; buf = t.buf; }
	inline void copy(const Array& t) { cnt = min(cnt, t.cnt); array::copy(buf, t.buf, cnt); }
	inline void fill(const T& val) { array::set(buf, cnt, val); }
	inline const T& get(int idx) const { ASSERT(0 <= idx && idx < cnt); return buf[idx]; }
	inline T& get(int idx) { ASSERT(0 <= idx && idx < cnt); return buf[idx]; }
	inline void set(int idx, const T& val) { ASSERT(0 <= idx && idx < cnt); buf[idx] = val; }

	inline const T *operator()(void) const { return buffer(); }
	inline T *operator()(void) { return buffer(); }
	inline const T& operator[](int idx) const { return get(idx); }
	inline T& operator[](int idx) { return get(idx); }
	inline Array<T>& operator=(const Array<T>& t) { set(t); return *this; }
	inline operator Iter(void) const { return items(); }
	inline Iter operator*(void) const { return items(); }

protected:
	int cnt;
	T *buf;
};

template <class T>
const Array<T> Array<T>::null;


template <class T>
class AllocArray: public Array<T> {
public:
	inline AllocArray(void) { }
	inline AllocArray(int count, T *buffer): Array<T>(count, buffer) { }
	inline AllocArray(int count): Array<T>(count, new T[count]) { }
	inline AllocArray(int count, const T& val): Array<T>(count, new T[count]) { fill(val); }
	inline AllocArray(const Array<T>& t): Array<T>(t.count(), new T[t.count()]) { Array<T>::copy(t); }
	inline AllocArray(const AllocArray<T>& t): Array<T>(t.cnt, new T[t.cnt]) { Array<T>::copy(t); }
	inline ~AllocArray(void) { if(this->buf) delete [] this->buf; }

	inline void copy(const Array<T>& t)
		{ if(this->count() < t.count()) { if(this->buf) delete [] this->buf;
		  Array<T>::set(t.count(), new T[t.count()]); } Array<T>::copy(t); }
	inline void tie(int cnt, T *buffer) { if(this->buf) delete [] this->buf; Array<T>::set(cnt, buffer); }
	inline void tie(const Array<T>& t) { if(this->buf) delete [] this->buf; Array<T>::set(t); }

	inline AllocArray<T>& operator=(const Array<T>& t) { copy(t); return *this; }
	inline AllocArray<T>& operator=(const AllocArray<T>& t) { copy(t); return *this; }
};

template <class T>
inline Array<T> _array(int n, T t[]) { return Array<T>(n, t); }

} // elm

#endif /* ELM_ARRAY_TABLE_H_ */
