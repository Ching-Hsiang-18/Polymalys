/*
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2009, IRIT UPS.
 *
 *	OTAWA is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	OTAWA is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with OTAWA; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


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

#include "PolyAnalysis.h"

namespace otawa { namespace poly {

/**
 * @class PolyAnalysis
 * TODO
 */

p::declare PolyAnalysis::reg = p::init("otawa::poly::PolyAnalysis", Version(1,0,0))
	.require(COLLECTED_CFG_FEATURE)
	.require(LOOP_INFO_FEATURE)
	.require(dfa::INITIAL_STATE_FEATURE)
	.provide(POLY_ANALYSIS_FEATURE);

/**
 */
PolyAnalysis::PolyAnalysis(p::declare& r): Processor(r) {
}


/**
 */
void PolyAnalysis::configure(const PropList &props) {
	Processor::configure(props);
	cout << "Configuring Poly Analysis." << endl;
	//GLOBAL_STATE_ENTRY(props);
	//entry(&pv, dfa::INITIAL_STATE(workspace()), allocator);
	// time = TIME(props);
}


class DumbOrder {
	public:
	bool isBefore(Block *b1, Block *b2) {
		return (b1->id() < b2->id());
	}
};


PPLManager *beurk = NULL;
/**
 */

void PolyAnalysis::analyzeGraph(CFG &cfg, state_t &s, bool do_init) {
	if (!strcmp(cfg.name().toCString(), "gsignal")) {
		return;
	}

	ai::CFGGraph graph(&cfg);
	PPLManager *man;
    if (do_init) {
		man = new PPLManager(); 
	} else {
		man = new PPLManager(s);
	}
	ai::EdgeStore<PPLManager, ai::CFGGraph> store(*man, graph);
	DumbOrder order;
	ai::OrderedDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph>, DumbOrder > ana(*man, graph, store, order);

	genstruct::HashTable<int, state_t> headerState;
	int first = 1;
	

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
			if (first) {
				/* FIXME TODO (hack dégueu) */
				first = 0;
				cout << "Preparing TOTAL bounds ... " << endl; 
				for (CFG::BlockIter iter(cfg.blocks()); iter; iter++) { 
					BasicBlock *bb = (BasicBlock*) *iter; 
					if (LOOP_HEADER(bb) && ENCLOSING_LOOP_HEADER(bb)) { 
			 
						Ident id_tot(bb->id() | LOOP_TOTAL, Ident::ID_LOOP); 
						PPL::Variable v_tot = s.create(id_tot, true); 
						s.poly.add_constraint(v_tot == 0); 
					} 
				} 
			}

			if (LOOP_HEADER(bl)) {
#ifdef POLY_DEBUG			
				cout << "doing loopheader: " << bl->id() << endl;
#endif
				Ident id(bl->id(), Ident::ID_LOOP);
				PPL::Coefficient binf_n, binf_d, bsup_n, bsup_d;
				man->get_range(id, s, binf_n, binf_d, bsup_n, bsup_d);
				int bound = -2;
				if ( PPL::raw_value(bsup_d).get_ui() != 0) {
					bound = PPL::raw_value(bsup_n).get_ui() / PPL::raw_value(bsup_d).get_ui();
				}
				if (ENCLOSING_LOOP_HEADER(bl)) {
					int outer_bound = -2;

#ifdef POLY_DEBUG			
					cout << "Is inner loop! " << endl;
#endif
					/* Attempt to detect triangular loop (FIXME TODO inspect all outer loops) */
					Ident outer_id(ENCLOSING_LOOP_HEADER(bl)->id(), Ident::ID_LOOP);

					PPL::Coefficient binf_n, binf_d, bsup_n, bsup_d;
					man->get_range(outer_id, s, binf_n, binf_d, bsup_n, bsup_d);
					if ( PPL::raw_value(bsup_d).get_ui() != 0) {
						outer_bound = PPL::raw_value(bsup_n).get_ui() / PPL::raw_value(bsup_d).get_ui();
						genstruct::HashTable<int,int> map;
						Variable v_inner = s.lookup(id, false);
						Variable v_outer = s.lookup(outer_id, false);
						map[v_inner.id()] = 0;
						map[v_outer.id()] = 1;
						PPL::C_Polyhedron tmp = s.poly;
						man->map_space_dimensions(MapWithHash(map), tmp);
						PPL::Constraint_System cons = tmp.minimized_constraints();
#ifdef POLY_DEBUG			
						cout << "SYSTEME: " ;
						cons.print();
						fflush(stdout);
						cout << endl;
#endif
						bool found = false;
						int total;

						for (PPL::Constraint_System::const_iterator it = cons.begin(); it != cons.end(); it++) {
							const PPL::Constraint &c = *it;
							if (c.is_nonstrict_inequality()) {
								const PPL::Coefficient divisor = -c.coefficient(PPL::Variable(0));
								const PPL::Coefficient coef = c.coefficient(PPL::Variable(1));
								if ((divisor > 0) && (coef > 0)) {
									const PPL::Coefficient cst = c.inhomogeneous_term();
									const PPL::Coefficient expr_bound(outer_bound + 1 /* account for last backedge  in outer loop */ );
									PPL::Coefficient sum = cst*expr_bound + (coef*(expr_bound - 1)* expr_bound) / 2;
									sum = sum / divisor;
									int temp_total = PPL::raw_value(sum).get_ui();
									if ((temp_total < total) || !found)
										total = temp_total;

									found = true;
								}
							}
						}
						if (found) {
							TOTAL_ITERATION(bl) = total;
						}
					}
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

			cout << "BB: " << bl << " dimension=" << s.poly.space_dimension() << endl;
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
						s.sanity_checks();
						cout << "BEFORE: " << s << endl;
						man->displayIdentMap(s);
						cout << "+++ IR +++: " << *semi << endl;
#endif
						// man->display_loc_vars(s);
						s = man->update(s, *semi, inst->address());
						// man->display_loc_vars(s);
#ifdef POLY_DEBUG			
						cout << "AFTER IR: " << s << endl;
						man->displayIdentMap(s);
#endif
						man->bring_out_your_dead(s);
						man->integer_wrap(s);
#ifdef POLY_DEBUG			
						cout << "AFTER CLEANUP: " << s << endl;
						man->displayIdentMap(s);
						cout << "===============================================" << endl;
						man->display_loc_vars(s);
						cout << "===============================================" << endl << endl;
#endif
				}
											
			}
			s.serial = bl->id();
			for (ai::CFGGraph::Successor e(graph, *ana); e; e++) {
#ifdef POLY_DEBUG			
				cout << "---inst outedge!" << *e << "taken= " << (e->isTaken()) << endl;
				fflush(stdout);
#endif
				bool hasEdgeState = false;
				state_t edgeState;
				if (LOOP_HEADER(e->sink()) || man->hasFilter() || LOOP_EXIT_EDGE(e)) {
					hasEdgeState = true;
					edgeState = s;
				}

				if (LOOP_HEADER(e->sink())) {
					if (Dominance::dominates(e->sink(), e->source())) {
						/* is back-edge */
						edgeState = man->loopIter(edgeState, e->sink()->id(), ENCLOSING_LOOP_HEADER(e->sink()));
						man->bring_out_your_dead(edgeState); // TODO PERF FIXME
					} else {
						/* is entry-edge */
						edgeState = man->loopEntry(edgeState, e->sink()->id(), ENCLOSING_LOOP_HEADER(e->sink()));
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
						edgeState = man->loopExit(edgeState, bb->id(), bound);
						if (!ENCLOSING_LOOP_HEADER(bb)) {
							/* bound inner total bounds FIXME TODO hack dégueu */ 
							for (CFG::BlockIter iter(cfg.blocks()); iter; iter++) { 
								BasicBlock *inner = (BasicBlock*) *iter;
								if (LOOP_HEADER(inner) && (ENCLOSING_LOOP_HEADER(inner) == bb)) {
									edgeState = man->loopTotal(edgeState, inner->id(), TOTAL_ITERATION(inner));

								}
							}

						}
						man->bring_out_your_dead(edgeState); // TODO PERF FIXME
				}

				if (man->hasFilter()) {
#ifdef POLY_DEBUG			
					cout << "BEFORE FILTERING: " << endl;
					man->displayIdentMap(edgeState);
					fflush(stdout);
					edgeState.print(cout); cout << endl;
					fflush(stdout);
					man->display_loc_vars(edgeState);
#endif
					edgeState = man->filter(edgeState, e->isTaken());
#ifdef POLY_DEBUG			
					cout << "FILTERED STATE: " << endl;
					man->displayIdentMap(edgeState);
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
			man->removeFilter();
		}
		ana++;
	}
	Block* bb = graph.exit();
	Block::EdgeIter edge(bb->ins());
	s = store.get(edge);
	cout << "Ending abstract interpretation for CFG: " << cfg.name() << endl;	
	if (do_init) {
		cout << "FINAL STATE: " << endl;
		man->displayIdentMap(s);
		fflush(stdout);
		s.print(cout); cout << endl;
		fflush(stdout);
		man->display_loc_vars(s);
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
	/*
	cout << "FINAL STATE: " << endl;
	*/
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

/*
	PPL::C_Polyhedron poly(dom.cons);
	bool maximum, minimum;
	if (var.id() >= poly.space_dimension()) {
		bsup_n = 0;
		binf_n = 0;
		bsup_d = 0;
		binf_d = 0;
		return;
	}
	poly.maximize(var, bsup_n, bsup_d, maximum);
	poly.minimize(var, binf_n, binf_d, minimum);
	gmp_printf("[");
	if (binf_d != 0) {
		gmp_printf("%Zd", &PPL::raw_value(binf_n));
		if (binf_d != 1)
			gmp_printf("/%Zd", &PPL::raw_value(binf_d));
	} else gmp_printf("-inf");
	gmp_printf("..");
	if (bsup_d != 0) {
		gmp_printf("%Zd", &PPL::raw_value(bsup_n));
		if (bsup_d != 1)
			gmp_printf("/%Zd", &PPL::raw_value(bsup_d));
	} else gmp_printf("+inf");
	gmp_printf("]");
	fflush(stdout);
	
*/	
	
	
}
typedef PPL::Variable* PVAR;

void PPLManager::integer_wrap(PPLManager::t &dom) {
	// FIXME
	return; // TODO
	
	// code below is crap
	
	PPL::Constraint_System cons = dom.poly.minimized_constraints();
	PPL::Constraint_System result;
	for (PPL::Constraint_System::const_iterator it = cons.begin(); it != cons.end(); it++) {
		const PPL::Constraint &c = *it;
		if (c.is_equality()) {
			PPL::Linear_Expression e;
			for (PPL::dimension_type i = c.space_dimension(); i-- > 0;) {
				e += c.coefficient(PPL::Variable(i)) * PPL::Variable(i);
			}
			const mpz_class &val = c.inhomogeneous_term();
			mpz_class val2 = val % 0x100000000;
			if (val2 < -0x7FFFFFFF)
				val2 = val2 + 0x100000000;
			e += val2;
			result.insert(e == 0);
		} else result.insert(c);
	}
	dom.poly = PPL::C_Polyhedron(result);
}

void PPLManager::bring_out_your_dead(PPLManager::t &dom) {
#ifdef POLY_DEBUG			
	dom.sanity_checks();
	if (PPLDomain::trash.countOnes() == 0) {
		cout << "Nothing to clean" << endl;
		dom.sanity_checks();
		return;
	}
#endif
	// displayIdentMap(dom);
	RemoveMarked rm(PPLDomain::trash, dom.poly.space_dimension());
	map_space_dimensions(rm, dom.poly);
	dom.num_axis -= PPLDomain::trash.countOnes();
	dom.map_identifiers(rm);
	PPLDomain::trash.clear();
	// displayIdentMap(dom);
#ifdef POLY_DEBUG			
	dom.sanity_checks();
#endif

	return; 
}

void PPLManager::display_loc_vars(PPLManager::t &dom) {
	PPL::Constraint_System mcons = dom.poly.minimized_constraints();
	if (dom.isBottom()) {
		cout << "diplay_loc_vars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	Variable ssp = dom.lookup(id_ssp);
	cout << mcons.space_dimension() << " Local variables: " << endl;
	for (int i = 0 ; i < NUM_LOC_VARS*LOC_VAR_SIZE; i += LOC_VAR_SIZE) {
		PPL::Constraint_System cons = mcons;
		PPL::Variable v(dom.num_axis);
		cons.insert(v == ssp - i - LOC_VAR_SIZE);
		cout << " [SP - " << hex(i) << "] == ";
		bool found = false;
		for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(dom.id2axis); it; it++)
		{
			elm::Pair<Ident, int> p = *it;
			Variable vsnd = dom.lookup(p.snd);
			if (p.fst.getType() == Ident::ID_MEM_ADDR) {
				PPL::C_Polyhedron poly(cons);
				PPL::Coefficient bsup_n, bsup_d;
				PPL::Coefficient binf_n, binf_d;
				bool maximum, minimum;
				poly.maximize(v - vsnd, bsup_n, bsup_d, maximum);
				poly.minimize(v - vsnd, binf_n, binf_d, minimum);
				if ((bsup_n == 0) && (binf_n == 0) && (bsup_d != 0) && (binf_d != 0)) {
					found = true;
					Ident idval(p.fst.getId(), Ident::ID_MEM_VAL);
					bool cst; 
					PPL::Coefficient num, den;
					cst = get_constant(idval, dom, num, den, true);
					cout <<  " (aka " << idval << ")";
					break;
				}
			}
		}
		if (!found)
			cout << "N/A" ;
		cout << endl;
	}

}

bool PPLManager::get_constant(PPL::Variable &var, PPLManager::t &dom, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d, bool display) {
	PPL::Coefficient binf_d, binf_n, bsup_d, bsup_n;
	get_range(var, dom, bsup_n, bsup_d, binf_n, binf_d, display); 
	if ((binf_d == bsup_d) && (binf_n == bsup_n) && (binf_d != 0)) {
		cst_n = binf_n;
		cst_d = binf_d;
		return true;
	}
	return false;
}
bool PPLManager::get_constant(Ident &id, PPLManager::t &dom, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d, bool display) {
	Variable v = dom.lookup(id);
	return get_constant(v, dom, cst_n, cst_d, display);
}
void PPLManager::get_range(Ident &id, PPLManager::t &dom, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display) {
#ifdef POLY_DEBUG			
	cout << "get_range(" << id << ") = ";
#endif
	Variable v = dom.lookup(id);
	get_range(v, dom, binf_n, binf_d, bsup_n, bsup_d, display);
}

bool PPLManager::is_constrained(Ident &id, PPLManager::t &dom){
	Variable v = dom.lookup(id);
	return is_constrained(v, dom);
}

int PPLDomain::gen = 0;
bool PPLManager::is_constrained(PPL::Variable &var, PPLManager::t &dom) {
	int axis = var.id();
	RemoveAllButOne pfunc(axis);
	map_space_dimensions(pfunc, dom.poly);
	return !dom.poly.is_universe();
}

void PPLManager::get_range(PPL::Variable &var, PPLManager::t &dom, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display) {
	bool maximum, minimum;
	if (var.id() >= dom.poly.space_dimension()) {
		bsup_n = 0;
		binf_n = 0;
		bsup_d = 0;
		binf_d = 0;
		return;
	}
	dom.poly.maximize(var, bsup_n, bsup_d, maximum);
	dom.poly.minimize(var, binf_n, binf_d, minimum);
#ifdef POLY_DEBUG			
	display = true;
#endif
	if (display) {
		gmp_printf("[");
		if (binf_d != 0) {
			gmp_printf("%Zd", &PPL::raw_value(binf_n));
			if (binf_d != 1)
				gmp_printf("/%Zd", &PPL::raw_value(binf_d));
		} else gmp_printf("-inf");
		gmp_printf("..");
		if (bsup_d != 0) {
			gmp_printf("%Zd", &PPL::raw_value(bsup_n));
			if (bsup_d != 1)
				gmp_printf("/%Zd", &PPL::raw_value(bsup_d));
		} else gmp_printf("+inf");
		gmp_printf("]");
		fflush(stdout);
	}


	return;
}

void PPLManager::scratch(Ident &id, PPLManager::t &dom) {
	if (!dom.exists(id)) 
		return;
	
#ifdef POLY_DEBUG			
	cout << "Before cylindrification: " << dom << endl;
#endif
	PPL::Variable v = dom.lookup(id);
	dom.poly.unconstrain(v);
#ifdef POLY_DEBUG			
	cout << "Scratch " << id << " (axis " << v << ")" << endl;
	cout << "After cylindrification: " << dom << endl;
#endif
}
PPL::Variable *PPLManager::make_var(Ident &id, PPLManager::t &dom) {
		dom.allocAxis(id);
		return new Variable(id, dom);
}

bool PPLManager::may_be_equal(PPLManager::t &s, PPL::Variable &v1, PPL::Variable &v2, int offset) {
	return !s.poly.relation_with(v1 == v2 + offset).implies(PPL::Poly_Con_Relation::is_disjoint()); 
}

bool PPLManager::must_be_equal(PPLManager::t &s, PPL::Variable &v1, PPL::Variable &v2, int offset) {
	return s.poly.relation_with(v1 == v2 + offset).implies(PPL::Poly_Con_Relation::is_included()); 
}

void PPLManager::binary_operation_helper(PPLManager::t &s, int op, PPL::Variable *v, PPL::Variable *vs1, PPL::Variable *vs2) {
	bool b;
	PPL::Coefficient cst_n, cst_d;
#ifdef POLY_DEBUG			
	cout << "Handle binary op: " << *v << " == " << *vs1 << " X " << *vs2 << endl;
#endif
	switch (op) {
		case sem::ADD:
			s.poly.add_constraint(*v == *vs1 + *vs2);
			break;
		case sem::SHL:          // d <- unsigned(a) << b
			b = get_constant(*vs2, s, cst_n, cst_d);
			if (b && (cst_d == 1))  {
				s.poly.add_constraint(*v == *vs1 * (1 << PPL::raw_value(cst_n).get_ui()));
			} else {
				s.poly.add_constraint(*v >= *vs1);
			}
			break;
		case sem::CMP:          // d <- a ~ b
		case sem::CMPU:          // d <- a ~u b // TODO handle signedness FIXME
		case sem::SUB:          // d <- a - b
			s.poly.add_constraint(*v == *vs1 - *vs2);
			break;
		case sem::SHR:          // d <- unsigned(a) >> b
		case sem::ASR:          // d <- a >> b
			b = get_constant(*vs2, s, cst_n, cst_d);
			if (b && (cst_d == 1))  {
				s.poly.add_constraint(*vs1 == *v * (1 << PPL::raw_value(cst_n).get_ui()));
			} else {
				s.poly.add_constraint(*vs1 >= *v);
			}
			break;
		case sem::MUL:
			b = get_constant(*vs1, s, cst_n, cst_d);
			if (b) {
				s.poly.add_constraint(*v * cst_d == *vs2 * cst_n);
			} else {
				b = get_constant(*vs2, s, cst_n, cst_d);
				s.poly.add_constraint(*v * cst_d == *vs1 * cst_n);
			} 
			break;
		case sem::MULH: // d <- (a * b) >> bitlength(d)
			b = get_constant(*vs1, s, cst_n, cst_d);
			if (b) {
				s.poly.add_constraint(*v * cst_d == *vs2 * cst_n);
			} else {
				b = get_constant(*vs2, s, cst_n, cst_d);
				s.poly.add_constraint(*v * cst_d == *vs1 * cst_n);
			}
			break;
		default:
			break;
	}
}
PPLManager::t PPLManager::loopExit(PPLManager::t s_in, int loop, int bound) {
	PPLManager::t s_out = s_in;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.exists(id)); /* You are supposed to be already inside the loop when you call loopExit() */ 
	Variable v = s_out.lookup(id);
	if (bound >= 0)
		s_out.poly.add_constraint(v <= bound);
    s_out.freeAxis(v.id());
	if (s_out.poly.is_empty()) {
#ifdef POLY_DEBUG			
		cout << "is empty after loopExit!" << endl;
#endif
		return _bot;
	}
	return s_out;
}

PPLManager::t PPLManager::loopTotal(PPLManager::t s_in, int loop, int bound) {
	PPLManager::t s_out = s_in;
	Ident id(loop | LOOP_TOTAL, Ident::ID_LOOP);
	ASSERT(s_out.exists(id)); /* You are supposed to be already inside the loop when you call loopExit() */ 
	Variable v = s_out.lookup(id);
	if (bound >= 0)
		s_out.poly.add_constraint(v <= bound);
    s_out.freeAxis(v.id());
	if (s_out.poly.is_empty()) {
#ifdef POLY_DEBUG			
		cout << "is empty after loopTotal!" << endl;
#endif
		return _bot;
	}
	return s_out;
}

PPLManager::t PPLManager::loopIter(PPLManager::t s_in, int loop, bool inner) {
	PPLManager::t s_out = s_in;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.exists(id)); /* You are supposed to be already inside the loop when you call loopIter() */ 
	Variable v_old = s_out.lookup(id);
	Variable v_new = s_out.create(id, true);
	s_out.poly.add_constraint(v_new == v_old + 1);

	if (inner) {
		Ident id_tot(loop | LOOP_TOTAL, Ident::ID_LOOP);
		ASSERT(s_out.exists(id_tot)); /* You are supposed to be already inside the loop when you call loopIter() */ 
		Variable v_old_tot = s_out.lookup(id_tot);
		Variable v_new_tot = s_out.create(id_tot, true);
		s_out.poly.add_constraint(v_new_tot == v_old_tot + 1);
	}

	return s_out;
}

