#include <ctime>
#include <otawa/cfg/Edge.h>
#include <otawa/dfa/FastState.h>
#include <otawa/dfa/ai.h>
#include <otawa/flowfact/features.h>
#include <otawa/graph/Graph.h>
#include <otawa/otawa.h>
#include <otawa/util/HalfAbsInt.h>
#include <otawa/util/WideningFixPoint.h>
#include <otawa/util/WideningListener.h>
#include <ppl.hh>

#include "include/PPLDomain.h"
#include "include/PPLManager.h"
#include "include/PolyAnalysis.h"

namespace otawa {
namespace poly {

t::hash HashCons::hash(const PPL::Constraint &key) {
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
	} else {
		kind = 2;
	}
	result ^= kind;
	return result;
}

void Ident::print(io::Output &out) const {
	switch (_type) {
	case ID_REG:
		if (_id < 0) {
			out << "T" << -_id;
		} else {
			out << "R" << _id;
		}
		break;
	case ID_REG_INPUT:
		if (_id < 0) {
			out << "T" << -_id << "_0";
		} else {
			out << "R" << _id << "_0";
		}
		break;
	case ID_MEM_ADDR:
		out << "ptr" << _id;
		break;
	case ID_MEM_VAL:
		out << "*ptr" << _id;
		break;
	case ID_MEM_VAL_INPUT:
		out << "*ptr" << _id << "_0";
		break;
	case ID_LOOP:
		out << "bound" << _id;
		break;
	case ID_SPECIAL:
		switch (_id) {
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

Output &operator<<(Output &o, const Variable pv) {
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

PPLDomain::~PPLDomain() {
	delete _summary;
}

void PPLDomain::print(io::Output &out) const {
	static char buf[64];

	if (isBottom()) {
		out << "BOTTOM";
		return;
	}

	PPL::Constraint_System cons = poly.minimized_constraints();
	int ncons = 0;

	out << "Constraints: ";

	for (PPL::Constraint_System::const_iterator it = cons.begin(); it != cons.end(); it++, ncons++) {
		const PPL::Constraint &c = *it;

		bool firstTerm = true;
		for (PPL::dimension_type i = 0; i < cons.space_dimension(); i++) {
			const PPL::Coefficient &coef = c.coefficient(Variable(i));

			if (coef != 0) {
				Variable v(i);

				if (coef == -1) {
					out << "- ";
				} else {
					if (!firstTerm)
						out << "+ ";
					if (coef != 1) {
						gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(coef));
						buf[sizeof(buf) - 1] = 0;
						out << buf << ".";
					}
				}

				if (isVarMapped(v)) {
					out << getIdent(v);
				} else {
					out << v;
				}

				out << " ";
				firstTerm = false;
			}
		}

		if (c.is_equality()) {
			out << "= ";
		} else {
			out << ">= ";
		}

		const PPL::Coefficient &cst = -c.inhomogeneous_term(); // Constraints form is: coef*X + ... + cst = 0
		gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(cst));
		buf[sizeof(buf) - 1] = 0;
		out << buf;

		out << "; ";
		;
	}
	out << endl;

	out << "Space dimension: " << poly.space_dimension() << ", Constraints count: " << ncons << endl;
	displayIdentMap(out);
	displayLocVars(out);
	displayGlobVars(out);
	out << endl;

	if (_summary != nullptr) {
		cout << "Summary info: " << endl;
		cout << "- Inputs: ";
		for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
			const Pair<Ident, int> &p = *it;
			if ((p.fst.getType() == Ident::ID_MEM_VAL_INPUT) || (p.fst.getType() == Ident::ID_REG_INPUT)) 
				cout << p.fst << ", ";
		}
		cout << endl;
		/*
		for (elm::genstruct::Vector<Ident>::Iterator it(_summary->_inputs); it; it++) {
			cout << *it << ", ";
		}
		*/

		cout << "- Outputs/Side-effects: ";
		for (elm::genstruct::Vector<Ident>::Iterator it(_summary->_damaged); it; it++) {
			cout << *it << ", ";
		}
		cout << endl;

		
	}
}

bool PPLDomain::equals(const PPLDomain &b) const {
	ASSERT((initState == nullptr) || (b.initState == nullptr) || (initState == b.initState));
	ASSERT(trash.countOnes() == 0);

	if (isBottom() != b.isBottom())
		return false;

	/*
	 * If we are summarizing, test if the summaries are equivalent
	 */
	ASSERT((_summary == nullptr) == (b._summary == nullptr));
	if (_summary != nullptr) {
		if (!_summary->equals(*b._summary))
			return false;
	}

	/*
	 * First, attempt to show that the states are different using quick checks.
	 */
	if (poly.space_dimension() != b.poly.space_dimension())
		return false;

	if (compare_reg != b.compare_reg)
		return false;

	if (compare_op != b.compare_op)
		return false;

	if (id2axis.count() != b.id2axis.count())
		return false;

	if (trash != b.trash)
		return false;

	if (bounds != b.bounds)
		return false;

	/*
	 * Try to show that the states are equals when ignoring memory locations (faster)
	 */
	PPLDomain r = b;
	PPLDomain l = *this;

	int expectedVarCount = 0;
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		const Pair<Ident, int> &p = *it;
		if ((p.fst.getType() == Ident::ID_MEM_VAL) || (p.fst.getType() == Ident::ID_MEM_ADDR) || (p.fst.getType() == Ident::ID_MEM_VAL_INPUT)) {
			continue;
		}
		expectedVarCount++;
	}

	_doUnify(l, r, true);

	if ((l.id2axis.count() != expectedVarCount) || (r.id2axis.count() != expectedVarCount)) {
		/* There was some unmatched registers */ 
		return false;
	}


	if (l.poly != r.poly)
		return false;
	
	/*
	 * At this point we are almost sure that the states are equal. We do a full unification (costly) to detect if the states are equal.
	 */
	r = b;
	l = *this;
	cout << " ================ EQUAL ==============" << endl;
	_doUnify(l, r);

	if ((l.id2axis.count() != id2axis.count()) || (r.id2axis.count() != b.id2axis.count())) {
		/* There was some unmatched memory locations */
		return false;
	}

	if (l.poly != r.poly)
		return false;

	return true;
}

template <class F>
PPLDomain::MapHelper<F>::MapHelper(F &pfunc, int max_in_domain)
    : _pfunc(pfunc), _max_in_domain(max_in_domain), _empty(true) {
	for (PPL::dimension_type i = 0; i <= _max_in_domain; i++) {
		PPL::dimension_type j;
		if (_pfunc.maps(i, j) && (_empty || (_max_in_codomain < j))) {
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
	for (unsigned int k = 0; k < i; k++) {
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
		out << "diplayGlobVars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	Variable ssp = getVar(id_ssp);
	out << "Local variables: " << endl;
	for (int i = 0; i < NUM_LOC_VARS(_props) * LOC_VAR_SIZE(_props); i += LOC_VAR_SIZE(_props)) {
		PPL::Constraint_System cons = mcons;
		Variable v(num_axis);
		cons.insert(v == ssp - i - LOC_VAR_SIZE(_props));
		out << " [SP - " << hex(i) << "] == ";
		bool found = false;
		for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
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

					out << " (aka " << idval << ")";
					break;
				}
			}
		}
		if (!found) {
			out << "N/A";
		}
		out << endl;
	}
}

void PPLDomain::displayGlobVars(io::Output &out) const {
	const PropList _props;
	PPL::Constraint_System mcons = poly.minimized_constraints();
	if (isBottom()) {
		out << "diplayGlobVars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	out << "Global variables: " << endl;
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		elm::Pair<Ident, int> p = *it;
		if (p.fst.getType() == Ident::ID_MEM_ADDR) {
			PPL::Coefficient num, den;
			if (getConstant(p.fst, num, den)) {
				out << " @addr 0x";
				displayFrac(out, num, den, true);
				out << ", value = ";
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
				out << " (aka " << idval << ")";
				out << endl;

			}
		}
	}
}

int PPLDomain::getConsCount() const {
	const PPL::Constraint_System &cons = poly.minimized_constraints();
	int ncons = 0;
	for (PPL::Constraint_System::const_iterator it = cons.begin(); it != cons.end(); it++, ncons++);
	return ncons;
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
void PPLDomain::getRange(const Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n,
                         PPL::Coefficient &bsup_d) const {
	Variable v = getVar(id);
	getRange(v, binf_n, binf_d, bsup_n, bsup_d);
}

void PPLDomain::displayFrac(io::Output &out, const PPL::Coefficient &num, const PPL::Coefficient &den, bool hex) const {
	static char buf[16];
	ASSERT(den != 0);
	gmp_snprintf(buf, sizeof(buf), hex ? "%Zx" : "%Zd", &PPL::raw_value(num));
	buf[sizeof(buf) - 1] = 0;
	out << buf;
	if (den != 1) {
		gmp_snprintf(buf, sizeof(buf), hex ? "/%Zx" : "/%Zd", &PPL::raw_value(den));
		buf[sizeof(buf) - 1] = 0;
		out << buf;
	}
}

void PPLDomain::getRange(const Variable &var, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d,
                         PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d) const {
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
	out << "Mapping: ";
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
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
	if (PPL::raw_value(bsup_d).get_ui() != 0) {
		return static_cast<bound_t>(PPL::raw_value(bsup_n).get_ui() / PPL::raw_value(bsup_d).get_ui());
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
	cout << "Filtering, compare_reg is: " << compare_reg << ", compare_op is: " << compare_op << ", taken=" << taken
	     << endl;
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
	trash.set(res.getVar(res.compare_reg).id());
	res.compare_reg = Ident();
	if (res.isBottom()) {
#ifdef POLY_DEBUG
		cout << "State is Bottom after Filtering (will not propagate states to branch destination)" << endl;
#endif
		return PPLDomain(); // bottom
	}
	return res;
}

PPLDomain PPLDomain::onMerge(const PPLDomain &r, bool widen) const {
	/*
	 * The merge works in two phases:
	 *
	 * 1. Unification phase: we perform a substitution (variable renumbering), so that variables representing the same
	 * object (i.e.
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

//	cout << "before " << (widen ? "widening" : "join") << ", l= " << l1.getConsCount() << " r=" << r1.getConsCount() << endl;
	for (int i = 0; i < r1.bounds.length(); i++)
		l1.setBound(i, r1.getBound(i));

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
		//l1.poly.BHRZ03_widening_assign(r1.poly);
		l1.poly.bounded_H79_extrapolation_assign(r1.poly, dummy);
	}
//	cout << "after" << (widen ? "widening" : "join") << ", l= " << l1.getConsCount() << endl;

	// also merge summaries
	if (r1._summary != nullptr) {
		if (l1._summary == nullptr)
			l1._summary = new PPLSummary();
		for (elm::genstruct::Vector<Ident>::Iterator it(r1._summary->_damaged); it; it++) {
			bool found = false;
			for (elm::genstruct::Vector<Ident>::Iterator it2(l1._summary->_damaged); it2; it2++) {
				cout << "compare " << (*it2) << " avec: " << (*it) << endl;
				if (l1.getVar(*it2).id() == r1.getVar(*it).id()) {
					found = true;
					break;
				}
			}
			if (!found) {
				ASSERT(l1.isVarMapped(r1.getVar((*it))));
				l1._summary->_damaged.add(l1.getIdent(r1.getVar((*it))));
			}
		}


		/*
		for (elm::genstruct::Vector<Ident>::Iterator it(r1._summary->_inputs); it; it++) {
			if (!l1._summary->_inputs.contains(*it)) {
				l1._summary->_inputs.add(*it);
			}
		}
		*/
	} else {
		if (l1._summary != nullptr) {
			delete l1._summary;
			l1._summary = nullptr;
		}
	}
#ifdef POLY_DEBUG
	cout << "Merge finished." << endl;
	cout << "result state:";
	cout << l1;
#endif

	return l1;
}

PPLDomain PPLDomain::onSemInst(const sem::inst &si, int /*instaddr*/) const {
	PPLDomain s_out = *this;
	ASSERT(!hasFilter() || (si.op == sem::BRANCH));

	switch (si.op) {
		case sem::NOP:
			break;
		case sem::TRAP: // perform a trap
			break;
		case sem::CONT: // continue in sequence with next instruction
			break;
		case sem::BRANCH: // perform a branch on content of register a
			break;
		case sem::SCRATCH: // d <- T
		{
			sem::reg_t dest = si.d();
			Ident id(dest, Ident::ID_REG);
			s_out.doScratch(id);
			break;
		}
		case sem::SETP: // page(d) <- cst
			break;
		case sem::SETI: // d <- cst
		{
			sem::reg_t dest = si.d();
			Ident id(dest, Ident::ID_REG);
			Variable v = s_out.varNew(id, true);
			int32_t cst = si.cst();
			s_out.poly.add_constraint(v == cst);
			break;
		}
		case sem::SET: // d <- a
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
			Variable v = s_out.varNew(id, true, true);

			s_out.poly.add_constraint(v == vs);
			break;
		}
		case sem::SHL: // d <- unsigned(a) << b
		case sem::SUB: // d <- a - b
		case sem::ASR: // d <- a >> b
		case sem::MUL:
		case sem::MULH: // d <- (a * b) >> bitlength(d)
		case sem::ADD:  // d <- a + b
		case sem::SHR:  // d <- unsigned(a) >> b
		case sem::AND:  // d <- a & b
		case sem::OR:   // d <- a | b
		case sem::XOR:  // d <- a ^ b
		case sem::CMP:  // d <- a ~ b
		case sem::CMPU: // d <- a ~u b
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
		case sem::STORE: // MEMb(a) <- d
		{

			sem::reg_t addr = si.a();
			Ident idStoreAddr(addr, Ident::ID_REG);
			Variable storeAddr = s_out.getVarOrNew(idStoreAddr, true);

			sem::reg_t src = si.d();
			Ident idStoreValue(src, Ident::ID_REG);
			Variable storeValue = s_out.getVarOrNew(idStoreValue, true);

/*
			for (MyHTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++)
			{
				elm::Pair<Ident, int> p = *it;
				if (p.fst.getType() == Ident::ID_LOOP) {
					MyHTable<int,int> map;
					Ident id_frame(Ident::ID_START_SP, Ident::ID_SPECIAL);
					Variable v_frame = s_out.getVar(id_frame);
					Variable v_bound = s_out.getVar(p.fst);
					map[storeAddr.id()] = 0;
					map[v_frame.id()] = 1;
					map[v_bound.id()] = 2;
					PPLDomain tmp = s_out;
					tmp.doMapPoly(MapWithHash(map));
					const PPL::Constraint_System &cons = tmp.poly.minimized_constraints();
					for (PPL::Constraint_System::const_iterator it2 = cons.begin(); it2 != cons.end(); it2++) {
						const PPL::Constraint &c = *it2;
						const PPL::Coefficient &coef = c.coefficient(Variable(2));
						const PPL::Coefficient &coef2 = c.coefficient(Variable(0));
						if ((coef != 0) && (coef2 != 0) && c.is_equality()) {
							cout << "[DEBUG] loop-indexed array write! " ;
							fflush(stdout);
							(*it2).print();
							fflush(stdout);
							cout << endl;
							break;
						}
					} 
				}
			}
*/

			Ident idEquiv;                          /* Identifier equivalent to the store addr, if any. */
			elm::genstruct::Vector<Ident> overlaps; /* List of identifiers overlapping the store addr. */

			for (MyHTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++) {
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
		case sem::LOAD: // d <- MEMb(a)
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
			for (MyHTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++) {
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
				cout << "LOAD: Not found, creating new ptr..." << endl;
#endif
				const Variable &v = s_out.memCreate(loadAddr, loadReg, false);

				uint32_t concreteAddress, initialValue;
				if (s_out.memGetInitial(idLoadAddr, concreteAddress, initialValue) && initialValue /* TODO test */) {
					s_out.doNewConstraint(loadReg == initialValue);
				} else if (s_out._summary != nullptr) {
					// Unknown LOAD value. If we are summarizing, create an input.
					Ident idInputAddr = s_out.getIdent(v);
					Ident idInputVal = Ident(idInputAddr.getId(), Ident::ID_MEM_VAL_INPUT);
					Ident idCurrentVal = Ident(idInputAddr.getId(), Ident::ID_MEM_VAL);
					Variable currentVal = s_out.getVar(idCurrentVal);
					Variable inputVal = s_out.varNew(idInputVal);
#ifdef POLY_DEBUG
					cout << "Summarizing: creating new input memory: " << " what= " << idInputVal << " where=" << idInputAddr << endl;
#endif
//					s_out._summary->_inputs.add(idInputVal);
					s_out.doNewConstraint(currentVal == inputVal);
				}
			}
			break;
		} 
		case sem::NOT: // d <- ~a
			break;
		case sem::IF: {
			Ident id(si.sr(), Ident::ID_REG);
			if (s_out.hasIdent(id)) {

				s_out.compare_reg = id;
				s_out.compare_op = si.cond();
				ASSERT(si.jump() == 1);
			}
			break;
		} 
		default:
			cout << "Invalid semantic instruction!" << endl;
			ASSERT(false);
			break;
	}
	s_out.doIntegerWrap();
	return s_out;
}

template <class F> void PPLDomain::doMapPoly(F pfunc, bool noproj) {
	if (poly.space_dimension() > 0) {
		MapHelper<F> a(pfunc, poly.space_dimension() - 1);
		if (noproj && (poly.space_dimension() == a.max_in_codomain() + 1)) {
			poly.add_space_dimensions_and_embed(1);
		}
		poly.map_space_dimensions(a);
	}
}

template <class F> void PPLDomain::doMapIdents(F pfunc) {
	genstruct::Vector<Ident> todel;
	genstruct::Vector<Ident> newAxis2id;
	newAxis2id.setLength(axis2id.length());
	Vector<Ident> newDamaged;

#ifdef POLY_DEBUG
	cout << "Remapping: ";
#endif
	for (MyHTable<Ident, int, HashIdent>::MutableIter it(id2axis); it; it++) {
		int &n = it.item();
		PPL::dimension_type old_axis = n;
		PPL::dimension_type new_axis = n;
		if (pfunc.maps(old_axis, new_axis)) {
			if (old_axis != new_axis) {
#ifdef POLY_DEBUG
				cout << axis2id[old_axis] << "[" << Variable(old_axis) << "->" << Variable(new_axis) << "] ";
#endif
				n = new_axis;
			}
			newAxis2id[new_axis] = axis2id[old_axis];
		} else {
			todel.add(axis2id[old_axis]);
		}
	}
#ifdef POLY_DEBUG
	cout << endl;
#endif
	for (genstruct::Vector<Ident>::Iterator it(todel); it; it++) {
		id2axis.remove(*it);
	}

	axis2id = newAxis2id;
}
template <class F> void PPLDomain::doMap(F pfunc, bool noproj) {
	doMapPoly(pfunc, noproj);
	doMapIdents(pfunc);
}

void PPLDomain::doScratch(Ident &id) {
	if (!hasIdent(id)) {
		return;
	}

	Variable v = getVar(id);
	poly.unconstrain(v);
}

void PPLDomain::doIntegerWrap() {
	// TODO(clement): integer wrap not supported yet
}

void PPLDomain::doKillTemporaries() {
	Vector<Ident> toDel;

	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++)
		if ((((*it).fst.getType() == Ident::ID_REG) && (*it).fst.getId() < 0)) {
			toDel.add((*it).fst);
#ifdef POLY_DEBUG
			cout << "Killing temporary register: " << (*it).fst << endl;
#endif
		}

	for (Vector<Ident>::Iter it(toDel); it; it++) {
		varKill(*it);
	}
}

void PPLDomain::doKillRegisters(BitVector bv) {
	Vector<Ident> toDel;

	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++)
		if (((*it).fst.getType() == Ident::ID_REG) && 
			(*it).fst.getId() >= 0 &&
			(*it).fst.getId() < bv.size() &&
			!bv.bit((*it).fst.getId())) {

			if ((*it).fst == compare_reg)
				continue; // skip compare_reg, as this will be handled in PPLDomain::filter()

			if ((*it).fst.getId() == 13)
				continue; // never kill SP, as we need it to detect out-of-scope stack variables

			if ((*it).fst.getId() == 0)
				continue; // avoid killing R0 as it holds the return value 
			// TODO performance: preserver R0 uniquement s'il atteint la fin de la fonction

#ifdef POLY_DEBUG
			cout << "Killing dead register: " << (*it).fst << endl;
#endif
			toDel.add((*it).fst);
		}

