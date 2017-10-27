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

t::hash HashCons::hash(const PPL::Constraint& key) { 
	int kind;
	unsigned int prime = 16777619;
	t::hash result = 2166136261;
	for (PPL::dimension_type i = 0; i < key.space_dimension(); i++) {
		result ^= key.coefficient(Variable(i)).get_ui();
		result *= prime;
	}
	result ^= key.inhomogeneous_term().get_ui();
	result *= prime;
	if (key.is_equality()) {
		kind = 0;
	} else if (key.is_strict_inequality()) {
		kind = 1; 
	} else kind = 2;
	result ^= kind;
	return result;
}

/*
Variable::Variable(const Ident &ident, const PPLDomain &dom) : Variable(dom.id2axis[ident]), _dom(dom), _ident(ident) { }
Variable::Variable(int axis, const PPLDomain &dom) 	: Variable(axis), _dom(dom), _ident(dom.axis2id[axis]) { 
	ASSERT(_ident.getType() != Ident::ID_INVALID);	
} 
*/

Output& operator<<(Output& o, const Variable pv) { 
		char letter = (pv.id() % 26) + 'A';
		int number = pv.id() / 26;
		char name[32];
		if (number) {
			snprintf(name, sizeof(name), "%c%u", letter, number);
		} else {
			snprintf(name, sizeof(name), "%c", letter);
		}
		o << name;
		return o;
}  

/**
 * Create PPLManager using a fresh init state.
 */
PPLManager::PPLManager(const PropList &props) : _init(MAX_AXIS(props)), _bot(), _top(MAX_AXIS(props)) {
	PPL::Constraint_System initcons;

	Variable var_sp = _init.varNew(Ident(13, Ident::ID_REG));
	Variable var_fp = _init.varNew(Ident(11, Ident::ID_REG));
	Variable var_lr = _init.varNew(Ident(14, Ident::ID_REG));
	Variable var_ssp = _init.varNew(Ident(Ident::ID_START_SP, Ident::ID_SPECIAL));
	Variable var_sfp = _init.varNew(Ident(Ident::ID_START_FP, Ident::ID_SPECIAL));
	Variable var_slr = _init.varNew(Ident(Ident::ID_START_LR, Ident::ID_SPECIAL));

	_init.poly.add_constraint(var_ssp == var_sp);
	_init.poly.add_constraint(var_sfp == var_fp);
	_init.poly.add_constraint(var_slr == var_lr);

} 

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

// PPLDOMAIN


template <class F> PPLDomain::MapHelper<F>::MapHelper(F &pfunc, int max_in_domain) : _pfunc(pfunc), _max_in_domain(max_in_domain), _empty(true) {
	for (PPL::dimension_type i = 0; i <= _max_in_domain; i++) {
		PPL::dimension_type j;
		if (_pfunc.maps(i, j) && (_empty || (_max_in_codomain < j)))  {
			_max_in_codomain = j;
			_empty = false;
		}
	}
}

bool PPLDomain::RemoveMarked::maps(PPL::dimension_type i, PPL::dimension_type &j) const {
	if (_bv.bit(i)) {
		return false;
	}

	int shift_amount = 0;
	for (int k = 0; k < i; k++) {
		if (_bv.bit(k)) {
			shift_amount++;
		}
	}
	j = i - shift_amount;
	return true;
}


template <class F> void PPLDomain::doMapPoly(F pfunc) {
	MapHelper<F> a(pfunc, poly.space_dimension() - 1);
	poly.map_space_dimensions(a);
}

template <class F> void PPLDomain::doMapIdents(F pfunc) {
	genstruct::Vector<Ident> todel;
	int old_length = axis2id.length();
	axis2id.clear();
	axis2id.setLength(old_length);
	// cout << "trash bitvector:" << PPLDomain::trash << endl;
#ifdef POLY_DEBUG			
	cout << "REMAP: ";
#endif
	for (elm::genstruct::HashTable<Ident, int, HashIdent>::MutableIter it(id2axis); it; it++) {
		int &n = it.item();
		PPL::dimension_type old_axis = n;
		PPL::dimension_type new_axis = n;
		if (pfunc.maps(old_axis, new_axis)) {
			if (old_axis != new_axis) {
#ifdef POLY_DEBUG			
				cout << it.key() << "[" << Variable(old_axis) << "->" << Variable(new_axis) << "] ";
#endif
				n = new_axis;
			}
			axis2id[new_axis] = it.key();
		} else todel.add(it.key());
	}
#ifdef POLY_DEBUG			
	cout << endl;
#endif
	for (genstruct::Vector<Ident>::Iterator it(todel); it; it++) {
		id2axis.remove(*it);
	}
}
template <class F> void PPLDomain::doMap(F pfunc) {
	doMapPoly(pfunc);
	doMapIdents(pfunc);
}

