#include <otawa/otawa.h>
#include <otawa/util/WideningListener.h>
#include <otawa/util/HalfAbsInt.h>
#include <otawa/dfa/FastState.h>
#include <otawa/poly/features.h>
#include <otawa/util/WideningFixPoint.h>
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

void Ident::print(io::Output &out) const {
	switch(_type) {
		case ID_REG:
			if (_id < 0) {
				out << "T" << -_id;
			} else {
				out << "R" << _id;
			}
			break;
		case ID_MEM_ADDR:
			out << "ptr" << _id;
			break;
		case ID_MEM_VAL:
			out << "*ptr" << _id;
			break;
		case ID_LOOP:
			out << "bound" << _id;
			break;
		case ID_SPECIAL:
			switch(_id) {
				case ID_START_FP:
					out << "START_FP";
					break;
				case ID_START_SP:
					out << "START_SP";
					break;
				case ID_START_LR:
					out << "START_LR";
					break;
				default:
					out << "[SPECIAL " << _id << "]";
					break;
			}
			break;
		default:
			out << "[ID=" << _id << ", TYPE=" << int(_type) << "]";
			break;
	};
}

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

void PPLDomain::print(io::Output & out) const {
	static char buf[64];

	if (isBottom()) {
		out << "BOTTOM";
		return;
	}

	PPL::Constraint_System cons = poly.minimized_constraints();
	int ncons = 0;

	out << "Constraints: ";;
	for (PPL::Constraint_System::const_iterator it = cons.begin(); it != cons.end(); it++, ncons++) {
		const PPL::Constraint &c = *it;

		for (PPL::dimension_type i = 0; i < cons.space_dimension(); i++) {
			const PPL::Coefficient &coef = c.coefficient(Variable(i));

			if (coef != 0) {
				Variable v(i);

				if (coef == -1) {
					out << "- ";
				} else if (coef != 1) { 
					gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(coef));
					buf[sizeof(buf)-1] = 0;
					out << buf << ".";
				}

				if (isVarMapped(v)) {
					out << getIdent(v);
				} else {
					out << v;
				}

				out << " ";
			}
		}

		if (c.is_equality()) {
			out << "= ";
		} else { 
			out << ">= ";
		}

		const PPL::Coefficient &cst = -c.inhomogeneous_term(); // Constraints form is: coef*X + ... + cst = 0
		gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(cst));
		buf[sizeof(buf)-1] = 0;
		out << buf;

		out << "; ";;
	}
	out << endl;

	out << "Space dimension: " << poly.space_dimension() << ", Constraints count: " << ncons << endl;
	displayIdentMap(out);
	displayLocVars(out);
	out << endl;
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

	if (compare_op != b.compare_op ) {
		return false;
	}

	// TODO(clement) should return true if there exists a substitution that makes the two states equivalent
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


void PPLDomain::displayLocVars(io::Output &out) const {
	const PropList _props;
	PPL::Constraint_System mcons = poly.minimized_constraints();
	if (isBottom()) {
		out << "diplay_loc_vars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	Variable ssp = getVar(id_ssp);
	out << "Local variables: " << endl;
	for (int i = 0 ; i < NUM_LOC_VARS(_props)*LOC_VAR_SIZE(_props); i += LOC_VAR_SIZE(_props)) {
		PPL::Constraint_System cons = mcons;
		Variable v(num_axis);
		cons.insert(v == ssp - i - LOC_VAR_SIZE(_props));
		out << " [SP - " << hex(i) << "] == ";
		bool found = false;
		for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++)
		{
			elm::Pair<Ident, int> p = *it;
			Variable vsnd = Variable(p.snd);
			if (p.fst.getType() == Ident::ID_MEM_ADDR) {
				PPL::C_Polyhedron poly(cons);
				PPL::Coefficient infNumAddr, infDenAddr, supNumAddr, supDenAddr;
				bool maximum, minimum;
				poly.maximize(v - vsnd, supNumAddr, supDenAddr, maximum);
				poly.minimize(v - vsnd, infNumAddr, infDenAddr, minimum);
				if ((supNumAddr == 0) && (infNumAddr == 0) && (supDenAddr != 0) && (infDenAddr != 0)) {
					found = true;
					Ident idval(p.fst.getId(), Ident::ID_MEM_VAL);
					PPL::Coefficient supNumVal, supDenVal, infNumVal, infDenVal;
					getRange(idval, infNumVal, infDenVal, supNumVal, supDenVal);

					if (infDenVal == 0) {
						out << "]-∞";
					} else {
						out << "[";
						displayFrac(out, infNumVal, infDenVal);
					}
					out << ";";
					if (supDenVal == 0) {
						out << "+∞[";
					} else {
						displayFrac(out, supNumVal, supDenVal);
						out << "]";
					}

					out <<  " (aka " << idval << ")";
					break;
				}
			}
		}
		if (!found) {
			out << "N/A" ;
		}
		out << endl;
	}

}

