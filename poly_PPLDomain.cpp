#include <otawa/otawa.h>
#include <otawa/util/WideningListener.h>
#include <otawa/util/HalfAbsInt.h>
#include <otawa/dfa/FastState.h>
#include <otawa/util/WideningFixPoint.h>
#include <otawa/poly/features.h>
#include <otawa/flowfact/features.h>
#include <otawa/cfg/Edge.h>
#include <otawa/graph/Graph.h>
#include <otawa/dfa/ai.h>
#include <ctime>
#include <ppl.hh>

#include "include/PPLDomain.h"
#include "include/PPLManager.h"
#include "include/PolyAnalysis.h"

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
	} else { kind = 2;
}
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
		if (number != 0) {
			snprintf(name, sizeof(name), "%c%u", letter, number);
		} else {
			snprintf(name, sizeof(name), "%c", letter);
		}
		o << name;
		return o;
}  

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
		} else { todel.add(it.key());
}
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

using PVAR = Variable *;

void PPLDomain::doIntegerWrap() {
	// TODO(clement): integer wrap not supported yet
	
}

void PPLDomain::doFinalizeUpdate() {
	_sanityChecks();
#ifdef POLY_DEBUG			
	if (PPLDomain::trash.countOnes() == 0) {
		cout << "Nothing to clean" << endl;
		_sanityChecks();
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
}

void PPLDomain::displayLocVars() {
	const PropList _props;
	PPL::Constraint_System mcons = poly.minimized_constraints();
	if (isBottom()) {
		cout << "diplay_loc_vars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	Variable ssp = getVar(id_ssp);
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
			Variable vsnd = Variable(p.snd);
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
					PPL::Coefficient num, den;
					getConstant(idval, num, den, true);
					cout <<  " (aka " << idval << ")";
					break;
				}
			}
		}
		if (!found) {
			cout << "N/A" ;
		}
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
	Variable v = getVar(id);
	return getConstant(v, cst_n, cst_d, display);
}
void PPLDomain::getRange(Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display) {
#ifdef POLY_DEBUG			
	cout << "get_range(" << id << ") = ";
#endif
	Variable v = getVar(id);
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
			if (binf_d != 1) {
				gmp_printf("/%Zd", &PPL::raw_value(binf_d));
			}
		} else { 
			gmp_printf("-inf");
		}
		gmp_printf("..");
		if (bsup_d != 0) {
			gmp_printf("%Zd", &PPL::raw_value(bsup_n));
			if (bsup_d != 1) {
				gmp_printf("/%Zd", &PPL::raw_value(bsup_d));
			}
		} else { 
			gmp_printf("+inf");
		}
		gmp_printf("]");
		fflush(stdout);
	}
}

void PPLDomain::doScratch(Ident &id) {
	if (hasIdent(id)) { 
		return;
}
	
#ifdef POLY_DEBUG			
	cout << "Before cylindrification: " << *this << endl;
#endif
	Variable v = getVar(id);
	poly.unconstrain(v);
#ifdef POLY_DEBUG			
	cout << "Scratch " << id << " (axis " << v << ")" << endl;
	cout << "After cylindrification: " << *this << endl;
#endif
}

bool PPLDomain::mayAlias(const Variable &v1, const Variable &v2, int offset) const {
	return !poly.relation_with(v1 == v2 + offset).implies(PPL::Poly_Con_Relation::is_disjoint()); 
}

bool PPLDomain::mustAlias(const Variable &v1, const Variable &v2, int offset) const {
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
				if (b)
					poly.add_constraint(*v * cst_d == *vs1 * cst_n);
			} 
			break;
		case sem::MULH: // d <- (a * b) >> bitlength(d)
			b = getConstant(*vs1, cst_n, cst_d);
			if (b) {
				poly.add_constraint(*v * cst_d == *vs2 * cst_n);
			} else {
				b = getConstant(*vs2, cst_n, cst_d);
				if (b)
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
	Variable v = s_out.getVar(id);
	if (bound >= 0) {
		s_out.poly.add_constraint(v <= bound);
}
    s_out.varKill(v);
	if (s_out.poly.is_empty()) {
#ifdef POLY_DEBUG			
		cout << "State is Bottom after onLoopExit (will not propagate states to exit-edges)" << endl;
#endif
		return PPLDomain(); // bottom
	}
	return s_out;
}


PPLDomain PPLDomain::onLoopIter(int loop, bool  /*inner*/) {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.hasIdent(id)); /* You are supposed to be already inside the loop when you call onLoopIter() */ 
	Variable v_old = s_out.getVar(id);
	Variable v_new = s_out.varNew(id, true);
	s_out.poly.add_constraint(v_new == v_old + 1);

	return s_out;
}