#ifdef POLY_DEBUG
void PPLDomain::_sanityChecks() {
	int max_axis = -1;
	if (isBottom())  {
		ASSERT(poly.is_empty());
		return;
	}
	ASSERT(!poly.is_empty());
	for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(id2axis); it; it++) {
		ASSERT((*it).snd < num_axis);
		if (trash.bit((*it).snd))
			continue;
		ASSERT(axis2id[(*it).snd] == (*it).fst);
		if ((*it).snd > max_axis)
			max_axis = (*it).snd;
	}
	for (int i = 0; i < num_axis; i++) {
		if (trash.bit(i))
			continue;
		Ident &ident = axis2id[i];
		ASSERT(id2axis[ident] == i);
	}
	ASSERT(max_axis + 1 == num_axis);
	ASSERT(poly.space_dimension() <= num_axis); // unused axis can exist at the end
	ASSERT(trash.size() >= num_axis);
	ASSERT(trash.countOnes() <= num_axis);
}
#endif

typedef Variable* PVAR;

void PPLDomain::doIntegerWrap() {
	return; // TODO integer wrap not supported yet
	
}

void PPLDomain::doFinalizeUpdate() {
	_sanityChecks();
#ifdef POLY_DEBUG			
	if (PPLDomain::trash.countOnes() == 0) {
		cout << "Nothing to clean" << endl;
		dom._sanityChecks();
		return;
	}
#endif
	// displayIdentMap(dom);
	PPLDomain::RemoveMarked rm(PPLDomain::trash, poly.space_dimension());
	doMapPoly(rm);
	num_axis -= PPLDomain::trash.countOnes();
	doMapIdents(rm);
	PPLDomain::trash.clear();
	// displayIdentMap(dom);
	_sanityChecks();
	return; 
}