PPLManager::t PPLManager::loopEntry(PPLManager::t s_in, int loop, bool inner) {
	PPLManager::t s_out = s_in;
	Ident id(loop, Ident::ID_LOOP);
	PPL::Variable v = s_out.create(id, true);
	s_out.poly.add_constraint(v == 0);
/*
	if (inner) {
		Ident id_tot(loop | LOOP_TOTAL, Ident::ID_LOOP);
		if (!s_out.exists(id_tot)) {
			PPL::Variable v_tot = s_out.create(id_tot, true);
			s_out.poly.add_constraint(v_tot == 0);
		}

	}
*/
	return s_out;
}

PPLManager::t PPLManager::update(t s_in, sem::inst si, int instaddr) {
        PPLManager::t s_out = s_in;
		ASSERT(!hasFilter() || (si.op == sem::BRANCH));

	switch(si.op) {
                case sem::NOP:
		        break;
		case sem::TRAP:         // perform a trap
		        break;
		case sem::CONT:         // continue in sequence with next instruction
		        break;
		case sem::BRANCH:               // perform a branch on content of register a
		        break;
		case sem::SCRATCH:      // d <- T
				{
					sem::reg_t dest = si.d();
					Ident id(dest, Ident::ID_REG);
					scratch(id, s_out);
				}
		        break;
		case sem::SETP:         // page(d) <- cst
		        break;
		case sem::SETI:         // d <- cst
		{  
				sem::reg_t dest = si.d();
				Ident id(dest, Ident::ID_REG);
				PPL::Variable v = s_out.create(id, true);
				int32_t cst = si.cst(); // FIXME TODO
				s_out.poly.add_constraint(v == cst);
				break;
		        
		}
		case sem::SET:          // d <- a
		{
				sem::reg_t dest = si.d();
				Ident id(dest, Ident::ID_REG);
				sem::reg_t source = si.a();
				Ident id2(source, Ident::ID_REG);
				PPL::Variable v = s_out.create(id, true);
				bool setToTop = false;
				if (!s_out.exists(id2)) {
					cout << "[WARN] Identifier " << id2 << " used, but not defined! Set to TOP." << endl;
					setToTop = true;
				}
				if (!setToTop) {
					Variable vs = s_out.lookup(id2);
					s_out.poly.add_constraint(v == vs);
				}
			break;
		}
		case sem::SHL:          // d <- unsigned(a) << b
		case sem::SUB:          // d <- a - b
		case sem::ASR:          // d <- a >> b
		case sem::MUL:
		case sem::MULH: // d <- (a * b) >> bitlength(d)
		case sem::ADD:          // d <- a + b
		case sem::SHR:          // d <- unsigned(a) >> b
		case sem::AND:          // d <- a & b
		case sem::OR:           // d <- a | b
		case sem::XOR:          // d <- a ^ b
		case sem::CMP:          // d <- a ~ b
		case sem::CMPU:         // d <- a ~u b
		{ 
			
		        sem::reg_t dest = si.d();
		        sem::reg_t op1 = si.a();
		        sem::reg_t op2 = si.b();
				Ident id1(op1, Ident::ID_REG);
				Ident id2(op2, Ident::ID_REG);
				Ident id(dest, Ident::ID_REG);
				bool setToTop = false;
		        if (!s_out.exists(id1)) {
					cout << "[WARN] Identifier " << id1 << " used, but not defined! Set to TOP" << endl;
					setToTop = true;
		        } else if (!s_out.exists(id2)) {
					cout << "[WARN] Identifier " << id2 << " used, but not defined! Set to TOP" << endl;
					setToTop = true;
		        } 
				if (!setToTop) {
					Variable vs1 = s_out.lookup(id1);
					Variable vs2 = s_out.lookup(id2);

					PPL::Variable v = s_out.create(id, true);
					binary_operation_helper(s_out, si.op, &v, &vs1, &vs2);
				} else s_out.create(id, true);
		        break;

		}
		case sem::STORE:                // MEMb(a) <- d
		{
				// TODO verifier si y'a pas une intersection possible avec un store existant

				bool had_exact_match = false;
				bool had_potential_match = false;
				bool need_join = false;
		        sem::reg_t src = si.d();
		        sem::reg_t addr = si.a();

				/* collect union of states resulting from all possible writes */
				PPL::C_Polyhedron all_writes = _bot.poly; 

				Ident id_new_addr, id_new_val;
				s_out.create_ptr(id_new_addr, id_new_val);

				Variable v_new_addr = s_out.create(id_new_addr);
				Variable v_new_val = s_out.create(id_new_val);

				Ident id_reg_src(src, Ident::ID_REG);
				Ident id_reg_addr(addr, Ident::ID_REG);

				Variable v_reg_src = s_out.lookup(id_reg_src, true);
				Variable v_reg_addr = s_out.lookup(id_reg_addr, true);

				/*
				 * Teste si l'adresse du store peut aliaser SSP+4
				 */

				Ident id_frame(Ident::ID_START_SP, Ident::ID_SPECIAL);
				Variable var_ssp = s_out.lookup(id_frame);
				if (may_be_equal(s_out, v_reg_addr, var_ssp, 4)) {
					cout << "warning: unsafe write at EIP=0x" << hex(instaddr) << endl;
				}

				

				for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++)
				{
					elm::Pair<Ident, int> p = *it;
					if (p.fst.getType() == Ident::ID_LOOP) {
						cout << "Projection de l'adresse du STORE (" << id_reg_addr << ") sur l'axe " << p.fst << ", STACK_FRAME" << endl;
						cout << "A=addr, B=frame, C=bound" << endl;
						genstruct::HashTable<int,int> map;
						Ident id_frame(Ident::ID_START_SP, Ident::ID_SPECIAL);
						Variable v_frame = s_out.lookup(id_frame);
						Variable v_bound = s_out.lookup(p.fst);
						map[v_reg_addr.id()] = 0;
						map[v_frame.id()] = 1;
						map[v_bound.id()] = 2;
						PPL::C_Polyhedron tmp = s_out.poly;
						map_space_dimensions(MapWithHash(map), tmp);
						tmp.minimized_constraints().print();
						cout << endl;
					}
					if ((p.fst.getType() == Ident::ID_MEM_ADDR) && (p.fst != id_new_addr)) {
						PPL::Variable v_ex_addr = s_out.lookup(p.snd);
						/* Store address may overlap with existing pointer */
						if (may_be_equal(s_out, v_reg_addr, v_ex_addr)) {
							had_potential_match = true;
							Ident id_ex_val(p.fst.getId(), Ident::ID_MEM_VAL);
							Variable v_ex_val = s_out.lookup(id_ex_val);
							if (must_be_equal(s_out, v_reg_addr, v_ex_addr)) {
#ifdef POLY_DEBUG
								 /* Our abstract domain should not have aliases, therefore an 
								 * exact match should not happen more than once. */
								ASSERT(!had_exact_match); 
								cout << "Address " << p.fst << " and " << id_reg_addr << " are equal (replacing)." << endl;
#endif								
								had_exact_match = true;
								s_out.freeAxis(v_ex_addr.id());
								s_out.freeAxis(v_ex_val.id());
							} else {
#ifdef POLY_DEBUG
								cout << "Address " << p.fst << " and " << id_reg_addr << " maybe equal (joining)." << endl;
#endif								
								PPL::C_Polyhedron this_write = s_out.poly;
								this_write.unconstrain(v_ex_val);
								this_write.add_constraint(v_reg_addr == v_ex_addr);
								this_write.add_constraint(v_reg_src == v_ex_val);
								poly_hull_helper(all_writes, this_write);
								need_join = true;
#ifdef POLY_DEBUG
								cout << "This write: " << endl;
								this_write.minimized_constraints().print();
								cout << "----" << endl;;
#endif
							}
						}
					}
				}
#ifdef POLY_DEBUG
				ASSERT(!had_exact_match || had_potential_match);
#endif
				if (need_join) {
#ifdef POLY_DEBUG
					cout << "Dynamic store!" << endl;
#endif
					poly_hull_helper(s_out.poly, all_writes);
				}
				s_out.poly.add_constraint(v_reg_addr == v_new_addr);
				s_out.poly.add_constraint(v_reg_src == v_new_val);
			break;
		}
		case sem::LOAD:         // d <- MEMb(a)
			{
		        sem::reg_t dst = si.d();
		        sem::reg_t addr = si.a();
				Ident id_dst(dst, Ident::ID_REG);
				Ident id_addr(addr, Ident::ID_REG);
				Variable vaddr = s_out.lookup(id_addr);
				PPL::Variable vdst = s_out.create(id_dst, true);
				bool found = false;
				
				// Look for matching ID_MEM_ADDR identifier 
				for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++)
				{
					elm::Pair<Ident, int> p = *it;
					Variable vsnd = s_out.lookup(p.snd);
					Ident id_val(p.fst.getId(), Ident::ID_MEM_VAL);
					if (p.fst.getType() == Ident::ID_MEM_ADDR) {
#ifdef POLY_DEBUG			
						if (may_be_equal(s_in, vaddr, vsnd)) {
							cout << "Candidate: " << p.fst << endl;
						}
#endif
						if (must_be_equal(s_in, vaddr, vsnd)) {
#ifdef POLY_DEBUG			
							cout << "Matched load source: " << p.fst << endl;
#endif
							Variable v_val = s_out.lookup(id_val);
							s_out.poly.add_constraint(vdst == v_val);
							found = true;
							break;

						}
						if (must_be_equal(s_in, vaddr, vsnd)) {
							cout << "Exact!" << endl;
						}
					}
				}

				if (!found) {
#ifdef POLY_DEBUG			
					cout << "Not found, creating new unconstrained ptr..." << endl;
#endif
					Ident addr, val;
					s_out.create_ptr(addr, val);
					Variable dummy_addr = s_out.create(addr);
					Variable dummy_val = s_out.create(val);
					s_out.poly.add_constraint(vdst == dummy_val);
					s_out.poly.add_constraint(vaddr == dummy_addr);
				}
			}
			break;
		case sem::NOT:          // d <- ~a
			break;
		case sem:: IF: {
				Ident id(si.sr(), Ident::ID_REG);
				if (s_out.exists(id)) {
					compare_reg = id;
					compare_op = si.cond();
					ASSERT(si.jump() == 1);
				}
			}
			break;
		default:
			cerr << "Invalid semantic instruction!" << endl;
			ASSERT(false);
			break;
	}
	// cout << "avant wrap" << s_out << endl;
	integer_wrap(s_out);
	// cout << "apres wrap" << s_out << endl;
	return s_out;
}

