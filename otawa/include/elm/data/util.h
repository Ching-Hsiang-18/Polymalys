/*
 *	utilities for data module
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
#ifndef ELM_DATA_UTIL_H_
#define ELM_DATA_UTIL_H_

#include <elm/PreIterator.h>
#include <elm/meta.h>
#include <elm/type_info.h>
#include <elm/types.h>

#include "util.h"

namespace elm {

// simple functions
template <class T>
struct Add {
	typedef T x_t;
	typedef T y_t;
	inline T operator()(const T& a, const T& b) const { return a + b; }
	static T null;
};
template <class T> struct scalar_zero
	{ static inline T zero(void) { return 0; } };
template <class T> struct class_zero
	{ static inline T zero(void) { return T::zero; } };
template <class T> T Add<T>::null = _if<type_info<T>::is_scalar, scalar_zero<T>, class_zero<T> >::_::zero();

template <class T>
struct Mul {
	typedef T x_t;
	typedef T y_t;
	inline T operator()(const T& a, const T& b) const { return a * b; }
	static T null;
};
template <class T> struct scalar_one
	{ static inline T one(void) { return 1; } };
template <class T> struct class_one
	{ static inline T one(void) { return T::zero; } };
template <class T> T Mul<T>::null = _if<type_info<T>::is_scalar, scalar_one<T>, class_one<T> >::_::one();

template <class T> struct true_pred
	{ inline bool operator()(const T& v) { return true; } };


// predicate operations
template <class C, class P>
inline int count(const C& c, const P& p)
	{ int r = 0; for(typename C::Iter i(c); i; i++) if(p(*i)) r++; return r; }

template <class C, class P, class A>
inline int count(const C& c, const P& p, const A& a)
	{ int r = 0; for(typename C::Iter i(c); i; i++) if(p(*i, a)) r++; return r; }

template <class C, class P>
inline bool forall(const C& c, const P& p)
	{ for(typename C::Iter i(c); i; i++) if(!p(*i)) return false; return true; }

template <class C, class P, class A>
inline bool forall(const C& c, const P& p, const A& a)
	{ for(typename C::Iter i(c); i; i++) if(!p(*i, a)) return false; return true; }

template <class C, class P>
inline bool exists(const C& c, const P& p)
	{ for(typename C::Iter i(c); i; i++) if(p(*i)) return true; return false; }

template <class C, class P, class A>
inline bool exists(const C& c, const P& p, const A& a)
	{ for(typename C::Iter i(c); i; i++) if(p(*i, a)) return true; return false; }


// lookup operation
template <class I, class P>
inline I find(I i, const P& p)
	{ for(; i; i++) if(p(*i)) break; return i; }

template <class I, class P, class A>
inline I find(I i, const P& p, const A& a)
	{ for(; i; i++) if(p(*i, a)) break; return i; }


// transformation operations
template <class C, class F, class D>
inline void map(const C& c, const F& f, D& d)
	{ for(typename C::Iter i(c); i; i++) d.add(f(*i)); }

template <class C, class F, class A, class D>
inline void map(const C& c, const F& f, const A& a, D& d)
	{ for(typename C::Iter i(c); i; i++) d.add(f(*i, a)); }

template <class C, class F>
inline void iter(const C& c, const F& f)
{ for(typename C::Iter i(c); i; i++) f(*i); }

template <class C, class F, class A>
inline void iter(const C& c, const F& f, const A& a)
{ for(typename C::Iter i(c); i; i++) f(*i, a); }

template <class C, class F>
inline typename F::y_t fold(const C& c, const F& f, typename F::y_t t = F::null)
	{ for(typename C::Iter i(c); i; i++) t = f(*i, t); return t; }

template <class C>
inline typename C::t sum(const C& c)
	{ return fold(c, Single<Add<typename C::t> >::_, 0); }

template <class C>
inline typename C::t product(const C& c)
	{ return fold(c, Single<Mul<typename C::t> >::_); }


// construction operations
template <class C>
inline void fill(C& c, int n, const typename C::t v = type_info<typename C::t>::null)
	{ for(int i = 0; i < n; i++) c.add(v); }

} // elm

#endif /* ELM_DATA_UTIL_H_ */