void PPLDomain::displayLocVars() {
	const PropList _props;
	PPL::Constraint_System mcons = poly.minimized_constraints();
	if (isBottom()) {
		cout << "diplay_loc_vars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	Variable ssp = lookup(id_ssp);
	cout << mcons.space_dimension() << " Local variables: " << endl;
	for (int i = 0 ; i < NUM_LOC_VARS(_props)*LOC_VAR_SIZE(_props); i += LOC_VAR_SIZE(_props)) {
		PPL::Constraint_System cons = mcons;
		Variable v(num_axis);
		cons.insert(v == ssp - i - LOC_VAR_SIZE(_props));
		cout << " [SP - " << hex(i) << "] == ";
		bool found = false;
		for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++)
		{
			elm::Pair<Ident, int> p = *it;
			Variable vsnd = getVar(p.snd);
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
					cst = getConstant(idval, num, den, true);
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

bool PPLDomain::getConstant(Variable &var, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d, bool display) {
	PPL::Coefficient binf_d, binf_n, bsup_d, bsup_n;
	getRange(var, bsup_n, bsup_d, binf_n, binf_d, display); 
	if ((binf_d == bsup_d) && (binf_n == bsup_n) && (binf_d != 0)) {
		cst_n = binf_n;
		cst_d = binf_d;
		return true;
	}
	return false;
}
bool PPLDomain::getConstant(Ident &id, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d, bool display) {
	Variable v = lookup(id);
	return getConstant(v, cst_n, cst_d, display);
}
void PPLDomain::getRange(Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display) {
#ifdef POLY_DEBUG			
	cout << "get_range(" << id << ") = ";
#endif
	Variable v = lookup(id);
	getRange(v, binf_n, binf_d, bsup_n, bsup_d, display);
}


void PPLDomain::getRange(Variable &var, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display) {
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

void PPLDomain::doScratch(Ident &id) {
	if (hasIdent(id)) 
		return;
	
#ifdef POLY_DEBUG			
	cout << "Before cylindrification: " << dom << endl;
#endif
	Variable v = lookup(id);
	poly.unconstrain(v);
#ifdef POLY_DEBUG			
	cout << "Scratch " << id << " (axis " << v << ")" << endl;
	cout << "After cylindrification: " << dom << endl;
#endif
}

bool PPLDomain::mayEqual(Variable &v1, Variable &v2, int offset) {
	return !poly.relation_with(v1 == v2 + offset).implies(PPL::Poly_Con_Relation::is_disjoint()); 
}

bool PPLDomain::mustEqual(Variable &v1, Variable &v2, int offset) {
	return poly.relation_with(v1 == v2 + offset).implies(PPL::Poly_Con_Relation::is_included()); 
}

void PPLDomain::_doBinaryOp(int op, Variable *v, Variable *vs1, Variable *vs2) {
	bool b;
	PPL::Coefficient cst_n, cst_d;
#ifdef POLY_DEBUG			
	cout << "Handle binary op: " << *v << " == " << *vs1 << " X " << *vs2 << endl;
#endif
	switch (op) {
		case sem::ADD:
			poly.add_constraint(*v == *vs1 + *vs2);
			break;
		case sem::SHL:          // d <- unsigned(a) << b
			b = getConstant(*vs2, cst_n, cst_d);
			if (b && (cst_d == 1))  {
				poly.add_constraint(*v == *vs1 * (1 << PPL::raw_value(cst_n).get_ui()));
			} else {
				poly.add_constraint(*v >= *vs1);
			}
			break;
		case sem::CMP:          // d <- a ~ b
		case sem::CMPU:          // d <- a ~u b // TODO handle signedness FIXME
		case sem::SUB:          // d <- a - b
			poly.add_constraint(*v == *vs1 - *vs2);
			break;
		case sem::SHR:          // d <- unsigned(a) >> b
		case sem::ASR:          // d <- a >> b
			b = getConstant(*vs2, cst_n, cst_d);
			if (b && (cst_d == 1))  {
				poly.add_constraint(*vs1 == *v * (1 << PPL::raw_value(cst_n).get_ui()));
			} else {
				poly.add_constraint(*vs1 >= *v);
			}
			break;
		case sem::MUL:
			b = getConstant(*vs1,cst_n, cst_d);
			if (b) {
				poly.add_constraint(*v * cst_d == *vs2 * cst_n);
			} else {
				b = getConstant(*vs2, cst_n, cst_d);
				poly.add_constraint(*v * cst_d == *vs1 * cst_n);
			} 
			break;
		case sem::MULH: // d <- (a * b) >> bitlength(d)
			b = getConstant(*vs1, cst_n, cst_d);
			if (b) {
				poly.add_constraint(*v * cst_d == *vs2 * cst_n);
			} else {
				b = getConstant(*vs2, cst_n, cst_d);
				poly.add_constraint(*v * cst_d == *vs1 * cst_n);
			}
			break;
		default:
			break;
	}
}
PPLDomain PPLDomain::onLoopExit(int loop, int bound) {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.hasIdent(id)); /* You are supposed to be already inside the loop when you call onLoopExit() */ 
	Variable v = s_out.lookup(id);
	if (bound >= 0)
		s_out.poly.add_constraint(v <= bound);
    s_out.varKill(v);
	if (s_out.poly.is_empty()) {
#ifdef POLY_DEBUG			
		cout << "is empty after onLoopExit!" << endl;
#endif
		return PPLDomain(); // bottom
	}
	return s_out;
}


PPLDomain PPLDomain::onLoopIter(int loop, bool inner) {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.hasIdent(id)); /* You are supposed to be already inside the loop when you call onLoopIter() */ 
	Variable v_old = s_out.lookup(id);
	Variable v_new = s_out.varNew(id, true);
	s_out.poly.add_constraint(v_new == v_old + 1);

	return s_out;
}

PPLDomain PPLDomain::onLoopEntry(int loop, bool inner) {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	Variable v = s_out.varNew(id, true);
	s_out.poly.add_constraint(v == 0);
	return s_out;
}



void PPLDomain::displayIdentMap() {
	cout << "IDMAP: " ;
	for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(id2axis); it; it++) {
		const Ident &ident = (*it).fst;
		cout << ident << ":" << Variable((*it).snd) << ", ";
	}
	cout << endl;
}


// Return the first constraint in poly for which the coef of specified variable axis is non-zero
const PPL::Constraint *PPLDomain::_getConstraintFor(int axis) {
	PPL::Constraint_System cons_sys = poly.minimized_constraints();
	for (PPL::Constraint_System::const_iterator it = cons_sys.begin(); it != cons_sys.end(); it++) {
		const PPL::Constraint &c = *it;
		if (!c.is_equality())
			continue;
		if (c.coefficient(Variable(axis)) != 0) {
			return new PPL::Constraint(c);
		}
	}
	return NULL;
}





/**
* Indexes the pointer in dom, by their expression in terms of registers referenced in map_regs. 
* Stores the result in map_ptr.
*/
void PPLDomain::_indexPointersByExpr(genstruct::HashTable<PPL::Constraint, int, HashCons> &map_ptr, genstruct::HashTable<int, int> map_regs) {
int axis = map_regs.count();
for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
	if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
		map_regs.put((*it).snd, axis);
		PPLDomain dom(*this); /* make a working copy to do the projections */
		dom.doMapPoly(MapWithHash(map_regs));

		const PPL::Constraint *cons = dom._getConstraintFor(axis);
		if (cons) {
			map_ptr[*cons] = (*it).fst.getId();
			delete cons;
		}
		map_regs.remove((*it).snd);
	}
}
}