p::feature POLY_ANALYSIS_FEATURE("otawa::poly::POLY_ANALYSIS_FEATURE", new Maker<PolyAnalysis>());

/*
Output& operator<<(Output& o) {
	o << "lala";
}
*/
/*
const PPL::Variable &Ident::getVar() {
	PPL::Variable *tmp = _dom->ids[*this];
	return *tmp;
}
*/

BitVector PPLDomain::trash(MAX_AXIS);

void PPLDomain::freeAxis(int axis) {
	ASSERT(axis2id[axis].getType() != Ident::ID_INVALID);
	Ident old = axis2id[axis];
	/*
	axis2id[axis] = Ident();
	id2axis.remove(old);
	*/
#ifdef POLY_DEBUG			
	cout << "L'axe " << PPL::Variable(axis) << ", qui etait alloue a l'identificateur " << old << ", est marque pour etre supprime. " << endl;
#endif
	trash.set(axis);
}

int PPLDomain::allocAxis(const Ident &ident, bool allow_replace) { 
	ASSERT(num_axis < MAX_AXIS);
	ASSERT(allow_replace || !id2axis.hasKey(ident));
	if (id2axis.hasKey(ident)) {
		int axis = id2axis[ident];
#ifdef POLY_DEBUG			
		cout << "L'identificateur " << ident << " etait deja associe a l'axe " << PPL::Variable(axis) << endl;
#endif
		freeAxis(axis);
	}
	id2axis[ident] = num_axis;
	if (axis2id.length() <= num_axis) {
		axis2id.setLength(num_axis + 1);
	}
	axis2id[num_axis] = ident;
#ifdef POLY_DEBUG			
	cout << "Nouvel axe " << PPL::Variable(num_axis) << " alloue pour l'identificateur " << ident << endl;
#endif
	num_axis++;
	if (poly.space_dimension() < num_axis) {
		poly.add_space_dimensions_and_embed(num_axis - poly.space_dimension());
	}
	return num_axis - 1;
/*	DomId *dom_id= new DomId(ident, *this);
	DomVar *nv = new DomVar(num_axis, *this);
	ASSERT(!id2var.hasKey(*dom_id));
	num_axis++;
	id2var.put(*dom_id, nv);
	return *nv; */
}

