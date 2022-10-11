/*
 *	AbstractClass and Class classes interface
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2007, IRIT UPS.
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
#ifndef ELM_RTTI_CLASS_H_
#define ELM_RTTI_CLASS_H_

#include <elm/data/List.h>
#include <elm/data/Vector.h>
#include <elm/util/Variant.h>
#include "type_of.h"

namespace elm { namespace rtti {

// operations
class Parameter {
public:
	inline Parameter(void): _type(&type_of<int>()) { }
	inline Parameter(const Type& type): _type(&type) { }
	inline Parameter(cstring name, const Type& type): _name(name), _type(&type) { }
	inline cstring name(void) const { return _name; }
	inline const Type& type(void) const { return *_type; }
private:
	cstring _name;
	const Type *_type;
};

class Operation {
public:
	typedef enum {
		NONE,
		CONSTRUCTOR,
		METHOD,
		STATIC,
		ITERATOR
	} kind_t;

	Operation(kind_t kind, cstring name, const Type& rtype = void_type);
	Operation(kind_t kind, cstring name, const List<Parameter>& pars, const Type& rtype = void_type);
	virtual ~Operation(void);

	inline kind_t kind(void) const { return _kind; }
	inline cstring name(void) const { return _name; }
	inline const Type& returnType(void) const { return _rtype; }
	const List<Parameter>& parameters(void) const { return _pars; }

	virtual Variant call(Vector<Variant>& args) const throw(Exception);

protected:
	void add(const Parameter& param);

private:
	kind_t _kind;
	cstring _name;
	List<Parameter> _pars;
	const Type& _rtype;
};

template <class T>
class Constructor0: public Operation {
public:
	Constructor0(cstring name): Operation(CONSTRUCTOR, name, T::__type.pointer()) { }
	virtual Variant call(const Vector<Variant>& args) { return new T(); }
};

template <class T, class T1>
class Constructor1: public Operation {
public:
	Constructor1(cstring name): Operation(CONSTRUCTOR, name, T::__type.pointer())
		{ add(Parameter(type_of<T1>())); }
	virtual Variant call(const Vector<Variant>& args) { return new T(args[0].as<T1>()); }
};

template <class T, class T1, class T2>
class Constructor2: public Operation {
public:
	Constructor2(cstring name): Operation(CONSTRUCTOR, name, T::__type.pointer())
		{ add(Parameter(type_of<T1>())); add(Parameter(type_of<T2>())); }
	virtual Variant call(const Vector<Variant>& args) { return new T(args[0].as<T1>(), args[1].as<T2>()); }
};

inline Variant __call_static0(void (*f)(void)) { f(); return Variant(); }
template <class T> inline Variant __call_static0(T (*f)(void)) { return Variant(f()); }
template <class T>
class Static0: public Operation {
public:
	Static0(cstring name, T (*f)(void)): Operation(STATIC, name, type_of<T>()), _f(f) { }
	virtual Variant call(const Vector<Variant>& args) { return __call_static0(_f); }
private:
	T (*_f)(void);
};

template <class T1> inline Variant __call_static1(void (*f)(T1), T1 a1) { f(a1); return Variant(); }
template <class T, class T1> inline Variant __call_static1(T (*f)(T1), T1 a1) { return Variant(f(a1)); }
template <class T, class T1>
class Static1: public Operation {
public:
	Static1(cstring name, T (*f)(T1)): Operation(STATIC, name, type_of<T>()), _f(f) { add(Parameter(type_of<T1>())); }
	virtual Variant call(const Vector<Variant>& args) { return __call_static1(_f, args[0].as<T1>()); }
private:
	T (*_f)(T1);
};

template <class T1, class T2> inline Variant __call_static2(void (*f)(T1, T2), T1 a1, T2 a2) { f(a1, a2); return Variant(); }
template <class T, class T1, class T2> inline Variant __call_static2(T (*f)(T1, T2), T1 a1, T2 a2) { return Variant(f(a1, a2)); }
template <class T, class T1, class T2>
class Static2: public Operation {
public:
	Static2(cstring name, T (*f)(T1, T2)): Operation(STATIC, name, type_of<T>()), _f(f)
		{ add(Parameter(type_of<T1>())); add(Parameter(type_of<T2>())); }
	virtual Variant call(const Vector<Variant>& args) { return __call_static2(_f, args[0].as<T1>(), args[1].as<T2>()); }
private:
	T (*_f)(T1, T2);
};

template <class C> inline Variant __call_method0(void (C::*f)(void), C *o) { (o->*f)(); return Variant(); }
template <class T, class C> inline Variant __call_method0(T (C::*f)(void), C *o) { return Variant((o->*f)()); }
template <class T, class C>
class Method0: public Operation {
public:
	typedef T (C::*fun_t)(void);
	Method0(cstring name, fun_t f): Operation(METHOD, name, type_of<T>()), _f() { add(Parameter(type_of<C>().pointer())); }
	virtual Variant call(const Vector<Variant>& args) { return __call_method0(_f, args[0].as<C *>()); }
private:
	fun_t _f;
};

template <class C, class T1> inline Variant __call_method1(void (C::*f)(T1), C *o, T1 a1) { (o->*f)(a1); return Variant(); }
template <class T, class C, class T1> inline Variant __call_method1(T (C::*f)(T1), C *o, T1 a1) { return Variant((o->*f)(a1)); }
template <class T, class C, class T1>
class Method1: public Operation {
public:
	typedef T (C::*fun_t)(T1);
	Method1(cstring name, fun_t f): Operation(METHOD, name, type_of<T>()), _f()
		{ add(Parameter(type_of<C>().pointer())); add(Parameter(type_of<T1>())); }
	virtual Variant call(const Vector<Variant>& args) { return __call_method1(_f, args[0].as<C *>(), args[1].as<T1>()); }
private:
	fun_t _f;
};

template <class C, class T1, class T2> inline Variant __call_method2(void (C::*f)(T1, T2), C *o, T1 a1, T2 a2) { (o->*f)(a1, a2); return Variant(); }
template <class T, class C, class T1, class T2> inline Variant __call_method2(T (C::*f)(T1, T2), C *o, T1 a1, T2 a2) { return Variant((o->*f)(a1, a2)); }
template <class T, class C, class T1, class T2>
class Method2: public Operation {
public:
	typedef T (C::*fun_t)(T2);
	Method2(cstring name, fun_t f): Operation(METHOD, name, type_of<T>()), _f()
		{ add(Parameter(type_of<C>().pointer())); add(Parameter(type_of<T1>())); add(Parameter(type_of<T2>())); }
	virtual Variant call(const Vector<Variant>& args) { return __call_method2(_f, args[0].as<C>(), args[1].as<T1>(), args[1].as<T2>()); }
private:
	fun_t _f;
};


// class definition
class make {
	friend class AbstractClass;
public:
	inline make(cstring name): _name(name), _base(&void_type.asClass()) { }
	template <class T> inline make& extend(void) { _base = &type_of<T>().asClass(); return *this; }
	inline make& extend(const AbstractClass& base) { _base = &base; return *this; }
	inline make& extend(const Type& base) { _base = &base.asClass(); return *this; }
	inline make& add(Operation *op) { _ops.add(op); return *this; }

	template <class T> inline make& construct(cstring name) { _ops.add(new Constructor0<T>(name)); return *this; }
	template <class T, class T1> inline make& construct(cstring name) { _ops.add(new Constructor1<T, T1>(name)); return *this; }
	template <class T, class T1, class T2> inline make& construct(cstring name) { _ops.add(new Constructor2<T, T1, T2>(name)); return *this; }

	template <class T> inline make& op(cstring name, T (*f)(void)) { _ops.add(new Static0<T>(name, f)); return *this; }
	template <class T, class T1> inline make& op(cstring name, T (*f)(T1)) { _ops.add(new Static1<T, T1>(name, f)); return *this; }
	template <class T, class T1, class T2> inline make& op(cstring name, T (*f)(T1, T2)) { _ops.add(new Static2<T, T1, T2>(name, f)); return *this; }

	template <class T, class C> inline make& op(cstring name, T (C::*f)(void)) { _ops.add(new Method0<T, C>(name, f)); return *this; }
	template <class T, class C, class T1> inline make& op(cstring name, T (C::*f)(T1)) { _ops.add(new Method1<T, C, T1>(name, f)); return *this; }
	template <class T, class C, class T1, class T2> inline make& op(cstring name, T (C::*f)(T1, T2)) { _ops.add(new Method2<T, C, T1, T2>(name, f)); return *this; }

private:
	cstring _name;
	const AbstractClass *_base;
	List<const Operation *> _ops;
};

class AbstractClass: public rtti::Type {
	friend class Operation;
public:

	AbstractClass(CString name, const AbstractClass& base);
	AbstractClass(const make& m);
	AbstractClass(const make& m, const AbstractClass& base);

	inline const AbstractClass& base(void) const { return _base; };
	virtual void *instantiate(void) const = 0;
	bool baseOf(const AbstractClass *clazz);
	virtual bool isClass(void) const;
	virtual const AbstractClass& asClass(void) const;
	virtual void *upCast(void *ptr) const;
	virtual void *downCast(void *ptr) const;

	void *upCast(void *ptr, const AbstractClass& cls) const;
	void *downCast(void *ptr, const AbstractClass& cls) const;

	inline const void *upCast(const void *ptr) const { return upCast(const_cast<void *>(ptr)); }
	inline const void *downCast(const void *ptr) const { return downCast(const_cast<void *>(ptr)); }
	inline const void *upCast(const void *ptr, const AbstractClass& cls) const { return upCast(const_cast<void *>(ptr), cls); }
	inline const void *downCast(const void *ptr, const AbstractClass& cls) const { return downCast(const_cast<void *>(ptr), cls); }

	inline const List<Operation *> operations(void) const { return _ops; }
private:
	const AbstractClass& _base;
	List<Operation *> _ops;
};


template <class T, class B = void>
class Class: public AbstractClass {
public:
	inline Class(CString name, const AbstractClass& base = type_of<B>().asClass()): AbstractClass(name, base) { };
	inline Class(const make& m): AbstractClass(m, type_of<B>().asClass()) { }
	virtual void *instantiate(void) const { return new T; };
	virtual void *upCast(void *ptr) const { return static_cast<B *>(static_cast<T *>(ptr)); }
	virtual void *downCast(void *ptr) const { return static_cast<T *>(static_cast<B *>(ptr)); }

	inline void *upCast(void *ptr, const AbstractClass& cls) const { return AbstractClass::upCast(const_cast<void *>(ptr), cls); }
	inline void *downCast(void *ptr, const AbstractClass& cls) const { return AbstractClass::downCast(const_cast<void *>(ptr), cls); }
	inline const void *upCast(const void *ptr) const { return AbstractClass::upCast(ptr); }
	inline const void *downCast(const void *ptr) const { return AbstractClass::downCast(ptr); }
	inline const void *upCast(const void *ptr, const AbstractClass& cls) const { return AbstractClass::upCast(ptr, cls); }
	inline const void *downCast(const void *ptr, const AbstractClass& cls) const { return AbstractClass::downCast(ptr, cls); }
};


// macros
#define ELM_IS_CLASS_EXTEND(name, base) \
	public: \
		typedef base __base; \
		static elm::rtti::Class<name, base> __type; \
	private:

#define ELM_CLASS_EXTEND(name, base) \
	class name: public base { \
	ELM_IS_CLASS_EXTEND(name, base)

#define ELM_IS_CLASS(name) \
	public: \
		typedef void __base; \
		static elm::rtti::Class<name, void> __type; \
	private:

#define ELM_CLASS(name) \
	class name { \
	ELM_IS_CLASS(name)

#define ELM_END_CLASS	};

#define ELM_IMPLEMENT(name) \
	elm::rtti::Class<name, typename name::__base> name::__type(#name)

#ifndef ELM_NO_SHORTCUT
#	define IS_CLASS_EXTEND(name, base)	ELM_IS_CLASS_EXTEND(name, base)
#	define CLASS_EXTEND(name, base)		ELM_CLASS_EXTEND(name, base)
#	define IS_CLASS(name)				ELM_IS_CLASS(name)
#	define CLASS(name)					ELM_CLASS(name)
#	define END_CLASS					ELM_END_CLASS
#	define IMPLEMENT(name)				ELM_IMPLEMENT(name)
#endif

} }	// elm::rtti

#endif /* ELM_RTTI_CLASS_H_ */