PPLDomain PPLDomain::onBranch(bool taken) {
	if (isBottom())
		return *this;
	sem::cond_t this_op;
	ASSERT(hasFilter());
	PPLDomain res = *this;
	this_op = taken ? compare_op : sem::invert(compare_op);
#ifdef POLY_DEBUG			
	cout << "compare_reg is: " << compare_reg << endl;
#endif
	switch (this_op) {
		case sem::NE: {
			PPL::C_Polyhedron poly2 = res.poly;
			res.poly.add_constraint(res.lookup(compare_reg) <= -1);
			poly2.add_constraint(res.lookup(compare_reg) >= 1);
			res.poly.poly_hull_assign(poly2);
			break;
#ifdef POLY_DEBUG			
		cout << "NE!" << endl;
#endif
		}
		case sem::EQ:
#ifdef POLY_DEBUG			
		cout << "EQ!" << endl;
#endif
			res.poly.add_constraint(res.lookup(compare_reg) == 0);
			break;
		case sem::GE:
		case sem::UGE:
			res.poly.add_constraint(res.lookup(compare_reg) >= 0);
#ifdef POLY_DEBUG			
		cout << "(U)GE!" << endl;
#endif
			break;
		case sem::GT:
		case sem::UGT:
			res.poly.add_constraint(res.lookup(compare_reg) >= 1);
#ifdef POLY_DEBUG			
		cout << "(U)GT!" << endl;
#endif
			break;
		case sem::LE:
		case sem::ULE:
			res.poly.add_constraint(res.lookup(compare_reg) <= 0);
#ifdef POLY_DEBUG			
		cout << "(U)LE!" << endl;
#endif
			break;
		case sem::LT:
		case sem::ULT:
			res.poly.add_constraint(res.lookup(compare_reg) <= -1);
#ifdef POLY_DEBUG			
		cout << "(U)LT!" << endl;
#endif
			break;
		default:
			break;
	};
#ifdef POLY_DEBUG			
	cout << "empty? " << res.poly.is_empty() << endl;
#endif
	res.compare_reg = Ident();
	return res;
}
inline void PPLDomain::_partialMerge(PPL::C_Polyhedron &poly1, PPL::C_Polyhedron &poly2) const {
	PPL::C_Polyhedron *src = &poly2;
	if (poly1.space_dimension() > poly2.space_dimension()) {
		src = new PPL::C_Polyhedron(poly2);
		src->add_space_dimensions_and_embed(poly1.space_dimension() - poly2.space_dimension());
	} else if (poly2.space_dimension() > poly1.space_dimension()) {
		poly1.add_space_dimensions_and_embed(poly2.space_dimension() - poly1.space_dimension());
	}
	poly1.poly_hull_assign(*src);
	if (src != &poly2)
		delete src;
}

