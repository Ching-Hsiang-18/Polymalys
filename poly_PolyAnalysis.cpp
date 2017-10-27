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

#include "PPLDomain.h"
#include "PPLManager.h"
#include "PolyAnalysis.h"
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


void PolyAnalysis::analyzeGraph(CFG &cfg, state_t &s, bool do_init) {
	ai::CFGGraph graph(&cfg);
	PPLManager *man;
    if (do_init) {
		man = new PPLManager(*_props); 
	} else {
		man = new PPLManager(s, *_props);
	}
	ai::EdgeStore<PPLManager, ai::CFGGraph> store(*man, graph);
	ai::OrderedDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph> > ana(*man, graph, store);

	genstruct::HashTable<int, state_t> headerState;

	cout << "Starting abstract interpretation for CFG: " << cfg.name() << endl;	
	while (ana) {
		state_t s;
		s = ana.input();
		if ((*ana)->isSynth()) {
			CFG *subCFG = (*ana)->toSynth()->callee();
			cout << "Call from " << (*ana)->toSynth()->caller()->name() << " to " << subCFG->name() << endl;
			analyzeGraph(*subCFG, s, false);
			cout << "Return from " << subCFG->name() << " to " << (*ana)->toSynth()->caller()->name() << endl;
			for (ai::CFGGraph::Successor e(graph, *ana); e; e++)
					ana.check(*e, s);
		} else {
			BasicBlock *bl = (BasicBlock*) *ana;
			cout << "handle bb: " << bl << "\n";
			cout << bl->id() << endl;

			if (LOOP_HEADER(bl)) {
#ifdef POLY_DEBUG			
				cout << "doing loopheader: " << bl->id() << endl;
#endif
				Ident id(bl->id(), Ident::ID_LOOP);
				PPL::Coefficient binf_n, binf_d, bsup_n, bsup_d;
				s.getRange(id, binf_n, binf_d, bsup_n, bsup_d);
				int bound = -2;
				if ( PPL::raw_value(bsup_d).get_ui() != 0) {
					bound = PPL::raw_value(bsup_n).get_ui() / PPL::raw_value(bsup_d).get_ui();
				}
#ifdef POLY_DEBUG			
				cout << "ITERATION: " << bound << endl;
#endif
				if ((MAX_ITERATION(bl) != -2) && ((MAX_ITERATION(bl) < bound)  || (bound == -2))) {
					MAX_ITERATION(bl) = bound;
					cout << "LOOP " << bl << " has bound: " << bound << endl;
					for (genstruct::Vector<Edge*>::Iterator exitedge(**EXIT_LIST(bl)); exitedge; exitedge++) {
#ifdef POLY_DEBUG			
						cout << "Trigger exit edge with bound: " << bound << "(" << *exitedge << ")" << endl;
#endif
						Block::EdgeIter e = exitedge->source()->ins();
						ana.change(e);
					}
				}
				if (headerState.hasKey(bl->id())) {
					s = headerState[bl->id()] = man->widening(s, headerState[bl->id()]);
				} else {
					headerState[bl->id()] = s;
				}

			}

			cout << "BB: " << bl << " dimension=" << s.getVarCount() << endl;
#ifdef POLY_DEBUG			
			cout << "inst! bb= ";
			cout << bl << endl;
#endif
			if (s.isBottom()) {
#ifdef POLY_DEBUG			
				cout << "! Skip block because input state is Bottom" << endl;
#endif
				ana++;
				continue;
			}
			for (BasicBlock::InstIter inst(bl); inst; inst++) {
#ifdef POLY_DEBUG			
					cout << ";;; inst: " << *inst << endl;
#endif
					sem::Block block;
				inst->semInsts(block);
				for(sem::Block::InstIter semi(block); semi; semi++) {
#ifdef POLY_DEBUG			
						cout << "===============================================" << endl;
						s._sanityChecks();
						cout << "BEFORE: " << s << endl;
						s->displayIdentMap();
						cout << "+++ IR +++: " << *semi << endl;
#endif
						// man->display_loc_vars(s);
						s = s.onSemInst(*semi, inst->address());
						// man->display_loc_vars(s);
#ifdef POLY_DEBUG			
						cout << "AFTER IR: " << s << endl;
						s->displayIdentMap();
#endif
						s.doFinalizeUpdate();
						s.doIntegerWrap();
#ifdef POLY_DEBUG			
						cout << "AFTER CLEANUP: " << s << endl;
						s->displayIdentMap();
						cout << "===============================================" << endl;
						man->display_loc_vars(s);
						cout << "===============================================" << endl << endl;
#endif
				}
											
			}
			for (ai::CFGGraph::Successor e(graph, *ana); e; e++) {
#ifdef POLY_DEBUG			
				cout << "---inst outedge!" << *e << "taken= " << (e->isTaken()) << endl;
				fflush(stdout);
#endif
				bool hasEdgeState = false;
				state_t edgeState;
				if (LOOP_HEADER(e->sink()) || s.hasFilter() || LOOP_EXIT_EDGE(e)) {
					hasEdgeState = true;
					edgeState = s;
				}

				if (LOOP_HEADER(e->sink())) {
					if (Dominance::dominates(e->sink(), e->source())) {
						/* is back-edge */
						edgeState = edgeState.onLoopIter(e->sink()->id(), ENCLOSING_LOOP_HEADER(e->sink()));
						edgeState.doFinalizeUpdate(); // TODO PERF FIXME
					} else {
						/* is entry-edge */
						edgeState = edgeState.onLoopEntry(e->sink()->id(), ENCLOSING_LOOP_HEADER(e->sink()));
						headerState.remove(e->sink()->id());
						/*
						*/
					}
				}
				if (LOOP_EXIT_EDGE(e)) {
					Block *bb = LOOP_EXIT_EDGE(e);
					int bound = MAX_ITERATION(bb);
#ifdef POLY_DEBUG			
						cout << "LOOPEXIT: " << bound << endl;
#endif
						edgeState = edgeState.onLoopExit(bb->id(), bound);
						edgeState.doFinalizeUpdate(); // TODO PERF FIXME
				}

				if (edgeState.hasFilter()) {
#ifdef POLY_DEBUG			
					cout << "BEFORE FILTERING: " << endl;
					edgeState->displayIdentMap();
					fflush(stdout);
					edgeState.print(cout); cout << endl;
					fflush(stdout);
					man->display_loc_vars(edgeState);
#endif
					edgeState = edgeState.onBranch(e->isTaken());
#ifdef POLY_DEBUG			
					cout << "FILTERED STATE: " << endl;
					edgeState->displayIdentMap();
					fflush(stdout);
					edgeState.print(cout); cout << endl;
					fflush(stdout);
					man->display_loc_vars(edgeState);
#endif
				}
				if (hasEdgeState) {
					ana.check(*e, edgeState);
				} else ana.check(*e, s);

			}
		}
		ana++;
	}
	Block* bb = graph.exit();
	Block::EdgeIter edge(bb->ins());
	s = store.get(edge);
	cout << "Ending abstract interpretation for CFG: " << cfg.name() << endl;	
	if (do_init) {
		cout << "FINAL STATE: " << endl;
		s.displayIdentMap();
		fflush(stdout);
		s.print(cout); cout << endl;
		fflush(stdout);
		s.displayLocVars();
		cout << endl;
	}
}

void PolyAnalysis::processWorkSpace(WorkSpace *ws) {
	const CFGCollection *coll = INVOLVED_CFGS(ws);
	ASSERT(coll);
	CFG *entry = coll->get(0);
	cout << "CFG count: " << coll->count() << endl;
	state_t dummy;
	analyzeGraph(*entry, dummy, true);
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