	for (Vector<Ident>::Iter it(toDel); it; it++) {
		varKill(*it);
	}
}

void PPLDomain::doLeaveFunction() {
	Vector<Ident> toDel;

	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		elm::Pair<Ident, int> p = *it;
		Variable v = Variable(p.snd);

		if (p.fst.getType() == Ident::ID_MEM_ADDR) {
			Ident idSp(13, Ident::ID_REG);
			Variable sp = getVar(idSp);

			if (poly.relation_with(v < sp).implies(PPL::Poly_Con_Relation::is_included()) &&
				poly.relation_with(v >= int(stackconf_t::STACK_TOP - stackconf_t::STACK_SIZE)).implies(PPL::Poly_Con_Relation::is_included())) {
				toDel.add(p.fst);
				toDel.add(Ident(p.fst.getId(), Ident::ID_MEM_VAL));
#ifdef POLY_DEBUG
				cout << "Killing out-of-scope stack variable: " << (*it).fst << endl;
#endif
			}
		}
	}

	for (Vector<Ident>::Iter it(toDel); it; it++)
		varKill(*it);
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

Variable PPLDomain::varNew(const Ident &ident, bool allow_replace, bool create_damaged) {
	if (_summary != nullptr && create_damaged) {
		if ((ident.getType() == Ident::ID_REG) && (ident.getId() == 0)) {
			// TODO tester les registres qu'il faut garder en fonction de la convention d'appel
			_summary->_damaged.add(ident);
		}
		if ((ident.getType() == Ident::ID_MEM_VAL)) { 
			Variable v = getVar(Ident(ident.getId(), Ident::ID_MEM_ADDR));

			Ident idSsp(Ident::ID_START_SP, Ident::ID_SPECIAL);
			Variable ssp = getVar(idSsp);

			if (poly.relation_with(v < ssp).implies(PPL::Poly_Con_Relation::is_included()) &&
				poly.relation_with(v >= int(stackconf_t::STACK_TOP - stackconf_t::STACK_SIZE)).implies(PPL::Poly_Con_Relation::is_included())) {
				// is local variable.. do not add in damaged set
#ifdef POLY_DEBUG
				cout << "Variable " << ident << " not added to damaged set because it is local var." << endl;
#endif
			} else {
#ifdef POLY_DEBUG
				cout << "Variable " << ident << " added to damaged set because it may be a non-local var." << endl;
#endif
				_summary->_damaged.add(ident);
			}
		}
	}
	return Variable(_doAllocAxis(ident, allow_replace));
}