PPLDomain PPLDomain::onMerge(const PPLDomain& r, bool widen) {
	PPLDomain &l = *this;
	int axis = 0;
	ASSERT(!l.trash.countOnes());
	ASSERT(!r.trash.countOnes());
	if (r.poly.is_empty()) {
#ifdef POLY_DEBUG			
		cerr << "trivial join (r empty)" << endl;
#endif
		return l;
		}
	if (l.poly.is_empty()) {
#ifdef POLY_DEBUG			
		cerr << "trivial join (l empty)" << endl;
#endif
		return r;
	}

#ifdef POLY_DEBUG			
	if (widen) {
		cerr << "================= WIDENING ==================" << endl;
	} else cerr << "=================== JOIN ====================" << endl;
	cerr << "=== prepare phase ===" << endl;
	cerr "left hand term dimension: " << l.poly.space_dimension() << endl;
	l.poly.minimized_constraints().print();
	cerr << endl;
	display_loc_vars((PPLManager::t&)l);
	displayIdentMap(l);
	cout << endl;
	cerr "right hand term dimension: " << r.poly.space_dimension() << endl;
	r.poly.minimized_constraints().print();
	cerr << endl;
	display_loc_vars((PPLManager::t&)r);
	displayIdentMap(r);
#endif

	genstruct::HashTable<PPL::Constraint, elm::Pair<int,int>, HashCons> map;
	genstruct::HashTable<PPL::Constraint, int, HashCons> mapl_ptr;
	genstruct::HashTable<PPL::Constraint, int, HashCons> mapr_ptr;

	genstruct::HashTable<int,int> mapl;
	genstruct::HashTable<int,int> mapr;

	genstruct::HashTable<int,int> mapl2;
	genstruct::HashTable<int,int> mapr2;

	PPLDomain r1 = r;
	PPLDomain l1 = l;

	// mapl/mapr : on map tout les id SPECIAL vers une numerotation commune
#ifdef POLY_DEBUG			
	cerr << "Mapping common ancestors to common destination axis\n";
#endif
	for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(l.id2axis); it; it++) {
		const Ident &ident = (*it).fst;
		if (ident.getType() != Ident::ID_SPECIAL)
			continue;
		if (r1.id2axis.hasKey(ident)) {
			mapl.put((*it).snd, axis);
			mapr.put(r1.id2axis[ident], axis);
			axis++;
		}
	}

	// chopper les constraints des pointeurs sur L, mettre dans la hashmap 
	// on recupere dans mapl_ptr/mapr_ptr un mapping de contrainte vers numero de pointeur (ptr1, ptr2, etc)

#ifdef POLY_DEBUG			
	cerr << "Identifying address expressions appearing on both sides\n";	
#endif
	l1._indexPointersByExpr(mapl_ptr, mapl);
	r1._indexPointersByExpr(mapr_ptr, mapr);

	// mapl/mapr: on ajoute tout les pointeurs communs, on map vers une numerotation commune
#ifdef POLY_DEBUG			
	cout << "COMMON PTRs: " ;
#endif
	for (genstruct::HashTable<PPL::Constraint, int, HashCons>::PairIterator it(mapl_ptr); it; it++) {
		const PPL::Constraint &cons = (*it).fst;
		if (mapr_ptr.hasKey(cons)) {
			int ptrl = (*it).snd;
			int ptrr = mapr_ptr[cons];
			Variable l_addr = l1.lookup(Ident(ptrl, Ident::ID_MEM_ADDR));
			Variable l_val = l1.lookup(Ident(ptrl, Ident::ID_MEM_VAL));
			Variable r_addr = r1.lookup(Ident(ptrr, Ident::ID_MEM_ADDR));
			Variable r_val = r1.lookup(Ident(ptrr, Ident::ID_MEM_VAL));
			mapl[l_addr.id()] = axis;
			mapr[r_addr.id()] = axis;
			mapl[l_val.id()] = axis + 1;
			mapr[r_val.id()] = axis + 1;
#ifdef POLY_DEBUG			
			cout << "ptr" << ptrl << "/ptr" << ptrr << ", ";
#endif
			axis += 2;
		}
	}
#ifdef POLY_DEBUG			
	cout << endl;