bool PPLDomain::getConstant(const Variable &var, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d) const {
	PPL::Coefficient binf_d, binf_n, bsup_d, bsup_n;
	getRange(var, bsup_n, bsup_d, binf_n, binf_d);
	if ((binf_d == bsup_d) && (binf_n == bsup_n) && (binf_d != 0)) {
		cst_n = binf_n;
		cst_d = binf_d;
		return true;
	}
	return false;
}
bool PPLDomain::getConstant(const Ident &id, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d) const {
	Variable v = getVar(id);
	return getConstant(v, cst_n, cst_d);
}
void PPLDomain::getRange(const Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d) const {
	Variable v = getVar(id);
	getRange(v, binf_n, binf_d, bsup_n, bsup_d);
}


void PPLDomain::displayFrac(io::Output &out, const PPL::Coefficient &num, const PPL::Coefficient &den) const {
	static char buf[16];
	ASSERT(den != 0);
	gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(num));
	buf[sizeof(buf) - 1] = 0;
	out << buf;
	if (den != 1) {
		gmp_snprintf(buf, sizeof(buf), "/%Zd", &PPL::raw_value(den));
		buf[sizeof(buf) - 1] = 0;
		out << buf;
	}
}

void PPLDomain::getRange(const Variable &var, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d) const {
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
}

bool PPLDomain::mayAlias(const Variable &v1, const Variable &v2) const {
	return !poly.relation_with(v1 == v2).implies(PPL::Poly_Con_Relation::is_disjoint()); 
}

bool PPLDomain::mustAlias(const Variable &v1, const Variable &v2) const {
	return poly.relation_with(v1 == v2).implies(PPL::Poly_Con_Relation::is_included()); 
}



void PPLDomain::displayIdentMap(io::Output &out) const {
	out << "Mapping: " ;
	for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(id2axis); it; it++) {
		const Ident &ident = (*it).fst;
		out << ident << ":" << Variable((*it).snd) << ", ";
	}
	out << endl;
}

bound_t PPLDomain::getLoopBound(int loopId) const {
	if (isBottom()) {
		return bound_t::UNREACHABLE;
}
	Ident id(loopId, Ident::ID_LOOP);
	PPL::Coefficient binf_n, binf_d, bsup_n, bsup_d;
	getRange(id, binf_n, binf_d, bsup_n, bsup_d);
	if ( PPL::raw_value(bsup_d).get_ui() != 0) {
		return static_cast<bound_t> (PPL::raw_value(bsup_n).get_ui() / PPL::raw_value(bsup_d).get_ui());
	}
	return bound_t::UNBOUNDED;
}

PPLDomain PPLDomain::onLoopExit(int loop, int bound) const {
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


PPLDomain PPLDomain::onLoopIter(int loop) const {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.hasIdent(id)); /* You are supposed to be already inside the loop when you call onLoopIter() */ 
	Variable v_old = s_out.getVar(id);
	Variable v_new = s_out.varNew(id, true);
	s_out.poly.add_constraint(v_new == v_old + 1);

	return s_out;
}

PPLDomain PPLDomain::onLoopEntry(int loop) const {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	Variable v = s_out.varNew(id, true);
	s_out.poly.add_constraint(v == 0);
	return s_out;
}