PPLDomain PPLDomain::onLoopEntry(int loop, bool  /*inner*/) {
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
		if (!c.is_equality()) {
			continue;
}
		if (c.coefficient(Variable(axis)) != 0) {
			return new PPL::Constraint(c);
		}
	}
	return nullptr;
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
		if (cons != nullptr) {
			map_ptr[*cons] = (*it).fst.getId();
			delete cons;
		}
		map_regs.remove((*it).snd);
	}
}
}

PPLDomain PPLDomain::onBranch(bool taken) {
	if (isBottom()) {
		return *this;
}
	sem::cond_t this_op;
	ASSERT(hasFilter());
	PPLDomain res = *this;
	this_op = taken ? compare_op : sem::invert(compare_op);
#ifdef POLY_DEBUG			
	cout << "Filtering, compare_reg is: " << compare_reg << ", compare_op is: " << compare_op << ", taken=" << taken <<  endl;
#endif
	switch (this_op) {
		case sem::NE: {
			PPL::C_Polyhedron poly2 = res.poly;
			res.poly.add_constraint(res.getVar(compare_reg) <= -1);
			poly2.add_constraint(res.getVar(compare_reg) >= 1);
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
			res.poly.add_constraint(res.getVar(compare_reg) == 0);
			break;
		case sem::GE:
		case sem::UGE:
			res.poly.add_constraint(res.getVar(compare_reg) >= 0);
#ifdef POLY_DEBUG			
		cout << "(U)GE!" << endl;
#endif
			break;
		case sem::GT:
		case sem::UGT:
			res.poly.add_constraint(res.getVar(compare_reg) >= 1);
#ifdef POLY_DEBUG			
		cout << "(U)GT!" << endl;
#endif
			break;
		case sem::LE:
		case sem::ULE:
			res.poly.add_constraint(res.getVar(compare_reg) <= 0);
#ifdef POLY_DEBUG			
		cout << "(U)LE!" << endl;
#endif
			break;
		case sem::LT:
		case sem::ULT:
			res.poly.add_constraint(res.getVar(compare_reg) <= -1);
#ifdef POLY_DEBUG			
		cout << "(U)LT!" << endl;
#endif
			break;
		default:
			break;
	};
	res.compare_reg = Ident();
	if (res.poly.is_empty()) {
#ifdef POLY_DEBUG			
		cout << "State is Bottom after Filtering (will not propagate states to branch destination)" << endl;
#endif
		return PPLDomain(); // bottom
	}
	return res;
}

void PPLDomain::_extendAndHull(PPL::C_Polyhedron &poly1, PPL::C_Polyhedron &poly2) const {
	PPL::C_Polyhedron *src = &poly2;
	if (poly1.space_dimension() > poly2.space_dimension()) {
		src = new PPL::C_Polyhedron(poly2);
		src->add_space_dimensions_and_embed(poly1.space_dimension() - poly2.space_dimension());
	} else if (poly2.space_dimension() > poly1.space_dimension()) {
		poly1.add_space_dimensions_and_embed(poly2.space_dimension() - poly1.space_dimension());
	}
	poly1.poly_hull_assign(*src);
	if (src != &poly2) {
		delete src;
}
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
	cerr << "left hand term dimension: " << l.poly.space_dimension() << endl;
	l.poly.minimized_constraints().print();
	cerr << endl;
	displayLocVars();
	displayIdentMap();
	cout << endl;
	cerr << "right hand term dimension: " << r.poly.space_dimension() << endl;
	r.poly.minimized_constraints().print();
	cerr << endl;
	displayLocVars();
	displayIdentMap();
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
		if (ident.getType() != Ident::ID_SPECIAL) {
			continue;
}
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
			Variable l_addr = l1.getVar(Ident(ptrl, Ident::ID_MEM_ADDR));
			Variable l_val = l1.getVar(Ident(ptrl, Ident::ID_MEM_VAL));
			Variable r_addr = r1.getVar(Ident(ptrr, Ident::ID_MEM_ADDR));
			Variable r_val = r1.getVar(Ident(ptrr, Ident::ID_MEM_VAL));
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
		if ((ident.getType() != Ident::ID_REG) && (ident.getType() != Ident::ID_LOOP)) {
			continue;
}
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
	displayLocVars();
	displayIdentMap();
	cout << endl;
	cerr << "right hand term dimension: " << r1.poly.space_dimension() << endl;
	r1.poly.minimized_constraints().print();
	cerr << endl;
	displayLocVars();
	displayIdentMap();

	cerr << "=== convex-hull phase ===" << endl;
#endif

	l1.poly.poly_hull_assign(r1.poly);
	if (widen) {
#ifdef POLY_DEBUG			
		cerr << "before widening: " << endl;
	cerr << "left hand term dimension: " << l1.poly.space_dimension() << endl;
	l1.poly.minimized_constraints().print();
	cerr << endl;
	displayLocVars();
	displayIdentMap();
	cout << endl;
	cerr << "right hand term dimension: " << r1.poly.space_dimension() << endl;
	r1.poly.minimized_constraints().print();
	cerr << endl;
	displayLocVars();
	displayIdentMap();
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
	displayLocVars();
	displayIdentMap();
	cerr << "=====================================" << endl;
#endif

	if (r1.hasFilter()) {
		ASSERT(false);
	}
	return l1;
}


Variable PPLDomain::memReplace(const Variable& address , const Variable& valueSource) {
	const Ident &idOldAddress = getIdent(address);
	const Ident &idOldValue = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL);
	Variable oldValue = getVar(idOldValue);

	Ident idNewAddress, idNewValue;
	varCreatePtr(idNewAddress, idNewValue);
	const Variable &newAddress = varNew(idNewAddress);
	const Variable &newValue = varNew(idNewValue);

	doNewConstraint(address == newAddress);
	doNewConstraint(newValue == valueSource);

	varKill(oldValue);
	varKill(address);
	return newAddress;
}

Variable PPLDomain::memCreate(const Variable& address, const Variable& valueSource) {
	Ident idNewAddress, idNewValue;
	varCreatePtr(idNewAddress, idNewValue);
	const Variable &newAddress = varNew(idNewAddress);
	const Variable &newValue = varNew(idNewValue);

	doNewConstraint(newValue == valueSource);
	doNewConstraint(newAddress == address);
	return newAddress;
}

Variable PPLDomain::memMerge(const Variable& address, const Variable& newValue) {
	const Ident &idOldAddress = getIdent(address);
	const Ident &idOldValue = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL);
	const Variable oldValue = getVar(idOldValue);

	PPLDomain tempState = *this;

	Variable newAddr1 = tempState.memReplace(address, newValue);
	Variable newAddr2 = memReplace(address, oldValue);

	ASSERT(axis2id == tempState.axis2id);
	ASSERT(newAddr1.id() == newAddr2.id());
	_extendAndHull(poly, tempState.poly);
	return newAddr1;
}

