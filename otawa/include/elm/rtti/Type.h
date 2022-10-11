/*
 *	Type class interface
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
#ifndef ELM_RTTI_TYPE_H_
#define ELM_RTTI_TYPE_H_

#include <elm/ptr.h>
#include <elm/string.h>
#include <elm/util/Initializer.h>

namespace elm {

namespace serial2 {
	class Serializer;
	class Unserializer;
};

namespace rtti {

class AbstractClass;
class Type;

class Enumerable {
public:
	virtual ~Enumerable(void);
	virtual const Type& type(void) const = 0;
	virtual int valueFor(string text) const = 0;
	virtual cstring nameFor(int value) const = 0;
};

class Serializable {
public:
	virtual ~Serializable(void);
	virtual const Type& type(void) const = 0;
	virtual void *instantiate(void) const = 0;
	virtual void serialize(serial2::Serializer& ser, const void *data) const = 0;
	virtual void unserialize(serial2::Unserializer& uns, void *data) const = 0;
};

class Type {
public:
	static const Type *get(string name);
	Type(string name = "unknown");
	virtual ~Type(void);
	inline string name(void) const { return _name; }
	const Type& pointer(void) const;
	virtual bool canCast(const Type *t) const;

	virtual bool isVoid(void) const;
	virtual bool isBool(void) const;
	virtual bool isInt(void) const;
	virtual bool isFloat(void) const;
	virtual bool isPtr(void) const;

	virtual bool isClass(void) const;
	virtual const AbstractClass& asClass(void) const;
	virtual bool isEnum(void) const;
	virtual const Enumerable& asEnum(void) const;
	virtual bool isSerial(void) const;
	virtual const Serializable& asSerial(void) const;

	void initialize(void);
	inline bool operator==(const Type& t) const { return this == &t; }
	inline bool operator!=(const Type& t) const { return !operator==(t); }
private:
	string _name;
	mutable UniquePtr<Type>_pointer;
	static Initializer<Type> _init;
};

} }		// otawa::rtti

#endif /* ELM_RTTI_TYPE_H_ */
