#ifndef POLYANALYSIS_H
#define POLYANALYSIS_H 1

#include <elm/util/BitVector.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/dfa/ai.h>
#include <otawa/dfa/State.h>
#include <otawa/ipet.h>
#include <otawa/otawa.h>
#include <otawa/prog/sem.h>
#include <ppl.hh>

#include "PPLManager.h"

namespace otawa {
namespace poly {
using namespace otawa;
using namespace otawa::util;


class PolyAnalysis : public Processor {
  public:
	static p::declare reg;
	explicit PolyAnalysis(p::declare &r = reg);

  protected:
	void processWorkSpace(WorkSpace * /*ws*/) override;
	void configure(const PropList &props) override;

  private:
	using state_t = PPLManager::t;
	void processCFG(CFG & /* cfg */, state_t & /* s */, bool /* isEntryCFG */);
	void processBB(PPLManager *man, ai::CFGGraph &graph,
	               ai::WorkListDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph>> &ana,
				   ai::EdgeStore<PPLManager, ai::CFGGraph> &store,
	               genstruct::HashTable<int, state_t> &headerState);
	const PropList *_props{};
	state_t processHeader(ai::CFGGraph &graph, BasicBlock *header, PPLManager& man, ai::EdgeStore<PPLManager, ai::CFGGraph>& store, genstruct::HashTable<int, state_t> &headerState);


	/**
	 * Computes a pseudo-topological order on the CFG. 
	 *
	 * This order is a topological order on the DAG that is the CFG where the back-edges have been removed.
	 * In addition, if blocks b1,b2 are respectively in loops l1,l2 and l1 is inside l2, then b2 comes before b1 in the order.
	 *
	 * This is useful for speeding up the abstract interpretation by using a smart processing order in the worklist.
	 *
	 * @param graph The CFG
	 * @return map associating each block index to its pseudo-topological order.
	 */
	genstruct::HashTable<int, int>* _getPseudoTopo(const ai::CFGGraph &graph);
	void _topoLoopHelper(const ai::CFGGraph &graph, Block *start, int currentLoop);
	void _topoNodeHelper(const ai::CFGGraph &graph, Block *end);

	BitVector *_visited{};
	int _current;
	genstruct::HashTable<int,int> _rankLoop;
	genstruct::HashTable<int,int> *_rank;

	dfa::State *initState = NULL;
};

extern Identifier<genstruct::HashTable<int, int>* > WORKLIST_PRIORITY;

} // namespace poly
} // namespace otawa
#endif