PPLDomain PPLDomain::onBranch(bool taken) const {
	if (isBottom()) {
		return PPLDomain();
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
	if (res.isBottom()) {
#ifdef POLY_DEBUG			
		cout << "State is Bottom after Filtering (will not propagate states to branch destination)" << endl;
#endif
		return PPLDomain(); // bottom
	}
	return res;
}


PPLDomain PPLDomain::onMerge(const PPLDomain& r, bool widen) const {
	/*
	 * The merge works in two phases:
	 *
	 * 1. Unification phase: we perform a substitution (variable renumbering), so that variables representing the same object (i.e.
	 * register, or memory location) in both states, have the same number.
	 * 2. Merge phase: this is the actual convex hull or widening, performed on the unified states.
	 */

	ASSERT(!trash.countOnes());
	ASSERT(!r.trash.countOnes());
	ASSERT(compare_reg.getType() == Ident::ID_INVALID);
	ASSERT(r.compare_reg.getType() == Ident::ID_INVALID);

	if (r.isBottom()) {
#ifdef POLY_DEBUG			
		cout << "Trivial merge (r is bottom)" << endl;
#endif
		return *this;
		}
	if (isBottom()) {
#ifdef POLY_DEBUG			
		cerr << "Trivial merge (l is bottom)" << endl;
#endif
		return r;
	}

#ifdef POLY_DEBUG			
	cout << "Non-trivial merge, type=" << (widen ? "widening" : "convex-hull") << endl;
#endif

	/* We have a non-trivial merge, so we will need working copies of l/r to perform the substitutions. */
	PPLDomain l1 = *this;
	PPLDomain r1 = r;

	_doUnify(l1, r1);


	l1.poly.poly_hull_assign(r1.poly);
	if (widen) {
#ifdef POLY_DEBUG			
		cout << "Before widening: " << endl;
		cout << "Left state: " << endl;
		cout << l1;

		cout << "Right state: " << endl;
		cout << r1;
		ASSERT(l1.poly.contains(r1.poly));
#endif
		PPL::Constraint_System dummy;
		l1.poly.bounded_BHRZ03_extrapolation_assign(r1.poly, dummy);
	}
#ifdef POLY_DEBUG			
	cout << "Merge finished." << endl;
	cout << "result state:";
	cout << l1;
#endif

	return l1;
}

PPLDomain PPLDomain::onSemInst(const sem::inst &si, int  /*instaddr*/) const {
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
				Variable storeAddr = s_out.getVarOrNew(idStoreAddr);

		        sem::reg_t src = si.d();
				Ident idStoreValue(src, Ident::ID_REG);
				Variable storeValue = s_out.getVarOrNew(idStoreValue);

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
			cout << "Invalid semantic instruction!" << endl;
			ASSERT(false);
			break;
	}
	s_out.doIntegerWrap();
	return s_out;
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
#ifdef POLY_DEBUG			
	cout << "Remapping: ";
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

void PPLDomain::doScratch(Ident &id) {
	if (hasIdent(id)) { 
		return;
	}
	
	Variable v = getVar(id);
	poly.unconstrain(v);
}


void PPLDomain::doIntegerWrap() {
	// TODO(clement): integer wrap not supported yet
}

void PPLDomain::doFinalizeUpdate() {
	/* Sets any bottom-equivalent state to the canonical bottom representation */
	if (isBottom()) {
		setBottom(); 
		return;
	}
	_sanityChecks();
#ifdef POLY_DEBUG			
	if (PPLDomain::trash.countOnes() == 0) {
		cout << "Nothing to clean" << endl;
		_sanityChecks();
		return;
	}
#endif
	PPLDomain::RemoveMarked rm(PPLDomain::trash, poly.space_dimension());
	doMap(rm);
	num_axis -= PPLDomain::trash.countOnes();
	PPLDomain::trash.clear();
	_sanityChecks();
}

Variable PPLDomain::varNew(const Ident &ident, bool allow_replace) {
	return Variable(_doAllocAxis(ident, allow_replace));
}

Variable PPLDomain::getVar(const Ident &ident) const {
	return Variable(id2axis[ident]);
}

Variable PPLDomain::getVarOrNew(const Ident &ident, bool allow_varNew) {
	if (allow_varNew && !hasIdent(ident)) {
		return varNew(ident, false);
	}
	return getVar(ident);
}

void PPLDomain::varCreatePtr(Ident &addr, Ident &val) {
	addr = Ident(mem_ref, Ident::ID_MEM_ADDR);
	val = Ident(mem_ref, Ident::ID_MEM_VAL);
	mem_ref++;
}

bool PPLDomain::hasIdent(const Ident &ident) const {
	return id2axis.hasKey(ident);
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
		if (trash.bit((*it).snd)) {
			continue;
		}
		ASSERT(axis2id[(*it).snd] == (*it).fst);
		if ((*it).snd > max_axis) {
			max_axis = (*it).snd;
		}
	}
	for (int i = 0; i < id2axis.count(); i++)
		ASSERT((axis2id[i].getType() == Ident::ID_INVALID) || (id2axis[axis2id[i]] == i));

	for (int i = 0; i < num_axis; i++) {
		if (trash.bit(i)) {
			continue;
		}
		Ident &ident = axis2id[i];
		ASSERT(id2axis[ident] == i);
	}
	ASSERT(max_axis + 1 == num_axis);
	ASSERT(poly.space_dimension() <= num_axis); // unused axis can exist at the end
	ASSERT(trash.size() >= num_axis);
	ASSERT(trash.countOnes() <= num_axis);
}
#endif