#endif

	// maplregs/mapr : ajout dans map de tous les axes registres (temporaires ou non) de l/r vers une numerotation commune
	for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(l.id2axis); it; it++) {
		const Ident &ident = (*it).fst;
		if ((ident.getType() != Ident::ID_REG) && (ident.getType() != Ident::ID_LOOP))
			continue;
		if (r1.id2axis.hasKey(ident)) {
			mapl.put((*it).snd, axis);
			mapr.put(r1.id2axis[ident], axis);
			axis++;
		}
	}

	l1.doMap(PPLDomain::MapWithHash(mapl));
	r1.doMap(PPLDomain::MapWithHash(mapr)); 

	// Fin preparation
#ifdef POLY_DEBUG			
	cerr << "=== prepare done ===" << endl;
	cerr << "left hand term dimension: " << l1.poly.space_dimension() << endl;
	l1.poly.minimized_constraints().print();
	cerr << endl;
	display_loc_vars((PPLManager::t&)l1);
	displayIdentMap(l1);
	cout << endl;
	cerr << "right hand term dimension: " << r1.poly.space_dimension() << endl;
	r1.poly.minimized_constraints().print();
	cerr << endl;
	display_loc_vars((PPLManager::t&)r1);
	displayIdentMap(r1);

	cerr << "=== convex-hull phase ===" << endl;
#endif

	l1.poly.poly_hull_assign(r1.poly);
	if (widen) {
#ifdef POLY_DEBUG			
		cerr << "before widening: " << endl;
	cerr << "left hand term dimension: " << l1.poly.space_dimension() << endl;
	l1.poly.minimized_constraints().print();
	cerr << endl;
	display_loc_vars((PPLManager::t&)l1);
	displayIdentMap(l1);
	cout << endl;
	cerr << "right hand term dimension: " << r1.poly.space_dimension() << endl;
	r1.poly.minimized_constraints().print();
	cerr << endl;
	display_loc_vars((PPLManager::t&)r1);
	displayIdentMap(r1);
	cerr << endl;
	cerr << "---" << endl;
#endif
	PPL::Constraint_System dummy;
#ifdef POLY_DEBUG
		ASSERT(l1.poly.contains(r1.poly));
#endif
		l1.poly.bounded_BHRZ03_extrapolation_assign(r1.poly, dummy);
	}
	l1.num_axis = l1.poly.space_dimension();
#ifdef POLY_DEBUG			
	cerr << "=== all done. ===" << endl;
	cerr << "result dimension: " << l1.poly.space_dimension() << endl;
	l1.poly.minimized_constraints().print();
	cerr << endl;
	display_loc_vars((PPLManager::t&)l1);
	displayIdentMap(l1);
	cerr << "=====================================" << endl;
#endif

	if (r1.hasFilter()) {
		ASSERT(false);
	}
	return l1;
}

