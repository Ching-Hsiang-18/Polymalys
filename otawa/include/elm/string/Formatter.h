/*
 * $Id$
 * Copyright (c) 2006, IRIT-UPS <casse@irit.fr>
 *
 * elm/util/Formatter.h -- Formatter class interface.
 */
#ifndef ELM_STRING_FORMATTER_H
#define ELM_STRING_FORMATTER_H

#include <elm/io.h>

namespace elm {

// Formatter class
class Formatter {
	char esc;
protected:
	static const int DONE = 0;
	static const int CONTINUE = 1;
	static const int REJECT = 2;
	virtual int process(io::OutStream& out, char chr) = 0;
public:
	Formatter(char esc = '%');
	virtual ~Formatter(void) { }
	int format(io::InStream& in, io::OutStream& out = io::out);
	inline char escape(void) const;
};

// Formatter inlines
inline char Formatter::escape(void) const {
	return esc;
}

} // elm

#endif	// ELM_STRING_FORMATTER_H