void PPLDomain::_doFreeAxis(int axis) {
	ASSERT(axis2id[axis].getType() != Ident::ID_INVALID);
#ifdef POLY_DEBUG			
	cout << "Variable " << Variable(axis) << ", mapped to identifier " << axis2id[axis] << ", is scheduled to be destroyed. " << endl;
#endif
	id2axis.remove(axis2id[axis]);
	axis2id[axis] = Ident();
	trash.set(axis);
}

int PPLDomain::_doAllocAxis(const Ident &ident, bool allow_replace) { 
	ASSERT(num_axis < trash.size())
	ASSERT(allow_replace || !id2axis.hasKey(ident));
	if (id2axis.hasKey(ident)) {
		int axis = id2axis[ident];
#ifdef POLY_DEBUG			
		cout << "The identifier " << ident << " was mapped to variable " << Variable(axis) << endl;
#endif
		_doFreeAxis(axis);
	}
	id2axis[ident] = num_axis;
	if (axis2id.length() <= num_axis) {
		axis2id.setLength(num_axis + 1);
	}
	axis2id[num_axis] = ident;
#ifdef POLY_DEBUG			
	cout << "New variable " << Variable(num_axis) << " created for identifier " << ident << endl;
#endif
	num_axis++;
	if (poly.space_dimension() < num_axis) {
		poly.add_space_dimensions_and_embed(num_axis - poly.space_dimension());
	}
	return num_axis - 1;
}

// Return the first constraint in poly for which the coef of specified variable axis is non-zero
const PPL::Constraint *PPLDomain::_getConstraintFor(int axis) const {
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

/*
 * Identify variables that were created in common ancstor of l and r (TODO now limited to starting register values, i.e. SP/BP)
 */
void PPLDomain::_identifyAncestorVars(PPLDomain &l, genstruct::HashTable<int, int> &commonVarsL, PPLDomain &r, genstruct::HashTable<int, int> &commonVarsR) const {
	int idx = 0;
#ifdef POLY_DEBUG			
	cout << "Identifying common variables\n";
#endif
	for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(l.id2axis); it; it++) {
		const Ident &ident = (*it).fst;
		if (ident.getType() != Ident::ID_SPECIAL) {
			continue;
		}
		if (r.id2axis.hasKey(ident)) {
			commonVarsL.put((*it).snd, idx);
			commonVarsR.put(r.id2axis[ident], idx);
#ifdef POLY_DEBUG			
			cout << "Using: " << ident << endl;
#endif
			idx++;
		}
	}
}

/**
* Indexes the pointer in dom, by their expression in terms of registers referenced in map_regs. 
* Stores the result in map_ptr.
*/
void PPLDomain::_indexPointersByExpr(genstruct::HashTable<PPL::Constraint, int, HashCons> &map_ptr, genstruct::HashTable<int, int>& commonRegs) const {
	int axis = commonRegs.count();
	for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
			commonRegs.put((*it).snd, axis);
			PPLDomain dom(*this); /* make a working copy to do the projections */
			dom.doMapPoly(MapWithHash(commonRegs));

			const PPL::Constraint *cons = dom._getConstraintFor(axis);
			if (cons != nullptr) {
				map_ptr[*cons] = (*it).fst.getId();
				delete cons;
			}
			commonRegs.remove((*it).snd);
		}
	} 

}