PPLDomain PPLDomain::onSemInst(sem::inst si, int instaddr) {
        PPLDomain s_out = *this;
/* 		ASSERT(!hasFilter() || (si.op == sem::BRANCH)); */

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
					s_out.doScratch(id);
				}
		        break;
		case sem::SETP:         // page(d) <- cst
		        break;
		case sem::SETI:         // d <- cst
		{  
				sem::reg_t dest = si.d();
				Ident id(dest, Ident::ID_REG);
				Variable v = s_out.varNew(id, true);
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
				Variable v = s_out.varNew(id, true);
				bool setToTop = false;
				if (!s_out.hasIdent(id2)) {
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
		        if (!s_out.hasIdent(id1)) {
					cout << "[WARN] Identifier " << id1 << " used, but not defined! Set to TOP" << endl;
					setToTop = true;
		        } else if (!s_out.hasIdent(id2)) {
					cout << "[WARN] Identifier " << id2 << " used, but not defined! Set to TOP" << endl;
					setToTop = true;
		        } 
				if (!setToTop) {
					Variable vs1 = s_out.lookup(id1);
					Variable vs2 = s_out.lookup(id2);

					Variable v = s_out.varNew(id, true);
					s_out._doBinaryOp(si.op, &v, &vs1, &vs2);
				} else s_out.varNew(id, true);
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
				PPL::C_Polyhedron all_writes = PPL::C_Polyhedron(0, PPL::EMPTY);

				Ident id_new_addr, id_new_val;
				s_out.varCreatePtr(id_new_addr, id_new_val);

				Variable v_new_addr = s_out.varNew(id_new_addr);
				Variable v_new_val = s_out.varNew(id_new_val);

				Ident id_reg_src(src, Ident::ID_REG);
				Ident id_reg_addr(addr, Ident::ID_REG);

				Variable v_reg_src = s_out.lookup(id_reg_src, true);
				Variable v_reg_addr = s_out.lookup(id_reg_addr, true);

				/*
				 * Teste si l'adresse du store peut aliaser SSP+4
				 */

				Ident id_frame(Ident::ID_START_SP, Ident::ID_SPECIAL);
				Variable var_ssp = s_out.lookup(id_frame);
				if (s_out.mayEqual(v_reg_addr, var_ssp, 4)) {
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
						PPLDomain tmp(s_out);
						tmp.doMapPoly(PPLDomain::MapWithHash(map));
						tmp.poly.minimized_constraints().print();
						cout << endl;
					}
					if ((p.fst.getType() == Ident::ID_MEM_ADDR) && (p.fst != id_new_addr)) {
						Variable v_ex_addr = s_out.getVar(p.snd);
						/* Store address may overlap with existing pointer */
						if (s_out.mayEqual(v_reg_addr, v_ex_addr)) {
							had_potential_match = true;
							Ident id_ex_val(p.fst.getId(), Ident::ID_MEM_VAL);
							Variable v_ex_val = s_out.lookup(id_ex_val);
							if (s_out.mustEqual(v_reg_addr, v_ex_addr)) {
#ifdef POLY_DEBUG
								 /* Our abstract domain should not have aliases, therefore an 
								 * exact match should not happen more than once. */
								ASSERT(!had_exact_match); 
								cout << "Address " << p.fst << " and " << id_reg_addr << " are equal (replacing)." << endl;
#endif								
								had_exact_match = true;
								s_out.varKill(v_ex_addr);
								s_out.varKill(v_ex_val);
							} else {
#ifdef POLY_DEBUG
								cout << "Address " << p.fst << " and " << id_reg_addr << " maybe equal (joining)." << endl;
#endif								
								PPL::C_Polyhedron this_write = s_out.poly;
								this_write.unconstrain(v_ex_val);
								this_write.add_constraint(v_reg_addr == v_ex_addr);
								this_write.add_constraint(v_reg_src == v_ex_val);
								_partialMerge(all_writes, this_write);
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
					_partialMerge(s_out.poly, all_writes);
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
				Variable vdst = s_out.varNew(id_dst, true);
				bool found = false;
				
				// Look for matching ID_MEM_ADDR identifier 
				for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++)
				{
					elm::Pair<Ident, int> p = *it;
					Variable vsnd = s_out.getVar(p.snd);
					Ident id_val(p.fst.getId(), Ident::ID_MEM_VAL);
					if (p.fst.getType() == Ident::ID_MEM_ADDR) {
#ifdef POLY_DEBUG			
						if (may_be_equal(s_in, vaddr, vsnd)) {
							cout << "Candidate: " << p.fst << endl;
						}
#endif
						if (mustEqual(vaddr, vsnd)) {
#ifdef POLY_DEBUG			
							cout << "Matched load source: " << p.fst << endl;
#endif
							Variable v_val = s_out.lookup(id_val);
							s_out.poly.add_constraint(vdst == v_val);
							found = true;
							break;

						}
						if (mustEqual(vaddr, vsnd)) {
							cout << "Exact!" << endl;
						}
					}
				}

				if (!found) {
#ifdef POLY_DEBUG			
					cout << "Not found, creating new unconstrained ptr..." << endl;
#endif
					Ident addr, val;
					s_out.varCreatePtr(addr, val);
					Variable dummy_addr = s_out.varNew(addr);
					Variable dummy_val = s_out.varNew(val);
					s_out.poly.add_constraint(vdst == dummy_val);
					s_out.poly.add_constraint(vaddr == dummy_addr);
				}
			}
			break;
		case sem::NOT:          // d <- ~a
			break;
		case sem:: IF: {
				Ident id(si.sr(), Ident::ID_REG);
				if (s_out.hasIdent(id)) {
					
					s_out.compare_reg = id;
					s_out.compare_op = si.cond();
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
	s_out.doIntegerWrap();
	// cout << "apres wrap" << s_out << endl;
	return s_out;
}

p::feature POLY_ANALYSIS_FEATURE("otawa::poly::POLY_ANALYSIS_FEATURE", new Maker<PolyAnalysis>());


void PPLDomain::_doFreeAxis(int axis) {
	ASSERT(axis2id[axis].getType() != Ident::ID_INVALID);
	Ident old = axis2id[axis];
#ifdef POLY_DEBUG			
	cout << "L'axe " << Variable(axis) << ", qui etait alloue a l'identificateur " << old << ", est marque pour etre supprime. " << endl;
#endif
	trash.set(axis);
}

int PPLDomain::_doAllocAxis(const Ident &ident, bool allow_replace) { 
	ASSERT(num_axis < trash.size())
	ASSERT(allow_replace || !id2axis.hasKey(ident));
	if (id2axis.hasKey(ident)) {
		int axis = id2axis[ident];
#ifdef POLY_DEBUG			
		cout << "L'identificateur " << ident << " etait deja associe a l'axe " << Variable(axis) << endl;
#endif
		_doFreeAxis(axis);
	}
	id2axis[ident] = num_axis;
	if (axis2id.length() <= num_axis) {
		axis2id.setLength(num_axis + 1);
	}
	axis2id[num_axis] = ident;
#ifdef POLY_DEBUG			
	cout << "Nouvel axe " << Variable(num_axis) << " alloue pour l'identificateur " << ident << endl;
#endif
	num_axis++;
	if (poly.space_dimension() < num_axis) {
		poly.add_space_dimensions_and_embed(num_axis - poly.space_dimension());
	}
	return num_axis - 1;
}

Variable PPLDomain::lookup(const Ident &ident, bool allow_varNew) {
	if (allow_varNew && !hasIdent(ident)) {
		return varNew(ident, false);
	}
	return Variable(id2axis[ident]);
}

void PPLDomain::varCreatePtr(Ident &addr, Ident &val) {
	addr = Ident(mem_ref, Ident::ID_MEM_ADDR);
	val = Ident(mem_ref, Ident::ID_MEM_VAL);
	mem_ref++;
}

bool PPLDomain::equals(const PPLDomain &b) const {
	if (!((poly == b.poly) &&
			(id2axis.count() == b.id2axis.count()))) {
		return false;
	}
	if (trash != b.trash)
		return false;

	if (compare_reg != b.compare_reg)
		return false;

	// FIXME TODO not correct because of same memory location having different names
	for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(id2axis); it; it++) {
		const Pair<Ident, int> &p = *it;
		if ((p.fst.getType() == Ident::ID_MEM_VAL) || (p.fst.getType() == Ident::ID_MEM_ADDR))
			continue;
		if (!b.id2axis.hasKey(p.fst))
			return false;
		int b_axis = b.id2axis[p.fst];
		if (b_axis != p.snd)
			return false;
	}
	return true;
}

Variable PPLDomain::getVar(int axis) {
	return Variable(axis);
}

Variable PPLDomain::varNew(const Ident &ident, bool allow_replace) {
	return Variable(_doAllocAxis(ident, allow_replace));
}

void PPLDomain::varRename(const Ident &ident, const Ident &newident, bool allow_replace) {
	int axis;
	ASSERT(allow_replace || !id2axis.hasKey(newident));
	if (id2axis.hasKey(newident)) {
		axis = id2axis[newident];
#ifdef POLY_DEBUG			
		cout << "L'identificateur " << ident << " etait deja associe a l'axe " << Variable(axis) << endl;
#endif
		_doFreeAxis(axis);
	}
	axis = id2axis[ident];

	axis2id[axis] = newident;
	id2axis[newident] = axis;
	id2axis.remove(ident);
#ifdef POLY_DEBUG			
	cout << "Renommage de l'identificateur " << ident << " en " << newident << " sur l'axe " << Variable(axis) << endl;
#endif
}

bool PPLDomain::hasIdent(const Ident &ident) {
	return id2axis.hasKey(ident);
}
bool PPLDomain::hasVar(int axis) {
	return axis2id[axis].getType() != Ident::ID_INVALID;
}


Identifier<int> LOC_VAR_SIZE("otawa::poly::LOC_VAR_SIZE", 4);
Identifier<int> NUM_LOC_VARS("otawa::poly::NUM_LOC_VARS", 8);
Identifier<int> MAX_AXIS("otawa::poly::MAX_AXIS", 512);


} }	// otawa::poly
