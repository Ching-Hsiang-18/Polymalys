#ifndef POLYANALYSIS_H
#define POLYANALYSIS_H 1

#include <elm/util/BitVector.h>
#include <elm/data/Vector.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/ai/WorkListDriver.h>
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
using namespace elm;
using namespace otawa::ai;

class PolyAnalysis : public Processor {
  private:
	/**
	 * Computes a pseudo-topological order on the CFG. 
	 *
	 * This order is a topological order on the DAG that is the CFG where the back-edges have been removed.
	 * In addition, if blocks b1,b2 are respectively in loops l1,l2 and l1 is inside l2, then b2 comes before b1 in the order.
	 *
	 * This is useful for speeding up the abstract interpretation by using a smart processing order in the worklist.
	 */
	class PseudoTopoOrder {
		private:
			void _getPseudoTopo(const ai::CFGGraph &graph);
			void _topoLoopHelper(const ai::CFGGraph &graph, Block *start, int currentLoop);
			void _topoNodeHelper(const ai::CFGGraph &graph, Block *end);
			MyHTable<int, int> _loopOrder;
			MyHTable<int, int> _blockOrder;
			MyHTable<int, int> _belongsToLoop;
			BitVector *_visited{};
			int _current;

		public:

			/**
			 * @param graph The CFG to order
			 */
			PseudoTopoOrder(const ai::CFGGraph &graph) {
				_getPseudoTopo(graph);
			}

			/**
			 * Determines the preferred processing order in the abstract interpretation
			 * 
			 * @param b1 A basic block
			 * @param b2 Another basic block
			 * @return true if b1 should be processed before b2
			 */
			bool isBefore(const Block *b1, const Block *b2) const;
	};

  public:
	static p::declare reg;
	explicit PolyAnalysis(p::declare &r = reg);

  protected:
	void processWorkSpace(WorkSpace * /*ws*/) override;
	void configure(const PropList &props) override;

  private:
	using state_t = PPLManager::t;
	void processCFG(CFG & /* cfg */, state_t & /* s */, bool /* isEntryCFG */, bool /* summarize */);
	void processBB(PPLManager *man, ai::CFGGraph &graph,
	               WorkListDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph>, PseudoTopoOrder> &ana,
				   ai::EdgeStore<PPLManager, ai::CFGGraph> &store,
	               MyHTable<int, state_t> &headerState);
	const PropList *_props{};
	state_t processHeader(ai::CFGGraph &graph, BasicBlock *header, PPLManager& man, ai::EdgeStore<PPLManager, ai::CFGGraph>& store, MyHTable<int, state_t> &headerState);



	dfa::State *initState = NULL;
	Vector<PseudoTopoOrder*> _orders;
};


} // namespace poly
} // namespace otawa
#endif