void PPLDomain::_doUnify(PPLDomain& l1, PPLDomain& r1) const {
	int axis = 0;

#ifdef POLY_DEBUG			
	cout << "Unify phase." << endl;
	cout << "Left state: " << endl;
	cout << l1;

	cout << "Right state: " << endl;
	cout << r1;
#endif

	/* 
	 * These hashtables will represent the substitution to perform in the two input states.
	 *
	 * Hashtable keys contains original numbering of the common variables in each states.
	 * Hashtable values contains a (new) common numbering of these varialbes.
	 */
	genstruct::HashTable<int,int> mappingL;
	genstruct::HashTable<int,int> mappingR;

	/* Create substitution entries for common vars created in merge ancestor */
	_identifyAncestorVars(l1, mappingL, r1, mappingR);

#ifdef POLY_DEBUG			
	cout << "Identifying address expressions appearing on both sides\n";	
#endif

	/* 
	 * Attempts to index each pointer by the expression of their address in terms of ancestor variables 
	 *
	 * The hashkey is the linear expression
	 * The hashvalue is the (original) memory location index.
	 * */
	genstruct::HashTable<PPL::Constraint, int, HashCons> indexedPtrsL;
	genstruct::HashTable<PPL::Constraint, int, HashCons> indexedPtrsR;

	l1._indexPointersByExpr(indexedPtrsL, mappingL);
	r1._indexPointersByExpr(indexedPtrsR, mappingR);

	/* 
	 * Pointer pairs with the same expression are equivalent, so we add a substitution for each one of them, so
	 * they will be mapped to the same variable number.
	 */
	axis=mappingL.count();
#ifdef POLY_DEBUG			
	cout << "Memory locations appearing on both states: " ;
#endif
	int mem_ref = 0;
	for (genstruct::HashTable<PPL::Constraint, int, HashCons>::PairIterator it(indexedPtrsL); it; it++) {
		const PPL::Constraint &cons = (*it).fst;
		if (indexedPtrsR.hasKey(cons)) {
			int ptrIdxL = (*it).snd;
			int ptrIdxR = indexedPtrsR[cons];
			/* pointer with index ptrIdxL in l represents the same address as pointer with index ptrIdxR in r */
			Variable addrL = l1.getVar(Ident(ptrIdxL, Ident::ID_MEM_ADDR));
			Variable valL = l1.getVar(Ident(ptrIdxL, Ident::ID_MEM_VAL));
			Variable addrR = r1.getVar(Ident(ptrIdxR, Ident::ID_MEM_ADDR));
			Variable valR = r1.getVar(Ident(ptrIdxR, Ident::ID_MEM_VAL));
			mappingL[addrL.id()] = axis;
			mappingR[addrR.id()] = axis;
			mappingL[valL.id()] = axis + 1;
			mappingR[valR.id()] = axis + 1;
			mem_ref++;
#ifdef POLY_DEBUG			
			cout << "ptr" << ptrIdxL << "/ptr" << ptrIdxR << ", ";
#endif
			axis += 2;
		}
	}
#ifdef POLY_DEBUG			
	cout << endl;
#endif

	/*
	 * Finally, add a substitution for each register that appears in both states.
	 */

	for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(l1.id2axis); it; it++) {
		const Ident &ident = (*it).fst;
		if ((ident.getType() != Ident::ID_REG) && (ident.getType() != Ident::ID_LOOP)) {
			continue;
}
		if (r1.id2axis.hasKey(ident)) {
			mappingL.put((*it).snd, axis);
			mappingR.put(r1.id2axis[ident], axis);
			axis++;
		}
	}

	l1.doMap(PPLDomain::MapWithHash(mappingL));
	r1.doMap(PPLDomain::MapWithHash(mappingR)); 
	ASSERT(l1.poly.space_dimension() == r1.poly.space_dimension());
	ASSERT(l1.poly.space_dimension() == axis);
	l1.num_axis = l1.poly.space_dimension();

#ifdef POLY_DEBUG			
	cout << "unify done. " << endl;
	cout << "Left state: " << endl;
	cout << l1;

	cout << "Right state: " << endl;
	cout << r1;

	cout << "Merge phase. " << endl;
#endif
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
				if (b) {
					poly.add_constraint(*v * cst_d == *vs1 * cst_n);
}
			} 
			break;
		case sem::MULH: // d <- (a * b) >> bitlength(d)
			b = getConstant(*vs1, cst_n, cst_d);
			if (b) {
				poly.add_constraint(*v * cst_d == *vs2 * cst_n);
			} else {
				b = getConstant(*vs2, cst_n, cst_d);
				if (b) {
					poly.add_constraint(*v * cst_d == *vs1 * cst_n);
}
			}
			break;
		default:
			break;
	}
}


p::feature POLY_ANALYSIS_FEATURE("otawa::poly::POLY_ANALYSIS_FEATURE", new Maker<PolyAnalysis>());

Identifier<int> LOC_VAR_SIZE("otawa::poly::LOC_VAR_SIZE", 4);
Identifier<int> NUM_LOC_VARS("otawa::poly::NUM_LOC_VARS", 8);
Identifier<int> MAX_AXIS("otawa::poly::MAX_AXIS", 512);


}  // namespace poly
} // namespace otawa
