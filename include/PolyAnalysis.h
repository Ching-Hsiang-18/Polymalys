// #define POLY_DEBUG 1
//
#ifndef POLYANALYSIS_H
#define POLYANALYSIS_H 1

#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/ipet.h>
#include <otawa/otawa.h>
#include <otawa/prog/sem.h>
#include <elm/util/BitVector.h>
#include <otawa/dfa/ai.h>
#include <ppl.hh>

#include "PPLManager.h"

namespace otawa { namespace poly {
using namespace otawa;
using namespace otawa::util;

class PolyAnalysis: public Processor {
public:
	static p::declare reg;
	explicit PolyAnalysis(p::declare& r = reg);

protected:
	void processWorkSpace(WorkSpace* /*ws*/) override ;
	void configure(const PropList &props) override ;
private:
	using state_t = PPLManager::t;
	void processCFG(CFG& /* cfg */, state_t& /* s */, bool /* isEntryCFG */); 
	void processBB(PPLManager *man, ai::CFGGraph &graph, ai::WorkListDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph> > &ana, genstruct::HashTable<int, state_t> &headerState);
	const PropList* _props{};
};

}  // namespace poly
 }  // namespace otawa
#endif

