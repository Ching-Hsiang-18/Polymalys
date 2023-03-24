#include <ctime>
#include <otawa/cfg/Edge.h>
#include <otawa/dfa/FastState.h>
#include <otawa/dfa/ai.h>
#include <otawa/flowfact/features.h>
#include <otawa/graph/Graph.h>
#include <otawa/otawa.h>
#include <otawa/util/HalfAbsInt.h>
#include <otawa/util/WideningFixPoint.h>
#include <otawa/hard/Memory.h>
#include <otawa/util/WideningListener.h>
#include <otawa/oslice/features.h>
#include <otawa/ai/WorkListDriver.h>
#include <ppl.hh>
#include <sys/time.h>

#include "elm/log/Log.h"
#include "include/PPLDomain.h"
#include "include/PPLManager.h"
#include "include/PolyAnalysis.h"

#include "include/LoopAnalyzer.h"

namespace otawa {
namespace poly {
using namespace otawa;
using namespace util;

p::declare PolyAnalysis::reg = p::init("otawa::poly::PolyAnalysis", Version(1, 0, 0))
                                   .require(COLLECTED_CFG_FEATURE)
                                   .require(LOOP_INFO_FEATURE)
                                   .require(dfa::INITIAL_STATE_FEATURE)
								   .require(MEMORY_ACCESS_FEATURE)
								   .require(oslice::LIVENESS_FEATURE)
                                   .provide(POLY_ANALYSIS_FEATURE);

PolyAnalysis::PolyAnalysis(p::declare &r) : Processor(r) {}

void PolyAnalysis::configure(const PropList &props) {

	Processor::configure(props);
	_props = &props;
    max_vars = 0;
    max_cons = 0;
}

PolyAnalysis::state_t PolyAnalysis::processHeader(ai::CFGGraph &graph, MyHTable<int,PPLDomain> &lb,
        BasicBlock *header, PPLManager& man, ai::EdgeStore<PPLManager, ai::CFGGraph>& store, MyHTable<int, HeaderState> &headerState) {

	// manage loop context
	if(CURRENT_LOOP(ROOT_CFG(header->cfg())) != header->id()){
		PREVIOUS_LOOP(header) = CURRENT_LOOP(ROOT_CFG(header->cfg()));
		CURRENT_LOOP(ROOT_CFG(header->cfg())) = header->id();
	}
	// reset function instances for each iteration to insert in the same place the conditionals
	std::map<int,std::map<string,int>> resetMap = CURRENT_INSTANCES(ROOT_CFG(header->cfg()));
	resetMap.erase(header->id());
	CURRENT_INSTANCES(ROOT_CFG(header->cfg())) = resetMap;

	state_t entryState = man.bot();
	state_t backState = man.bot();

	for (ai::CFGGraph::Predecessor e(graph, header); e; e++) {
		state_t edgeState = store.get(*e);

		if (Dominance::dominates(e->sink(), e->source())) {
			/* back edge */
			backState = man.join(backState, edgeState);
		} else {
			/* entry edge */
			entryState = man.join(entryState, edgeState);
		}
	}

    if (!headerState.hasKey(header->id())) {
        HeaderState hs;
        hs.state = man.bot();
        hs.avcreate = false; /* will be set to true below except if entrystate is bottom */
        headerState[header->id()] = hs; // optimisation qui permet de virer le join avec entryState
	}
    HeaderState &hs =  headerState[header->id()];

    if (!entryState.equals(hs.entry)) { /* if entrystate updates headerstate, recompute avatars */
        hs.avcreate = true;
        //cout << " avcreate set to true" << endl;
        hs.entry = entryState;
        hs.state = entryState;
    }



PDBG("Widening, backState = " << backState << endl)
PDBG("Widening, entryState = " << entryState << endl)
PDBG("Widening, oldHeaderState = " << HeaderState(headerState[header->id()]).state << endl)

DBG("Widening, backState = " << backState)
DBG("Widening, entryState = " << entryState)
// DBG("Widening, oldHeaderState = " << state_t(headerState[header->id()]))
	//state_t oldState = headerState[header->id()];

#ifdef LINBOUND
	PPLDomain linearBound = backState.getLinearExpr(Ident(header->id(), Ident::ID_LOOP));
	backState.setLinBound(header->id(), linearBound);
#else
	bound_t bound = backState.getLoopBound(header->id());
	backState.setBound(header->id(), bound);
#endif
/*	cout << "\n\n\nBackState pour calcul de bornes de boucles parametrique: \n" << backState << "\n\n\n"; */

	backState.computeParamBound(header);

	/*PDBG("ITERATION: bound=" << int(bound) << endl)
	if(bound == -1)
		DBG("ITERATION: bound=" << color::IRed << "UNREACHABLE")
	else if(bound == -2)
		DBG("ITERATION: bound=" << color::IRed << "UNBOUNDED")
	else
		DBG("ITERATION: bound=" << color::IRed << int(bound))*/


#ifndef LINBOUND
	if ((MAX_ITERATION(header) != bound_t::UNBOUNDED) &&
		((MAX_ITERATION(header) < bound) || (bound == bound_t::UNBOUNDED)))
		MAX_ITERATION(header) = bound;
#endif

	if (!lb.hasKey(header->id()))
		lb[header->id()] = PPLDomain();

#ifdef LINBOUND
	PPLDomain &oldBound = lb[header->id()];
	lb[header->id()] = oldBound.onMerge(linearBound, false);
#endif
	
    // DBG("NOT applying widening")
#if 1
     /* headerState = headerState \/ (headerState U backState) */
    //state_t old = HeaderState(headerState[header->id()]).state;
    //cout << "avcreate: " <<(*headerState[header->id()]).avcreate  << endl;
    (*headerState[header->id()]).state = man.widening(backState, (*headerState[header->id()]).state, (*headerState[header->id()]).avcreate);
    if (backState.isBottom() == false)
        (*headerState[header->id()]).avcreate = false;
    //cout << "Test if header converges:" << endl;
    //cout << "backState: " << backState << endl;
    //cout << "Old headerState: " << old << endl;
    //cout << "New headerState: " << HeaderState(headerState[header->id()]).state << endl;
    /*
    if (old.equals((*headerState[header->id()]).state)) {
        cout << "header " << header->id();

        cout << "HEADER CONVERGES" << endl;
    } else {
        cout << "header " << header->id();

        cout << "HEADER DIFF" << endl;
    }
    */
    //PDBG("Widening, after Widening = [\n" << state_t(headerState[header->id()]) << endl)
    //DBG("Widening, after Widening = [\n" << state_t(headerState[header->id()]))

    // headerState[header->id()] = man.join(entryState, headerState[header->id()]); // sert a rien sauf 1ere iteration
    //PDBG("Widening, newHeaderState after Join with entryState = " << state_t(headerState[header->id()]) << endl)
    //DBG("Widening, newHeaderState after Join with entryState = " << state_t(headerState[header->id()]))
#else
    headerState[header->id()] = man.join(entryState, backState, true);
    PDBG("no widening, new headerState after join(backState, entryState) = " << state_t(headerState[header->id()]))
#endif

	/*
	cout << "Widening, newHeaderState = " << state_t(headerState[header->id()]) << endl;
	if (oldState.equals(headerState[header->id()])) {
		cout << "EQUAL" << endl;
	} else {
		cout << "NOT EQUAL" << endl;
	}
	*/
    return (*headerState[header->id()]).state;
}

void PolyAnalysis::processBB(PPLManager *man, ai::CFGGraph &graph, MyHTable<int,PPLDomain> &lb,
                             WorkListDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph>, PseudoTopoOrder> &ana,
							 ai::EdgeStore<PPLManager, ai::CFGGraph>& store,
                             MyHTable<int, HeaderState> &headerState) {
	/*
	 * The processing is made up of two main parts:
	 *
	 * 1. Basic Block processing: Performing the actual abstract Update on the current basic block.
	 * 2. Edge Processing: Propagating state on successors, potentially taking care of filtering (conditional branches).
	 */
	state_t s = man->bot();

	if(LOOP_RESET(*ana)){
		Block* h = nullptr;
		auto iter = (*ana)->ins();
		for(int i=0;i<(*ana)->countIns();i++){
			if(ROOT_CFG((*ana)->cfg()) != nullptr && (*iter)->source()->id() == CURRENT_LOOP(ROOT_CFG((*ana)->cfg()))){
				h = (*iter)->source();
				break;
			}
			iter++;
		}
		if(h != nullptr){
			CURRENT_LOOP(ROOT_CFG((*ana)->cfg())) = PREVIOUS_LOOP(h);
		}
		else{
			cout << "ERROR : h is null" << endl;
			//exit(1);
		}
	}

	DBG("Processing " << color::On_Blu << "BB " << ((BasicBlock*)*ana)->index() << (LOOP_HEADER(*ana) ? " (loop header)" : "") << color::RCol)
	/* Special processing for loop headers, to handle loop bounds and widening */
	if (LOOP_HEADER(*ana)) {
#ifdef POLY_DEBUG
		cout << "Basic block is loop header: " << (*ana)->toBasic()->id() << endl;
#endif
		s = processHeader(graph, lb, (*ana)->toBasic(), *man, store, headerState);
	} else {
		s = ana.input();
	}

	/* doing this avoid bottom state */
	if(((BasicBlock*)*ana)->index() == 1){
		ArmArgumentsDetector detector;
		detector.initFunctionParameters(&s, ((BasicBlock*)*ana)->cfg()->entry());
	}

	if (s.isBottom()) {
#ifdef POLY_DEBUG
		cout << "! Skip block because input state is Bottom" << endl;
#endif
		return;
	}

#ifdef POLY_DEBUG
	cout << "State at basic block start: " << endl << s << endl;
	cout << "isSynth: " << (*ana)->isSynth() << endl;
#endif

	if ((*ana)->isSynth()) {
		/* Handle call to another function */
		CFG *subCFG = (*ana)->toSynth()->callee();
		
		/* handle crash on null CFG */
		if (subCFG == nullptr) {
			cout << "Error: calling a null CFG" << endl;
			exit(1);
		}

		ROOT_CFG(subCFG) = ROOT_CFG((*ana)->cfg());
		int oldLoop = CURRENT_LOOP(ROOT_CFG(subCFG)); //< store old loop id to restore it after return
		//CURRENT_LOOP(ROOT_CFG(subCFG)) = -1;
		string name = subCFG->name();
		
		// create artificiel loop context for function calls
		std::map<int,std::map<string,int>> currentInstancesAll = CURRENT_INSTANCES(ROOT_CFG(subCFG));
		std::map<string,int> currentInstances;
		if(currentInstancesAll.find(oldLoop) == currentInstancesAll.end()){
			currentInstances = std::map<string,int>();
		}
		else{
			currentInstances = currentInstancesAll.at(oldLoop);
			currentInstancesAll.erase(oldLoop);
		}
		int instance = 1; // default value
		if(currentInstances.find(name) != currentInstances.end()){
			instance = currentInstances.at(name) + 1; //< new value
			currentInstances.erase(name); //< to replace the value	
		}
		currentInstances.emplace(name,instance);
		CURRENT_LOOP(ROOT_CFG(subCFG)) = - MAX_LOOP * oldLoop + instance; // unique negative value for each loop
		currentInstancesAll.emplace(oldLoop,currentInstances);
		CURRENT_INSTANCES(ROOT_CFG(subCFG)) = currentInstancesAll;
		// end of artificial loop context

		cout << "Call from " << (*ana)->toSynth()->caller()->name() << " to " << subCFG->name() << endl;
		
		bool compose_ok = false;
#ifdef INTER_PROCEDURAL
		if (SUMMARIZE(workspace())) {
			state_t sum;
			if (SUMMARY(subCFG) == nullptr) {
				cout << "No summary exists for " << (*ana)->toSynth()->callee()->name() << ", creating one..." << endl;
				MyHTable<int, PPLDomain> *sublb = new MyHTable<int, PPLDomain>();
				processCFG(*subCFG, sum, *sublb, true, true); 

				cout << "Finished creating summary of " << subCFG->name() << ", returning to " << (*ana)->toSynth()->caller()->name() << endl;
				SUMMARY(subCFG) = new PPLDomain(sum);
				MAX_LINEAR(subCFG) = sublb;
#ifdef POLY_DEBUG				
				cout << "summary = " << endl;
				cout << sum << endl;
				cout << "parametric bounds = " << endl;
				for (MyHTable<int, PPLDomain>::PairIterator it(*sublb); it; it++) {
					cout << (*it).fst << " --> " << (*it).snd << endl;
				}
				cout << endl;
#endif				
			} else {
				cout << "Reusing to reusing existing summary." << endl;
				PPLDomain *p = SUMMARY(subCFG);
				sum = *p;
			}
#ifdef POLY_DEBUG
			cout << "Composing state with summary. Caller state = " << endl;
			cout << s << endl;
			cout << "Summary = " << endl;
			cout << sum << endl;
#endif
			state_t tmp = s.onCompose(sum);
			if (tmp.isBottom()) {
				cout << "Failed to use summary due to constraint violation" << endl;
			} else {
				compose_ok = true;
				MyHTable<int, PPLDomain> *sublb = MAX_LINEAR(subCFG);
				for (MyHTable<int, PPLDomain>::PairIterator it(*sublb); it; it++) {
						PPLDomain composed((*it).snd);
						cout << "linear bounds: " << composed << endl;
						cout << "caller state: " << s << endl;
						composed = s.onCompose(composed);
						cout << "composed bounds: " << composed << endl;
						composed = composed.getLinearExpr(Ident((*it).fst, Ident::ID_LOOP));
						if (!lb.hasKey((*it).fst))
							lb[(*it).fst] = PPLDomain();
						lb[(*it).fst] = lb.get((*it).fst).value().onMerge(composed, false);

				}
				s = tmp;

#ifdef POLY_DEBUG
				cout << "Composed state = " << endl;
				cout << s << endl;
#endif
			}
		} 
#endif
		if (!compose_ok) { // no summarizing
			if ((*ana)->toSynth()->callee()->name() == "__divsi3" ||  
			    (*ana)->toSynth()->callee()->name() == "__aeabi_i2d" ||  
			    (*ana)->toSynth()->callee()->name() == "__muldf3" ||  
			    (*ana)->toSynth()->callee()->name() == "__divdf3" ||  
			    (*ana)->toSynth()->callee()->name() == "__adddf3" ||  
			    (*ana)->toSynth()->callee()->name() == "__fixdfsi" ||  
			    (*ana)->toSynth()->callee()->name() == "__aeabi_dsub" || 
			    (*ana)->toSynth()->callee()->name() == "gsignal") {
				cout << "[FIXME] Ignoring call to function: " << (*ana)->toSynth()->callee()->name() << endl;
			} else {
				processCFG(*subCFG, s, lb, false, false);
				cout << "Return from " << subCFG->name() << " to " << (*ana)->toSynth()->caller()->name() << endl;
				CURRENT_LOOP(ROOT_CFG(subCFG)) = oldLoop;
			}
		}
	
		for (ai::CFGGraph::Successor e(graph, *ana); e; e++) {
			ana.check(*e, s);
		}
	} else {
		/* Handle normal basic block */
		BasicBlock *bl = (*ana)->toBasic();
        cout << "Processing basic block: " << bl << " spaceDimension=" << s.getVarIDCount() << ", numCons=" << s.getConsCount() << "\n" ;
        if (s.getVarIDCount() > max_vars) {
            max_vars = s.getVarIDCount();
        }
        if (s.getConsCount() > max_cons) {
            max_cons = s.getConsCount();
        }
		/* Basic block processing */
		for (BasicBlock::InstIter inst(bl); inst; inst++) {
			/* cout << "Starting update for CPU (concrete) instruction: " << *inst << endl; */ 
			DBG("\t" << *inst)
			sem::Block block;
			inst->semInsts(block);
			for (sem::Block::InstIter semi(block); semi; semi++) {
				/* cout << "Updating for semantic instruction (IR): " << *semi << endl; */ 
                s = s.onSemInst(bl, *semi, inst->address());
				PDBG("State after semantic instruction update: " << endl << s << endl << endl)
				s.doIntegerWrap();
			}
			PDBG("Finished update for CPU (concrete) instruction: " << *inst << endl)

			s.doKillTemporaries();
			s.doFinalizeUpdate();
			PDBG("State after cleanup: " << endl << s << endl)
		}

		s.doKillRegisters(oslice::REG_BB_END_IN(bl));
        s.doRemoveUselessAvatars();
		s.doFinalizeUpdate();
        s.listMemoryVariables();

	}


	/* Edge Processing, propagate updated state to successors */
	for (int doExit = 0; doExit < 2; doExit++) {

		/* Do loop exit edges last (improves performance) */
		for (ai::CFGGraph::Successor e(graph, *ana); e; e++) {
			if ((LOOP_EXIT_EDGE(e) == nullptr) && !doExit)
				continue;
			if ((LOOP_EXIT_EDGE(e) != nullptr) && doExit)
				continue;
			PDBG("OutEdge: " << *e << ", taken= " << (e->isTaken()) << endl)
			/*
			 * In most cases, the state is not updated (i.e. modified) by edge processing.
			 * We need update on edge if any of these (non-mutually-exclusive) following conditions are true:
			 *
			 * 1. The current successor of current block is a loop header (need to do onLoopEntry or onLoopIter)
			 * 2. The current output edge of current block is an exit-edge (need to do onLoopExit)
			 * 3. The current block has a conditional branch (need to do onBranch, for filtering)
			 */
			if (!LOOP_HEADER(e->sink()) && (LOOP_EXIT_EDGE(e) == nullptr) && !s.hasFilter()) {
				/* no edge update: simply copy output state to successor input state */
				ana.check(*e, s);
			} else {
				/* process edge update */
				state_t edgeState = s;

				if (edgeState.hasFilter()) {
					/* Filtering: apply branch condition on edge state */
					PDBG("BEFORE FILTERING: " << endl)
					PDBG(edgeState)
					edgeState = edgeState.onBranch(e->isTaken(), *ana);
					if (edgeState.isBottom()) {
						continue;
					}
					PDBG("FILTERED STATE: " << endl)
					PDBG(edgeState)
				}

				if (LOOP_HEADER(e->sink())) {
					if (Dominance::dominates(e->sink(), e->source())) {
						/* Back-Edge: increment virtual loop counter */
						edgeState = edgeState.onLoopIter(e->sink()->id());
					} else {
						/* Entry-Edge: initialize virtal loop counter */
						edgeState = edgeState.onLoopEntry(e->sink()->id());

						/* Avoid unnecessary widening before first loop iteration of inner loops */
						// headerState.remove(e->sink()->id()); 
					}
				}

				if (LOOP_EXIT_EDGE(e) != nullptr) {
					/* Exit edge: remove virtual loop counter, and apply loop bound constraint on state */
					Block *bb = LOOP_EXIT_EDGE(e);
					
					// tells that we get out of a loop
					if(LOOP_HEADER(e->source())){
						LOOP_RESET(e->sink()) = true;
					}

					/* 
					 * FIXME: should be bound = s.getLoopBound(bb->id()) but we need to fix the widening to make it work
					 */
					int bound = edgeState.getBound(bb->id());
#ifdef LINBOUND

					PPLDomain linBound = edgeState.getLinBound(bb->id());

					PDBG("Bound on loop exit: " << bound << endl)
					if (!linBound.isBottom()) {
						edgeState = edgeState.onLoopExitLinear(bb->id(), linBound, bb);
						if (edgeState.isBottom())
							continue;
					}
					PDBG("linbound for " << bb->id() << " is " << linBound << endl)
#endif
                            edgeState.doRemoveUselessAvatars();
                            edgeState.doRemoveAllAvatars();
#ifndef LINBOUND
					if (bound >= 0) {
						edgeState = edgeState.onLoopExit(bb->id(), bound, bb);
						if (edgeState.isBottom()) {
							continue;
						}
					}
#endif
				}
				edgeState.doFinalizeUpdate();
				ana.check(*e, edgeState);
			}
		}
	}
}


void PolyAnalysis::PseudoTopoOrder::_topoLoopHelper(const ai::CFGGraph &graph, Block *start, int currentLoop) {
	for (ai::CFGGraph::Iterator it(graph); !it.ended(); it++) {
		Block *bb = (*it);
		if (LOOP_HEADER(bb)) {
			if (((currentLoop == -1) && (ENCLOSING_LOOP_HEADER(bb) == nullptr)) ||
				((currentLoop != -1) && (ENCLOSING_LOOP_HEADER(bb) != nullptr) && (ENCLOSING_LOOP_HEADER(bb)->index() == currentLoop))) {
				_topoLoopHelper(graph, bb, bb->index());
			}
		}
	}

	if (currentLoop != -1) {
		_loopOrder[currentLoop] = _current;
		_current++;
	}
}

bool PolyAnalysis::PseudoTopoOrder::isBefore(const Block *b1, const Block *b2) const {
	if (_belongsToLoop.hasKey(b1->index()) && !_belongsToLoop.hasKey(b2->index()))
		return true;

	if (!_belongsToLoop.hasKey(b1->index()) && _belongsToLoop.hasKey(b2->index()))
		return false;

	if (_belongsToLoop.hasKey(b1->index())) {
		int loopId = _belongsToLoop[b1->index()];

		if (_loopOrder[loopId] < _loopOrder[loopId]) 
			return true;

		if (_loopOrder[loopId] > _loopOrder[loopId]) 
			return false;
	}

	return (_blockOrder[b1->index()] < _blockOrder[b2->index()]);
}

void PolyAnalysis::PseudoTopoOrder::_topoNodeHelper(const ai::CFGGraph &graph, Block *end) {
	if (_visited->bit(end->index()))
		return;

	for (ai::CFGGraph::Predecessor e(graph, end); e; e++) {
		if (!BACK_EDGE(e))
			_topoNodeHelper(graph, e->source());
	}

	if (LOOP_HEADER(end)) {
		_belongsToLoop[end->index()] = end->index();
	} else if (ENCLOSING_LOOP_HEADER(end) != nullptr)
		_belongsToLoop[end->index()] = ENCLOSING_LOOP_HEADER(end)->index();

	_blockOrder[end->index()] = _current;
	_current++;
	_visited->set(end->index());
}

void PolyAnalysis::PseudoTopoOrder::_getPseudoTopo(const ai::CFGGraph &graph) {
	_visited = new BitVector(graph.count());

	_current = 0;
	_topoLoopHelper(graph, graph.entry(), -1);

	_current = 0;
	for (ai::CFGGraph::Iterator it(graph); !it.ended(); it++) {
		bool hasNonBackEdges = false;
		for (ai::CFGGraph::Successor e(graph, (*it)); !e.ended() && !hasNonBackEdges; e++) {
			if (!BACK_EDGE(e))
				hasNonBackEdges = true;
		}
		if (!hasNonBackEdges)
			_topoNodeHelper(graph, *it);
	}
	delete _visited;
	_visited = nullptr;
}

void PolyAnalysis::processCFG(CFG &cfg, state_t &s, MyHTable<int, PPLDomain> &lb, bool isEntryCFG, bool summarize){
	#ifdef CheckReturnAddress
	s.doEnterFunction();
	#endif
	PPLManager *man = isEntryCFG ? (new PPLManager(*_props, workspace())) : (new PPLManager(s, *_props, workspace()));
	if (summarize) {
		ASSERT(isEntryCFG);
		man->enableSummary();
	}
    MyHTable<int, HeaderState> headerState;
	ai::CFGGraph graph(&cfg);
	ai::EdgeStore<PPLManager, ai::CFGGraph> store(*man, graph);

	PDBG("Init state: " << s << endl)
	cout << "Entering CFG: " << cfg.name() << endl;
	WorkListDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph>, PseudoTopoOrder> ana(*man, graph, store, _orders[cfg.index()]);

	while (ana) {
		processBB(man, graph, lb, ana, store, headerState);
		ana++;
	}

	cout << "Exiting CFG: " << cfg.name() << endl;

	Block *bb = graph.exit();
	Block::EdgeIter edge(bb->ins());
	s = store.get(edge);

	PDBG("FINAL STATE: " << endl)
	PDBG(s)
	if (isEntryCFG && !summarize) {
		// ...
    cout << endl << "---- ANALYSIS ENDS ----" << endl << endl;
	cout << "FINAL STATE: " << endl;
	cout << s;

    /* delta test */
    /*
    WVar v1(27);
    WVar v2(35);
    int result = s.delta(v1, v2, 4);
    cout << " delta result: *ptr3 - *ptr2 mod 4 = " << result << endl;
    */


	} else {
		s.doLeaveFunction(cfg.name());
		s.doFinalizeUpdate();
		PDBG("SUMMARY STATE: " << endl)
		PDBG(s)
	}



}

void PolyAnalysis::processWorkSpace(WorkSpace *ws) {
#ifndef ELM_LOG
	// logging
	elm::log::Debug::setDebugFlag(false);
	elm::log::Debug::setColorFlag(false);
#endif

    ConstraintExporter exporter;
    exporter.resetFile();

    struct timeval now;
    gettimeofday(&now, nullptr);

	const CFGCollection *coll = INVOLVED_CFGS(ws);
	ASSERT(coll);

	_orders.setLength(coll->count());
	for (CFGCollection::Iter iter2(coll); iter2; iter2++) {
		cout << "Preparing CFG: " << (*iter2)->name() << endl;
		ai::CFGGraph graph((*iter2));
		PseudoTopoOrder *pto = new PseudoTopoOrder(graph);
		_orders[(*iter2)->index()] = pto;
	}


	CFG *entry = coll->get(0);
	ROOT_CFG(entry) = entry;
	state_t dummy;
	ASSERT(dummy.getSummary() == nullptr);
	SUMMARIZE(ws) = false;
	MyHTable<int, PPLDomain> bounds;
	MyHTable<int, int> static_bounds;
	processCFG(*entry, dummy, bounds, true /* is entry */, false /* summarize */);
	
	// add non parametric loop bounds to the loop bounds
	std::map<int,LoopBound> loop_bounds;

#ifdef LINBOUND
	cout << "PARAMETRIC LOOP BOUNDS: " << endl;
	for (MyHTable<int, PPLDomain>::PairIterator it(bounds); it; it++) {
		cout << "linear bound expr for (" << (*it).fst << "): " << (*it).snd << endl;
		PPL::Coefficient binf_n, binf_d, bsup_n, bsup_d;
		Ident id((*it).fst, Ident::ID_LOOP);
		if (!(*it).snd.hasIdent(id)) {
			static_bounds[(*it).fst] = bound_t::UNREACHABLE;
			continue;
		}
	    (*it).snd.getRange(id, binf_n, binf_d, bsup_n, bsup_d);
		if (PPL::raw_value(bsup_d).get_ui() != 0) {
			static_bounds[(*it).fst] = 
				static_cast<bound_t>(PPL::raw_value(bsup_n).get_ui() / PPL::raw_value(bsup_d).get_ui());
		} else static_bounds[(*it).fst] = bound_t::UNBOUNDED;
	}

	cout << endl;
	cout << "LOOP BOUNDS: " << endl;
	for (CFGCollection::Iter iter2(coll); iter2; iter2++) {
		for (CFG::BlockIter iter((*iter2)->blocks()); iter; iter++) {
			Block *bb = (*iter);
			
			if(loop_bounds.size() == 0 && ROOT_CFG(bb->cfg()) != nullptr)
				loop_bounds = LOOP_BOUNDS(ROOT_CFG(bb->cfg())->entry());

			if (LOOP_HEADER(bb)) {
				if (static_bounds.hasKey(bb->id())) {
					MAX_ITERATION(bb) = static_bounds[bb->id()];
				} else MAX_ITERATION(bb) = bound_t::UNREACHABLE;
				cout << "[" << (*iter2)->name() << "]"
				     << "MAX_ITERATION(" << bb->id() << ") = " << MAX_ITERATION(bb) << endl;
				/*
				cout << "[" << (*iter2)->name() << "]"
				     << "TOTAL_ITERATION(" << bb->id() << ") = " << TOTAL_ITERATION(bb) << endl;
					 */
				if(MAX_ITERATION(bb) >= 0){ // if found without error, add non parametric loop bounds to the file
					LoopBound lb(false, MAX_ITERATION(bb), "");
					loop_bounds.emplace(bb->id(), lb);
				}
			}
		}
		delete _orders[(*iter2)->index()];
	}
#else
	cout << "LOOP BOUNDS: " << endl;
	for (CFGCollection::Iter iter2(coll); iter2; iter2++) {
		for (CFG::BlockIter iter((*iter2)->blocks()); iter; iter++) {
			Block *bb = (*iter);

			if(loop_bounds.size() == 0 && ROOT_CFG(bb->cfg()) != nullptr)
				loop_bounds = LOOP_BOUNDS(ROOT_CFG(bb->cfg())->entry());

			if (LOOP_HEADER(bb)) {
				cout << "[" << (*iter2)->name() << "]"
				     << "MAX_ITERATION(" << bb->id() << ") = " << MAX_ITERATION(bb) << endl;
				/*
				cout << "[" << (*iter2)->name() << "]"
				     << "TOTAL_ITERATION(" << bb->id() << ") = " << TOTAL_ITERATION(bb) << endl;
					 */
				if(MAX_ITERATION(bb) >= 0){ // if found without error, add non parametric loop bounds to the file
					LoopBound lb(false, MAX_ITERATION(bb), "");
					loop_bounds.emplace(bb->id(), lb);
				}
			}
		}
		delete _orders[(*iter2)->index()];
	}
#endif
    // export constraints
    bool exported = false;
     for (CFGCollection::Iter iter2(coll); iter2; iter2++) {
        for (CFG::BlockIter iter((*iter2)->blocks()); iter; iter++) {
            Block *bb = (*iter);
            ConstraintExporter exp;
	    if(ROOT_CFG(bb->cfg()) == nullptr)
		    continue;
	    OrderedElements order = EXPORT_ORDER(ROOT_CFG(bb->cfg())); //< order also contains non loop conditionals
	    std::map<int,std::map<bool,Conditional>> loopConditions = LOOP_CONDITIONALS(ROOT_CFG(bb->cfg()));
	    exp.exportOrderedConstraints(order, loopConditions);
	    LoopBoundsExporter lbe;
	    lbe.exportToFile(loop_bounds);
	    exported = true;
	    break;
        }
	if(exported)
		break;
    }
    cout << "Max variable count: " << max_vars << endl;
    cout << "Max constraint count:  " << max_cons << endl;
    struct timeval after;
    gettimeofday(&after, nullptr);
    int elapsed = ((after.tv_usec - now.tv_usec)/1000) + 1000*(after.tv_sec - now.tv_sec);
    cout << "Time elapsed: " << elapsed << endl;
    int num_inst = 0;
    for (CFGCollection::Iter iter2(coll); iter2; iter2++) {
        for (CFG::BlockIter iter((*iter2)->blocks()); iter; iter++) {
            Block *bb = (*iter);
            if (bb->isBasic()) {
                for (BasicBlock::InstIter inst(*bb); inst; inst++) {
                    num_inst ++;
                }
            }
        }
    }
    cout << "Number of instructions: " << num_inst << endl;
    cout << " & " << num_inst << " & " << max_vars << " & " << max_cons << " & " << elapsed << " \\\\ " << endl;
}

} // namespace poly
} // namespace otawa