Variable PPLDomain::getVar(const Ident &ident) const { return Variable(id2axis[ident]); }

Variable PPLDomain::getVarOrNew(const Ident &ident, bool create_input) {
	if (!hasIdent(ident)) {
		Variable v = varNew(ident, false);
		if (create_input && _summary != nullptr) {
			/* Read from untracked register. If we are summarizing, create a new input register */
			ASSERT(ident.getType() == Ident::ID_REG);
			Ident idInput(ident.getId(), Ident::ID_REG_INPUT);
//			_summary->_inputs.add(idInput);
			Variable input = varNew(idInput);
			doNewConstraint(input == v);
#ifdef POLY_DEBUG
			cout << "Summarizing: creating new input register " << input << endl;
#endif
		}
		return v;
	}
	return getVar(ident);
}

void PPLDomain::varCreatePtr(Ident &addr, Ident &val) {
	addr = Ident(mem_ref, Ident::ID_MEM_ADDR);
	val = Ident(mem_ref, Ident::ID_MEM_VAL);
	mem_ref++;
}

bool PPLDomain::hasIdent(const Ident &ident) const { return id2axis.hasKey(ident); }

Variable PPLDomain::memReplace(const Variable &address, const Variable &valueSource) {
	const Ident &idOldAddress = getIdent(address);
	const Ident &idOldValue = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL);
	Variable oldValue = getVar(idOldValue);

	Ident idNewAddress, idNewValue;
	varCreatePtr(idNewAddress, idNewValue);

	const Variable &newAddress = varNew(idNewAddress, false);
	doNewConstraint(address == newAddress);

	const Variable &newValue = varNew(idNewValue, false, true);
	doNewConstraint(newValue == valueSource);

	const Ident &idOldInput = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL_INPUT);
	if (hasIdent(idOldInput)) {
		ASSERT(_summary);
		const Variable &oldInput = getVar(idOldInput);
		const Ident idNewInput(idNewAddress.getId(), Ident::ID_MEM_VAL_INPUT);
		const Variable &newInput= varNew(idNewInput, false, false);
		doNewConstraint(newInput == oldInput);
/*		_summary->_inputs.remove(idOldInput);
		_summary->_inputs.add(idNewInput); */ 
#ifdef POLY_DEBUG
		cout << "Summarizing: migrating input from " << idOldInput << " to " << idNewInput << endl;
#endif
		varKill(oldInput);
	}

	varKill(oldValue);
	varKill(address);
	return newAddress;
}

