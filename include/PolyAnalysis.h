// #define POLY_DEBUG 1
//
#ifndef OTAWA_POLY_ANALYSIS_H
#define OTAWA_POLY_ANALYSIS_H 1

#include <otawa/otawa.h>
#include <otawa/ipet.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/prog/sem.h>
#include <elm/util/BitVector.h>
#include <ppl.hh>

#include "PPLManager.h"

namespace otawa { namespace poly {
using namespace otawa;
using namespace otawa::util;

class PolyAnalysis: public Processor {
public:
	static p::declare reg;
	PolyAnalysis(p::declare& r = reg);

protected:
	void processWorkSpace(WorkSpace*) ;
	void configure(const PropList &props) ;
private:
	using state_t = PPLManager::t;
	void analyzeGraph(CFG &cfg, state_t &s, bool do_init); 
	const PropList* _props;
};




} } // namespace otawa::poly
#endif

