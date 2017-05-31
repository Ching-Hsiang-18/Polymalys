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
void PolyAnalysis::processWorkSpace(WorkSpace *ws) {
	cout << "Processing Poly Analysis." << endl;
	const CFGCollection *coll = INVOLVED_CFGS(ws);
	/*
	// test ppl
	PPL::Variable x(0);
	PPL::Constraint_System cs;
	cs.insert(x >= 0);
	cs.print();
	PPL::Constraint_System cs2;
	cs2 = cs;
	*/
	ASSERT(coll);
	
	CFG *main = coll->get(0);
	cout << "nombre de CFG: " << coll->count() << endl;
	
	ai::CFGGraph graph(main);
	PPLManager *man = new PPLManager(); //ws, cfg);
	beurk = man;
	ai::EdgeStore<PPLManager, ai::CFGGraph> store(*man, graph);
	DumbOrder order;
	ai::OrderedDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph>, DumbOrder > ana(*man, graph, store, order);
	// ai::OrderedDriver<PPLManager, ai::CFGGraph, ai::EdgeStore<PPLManager, ai::CFGGraph> > ana(*man, graph, store);

	genstruct::HashTable<int, state_t> headerState;

	cout << "Starting abstract interpretation." << endl;	
	while (ana) {
		state_t s;
		s = ana.input();
			
		BasicBlock *bl = (BasicBlock*) *ana; 

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
#ifdef POLY_DEBUG			
			cout << "ITERATION: " << bound << endl;
#endif
			if ((MAX_ITERATION(bl) != -2) && ((MAX_ITERATION(bl) < bound)  || (bound == -2))) {
				MAX_ITERATION(bl) = bound;
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
					s = man->update(s, *semi);
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
					edgeState = man->loopIter(edgeState, e->sink()->id());
					man->bring_out_your_dead(edgeState); // TODO PERF FIXME
				} else {
					/* is entry-edge */
					edgeState = man->loopEntry(edgeState, e->sink()->id());
					headerState.remove(e->sink()->id());
					/*
					*/
				}
			}
			if (LOOP_EXIT_EDGE(e)) {
				Block *bb = LOOP_EXIT_EDGE(e);
				int bound = MAX_ITERATION(bb);
				if (bound >= 0) {
#ifdef POLY_DEBUG			
					cout << "LOOPEXIT: " << bound << endl;
#endif
					edgeState = man->loopExit(edgeState, bb->id(), bound);
					man->bring_out_your_dead(edgeState); // TODO PERF FIXME
				}
			}

			if (man->hasFilter()) {
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
		ana++;
	}
	cout << endl;
	cout << "FINAL STATE: " << endl;
	Block* bb = main->exit();
	Block::EdgeIter edge(bb->ins());
	Edge *exitEdge = *edge;
	
	state_t fs = store.get(edge);
	man->displayIdentMap(fs);
	fflush(stdout);
	fs.print(cout); cout << endl;
	fflush(stdout);
	man->display_loc_vars(fs);
	cout << endl;
	cout << "LOOP BOUNDS: " << endl;
	for (CFG::BlockIter iter(main->blocks()); iter; iter++) {
		BasicBlock *bb = (BasicBlock*) *iter;
		if (LOOP_HEADER(bb))
			cout << "MAX_ITERATION(" << bb->id() << ") = " << MAX_ITERATION(bb) << endl;
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
	PPL::Constraint_System result;
	for (PPL::Constraint_System::const_iterator it = dom.cons.begin(); it != dom.cons.end(); it++) {
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
	dom.cons = result;
}

void PPLManager::bring_out_your_dead(PPLManager::t &dom) {

	dom.sanity_checks();
	if (PPLDomain::trash.countOnes() == 0) {
#ifdef POLY_DEBUG			
		cout << "Nothing to clean" << endl;
		dom.sanity_checks();
#endif

		return;
	}
	// displayIdentMap(dom);
	PPL::C_Polyhedron poly(dom.cons);
	RemoveMarked rm(PPLDomain::trash, dom.cons.space_dimension());
	map_space_dimensions(rm, poly);


		int numcons = 0;
		for (PPL::Constraint_System::const_iterator it = dom.cons.begin(); it != dom.cons.end(); it++) {
			numcons++;
		}
		int mintime;
		struct timespec ts,ts2;
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
		dom.cons = poly.minimized_constraints();
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts2);
		mintime = (ts2.tv_sec - ts.tv_sec)*1000000 + (ts2.tv_nsec - ts.tv_nsec)/1000;

		cout << "Num constraints: " << numcons << ", time=" << mintime << " numtrash= " << PPLDomain::trash.countOnes() << endl;
		/*
		if (numcons == 41) {
			dom.cons.print();
			fflush(stdout);
			cout << endl;
		}
		*/


	dom.num_axis -= PPLDomain::trash.countOnes();
	dom.map_identifiers(rm);
	PPLDomain::trash.clear();
	// displayIdentMap(dom);
#ifdef POLY_DEBUG			
	dom.sanity_checks();
#endif

	return; 

	/*

	int axis,old_axis;
	bool changes = false;
	for (genstruct::Vector<PPL::Variable*>::Iterator it(to_remove); it; it++) {
		PPL::Variable *bye = *it;
		PPL::Variables_Set vset;
		vset.insert(*bye);
		PPL::C_Polyhedron poly(dom.cons);
		poly.remove_space_dimensions(vset);
		dom.cons = poly.minimized_constraints();
		cout << "Removing dead variable: " << *bye << endl;
		dom.num_axis--;
		changes = true;
		old_axis = bye->id();
		for (elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent>::MutableIter it(dom.ids); it; it++) {
				PPL::Variable *&v2 = it.item();
				if (v2 == NULL)
					continue;
				axis = v2->id();
				if (axis == old_axis) {
					v2 = NULL;
				} else if (axis > old_axis) {
					delete v2;
					PPL::Variable *v3 = new PPL::Variable((axis > old_axis) ? (axis-1) : axis);
					v2 = v3;
				}
		}
	}
	if (changes) {
		genstruct::Vector<Ident*> idents;
		idents.clear();
		for (elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent>::PairIterator it(dom.ids); it; it++) {
			elm::Pair<Ident, PPL::Variable *> p = *it;
			Ident *id = new Ident(p.fst);
			if (p.snd == NULL) {
				cout << *id << endl;
				idents.add(id);
			}
		}
		for (genstruct::Vector<Ident*>::Iterator it(idents); it; it++) {
			cout << "Removing ID: " << **it << endl;
			dom.ids.remove(**it);
		}
		cout << "After dead variable removal: " << dom << endl;
		cout << "Ident: ";  
		for (elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent>::PairIterator it(dom.ids); it; it++) {
			elm::Pair<Ident, PPL::Variable *> p = *it;
			cout << p.fst << " = " << *p.snd << ", ";
		}
		cout << endl;
	}
	to_remove.clear();
	*/
}

void PPLManager::display_loc_vars(PPLManager::t &dom) {
	if (dom.isBottom()) {
		cout << "diplay_loc_vars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	Variable ssp = dom.lookup(id_ssp);
	cout << dom.cons.space_dimension() << " Local variables: " << endl;
	for (int i = 0 ; i < NUM_LOC_VARS*LOC_VAR_SIZE; i += LOC_VAR_SIZE) {
		PPL::Constraint_System cons = dom.cons;
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
					cst = get_constant(idval, dom, num, den);
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

bool PPLManager::get_constant(PPL::Variable &var, PPLManager::t &dom, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d) {
	PPL::Coefficient binf_d, binf_n, bsup_d, bsup_n;
	get_range(var, dom, bsup_n, bsup_d, binf_n, binf_d); 
	if ((binf_d == bsup_d) && (binf_n == bsup_n) && (binf_d != 0)) {
		cst_n = binf_n;
		cst_d = binf_d;
		return true;
	}
	return false;
}
bool PPLManager::get_constant(Ident &id, PPLManager::t &dom, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d) {
	Variable v = dom.lookup(id);
	return get_constant(v, dom, cst_n, cst_d);
}
void PPLManager::get_range(Ident &id, PPLManager::t &dom, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d) {
#ifdef POLY_DEBUG			
	cout << "get_range(" << id << ") = ";
#endif
	Variable v = dom.lookup(id);
	get_range(v, dom, binf_n, binf_d, bsup_n, bsup_d);
}

bool PPLManager::is_constrained(Ident &id, PPLManager::t &dom){
	Variable v = dom.lookup(id);
	return is_constrained(v, dom);
}

int PPLDomain::gen = 0;
bool PPLManager::is_constrained(PPL::Variable &var, PPLManager::t &dom) {
	int axis = var.id();
	RemoveAllButOne pfunc(axis);
	PPL::C_Polyhedron poly(dom.cons);
	map_space_dimensions(pfunc, poly);
	return !poly.is_universe();
}

void PPLManager::get_range(PPL::Variable &var, PPLManager::t &dom, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d) {
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
#ifdef POLY_DEBUG			
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

//	gmp_printf("[%Zd/%Zd..%Zd/%Zd]\n", &PPL::raw_value(binf_n), &PPL::raw_value(binf_d), &PPL::raw_value(bsup_n), &PPL::raw_value(binf_d));
	fflush(stdout);
#endif

/*
	int axis = var.id();
	RemoveAllButOne pfunc(axis);
	PPL::C_Polyhedron poly(dom.cons);
	if (poly.space_dimension() <= axis) {
		binf = INT_MIN;
		bsup = INT_MAX;
		cout << "]-infty..+infty[" << endl;;
		return;
	}
	poly.map_space_dimensions(pfunc);
	ASSERT(!poly.is_empty());
	if (poly.is_universe()) {
		binf = INT_MIN;
		bsup = INT_MAX;
		cout << "]-infty..+infty[" << endl;
		return;
	}
	PPL::Constraint_System cons = poly.minimized_constraints();
	PPL::Variable v(0);
	for (PPL::Constraint_System::const_iterator it = cons.begin(); it != cons.end(); it++) {
		const PPL::Constraint &c = *it;
		unsigned long long val = PPL::raw_value(c.inhomogeneous_term()).get_ui();
		unsigned long long coef = PPL::raw_value(c.coefficient(v)).get_ui();
		if (c.is_equality()) {
			binf = val;
			bsup = val;
		}
		if (c.is_inequality()) {
			ASSERT(coef != 0);
			bool strict = c.is_strict_inequality();
			if ((coef > 0) && (binf < (val + strict)))
				binf = val + strict;
			if ((coef < 0) && (bsup < (val - strict)))
				bsup = val - strict;
		}
	}
	cout << "[" << binf << ".." << bsup << "]";
	cout << endl;
	*/
	return;
}

void PPLManager::scratch(Ident &id, PPLManager::t &dom) {
	if (!dom.exists(id)) 
		return;
	
#ifdef POLY_DEBUG			
	cout << "Before cylindrification: " << dom << endl;
#endif
	PPL::Variable v = dom.lookup(id);
	PPL::C_Polyhedron poly(dom.cons);
	poly.unconstrain(v);
	dom.cons = poly.minimized_constraints();
#ifdef POLY_DEBUG			
	cout << "Scratch " << id << " (axis " << v << ")" << endl;
	cout << "After cylindrification: " << dom << endl;
#endif
}
PPL::Variable *PPLManager::make_var(Ident &id, PPLManager::t &dom) {
	/*
		int axis = dom.num_axis;
		int old_axis;
		dom.num_axis++;
	*/
		/*
		PPL::Variable *v = new PPL::Variable(axis);
		PPL::Variable *old_v = NULL;
		*/
	/*
		if (!dom.id2axis.hasKey(id)) {
			cout << "Nouvelle variable: " << id << " sur l'axe: " << axis << endl;
		} else {
			old_axis = dom.id2axis[id];
			cout << "La variable: " << id << " etait sur l'axe " << old_axis << " maintenant elle sera sur l'axe " << axis << endl;
		}*/

		dom.allocAxis(id);
		return new Variable(id, dom);
		//ids.put(id, v);
		//dom.id2axi
		//dom.ids[id] = v;
		//
		/*
		if (old_v) {
			to_remove.add(old_v); */ 
//			cout << "Elimination de variable sur l'axe: " << *old_v << endl;

			// On decale...  
			/* 
			cout << "avant decalage: " << endl;
			for (elm::genstruct::HashTable<Ident, PPL::Variable*>::PairIterator it(ids); it; it++) {
				elm::Pair<Ident, PPL::Variable *> p = *it;
				cout << p.fst << " = " << *p.snd << ", ";
			}
			cout << endl; */  /* 
			for (elm::genstruct::HashTable<Ident, PPL::Variable*>::MutableIter it(ids); it; it++) {

					PVAR &v2 = it.item();
					axis = v2->id();
					delete v2;
					PPL::Variable *v3 = new PPL::Variable((axis > old_axis) ? (axis-1) : axis);
					v2 = v3;
			}   */ 
/*
			for (elm::genstruct::HashTable<Ident, PPL::Variable*>::PairIterator it(ids); it; it++) {
				elm::Pair<Ident, PPL::Variable *> p = *it;
				cout << p.fst << " = " << *p.snd << ", ";
			}
*/
}

bool PPLManager::may_be_equal(PPLManager::t &s, PPL::Variable &v1, PPL::Variable &v2) {
	PPL::Constraint_System tmp = s.cons;
	tmp.insert(v1 == v2);
	/*
	Ident lala1(1, Ident::ID_REG);
	Variable x(lala1, s);
	Variable y(1, s);
	tmp.insert(x == y);
	*/
/*
	DomId dom_id(lala1, s);
	const DomVar &v3 = dom_id.getVar();
	const PPL::Variable &v4 = v3;

	tmp.insert(dom_id.getVar() == v3); */ 

 	//tmp.insert(dom_id.getVar() == dom_id.getVar());
	
	PPL::C_Polyhedron poly(tmp);
	return !poly.is_empty();
}

bool PPLManager::must_be_equal(PPLManager::t &s, PPL::Variable &v1, PPL::Variable &v2) {
	PPL::Constraint_System tmp = s.cons;
	PPL::C_Polyhedron poly(tmp);
	PPL::Coefficient bsup_n, bsup_d;
	PPL::Coefficient binf_n, binf_d;
	bool maximum, minimum;
	poly.maximize(v1 - v2, bsup_n, bsup_d, maximum);
	poly.minimize(v1 - v2, binf_n, binf_d, minimum);
	return ((bsup_n == 0) && (binf_n == 0) && (bsup_d != 0) && (binf_d != 0));
}

void PPLManager::binary_operation_helper(PPLManager::t &s, int op, PPL::Variable *v, PPL::Variable *vs1, PPL::Variable *vs2) {
	bool b;
	PPL::Coefficient cst_n, cst_d;
#ifdef POLY_DEBUG			
	cout << "Handle binary op: " << *v << " == " << *vs1 << " X " << *vs2 << endl;
#endif
	switch (op) {
		case sem::ADD:
			s.cons.insert(*v == *vs1 + *vs2);
			break;
		case sem::SHL:          // d <- unsigned(a) << b
			b = get_constant(*vs2, s, cst_n, cst_d);
			if (b && (cst_d == 1))  {
				s.cons.insert(*v == *vs1 * (1 << PPL::raw_value(cst_n).get_ui()));
			}
			break;
		case sem::CMP:          // d <- a ~ b
		case sem::CMPU:          // d <- a ~u b // TODO handle signedness FIXME
		case sem::SUB:          // d <- a - b
			s.cons.insert(*v == *vs1 - *vs2);
			break;
		case sem::SHR:          // d <- unsigned(a) >> b
		case sem::ASR:          // d <- a >> b
			b = get_constant(*vs2, s, cst_n, cst_d);
			if (b && (cst_d == 1))  {
				s.cons.insert(*vs1 == *v * (1 << PPL::raw_value(cst_n).get_ui()));
			}
			break;
		case sem::MUL:
			b = get_constant(*vs1, s, cst_n, cst_d);
			if (b) {
				s.cons.insert(*v * cst_d == *vs2 * cst_n);
			} else {
				b = get_constant(*vs2, s, cst_n, cst_d);
				s.cons.insert(*v * cst_d == *vs1 * cst_n);
			}
			break;
		case sem::MULH: // d <- (a * b) >> bitlength(d)
			b = get_constant(*vs1, s, cst_n, cst_d);
			if (b) {
				s.cons.insert(*v * cst_d == *vs2 * cst_n);
			} else {
				b = get_constant(*vs2, s, cst_n, cst_d);
				s.cons.insert(*v * cst_d == *vs1 * cst_n);
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
	s_out.cons.insert(v <= bound);
	PPL::C_Polyhedron poly(s_out.cons);
	//s_out.freeAxis(v.id());
	if (poly.is_empty()) {
#ifdef POLY_DEBUG			
		cout << "is empty after loopExit!" << endl;
#endif
		return _bot;
	}
	return s_out;
}

PPLManager::t PPLManager::loopIter(PPLManager::t s_in, int loop) {
	PPLManager::t s_out = s_in;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.exists(id)); /* You are supposed to be already inside the loop when you call loopIter() */ 
	Variable v_old = s_out.lookup(id);
	Variable v_new = s_out.create(id, true);
	s_out.cons.insert(v_new == v_old + 1);
	return s_out;
}

PPLManager::t PPLManager::loopEntry(PPLManager::t s_in, int loop) {
	PPLManager::t s_out = s_in;
	Ident id(loop, Ident::ID_LOOP);
	PPL::Variable v = s_out.create(id, true);
	s_out.cons.insert(v == 0);
	return s_out;
}

PPLManager::t PPLManager::update(t s_in, sem::inst si) {
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
		        s_out.cons.insert(v == si.cst());
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
					s_out.cons.insert(v == vs);
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
				} else s_out.create(id);
		        break;

		}
		case sem::STORE:                // MEMb(a) <- d
		{
				// TODO verifier si y'a pas une intersection possible avec un store existant
				bool need_join = false;
				PPL::Constraint_System old_cons = s_out.cons;
		        sem::reg_t src = si.d();
		        sem::reg_t addr = si.a();
				Ident id1, id2;
				s_out.create_ptr(id2, id1);
				Ident id3(src, Ident::ID_REG); //registre donnee a stocker
				Ident id4(addr, Ident::ID_REG); //registre adresse
				Variable vsrc = s_out.lookup(id3, true);
				Variable vaddr = s_out.lookup(id4, true);

				bool replace_done = false;
				for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++)
				{
					elm::Pair<Ident, int> p = *it;
					if (p.fst.getType() == Ident::ID_MEM_ADDR) {
						PPL::Variable vaddr2 = s_out.lookup(p.snd);
						// cout << "[STORE] Comparing " << p.fst << " and " << id4 << endl;
						if (may_be_equal(s_out, vaddr, vaddr2)) {
							if (must_be_equal(s_out, vaddr, vaddr2)) {
								ASSERT(!replace_done);
								replace_done = true;
#ifdef POLY_DEBUG			
								cout << "Address " << p.fst << " and " << id4 << " are equal (replacing)." << endl;
#endif
								s_out.freeAxis(vaddr2.id());
								Ident old_val(p.fst.getId(), Ident::ID_MEM_VAL);
#ifdef POLY_DEBUG			
								cout << "Will be replaced: " << p.fst << " and " << old_val << endl;
#endif
								Variable vaddr2_val = s_out.lookup(old_val);
								Variable new_val = s_out.lookup(id3);
								s_out.freeAxis(vaddr2_val.id());
								//PPLDomain::trash.set(vaddr2_val.id());
								
							} else {
#ifdef POLY_DEBUG			
								cout << "Address " << p.fst << " and " << id4 << " may overlap (joining)." << endl;
#endif
								Ident old_val(p.fst.getId(), Ident::ID_MEM_VAL);
								Variable vaddr2_val = s_out.lookup(old_val);
								Variable new_val = s_out.lookup(id3);
								PPL::C_Polyhedron poly(old_cons);
								poly.unconstrain(vaddr2_val);
								old_cons = poly.minimized_constraints();
								old_cons.insert(vaddr2_val == vsrc);
								need_join = true;
							}
							// TODO: set to TOP
						} else {
							if (must_be_equal(s_out, vaddr, vaddr2)) {
								cout << "ERROR: " << s_out << endl;
								ASSERT(0 == 1);
							}
#ifdef POLY_DEBUG			
							cout << "Address " << p.fst << " and " << id4 << " cannot overlap." << endl;
#endif
						}
					} 
				}
				Variable v1 = s_out.create(id1);
				Variable v2 = s_out.create(id2);
				//cout << " ajout: " << id1 << " == " <<  id3 << endl;
				//cout << "avant: " << endl;
				//s_out.cons.print();
				//cout << endl;
				s_out.cons.insert(v2 == vaddr);
				//cout << "apres1: " << endl;
				//s_out.cons.print();
				//cout << endl;
				s_out.cons.insert(v1 == vsrc) ;
				//cout << "apres2: " << endl;
				//s_out.cons.print();
				//cout << endl;
				if (need_join) { 
					old_cons.insert(v1 == vsrc) ;
					old_cons.insert(v2 == vaddr);
					PPL::C_Polyhedron poly1(s_out.cons);
#ifdef POLY_DEBUG			
					cout << "Hull 1: " ;
					poly1.minimized_constraints().print();
					fflush(stdout);
					cout << endl;
#endif
					PPL::C_Polyhedron poly2(old_cons);
#ifdef POLY_DEBUG			
					cout << "Hull 2: " ;
					poly2.minimized_constraints().print();
					fflush(stdout);
					cout << endl;
#endif
					poly_hull_helper(poly1, poly2);
					s_out.cons = poly1.minimized_constraints();
#ifdef POLY_DEBUG			
					cout << "Hull: " ;
					poly1.minimized_constraints().print();
					fflush(stdout);
					cout << endl;
#endif
				}
			break;
		}
		case sem::LOAD:         // d <- MEMb(a)
			{
		        sem::reg_t dst = si.d();
		        sem::reg_t addr = si.a();
				Ident id_dst(dst, Ident::ID_REG);
				Ident id_addr(addr, Ident::ID_REG);
				Variable vaddr = s_out.lookup(id_addr);
				Variable *v_val = NULL;
				
				// Look for matching ID_MEM_ADDR identifier 
				for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++)
				{
					elm::Pair<Ident, int> p = *it;
					Variable vsnd = s_out.lookup(p.snd);
					if (p.fst.getType() == Ident::ID_MEM_ADDR) {
						// cout << "[LOAD] Comparing " << p.fst << " and " << id_addr << endl;
						if (must_be_equal(s_out, vaddr, vsnd)) {
#ifdef POLY_DEBUG			
							cout << "Found! " << p.fst << endl;
#endif
							Ident id_val(p.fst.getId(), Ident::ID_MEM_VAL);
							v_val= new Variable(s_out.lookup(id_val));
							break;
						}
						if (may_be_equal(s_out, vaddr, vsnd)) {
#ifdef POLY_DEBUG			
							cout << "Candidate: " << p.fst << endl;
#endif
							// TODO faire un hull quand il y a que des candidats mais pas de valeur sure
						}
					}
				}
				PPL::Variable vdst = s_out.create(id_dst, true);
				if (v_val != NULL) {
					s_out.cons.insert(vdst == *v_val);
				} else {
#ifdef POLY_DEBUG			
					cout << "Not found, creating new unconstrained ptr..." << endl;
#endif
					Ident addr, val;
					s_out.create_ptr(addr, val);
					Variable dummy_addr = s_out.create(addr);
					Variable dummy_val = s_out.create(val);
					s_out.cons.insert(vdst == dummy_val);
					s_out.cons.insert(vaddr == dummy_addr);
				}
			}
			break;
		case sem::NOT:          // d <- ~a
			break;
		case sem:: IF: {
				Ident id(si.sr(), Ident::ID_REG);
				ASSERT(s_out.exists(id));
				compare_reg = id;
				compare_op = si.cond();
				ASSERT(si.jump() == 1);
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
