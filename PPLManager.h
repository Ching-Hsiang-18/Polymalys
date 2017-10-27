
#ifndef OTAWA_POLY_MANAGER_H
#define OTAWA_POLY_MANAGER_H 1

#include <otawa/otawa.h>
#include <otawa/ipet.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/prog/sem.h>
#include <elm/util/BitVector.h>
#include <ppl.hh>

#include "PPLDomain.h"

namespace otawa { namespace poly {

class PPLDomain;

class PPLManager {
public:
	using t = PPLDomain;
	private:

public:
	/**
	 * Create PPLManager for existing init state.
	 */
	inline PPLManager(t &init, const PropList &props) :  _init(init), _bot(), _top(MAX_AXIS(props)) { }


	/**
	 * Create PPLManager using a fresh init state.
	 */
	PPLManager(const PropList &props);

	inline ~PPLManager() { }

	inline t& init(void) { return _init; }
	inline t& bot(void) { return _bot; }
	inline t& top(void) { return _top; }

	inline t join(t& v1, const t& v2) { return v1.onMerge(v2, false); }
	inline t widening(t& v1, const t& v2) { return v1.onMerge(v2, true); }
	inline bool equals(const t& v1, const t& v2) { return v1.equals(v2); }

private:
	t _init;
	t _bot;
	t _top;
};

} } 
#endif