Variable PPLDomain::lookup(const Ident &ident, bool allow_create) {
	if (allow_create && !exists(ident)) {
		return create(ident, false);
	}
	return Variable(ident, *this);
}

void PPLDomain::create_ptr(Ident &addr, Ident &val) {
	addr = Ident(mem_ref, Ident::ID_MEM_ADDR);
	val = Ident(mem_ref, Ident::ID_MEM_VAL);
	mem_ref++;
}

Variable PPLDomain::lookup(int axis) {
	return Variable(axis, *this);
}

Variable PPLDomain::create(const Ident &ident, bool allow_replace) {
	return Variable(allocAxis(ident, allow_replace), *this);
}

void PPLDomain::rename(const Ident &ident, const Ident &newident, bool allow_replace) {
	int axis;
	ASSERT(allow_replace || !id2axis.hasKey(newident));
	if (id2axis.hasKey(newident)) {
		axis = id2axis[newident];
#ifdef POLY_DEBUG			
		cout << "L'identificateur " << ident << " etait deja associe a l'axe " << PPL::Variable(axis) << endl;
#endif
		freeAxis(axis);
	}
	axis = id2axis[ident];

	axis2id[axis] = newident;
	id2axis[newident] = axis;
	id2axis.remove(ident);
#ifdef POLY_DEBUG			
	cout << "Renommage de l'identificateur " << ident << " en " << newident << " sur l'axe " << PPL::Variable(axis) << endl;
#endif
}

bool PPLDomain::exists(const Ident &ident) {
	return id2axis.hasKey(ident);
}
bool PPLDomain::exists(int axis) {
	return axis2id[axis].getType() != Ident::ID_INVALID;
}
} }	// otawa::poly
