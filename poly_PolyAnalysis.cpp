#include <otawa/otawa.h>
#include <otawa/util/WideningListener.h>
#include <otawa/util/WideningFixPoint.h>
#include <otawa/util/HalfAbsInt.h>
#include <otawa/dfa/FastState.h>
#include <otawa/poly/features.h>
#include <otawa/flowfact/features.h>
#include <otawa/cfg/Edge.h>
#include <otawa/graph/Graph.h>
#include <otawa/dfa/ai.h>
#include <time.h>
#include <ppl.hh>

#include "include/PPLDomain.h"
#include "include/PPLManager.h"
#include "include/PolyAnalysis.h"
namespace otawa { namespace poly { 
using namespace otawa;
using namespace util;

p::declare PolyAnalysis::reg = p::init("otawa::poly::PolyAnalysis", Version(1,0,0))
.require(COLLECTED_CFG_FEATURE)
.require(LOOP_INFO_FEATURE)
.require(dfa::INITIAL_STATE_FEATURE)
.provide(POLY_ANALYSIS_FEATURE);

/**
 */
PolyAnalysis::PolyAnalysis(p::declare& r): Processor(r) { }


/**
 */
void PolyAnalysis::configure(const PropList &props) {

	Processor::configure(props);
	_props = &props;
}

void PolyAnalysis::processBB(PPLManager *man, ai::CFGGraph &graph, ai::WorkListDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph> > &ana, genstruct::HashTable<int, state_t> &headerState) {
	/*
	 * The processing is made up of two main parts:
	 *
	 * 1. Basic Block processing: Performing the actual abstract Update on the current basic block.
	 * 2. Edge Processing: Propagating state on successors, potentially taking care of filtering (conditional branches).
	 */

	state_t s = ana.input();

	if ((*ana)->isSynth()) {
		/* Handle call to another function */
		CFG *subCFG = (*ana)->toSynth()->callee();
		cout << "Call from " << (*ana)->toSynth()->caller()->name() << " to " << subCFG->name() << endl;

		processCFG(*subCFG, s, false);

		cout << "Return from " << subCFG->name() << " to " << (*ana)->toSynth()->caller()->name() << endl;
		for (ai::CFGGraph::Successor e(graph, *ana); e; e++)
				ana.check(*e, s);
	} else {
		/* Handle normal basic block */
		BasicBlock *bl = (*ana)->toBasic();
#ifdef POLY_DEBUG			
		cout << "Processing basic block: " << bl << " spaceDimension=" << s.getVarIDCount() << "\n";
#endif

		/* Special processing for loop headers, to handle loop bounds and widening */
		if (LOOP_HEADER(bl)) {
#ifdef POLY_DEBUG			
			cout << "Basic block is loop header: " << bl->id() << endl;
#endif
			bound_t bound = s.getLoopBound(bl->id());

#ifdef POLY_DEBUG			
			cout << "ITERATION: " << int(bound) << endl;
#endif
			if ((MAX_ITERATION(bl) != bound_t::UNBOUNDED) && ((MAX_ITERATION(bl) < bound)  || (bound == bound_t::UNBOUNDED)))
				MAX_ITERATION(bl) = bound;

			/* We record state at each loop header to be able to handle widening */
			if (headerState.hasKey(bl->id()))
				s = man->widening(s, headerState[bl->id()]);
			headerState[bl->id()] = s;

		}

		if (s.isBottom()) {
#ifdef POLY_DEBUG			
			cout << "! Skip block because input state is Bottom" << endl;
#endif
			return;
		}

		/* Basic block processing */
		for (BasicBlock::InstIter inst(bl); inst; inst++) {
#ifdef POLY_DEBUG			
				cout << "Starting update for CPU (concrete) instruction: " << *inst << endl;
#endif
				sem::Block block;
			inst->semInsts(block);
			for(sem::Block::InstIter semi(block); semi; semi++) {
#ifdef POLY_DEBUG			
					cout << "Starting update before semantic instruction." << endl;
					cout << "State before semantic instruction update: " << endl << s << endl;
					cout << "Updating for semantic instruction (IR): " << *semi << endl;
#endif
					s = s.onSemInst(*semi, inst->address());
#ifdef POLY_DEBUG			
					cout << "State after semantic instruction update: " << endl << s << endl;
#endif
					s.doIntegerWrap();
			}
#ifdef POLY_DEBUG			
			cout << "Finished update for CPU (concrete) instruction: " << *inst << endl;
#endif
			s.doFinalizeUpdate();
#ifdef POLY_DEBUG			
					cout << "State after cleanup: " << endl << s << endl;
#endif
		}

		/* Edge Processing, propagate updated state to successors */
		for (ai::CFGGraph::Successor e(graph, *ana); e; e++) {
#ifdef POLY_DEBUG			
			cout << "OutEdge: " << *e << ", taken= " << (e->isTaken()) << endl;
#endif
			/* 
			 * In most cases, the state is not updated (i.e. modified) by edge processing. 
			 * We need update on edge if any of these (non-mutually-exclusive) following conditions are true:
			 *
			 * 1. The current successor of current block is a loop header (need to do onLoopEntry or onLoopIter)
			 * 2. The current output edge of current block is an exit-edge (need to do onLoopExit)
			 * 3. The current block has a conditional branch (need to do onBranch, for filtering)
			 */
			if (!LOOP_HEADER(e->sink()) && !LOOP_EXIT_EDGE(e) && !s.hasFilter()) { 
				/* no edge update: simply copy output state to successor input state */
				ana.check(*e, s);
			} else {
				/* process edge update */
				state_t edgeState = s;

				if (edgeState.hasFilter()) {
					/* Filtering: apply branch condition on edge state */
#ifdef POLY_DEBUG			
					cout << "BEFORE FILTERING: " << endl;
					cout << edgeState;
#endif
					edgeState = edgeState.onBranch(e->isTaken());
					if (edgeState.isBottom())
						continue;
#ifdef POLY_DEBUG			
					cout << "FILTERED STATE: " << endl;
					cout << edgeState;
#endif
				}

				if (LOOP_HEADER(e->sink())) {
					if (Dominance::dominates(e->sink(), e->source())) {
						/* Back-Edge: increment virtual loop counter */
						edgeState = edgeState.onLoopIter(e->sink()->id());
					} else {
						/* Entry-Edge: initialize virtal loop counter */
						edgeState = edgeState.onLoopEntry(e->sink()->id());
					}
				}

				if (LOOP_EXIT_EDGE(e)) {
					/* Exit edge: remove virtual loop counter, and apply loop bound constraint on state */
					Block *bb = LOOP_EXIT_EDGE(e);

					/* FIXME: should be bound = s.getLoopBound(bb->id()) but we need to fix the widening to make it work */
					int bound = MAX_ITERATION(bb); // HACK
#ifdef POLY_DEBUG			
					cout << "Bound on loop exit: " << bound << endl;
#endif
					if (bound >= 0) {
						edgeState = edgeState.onLoopExit(bb->id(), bound);
						if (edgeState.isBottom())
							continue;
					}
				}
				edgeState.doFinalizeUpdate();
				ana.check(*e, edgeState);
			}
		}
	}
}

void PolyAnalysis::processCFG(CFG &cfg, state_t &s, bool isEntryCFG) {
	PPLManager *man = isEntryCFG ? (new PPLManager(*_props)) : (new PPLManager(s, *_props));;
	genstruct::HashTable<int, state_t> headerState;
	ai::CFGGraph graph(&cfg);
	ai::EdgeStore<PPLManager, ai::CFGGraph> store(*man, graph);
	ai::WorkListDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph> > ana(*man, graph, store);

	cout << "Entering CFG: " << cfg.name() << endl;	
	while (ana) {
		processBB(man, graph, ana, headerState);
		ana++;
	}
	Block* bb = graph.exit();
	Block::EdgeIter edge(bb->ins());
	s = store.get(edge);
	cout << "Ending abstract interpretation for CFG: " << cfg.name() << endl;	
	if (isEntryCFG) {
		cout << "FINAL STATE: " << endl;
		cout << s;
	}
}

void PolyAnalysis::processWorkSpace(WorkSpace *ws) {
	const CFGCollection *coll = INVOLVED_CFGS(ws);
	ASSERT(coll);
	CFG *entry = coll->get(0);
	cout << "CFG count: " << coll->count() << endl;
	state_t dummy;
	processCFG(*entry, dummy, true);
	cout << "LOOP BOUNDS: " << endl;
	for (CFGCollection::Iterator iter2(coll); iter2; iter2++) {
		for (CFG::BlockIter iter((*iter2)->blocks()); iter; iter++) {
			BasicBlock *bb = (BasicBlock*) *iter;
			if (LOOP_HEADER(bb)) {
				cout << "[" << (*iter2)->name() << "]" << "MAX_ITERATION(" << bb->id() << ") = " << MAX_ITERATION(bb) << endl;
				cout << "[" << (*iter2)->name() << "]" << "TOTAL_ITERATION(" << bb->id() << ") = " << TOTAL_ITERATION(bb) << endl;
			}
		}
	}
}
} } 
