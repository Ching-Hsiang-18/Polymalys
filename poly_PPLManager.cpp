#include <ctime>
#include <otawa/cfg/Edge.h>
#include <otawa/dfa/FastState.h>
#include <otawa/dfa/ai.h>
#include <otawa/dfa/State.h>
#include <otawa/flowfact/features.h>
#include <otawa/graph/Graph.h>
#include <otawa/otawa.h>
#include <otawa/util/HalfAbsInt.h>
#include <otawa/util/WideningFixPoint.h>
#include <otawa/util/WideningListener.h>
#include <ppl.hh>

#include "include/PPLDomain.h"
#include "include/PPLManager.h"
#include "include/PolyAnalysis.h"

namespace otawa {
namespace poly {
using namespace otawa;
using namespace util;

/**
 * Create PPLManager using a fresh init state.
 */
PPLManager::PPLManager(const PropList &props, WorkSpace *ws)
	: _init(MAX_AXIS(props), ws), _bot(), _top(MAX_AXIS(props), ws) {

	WVar var_sp = _init.varNew(Ident(13, Ident::ID_REG), NO_STEP, false, false);
	WVar var_fp = _init.varNew(Ident(11, Ident::ID_REG), NO_STEP, false, false);
	WVar var_lr = _init.varNew(Ident(14, Ident::ID_REG), NO_STEP, false, false);
	WVar var_ssp = _init.varNew(Ident(Ident::ID_START_SP, Ident::ID_SPECIAL), NO_STEP, false, false);
	WVar var_sfp = _init.varNew(Ident(Ident::ID_START_FP, Ident::ID_SPECIAL), NO_STEP, false, false);
	WVar var_slr = _init.varNew(Ident(Ident::ID_START_LR, Ident::ID_SPECIAL), NO_STEP, false, false);

	_init.doNewConstraint(var_ssp == var_sp);
	_init.doNewConstraint(var_sfp == var_fp);
	_init.doNewConstraint(var_slr == var_lr);

	/* TODO(clement): get real stack base address */
	_init.doNewConstraint(var_ssp >= int(stackconf_t::STACK_TOP));
	_init.doNewConstraint(var_sfp >= int(stackconf_t::STACK_TOP));
	_init.doNewConstraint(var_ssp <= int(stackconf_t::STACK_TOP + 0x1000000));
	_init.doNewConstraint(var_sfp <= int(stackconf_t::STACK_TOP + 0x1000000));

}

} // namespace poly
} // namespace otawa
