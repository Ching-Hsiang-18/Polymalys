/*
 *	type_of function definition
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2017, IRIT UPS.
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
#ifndef ELM_RTTI_TYPE_OF_H_
#define ELM_RTTI_TYPE_OF_H_

#include <elm/string.h>
#include <elm/types.h>

#include "Type.h"

namespace elm { namespace rtti {

class Object {
};

// base types
extern const Type &int8_type, &uint8_type, &int16_type, &uint16_type, &int32_type, &uint32_type, &int64_type, &uint64_type;
extern const Type &float_type, &double_type, &long_double_type;
extern const Type &bool_type, &string_type, &cstring_type, &void_type;

// type determination
template <class T> struct _type
	{ static inline const Type& _(void) { return T::__type; } };
template <> inline const Type& _type<t::int8>::_(void) { return int8_type; }
template <> inline const Type& _type<t::int16>::_(void) { return int16_type; }
template <> inline const Type& _type<t::int32>::_(void) { return int32_type; }
template <> inline const Type& _type<t::int64>::_(void) { return int64_type; }
template <> inline const Type& _type<t::uint8>::_(void) { return uint8_type; }
template <> inline const Type& _type<t::uint16>::_(void) { return uint16_type; }
template <> inline const Type& _type<t::uint32>::_(void) { return uint32_type; }
template <> inline const Type& _type<t::uint64>::_(void) { return uint64_type; }
template <> inline const Type& _type<float>::_(void) { return float_type; }
template <> inline const Type& _type<double>::_(void) { return double_type; }
template <> inline const Type& _type<long double>::_(void) { return long_double_type; }
template <> inline const Type& _type<bool>::_(void) { return bool_type; }
template <> inline const Type& _type<cstring>::_(void) { return cstring_type; }
template <> inline const Type& _type<string>::_(void) { return string_type; }
template <> inline const Type& _type<void>::_(void) { return void_type; }

template <class T> struct _type<T *>
	{ static inline const Type& _(void) { return _type<T>::_().pointer(); } };

} // rtti

template <class T> inline const rtti::Type& type_of(void) { return rtti::_type<T>::_(); }
template <class T> inline const rtti::Type& type_of(const T& v) { return rtti::_type<T>::_(); }

#define ELM_DECLARE_CLASS(clazz) \
	namespace elm { namespace rtti { template <> const Type& _type<clazz>::_(void); } }

#define ELM_DEFINE_CLASS(clazz, desc) \
		namespace elm { namespace rtti { template <> const Type& _type<clazz>::_(void) { return desc; } } }

#ifndef ELM_NO_SHORTCUT
#	define DECLARE_CLASS(name) 		ELM_DECLARE_CLASS(name)
#	define DEFINE_CLASS(type, desc)	ELM_DEFINE_CLASS(type, desc)
#endif


}		// elm

#endif /* ELM_RTTI_TYPE_OF_H_ */