Variable PPLDomain::memCreate(const PPL::Linear_Expression & address , const PPL::Linear_Expression &valueSource, bool damage) {
	Ident idNewAddress, idNewValue;
	varCreatePtr(idNewAddress, idNewValue);

	const Variable &newAddress = varNew(idNewAddress, false);
	doNewConstraint(newAddress == address);

	const Variable &newValue = varNew(idNewValue, false, damage);
	doNewConstraint(newValue == valueSource);

	


	return newAddress;
}

Variable PPLDomain::memMerge(const Variable &address, const Variable &newValue) {
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

bool PPLDomain::memGetInitial(const Ident &id, uint32_t &address, uint32_t &value) {
	PPL::Coefficient num, den;
	/* TODO(clement) : use correct size  */
	if (getConstant(id, num, den)) {
		address = PPL::raw_value(num).get_ui() / PPL::raw_value(den).get_ui();
#ifdef POLY_DEBUG
		cout << "Address is statically known (" 
			<< hex(address) 
			<< "), attempting to read value from initial state" << endl;
#endif

		try {
			initState->get(address, value);
#ifdef POLY_DEBUG
			cout << "Initial state contains a value for this address: " << value << endl;
#endif
			return true;
		} catch (Exception ex) { 
			/* TODO(clement): Find the exact exception that is thrown in this case, it appears to be undocumented */
#ifdef POLY_DEBUG
			cout << "Could not find value (address out of bounds?)" << endl;
			return false;
#endif
		}
	}
	return false;
}

#ifdef POLY_DEBUG
void PPLDomain::_sanityChecks() {
	int max_axis = -1;
	if (isBottom()) {
		ASSERT(poly.is_empty());
		return;
	}
	ASSERT(!poly.is_empty());
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
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
	ASSERT(max_axis + 1 <= num_axis);
	ASSERT(max_axis + 1 + trash.countOnes() >= num_axis);
	ASSERT(poly.space_dimension() <= (unsigned)num_axis); // unused axis can exist at the end
	ASSERT(trash.size() >= num_axis);
	ASSERT(trash.countOnes() <= num_axis);
}
#endif

void PPLDomain::_doFreeAxis(int axis) {
	ASSERT(axis2id[axis].getType() != Ident::ID_INVALID);
#ifdef POLY_DEBUG
	cout << "Variable " << Variable(axis) << ", mapped to identifier " << axis2id[axis]
	     << ", is scheduled to be destroyed. " << endl;
#endif
	if (this->_summary) {
		if (this->_summary->_damaged.contains(axis2id[axis])) {
			this->_summary->_damaged.remove(axis2id[axis]);
#ifdef POLY_DEBUG
			cout << "Also destroying from damaged set." << endl;
#endif
		}
	}
	id2axis.remove(axis2id[axis]);
	axis2id[axis] = Ident();
	trash.set(axis);
}

int PPLDomain::_doAllocAxis(const Ident &ident, bool allow_replace) {
	ASSERT(!isBottom());
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
	if (poly.space_dimension() < (unsigned)num_axis) {
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
 * Identify variables that were created in common ancstor of l and r 
 * (TODO now limited to starting register values, i.e.  * SP/BP)
 */
void PPLDomain::_identifyAncestorVars(PPLDomain &l, MyHTable<int, int> &commonVarsL, PPLDomain &r,
                                      MyHTable<int, int> &commonVarsR) const {
	int idx = 0;
#ifdef POLY_DEBUG
	cout << "Identifying common variables\n";
#endif
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(l.id2axis); it; it++) {
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

/*
 * Indexes the pointer in dom, by their expression in terms of registers referenced in map_regs.
 * Stores the result in map_ptr.
 */
void PPLDomain::_indexPointersByExpr(MyHTable<PPL::Constraint, int, HashCons> &map_ptr,
                                     MyHTable<int, int> &commonRegs) const {
	int axis = commonRegs.count();
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
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

void PPLDomain::_doMatchSummaries(PPLDomain &l1, PPLDomain &r1, unsigned int& axis, 
		MyHTable<int,int> &mappingL, MyHTable<int,int> &mappingR,
		MyHTable<PPL::Constraint, int, HashCons> &indexPtrsL,
		MyHTable<PPL::Constraint, int, HashCons> &indexPtrsR) const {


	for (MyHTable<Ident, int, HashIdent>::PairIterator it(l1.id2axis); it; it++) {
		PPL::dimension_type old_axis = (*it).snd;
		PPL::dimension_type new_axis = (*it).snd;

		if (!PPLDomain::MapWithHash(mappingL).maps(old_axis, new_axis)) {
			bool keep = false;
			if (l1._summary->_damaged.contains((*it).fst)) {
#ifdef POLY_DEBUG
				cout << "On doit garder " << (*it).fst << " car il est dans l'ensemble Damaged" << endl;
#endif
				keep = true;
			} 
			if ((*it).fst.getType() == Ident::ID_REG_INPUT || (*it).fst.getType() == Ident::ID_MEM_VAL_INPUT) {
#ifdef POLY_DEBUG
				cout << "On doit garder " << (*it).fst << " car c'est un Input" << endl;
#endif
				keep = true;
			} 
			if (keep) {
				if (((*it).fst.getType()== Ident::ID_MEM_VAL) || (*it).fst.getType() == Ident::ID_MEM_VAL_INPUT) {
					// need to also add address variable
					Ident idAddrR, idObjR;
					idAddrR = Ident(mem_ref, Ident::ID_MEM_ADDR);
					idObjR = Ident(mem_ref, (*it).fst.getType());
					r1.mem_ref++;

					// value or input
					mappingL.put((*it).snd, axis); //idValL
					mappingR.put(r1.varNew(idObjR).id(), axis);
					axis++;

					// address
					Ident idAddrL((*it).fst.getId(), Ident::ID_MEM_ADDR);
					Variable addrL = l1.getVar(idAddrL);
					mappingL.put(addrL.id(), axis);
					Variable addrR = r1.varNew(idAddrR);
					mappingR.put(addrR.id(), axis);

					axis++;
					// if input, also add value
					if ((*it).fst.getType() == Ident::ID_MEM_VAL_INPUT) {
						Ident idValL((*it).fst.getId(), Ident::ID_MEM_VAL);
						Ident idValR(idAddrR.getId(), Ident::ID_MEM_VAL);
						mappingL.put(l1.getVar(idValL).id(), axis);
						mappingR.put(r1.varNew(idValR).id(), axis);
						axis++;
					}

#ifdef POLY_DEBUG
					cout << idAddrR << " <==> " << idAddrL << endl;
#endif
					PPL::Constraint c;
					PPL::Linear_Expression le;
					bool found = false;
					for (MyHTable<PPL::Constraint, int, HashCons>::PairIterator it(indexPtrsL); it; it++) {
						if ((*it).snd == idAddrL.getId()) {
							c = (*it).fst;
							found = true;
							break;
						}
					}
					ASSERT(found);
					char buf[128];

					for (PPL::dimension_type i = 0; i < c.space_dimension(); i++) {
						Variable v(i);
						Variable vc(0);
						const PPL::Coefficient &coef = c.coefficient(v);
						if (coef != 0) {
#ifdef POLY_DEBUG
							gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(coef));
							buf[sizeof(buf) - 1] = 0;
							cout << buf << ".";
#endif
							ASSERT(mappingL.hasKey(i));
							int axis_in_R = -1;
							if (i < (c.space_dimension() - 1)) {
								int common_axis = i; //mappingL[i];
								vc = Variable(common_axis);
								for (MyHTable<int, int>::PairIterator itm(mappingR); itm; itm++) {
									if ((*itm).snd == common_axis) {
										axis_in_R = (*itm).fst;
										break;
									}
								}
							} else {
								axis_in_R = addrR.id();
							}
							ASSERT(axis_in_R != -1);
							Variable v2(axis_in_R);
							le = le + coef * v2;
							cout << v << "|" << v2 << "," << vc << " + ";
						}
					}
					const PPL::Coefficient &cst = c.inhomogeneous_term();
#ifdef POLY_DEBUG
					gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(cst));
					buf[sizeof(buf) - 1] = 0;
					cout << buf;
#endif
					le = le + cst;

					PPL::Constraint c2;
					if (c.is_equality()) {
						c2 = (le == 0);
#ifdef POLY_DEBUG
						cout << " == 0";
#endif
					} else {
						c2 = (le >= 0);
#ifdef POLY_DEBUG
						cout << " >= 0 ";
#endif
					}
					r1.doNewConstraint(c2);

				} else {
					// register
					Variable regVar = r1.varNew((*it).fst);
					mappingL.put((*it).snd, axis);
					mappingR.put(regVar.id(), axis);
					axis++;

				}
/*
				mappingL.put((*it).snd, axis);
				axis++;
*/
			}
		}
	}
}

void PPLDomain::_doMatchGlobals(PPLDomain &l1, PPLDomain &r1, unsigned int& axis, 
		MyHTable<int,int> &mappingL, MyHTable<int,int> &mappingR) const {
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(l1.id2axis); it; it++) {
		if ((*it).fst.getType() != Ident::ID_MEM_ADDR)
			continue;
		uint32_t addressL, valueL;
		if (l1.memGetInitial((*it).fst, addressL, valueL) && valueL) {
			bool found = false;
			for (MyHTable<Ident, int, HashIdent>::PairIterator it2(r1.id2axis); it2; it2++) {
				if ((*it2).fst.getType() != Ident::ID_MEM_ADDR)
					continue;
				uint32_t addressR, valueR;
				if (r1.memGetInitial((*it2).fst, addressR, valueR) && valueL) {
					if (addressR ==  addressL) {
						found = true;
						break;
					}
				}
			}

			if (!found) { /* TODO need faire dans les deux sens */ 
				/*
				 * Found a static address variable in l1 that was not in r1, and it is possible to
				 * recover the value from the initial state. So we create the corresponding memory location
				 * on r1 and initialize it with the value recovered from the initial state.
				 */
#ifdef POLY_DEBUG
				cout << "Left-side identifier " << (*it).fst << " has static address 0x" << hex(addressL)
					<< " and no corresponding identifier in right-side state." << endl;

#endif
				Variable addrL = Variable((*it).snd);
				Variable valL = l1.getVar(Ident((*it).fst.getId(), Ident::ID_MEM_VAL));
				Ident idAddrR, idValR;

				r1.varCreatePtr(idAddrR, idValR);
				Variable addrR = r1.varNew(idAddrR);
				Variable valR = r1.varNew(idValR);

				r1.doNewConstraint(addrR == addressL);
				r1.doNewConstraint(valR == valueL);

				mappingL[addrL.id()] = axis;
				mappingR[addrR.id()] = axis;
				mappingL[valL.id()] = axis + 1;
				mappingR[valR.id()] = axis + 1;
				axis += 2;
				if (l1.hasIdent(Ident((*it).fst.getId(), Ident::ID_MEM_VAL_INPUT))) {
					Variable inputL = l1.getVar(Ident((*it).fst.getId(), Ident::ID_MEM_VAL_INPUT));
					Ident idInput(idAddrR.getId(), Ident::ID_MEM_VAL_INPUT);
					Variable inputR = r1.varNew(idInput);
					mappingL[inputL.id()] = axis;
					mappingR[inputR.id()] = axis;
					axis++;

				}
			}
		}
	}
}

// TODO modifier pour garder quand meme des deux cotés ce qui est dans DAMAGED
// TODO unifier le summary aussi ici
// TODO faire la projection sur le damaged
void PPLDomain::_doUnify(PPLDomain &l1, PPLDomain &r1, bool noPtr) const {
	unsigned int axis = 0;

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
	MyHTable<int, int> mappingL;
	MyHTable<int, int> mappingR;

	/* Create substitution entries for common vars created in merge ancestor */
	_identifyAncestorVars(l1, mappingL, r1, mappingR);
	axis = mappingL.count();

	MyHTable<PPL::Constraint, int, HashCons> indexedPtrsL;
	MyHTable<PPL::Constraint, int, HashCons> indexedPtrsR;

	if (!noPtr) {
#ifdef POLY_DEBUG
		cout << "Identifying address expressions appearing on both sides\n";
#endif

		/*
		 * Attempts to index each pointer by the expression of their address in terms of ancestor variables
		 *
		 * The hashkey is the linear expression
		 * The hashvalue is the (original) memory location index.
		 * */

		l1._indexPointersByExpr(indexedPtrsL, mappingL);
		r1._indexPointersByExpr(indexedPtrsR, mappingR);

		/*
		 * Pointer pairs with the same expression are equivalent, so we add a substitution for each one of them, so
		 * they will be mapped to the same variable number.
		 */
#ifdef POLY_DEBUG
				
		cout << "Memory locations appearing on both states: ";
#endif
		for (MyHTable<PPL::Constraint, int, HashCons>::PairIterator it(indexedPtrsL); it; it++) {
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

				axis += 2;
				Ident idInputL(ptrIdxL, Ident::ID_MEM_VAL_INPUT);
				Ident idInputR(ptrIdxR, Ident::ID_MEM_VAL_INPUT);
				bool hasInput = false;
				if (l1.hasIdent(idInputL) || r1.hasIdent(idInputR)) {
					Variable valLI = l1.getVarOrNew(idInputL);
					Variable valRI = r1.getVarOrNew(idInputR);
					mappingL[valLI.id()] = axis;
					mappingR[valRI.id()] = axis;
					axis++;
					hasInput = true;
				}

#ifdef POLY_DEBUG
				cout << "ptr" << ptrIdxL << "/ptr" << ptrIdxR << "(input=" << hasInput << "), ";
#endif
			}
		}
#ifdef POLY_DEBUG
		cout << endl;
#endif

		/*
		 * Add pointer-from-initial-state for each global variable without a corresponding ptr in other state
		 */

		_doMatchGlobals(l1, r1, axis, mappingL, mappingR);
		_doMatchGlobals(r1, l1, axis, mappingR, mappingL);  // TODO check
	}

	/*
	 * Finally, add a substitution for each register that appears in both states.
	 */

	for (MyHTable<Ident, int, HashIdent>::PairIterator it(l1.id2axis); it; it++) {
		const Ident &ident = (*it).fst;
		if ((ident.getType() != Ident::ID_REG_INPUT) && (ident.getType() != Ident::ID_REG) && (ident.getType() != Ident::ID_LOOP)) {
			continue;
		}
		if (r1.id2axis.hasKey(ident)) {
			mappingL.put((*it).snd, axis);
			mappingR.put(r1.id2axis[ident], axis);
			axis++;
		}
	}
	if (!noPtr) {
#ifdef POLY_DEBUG
		cout << "Dans L: " << endl;
#endif
		_doMatchSummaries(l1, r1, axis, mappingL, mappingR, indexedPtrsL, indexedPtrsR);
#ifdef POLY_DEBUG
		cout << "Dans R: " << endl;
#endif
		_doMatchSummaries(r1, l1, axis, mappingR, mappingL, indexedPtrsR, indexedPtrsL);
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
		case sem::SHL: // d <- unsigned(a) << b
			b = getConstant(*vs2, cst_n, cst_d);
			if (b && (cst_d == 1)) {
				poly.add_constraint(*v == *vs1 * (1 << PPL::raw_value(cst_n).get_ui()));
			} else {
				poly.add_constraint(*v >= *vs1);
			}
			break;
		case sem::CMP:  // d <- a ~ b
		case sem::CMPU: // d <- a ~u b // TODO handle signedness FIXME
		case sem::SUB:  // d <- a - b
			poly.add_constraint(*v == *vs1 - *vs2);
			break;
		case sem::SHR: // d <- unsigned(a) >> b
		case sem::ASR: // d <- a >> b
			b = getConstant(*vs2, cst_n, cst_d);
			if (b && (cst_d == 1)) {
				poly.add_constraint(*vs1 == *v * (1 << PPL::raw_value(cst_n).get_ui()));
			} else {
				poly.add_constraint(*vs1 >= *v);
			}
			break;
		case sem::MUL:
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

void PPLDomain::enableSummary() {
	_summary = new PPLSummary();
}
p::feature POLY_ANALYSIS_FEATURE("otawa::poly::POLY_ANALYSIS_FEATURE", new Maker<PolyAnalysis>());

Identifier<int> LOC_VAR_SIZE("otawa::poly::LOC_VAR_SIZE", 4);
Identifier<int> NUM_LOC_VARS("otawa::poly::NUM_LOC_VARS", 8);
Identifier<int> MAX_AXIS("otawa::poly::MAX_AXIS", 512);

} // namespace poly
} // namespace otawa