PPLDomain PPLDomain::onSemInst(sem::inst si, int instaddr) {
        PPLDomain s_out = *this;
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
				int32_t cst = si.cst();
				s_out.poly.add_constraint(v == cst);
				break;
		        
		}
		case sem::SET:          // d <- a
		{
				sem::reg_t source = si.a();
				Ident id2(source, Ident::ID_REG);
				if (!s_out.hasIdent(id2)) {
					cout << "[WARN] Identifier " << id2 << " used, but not defined!" << endl;
					break;
				}
				Variable vs = s_out.getVar(id2);

				sem::reg_t dest = si.d();
				Ident id(dest, Ident::ID_REG);
				Variable v = s_out.varNew(id, true);

				s_out.poly.add_constraint(v == vs);
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
		
				/*
				 * Binary operation:
				 *
				 * This is common code, responsible for preparing the input and output variables for the operation.
				 * The constraint(s) to represent the actual binary operation are added by PPLDomain::_doBinaryOp().
				 */	
		        sem::reg_t op1 = si.a();
		        sem::reg_t op2 = si.b();
				Ident id1(op1, Ident::ID_REG);
				Ident id2(op2, Ident::ID_REG);
		        if (!s_out.hasIdent(id1)) {
					cout << "[WARN] Identifier " << id1 << " used, but not defined!" << endl;
					break;
		        } else if (!s_out.hasIdent(id2)) {
					cout << "[WARN] Identifier " << id2 << " used, but not defined!" << endl;
					break;
		        } 

		        sem::reg_t dest = si.d();
				Ident id(dest, Ident::ID_REG);

				Variable vs1 = s_out.getVar(id1);
				Variable vs2 = s_out.getVar(id2);
				Variable v = s_out.varNew(id, true);
				s_out._doBinaryOp(si.op, &v, &vs1, &vs2);
		        break;

		}
		case sem::STORE:                // MEMb(a) <- d
		{

				sem::reg_t addr = si.a();
				Ident idStoreAddr(addr, Ident::ID_REG);
				Variable storeAddr = s_out.getVar(idStoreAddr, true);

		        sem::reg_t src = si.d();
				Ident idStoreValue(src, Ident::ID_REG);
				Variable storeValue = s_out.getVar(idStoreValue, true);

				Ident idEquiv; /* Identifier equivalent to the store addr, if any. */
				elm::genstruct::Vector<Ident> overlaps; /* List of identifiers overlapping the store addr. */

				for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++) {
					const Ident &idCurrent = (*it).fst;
					if (idCurrent.getType() == Ident::ID_MEM_ADDR) {
						const Variable &current = Variable((*it).snd);
						if (s_out.mustAlias(current, storeAddr)) {
							ASSERT(idEquiv.getType() == Ident::ID_INVALID); /* must be unique */
							idEquiv = idCurrent;
						} else if (s_out.mayAlias(current, storeAddr)) {
							overlaps.add(idCurrent);
						}
					}
				}

				if (idEquiv.getType() != Ident::ID_INVALID) {
					/* Replace existing equivalent abstract location */
					const Variable &equiv = s_out.getVar(idEquiv);
#ifdef POLY_DEBUG
					cout << "STORE: Replacing existing location " << equiv << " with new value." << endl;
#endif					
					s_out.memReplace(equiv, storeValue);
				} else {
					/* No exact match: Create new abstract location */
#ifdef POLY_DEBUG
					cout << "STORE: Creating new memory location. " << endl;
#endif					
					s_out.memCreate(storeAddr, storeValue);
				}

				/* Merge with overlapping abstract locations */
				for (elm::genstruct::Vector<Ident>::Iterator it(overlaps); it; it++) {
					const Variable &overlap = s_out.getVar((*it));
#ifdef POLY_DEBUG
					cout << "STORE: Merging existing location " << (*it) << " with new value." << endl;
#endif					
					s_out.memMerge(overlap, storeValue);
				}

			break;
		}
		case sem::LOAD:         // d <- MEMb(a)
			{
		        sem::reg_t dst = si.d();
		        sem::reg_t addr = si.a();
				Ident idLoadReg(dst, Ident::ID_REG);
				Ident idLoadAddr(addr, Ident::ID_REG);
				Variable loadAddr = s_out.getVar(idLoadAddr);
				Variable loadReg = s_out.varNew(idLoadReg, true);
				bool found = false;

				/*
				 * Looking for existing abstract location equivalent to load address.
				 */
				for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++)
				{
					if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
						Variable current = Variable((*it).snd);

						if (mustAlias(loadAddr, current)) {
#ifdef POLY_DEBUG			
							cout << "LOAD: Found equivalent abstract location: " << (*it).fst << endl;
#endif
							Ident idExistingValue((*it).fst.getId(), Ident::ID_MEM_VAL);
							Variable existingValue = s_out.getVar(idExistingValue);
							s_out.doNewConstraint(loadReg == existingValue);
							found = true;
							break;
						}
					}
				}

				if (!found) {
#ifdef POLY_DEBUG			
					cout << "LOAD: Not found, creating new unconstrained ptr..." << endl;
#endif
					s_out.memCreate(loadAddr, loadReg);
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



bound_t PPLDomain::getLoopBound(int loopId) {
	if (isBottom())
		return bound_t::UNREACHABLE;
	Ident id(loopId, Ident::ID_LOOP);
	PPL::Coefficient binf_n, binf_d, bsup_n, bsup_d;
	getRange(id, binf_n, binf_d, bsup_n, bsup_d);
	if ( PPL::raw_value(bsup_d).get_ui() != 0) {
		return (bound_t) (PPL::raw_value(bsup_n).get_ui() / PPL::raw_value(bsup_d).get_ui());
	}
	return bound_t::UNBOUNDED;
}

void PPLDomain::_doFreeAxis(int axis) {
	ASSERT(axis2id[axis].getType() != Ident::ID_INVALID);
#ifdef POLY_DEBUG			
	cout << "L'axe " << Variable(axis) << ", qui etait alloue a l'identificateur " << axis2id[axis] << ", est marque pour etre supprime. " << endl;
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

Variable PPLDomain::getVar(const Ident &ident, bool allow_varNew) {
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
	if (trash != b.trash) {
		return false;
}

	if (compare_reg != b.compare_reg) {
		return false;
}

	// FIXME TODO not correct because of same memory location having different names
	for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(id2axis); it; it++) {
		const Pair<Ident, int> &p = *it;
		if ((p.fst.getType() == Ident::ID_MEM_VAL) || (p.fst.getType() == Ident::ID_MEM_ADDR)) {
			continue;
}
		if (!b.id2axis.hasKey(p.fst)) {
			return false;
}
		int b_axis = b.id2axis[p.fst];
		if (b_axis != p.snd) {
			return false;
}
	}
	return true;
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

p::feature POLY_ANALYSIS_FEATURE("otawa::poly::POLY_ANALYSIS_FEATURE", new Maker<PolyAnalysis>());

Identifier<int> LOC_VAR_SIZE("otawa::poly::LOC_VAR_SIZE", 4);
Identifier<int> NUM_LOC_VARS("otawa::poly::NUM_LOC_VARS", 8);
Identifier<int> MAX_AXIS("otawa::poly::MAX_AXIS", 512);


}  // namespace poly
} // namespace otawa
