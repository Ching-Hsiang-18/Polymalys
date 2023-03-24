#ifndef PPLMANAGER_H
#define PPLMANAGER_H 1

#include <elm/util/BitVector.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/ipet.h>
#include <otawa/otawa.h>
#include <otawa/prog/sem.h>
#include <ppl.hh>

#include "PPLDomain.h"
#include "PolyCommon.h"

namespace otawa {
namespace poly {

class PPLManager {
  public:
	using t = PPLDomain;

  private:
  public:
	/**
	 * Create PPLManager for existing init state.
	 */
	inline PPLManager(t &init, const PropList &props, WorkSpace *ws) 
		: _init(init), _bot(), _top(MAX_AXIS(props), ws) {}

	/**
	 * Create PPLManager using a fresh init state.
	 */
	explicit PPLManager(const PropList &props, WorkSpace *ws);

	inline ~PPLManager() = default;

	inline t &init() { return _init; }
	inline t &bot() { return _bot; }
	inline t &top() { return _top; }

    inline t join(t &v1, const t &v2, bool avcreate = false) { return v1.onMerge(v2, false, avcreate); }
    inline t widening(t &v1, const t &v2, bool avcreate = false) { return v1.onMerge(v2, true, avcreate); }
	inline bool equals(const t &v1, const t &v2) { return v1.equals(v2); }

	inline void enableSummary() { _init.enableSummary(); }
  private:
	t _init;
	t _bot;
	t _top;
};

} // namespace poly
} // namespace otawa
#endif
