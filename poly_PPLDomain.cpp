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
	delete linbounds;
}

void PPLDomain::print(io::Output &out) const {
	static char buf[64];

	if (isBottom()) {
		out << "BOTTOM";
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	bool is_loop_bound = !hasIdent(id_ssp);

	int ncons = 0;

	if (!is_loop_bound) {
		out << "Constraints: ";
	} else out << "Bound: " ;

	out << poly << endl;

	for (WPoly::ConsIterator it(poly); it; it++) {
		const WCons &c = *it;
		ncons++;

		bool firstTerm = true;
		for (WCons::TermIterator it2(*it); it2; it2++) {
			WVar v = *it2;
			const PPL::Coefficient &coef = c.coefficient(v);

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
	if (is_loop_bound)
		return;
	out << endl;

	out << "Variable count: " << poly.variable_count() << ", Constraints count: " << ncons << endl;
	displayIdentMap(out);
	displayLocVars(out);
	displayGlobVars(out);
	out << endl;

	if (_summary != nullptr) {
		cout << "Summary info: " << endl;
		cout << "- Inputs: ";
		for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
			const Pair<Ident, guid_t> &p = *it;
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

	ASSERT((_ws == nullptr) || (b._ws == nullptr) || (_ws == b._ws));
	ASSERT(victims.count() == 0);

	if (isBottom() != b.isBottom())
		return false;

	/*
	 * If we are summarizing, test if the summaries are equivalent
	 */
	ASSERT((_summary == nullptr) == (b._summary == nullptr));
	if (_summary != nullptr) {
		if (!_summary->equals(*b._summary)) {
			return false;
		}
	}

	/*
	 * First, attempt to show that the states are different using quick checks.
	 */
	if (poly.variable_count() != b.poly.variable_count()) {
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: pas le meme nombre de variables" << endl;
#endif
		return false;
	}

	if (compare_reg != b.compare_reg) {
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: pas le meme compare reg" << endl;
#endif
		return false;
	}

	if (compare_op != b.compare_op) {
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: pas le meme compare op" << endl;
#endif
		return false;
	}

	if (idmap.count() != b.idmap.count()) {
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: pas le meme idmap count" << endl;
#endif
		return false;
	}

	if (victims != b.victims) {
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: pas le meme victim map" << endl;
#endif
		return false;
	}

	if (bounds != b.bounds) {
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: pas le meme bounds" << endl;
#endif
		return false;
	}

	/*
	 * Try to show that the states are equals when ignoring memory locations (faster)
	 */
	PPLDomain r = b;
	PPLDomain l = *this;

	int expectedVarCount = 0;
	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		const Pair<Ident, guid_t> &p = *it;
		if ((p.fst.getType() == Ident::ID_MEM_VAL) || (p.fst.getType() == Ident::ID_MEM_ADDR) || (p.fst.getType() == Ident::ID_MEM_VAL_INPUT)) {
			continue;
		}
#ifdef POLY_DEBUG
		cout << "expect: " << p.fst << endl;
#endif
		expectedVarCount++;
	}

	_doUnify(l, r, true);

	if ((l.idmap.count() != expectedVarCount)) {
		/* There was some unmatched registers */ 
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: different ensemble de registres" << endl;
#endif
		return false;
	}

/* TODO
	if (l.poly != r.poly) {
		cout << "c est pas egal pcq: poly pas egal (reg)" << endl;
		return false;
	}
*/	
	/*
	 * At this point we are almost sure that the states are equal. We do a full unification (costly) to detect if the states are equal.
	 */
	r = b;
	l = *this;
	_doUnify(l, r);

	if ((l.idmap.count() != idmap.count()) || (r.idmap.count() != b.idmap.count())) {
		/* There was some unmatched memory locations */
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: different ensemble de memory locations" << endl;
#endif
		return false;
	}

	if (l.poly != r.poly) {
#ifdef POLY_DEBUG
		cout << "c est pas egal pcq: poly pas egal (mem)" << endl;
#endif
		return false;
	}

	return true;
}

template <class F>
PPLDomain::MapHelper<F>::MapHelper(F &pfunc, int max_in_domain)
    : _pfunc(pfunc) {
}

bool PPLDomain::RemoveMarked::maps(guid_t i, guid_t &j) const {
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
	if (isBottom()) {
		out << "diplayGlobVars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	if (!hasIdent(id_ssp)) {
		cout << "Local variables: NOT APPLICABLE" << endl;
		return;
	}
	WVar ssp = getVar(id_ssp);
	out << "Local variables: " << endl;
	for (int i = 0; i < NUM_LOC_VARS(_props) * LOC_VAR_SIZE(_props); i += LOC_VAR_SIZE(_props)) {
		WPoly poly_copy = poly;
		WVar v(num_axis);
		poly_copy.add_constraint(v == ssp - i - LOC_VAR_SIZE(_props));
		out << " [SP - " << hex(i) << "] == ";
		bool found = false;
		for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
			elm::Pair<Ident, guid_t> p = *it;
			WVar vsnd = WVar(p.snd);
			if (p.fst.getType() == Ident::ID_MEM_ADDR) {
				PPL::Coefficient infNumAddr, infDenAddr, supNumAddr, supDenAddr;
				bool maximum, minimum;
				poly_copy.maximize(v - vsnd, supNumAddr, supDenAddr, maximum);
				poly_copy.minimize(v - vsnd, infNumAddr, infDenAddr, minimum);
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
	if (isBottom()) {
		out << "diplayGlobVars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	out << "Global variables: " << endl;
	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		elm::Pair<Ident, guid_t> p = *it;
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
	int ncons = 0;
	for (WPoly::ConsIterator it(poly); it; it++, ncons++);
	return ncons;
}

bool PPLDomain::getConstant(const WVar &var, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d) const {
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
	WVar v = getVar(id);
	return getConstant(v, cst_n, cst_d);
}
void PPLDomain::getRange(const Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n,
                         PPL::Coefficient &bsup_d) const {
	WVar v = getVar(id);
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

void PPLDomain::getRange(const WVar &var, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d,
                         PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d) const {
	bool maximum, minimum;
	poly.maximize(var, bsup_n, bsup_d, maximum);
	poly.minimize(var, binf_n, binf_d, minimum);
}

bool PPLDomain::mayAlias(const WVar &v1, const WVar &v2) const {
	return !poly.relation_with(v1 == v2).implies(PPL::Poly_Con_Relation::is_disjoint());
}

bool PPLDomain::mustAlias(const WVar &v1, const WVar &v2, int offset) const {
	return poly.relation_with(v1 == v2 + offset).implies(PPL::Poly_Con_Relation::is_included());
}

void PPLDomain::displayIdentMap(io::Output &out) const {
	out << "Mapping: ";
	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		const Ident &ident = (*it).fst;
		WVar v = WVar((*it).snd);
		out << ident << ":" << v << ", ";
	}
	out << endl;
}

//comme getLoopBound() mais renvoie une surapproximation de l'expression lineaire de la variable d'induction
PPLDomain PPLDomain::getLinearExpr(const Ident &id) {
	if (isBottom()) {
		return *this;
	}
	MyHTable<int, int> inputs;
	int axis = 1;
	PPLDomain dom(*this); /* make a working copy to do the projections */
	inputs[getVar(id).id()] = 0;
	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		if (((*it).fst.getType() == Ident::ID_REG_INPUT) ||
			((*it).fst.getType() == Ident::ID_MEM_VAL_INPUT)) {
			inputs[(*it).snd] = axis;
			axis++;
		}
	}
	dom.doMap(MapWithHash(inputs));
	return dom;
}

// getLoopBound: a appeler a l'INTERIEUR de la boucle pour avoir une maximisation de la variable d'induction
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

PPLDomain PPLDomain::onLoopExitLinear(int loop, const PPLDomain &bound) const {
	PPLManager::t s_out = *this;
	cout << "before onLoopExitLinear: " << s_out << endl;
	cout << "bound: " << bound << endl;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.hasIdent(id)); /* You are supposed to be already inside the loop when you call onLoopExit() */
	WVar v = s_out.getVar(id);
	MyHTable<int,int> map;
	Vector<int> mapped;
	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		if (((*it).fst.getType() == Ident::ID_REG_INPUT) || ((*it).fst.getType() == Ident::ID_MEM_VAL_INPUT) || ((*it).fst == id)) {
			if (hasIdent((*it).fst)) {
				const WVar &v2 = getVar((*it).fst);
				map[(*it).snd] = v2.id();
				mapped.add(v2.id());
			}
		}
	}
	PPLDomain copy(bound);

	copy.doMapPoly(MapWithHash(map));
	for (WPoly::VarIterator it(copy.poly); it; it++) {
		if (!mapped.contains((*it).guid())) {
			copy.poly.unconstrain(*it);
		}
	}


	s_out.poly.intersection_assign(copy.poly);
	s_out.varKill(v);
	cout << "after onLoopExitLinear: " << s_out << endl;
	return s_out;
}

PPLDomain PPLDomain::onLoopExit(int loop, int bound) const {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.hasIdent(id)); /* You are supposed to be already inside the loop when you call onLoopExit() */
	WVar v = s_out.getVar(id);
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
	WVar v_old = s_out.getVar(id);
	WVar v_new = s_out.varNew(id, true); //pas de damage car la boucle appartient forcement a la fonction
	s_out.poly.add_constraint(v_new == v_old + 1);

	return s_out;
}

PPLDomain PPLDomain::onLoopEntry(int loop) const {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	WVar v = s_out.varNew(id, true); // idem que sur onLoopIter
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
			WPoly poly2 = res.poly;
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
	//res.victims.add(res.getVar(res.compare_reg).guid());
	res.varKill(res.compare_reg);
	res.compare_reg = Ident();
	if (res.isBottom()) {
#ifdef POLY_DEBUG
		cout << "State is Bottom after Filtering (will not propagate states to branch destination)" << endl;
#endif
		return PPLDomain(); // bottom
	}
	return res;
}
PPLDomain PPLDomain::onComposeBounds(const PPLDomain &bound) const {
	cout << "not implemented" << endl; abort();
#ifdef TODO
	PPLDomain out = bound;
	// decaler
	out.doMap(MapShift(bound.poly.space_dimension(), poly.space_dimension()));
	PPL::dimension_type i;
	for (i = 0; i < poly.space_dimension(); i++)
		out.poly.unconstrain(WVar(i));

	WPoly src(poly);
	src.add_space_dimensions_and_embed(bound.poly.space_dimension());
	out.poly.intersection_assign(src);

	// registers
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(out.id2axis); it; it++) {
		if ((*it).fst.getType() == Ident::ID_REG_INPUT) {
#ifdef POLY_DEBUG
			cout << "Link input register: " << (*it).fst << endl;;
#endif
			int nreg = (*it).fst.getId();
			Ident idRegCaller(nreg, Ident::ID_REG);
			if (hasIdent(idRegCaller))
				out.doNewConstraint(out.getVar((*it).fst) == getVar(idRegCaller));
		}
	}
	PPL::dimension_type max_axis = 0;
	for (PPL::dimension_type i = 0; i < out.poly.space_dimension(); i++) {
		if (out.axis2id[i].getType() == Ident::ID_INVALID) {
			WVar v(i);
#ifdef POLY_DEBUG
			cout << "Killing variable: " << v << endl;
#endif
			out.trash.set(i);
		}
		if (max_axis < i)
			max_axis = i;
	}
	out.num_axis = max_axis + 1;
	out.doFinalizeUpdate();
	return out;
#endif
}

PPLDomain PPLDomain::onCompose(const PPLDomain &summary) const {
	cout << "not implemented" << endl; abort();
#ifdef TODO
	PPLDomain out = summary;
	// decaler
	out.doMap(MapShift(summary.poly.space_dimension(), poly.space_dimension()));
	PPL::dimension_type i;
	for (i = 0; i < poly.space_dimension(); i++)
		out.poly.unconstrain(WVar(i));
#ifdef POLY_DEBUG
	cout << "summary decale: " << endl;
	cout << out;
#endif
	out._sanityChecks(true);
	// injecter les contraintes de l'etat appelant

	WPoly src(poly);
	src.add_space_dimensions_and_embed(summary.poly.space_dimension());
	out.poly.intersection_assign(src);

#ifdef POLY_DEBUG
	cout << "summary inter: " << endl;
	cout << out;
#endif
	out._sanityChecks(true);

	// link inputs
	genstruct::Vector<Ident> link_reg;
	genstruct::Vector<Ident> link_mem;

	// registers
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(out.id2axis); it; it++) {
		if ((*it).fst.getType() == Ident::ID_REG_INPUT) {
			/*
			if ((*it).fst.getId() == 13)
				break; // FIXME
				*/
#ifdef POLY_DEBUG
			cout << "Link input register: " << (*it).fst << endl;;
#endif
			int nreg = (*it).fst.getId();
			Ident idRegCaller(nreg, Ident::ID_REG);
			link_reg.add(idRegCaller);
			if (hasIdent(idRegCaller))
				out.doNewConstraint(out.getVar((*it).fst) == getVar(idRegCaller));
		}
	}
#ifdef POLY_DEBUG
	cout << "w/ linked input registers: " << endl;
	cout << out;
#endif
	out._sanityChecks(true);

	// memory 
	bool changes = true;
	Vector<Ident> input_done;
	Vector<Ident> input_preserve;
	while (changes) {
		changes = false;
		for (MyHTable<Ident, int, HashIdent>::PairIterator it(out.id2axis); it; it++) {
			if ((*it).fst.getType() == Ident::ID_MEM_VAL_INPUT) {
				if (input_done.contains((*it).fst))
					continue;
#ifdef POLY_DEBUG
				cout << "Link input memory variable: " << (*it).fst << endl;
#endif
				bool found = false;
				for (MyHTable<Ident, int, HashIdent>::PairIterator it2(id2axis); it2; it2++) {
					if ((*it2).fst.getType() == Ident::ID_MEM_ADDR) {
						Ident idFormalAddr((*it).fst.getId(), Ident::ID_MEM_ADDR);
						WVar formalAddr = out.getVar(idFormalAddr);
						WVar effectiveAddr = getVar((*it2).fst);
						cout << "test " << formalAddr << " avec " << effectiveAddr << endl;
						if (out.mustAlias(formalAddr, effectiveAddr)) {
							found = true;
							link_mem.add(idFormalAddr);
#ifdef POLY_DEBUG
							cout << "ajout formalAddr:" << idFormalAddr << endl;
							cout << "ajout effectiveAddr :" << (*it2).fst << endl;
#endif
							WVar formalArg = out.getVar((*it).fst);
							WVar effectiveArg = getVar(Ident((*it2).fst.getId(), Ident::ID_MEM_VAL));
							out.doNewConstraint(formalArg == effectiveArg);
							changes = true;
							input_done.add((*it).fst);
						}
					}
				}
				if (!found) {
#ifdef POLY_DEBUG
					cout << "Not found... Try initial data." << endl;
#endif
					Ident idFormalAddr((*it).fst.getId(), Ident::ID_MEM_ADDR);
					WVar formalArg = out.getVar((*it).fst);
					uint32_t address, value;
					bool ok = out.memGetInitial(idFormalAddr, address, value, false); //TODO force?
					/*
					 * Don't force. Maybe we are summarizing caller function too.
					 */
					if (ok) {
#ifdef POLY_DEBUG
						cout << "Found initial data. Value= " << hex(value) << endl;
#endif
						out.doNewConstraint(formalArg == value);
						input_done.add((*it).fst);
						changes = true;
						found = true;
					}
				}
				if (!found) {
					/* Failed to associate a value to the input. In that case, if we are summarizing caller
					 * function, then the input is propagated to caller function. */
					input_preserve.add((*it).fst);

				}
			}
		}
	}

#ifdef POLY_DEBUG
	cout << "State w/ linked inputs: " << endl;
	cout << out;
#endif
	out._sanityChecks(true);

#ifdef POLY_DEBUG
	cout << "Link special inputs " << endl;
#endif
	Ident id_sp(13, Ident::ID_REG);
	Ident id_fp(11, Ident::ID_REG);
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	Ident id_sfp(Ident::ID_START_FP, Ident::ID_SPECIAL);
	Ident id_slr(Ident::ID_START_LR, Ident::ID_SPECIAL);
	if (hasIdent(id_ssp) && out.hasIdent(id_ssp)) {
		out.doNewConstraint(getVar(id_sp) == out.getVar(id_ssp));
		out.doNewConstraint(getVar(id_fp) == out.getVar(id_sfp));
		out.doNewConstraint(getVar(id_slr) == out.getVar(id_slr));
	}

#ifdef POLY_DEBUG
	cout << "Inject loop bounds" << endl;
#endif
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		if ((*it).fst.getType() == Ident::ID_LOOP) {
			WVar v = getVar((*it).fst);
			ASSERT(!out.isVarMapped(v));
			out.id2axis[(*it).fst] = v.id();
			out.axis2id[v.id()] = (*it).fst;
		}
	}

#ifdef POLY_DEBUG
	cout << "Merge inputs" << endl;
#endif
	// Virer les inputs de la fonction interne, qui n'en sont plus
	genstruct::Vector<Ident> bye;
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(out.id2axis); it; it++) {
		if (((*it).fst.getType() == Ident::ID_MEM_VAL_INPUT) || ((*it).fst.getType() == Ident::ID_REG_INPUT)){
			if (!input_preserve.contains((*it).fst))
				bye.add((*it).fst);
		} 
	}
	for (genstruct::Vector<Ident>::Iterator it(bye); it; it++) {
		out.varKill(*it);
	}

	if (out.hasIdent(Ident(Ident::ID_START_SP, Ident::ID_SPECIAL))) {
		out.varKill(Ident(Ident::ID_START_SP, Ident::ID_SPECIAL));
		out.varKill(Ident(Ident::ID_START_FP, Ident::ID_SPECIAL));
		out.varKill(Ident(Ident::ID_START_LR, Ident::ID_SPECIAL));

		out.id2axis[Ident(Ident::ID_START_SP, Ident::ID_SPECIAL)] 
			= getVar(Ident(Ident::ID_START_SP, Ident::ID_SPECIAL)).id();
		out.axis2id[getVar(Ident(Ident::ID_START_SP, Ident::ID_SPECIAL)).id()] = Ident(Ident::ID_START_SP, Ident::ID_SPECIAL);
		out.id2axis[Ident(Ident::ID_START_FP, Ident::ID_SPECIAL)] 
			= getVar(Ident(Ident::ID_START_FP, Ident::ID_SPECIAL)).id();
		out.axis2id[getVar(Ident(Ident::ID_START_FP, Ident::ID_SPECIAL)).id()] = Ident(Ident::ID_START_FP, Ident::ID_SPECIAL);
		out.id2axis[Ident(Ident::ID_START_LR, Ident::ID_SPECIAL)] 
			= getVar(Ident(Ident::ID_START_LR, Ident::ID_SPECIAL)).id();
		out.axis2id[getVar(Ident(Ident::ID_START_LR, Ident::ID_SPECIAL)).id()] = Ident(Ident::ID_START_LR, Ident::ID_SPECIAL);
	}

#ifdef POLY_DEBUG
	cout << "Taking care of side-effects..." << endl;
#endif
	genstruct::Vector<Ident> injected_inputs;
	
	// re-inject unchanged caller variables
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
			// inject in-memory variables
			WVar callerVar = getVar((*it).fst);
			bool dmg = false;
			bool exact_dmg = false;
			for (elm::genstruct::Vector<Ident>::Iterator it2(out._summary->_damaged); it2; it2++) {
				if ((*it2).getType() != Ident::ID_MEM_VAL)
					continue;
				WVar calleeVar = out.getVar(Ident((*it2).getId(), Ident::ID_MEM_ADDR));
				if (out.mustAlias(calleeVar, callerVar)) {
					dmg = true;
					exact_dmg = true;
					break;
				}
				if (out.mayAlias(calleeVar, callerVar)) {
					dmg = true;
				}
			}
			for (elm::genstruct::Vector<Ident>::Iterator it2(link_mem); it2; it2++) {
				if ((*it2).getType() != Ident::ID_MEM_ADDR)
					continue;
				WVar calleeVar = out.getVar((*it2));
				if (out.mustAlias(calleeVar, callerVar)) {
#ifdef POLY_DEBUG
					cout << "Don't keep " << (*it).fst << " because it matches an input" << endl;
#endif
					dmg = true;
					exact_dmg = true;
					break;
				}
			}
			if (!dmg) {
				Ident idCallerVal((*it).fst.getId(), Ident::ID_MEM_VAL);
				WVar callerVal = getVar(idCallerVal);
				Ident addr, val;
				out.varCreatePtr(addr, val);
#ifdef POLY_DEBUG
				cout << "Memory variable " << (*it).fst << " was not affected by the call" << " (now known as: " << addr << ")" << endl;
#endif
				// TODO
				if (out.hasIdent(addr)) {
					int oldAxis = out.id2axis[addr];
					out.axis2id[oldAxis] = Ident();
				}
				out.id2axis[addr] = callerVar.id();
				out.axis2id[callerVar.id()] = addr;

				if (out.hasIdent(val)) {
					int oldAxis = out.id2axis[val];
					out.axis2id[oldAxis] = Ident();
				}
				out.id2axis[val] = callerVal.id();
				out.axis2id[callerVal.id()] = val;

				// verifier si cette variable est damaged dans l'appelant, et si oui le rajouter dans le damage
				if (_summary && _summary->_damaged.contains(idCallerVal)) {
					out._summary->_damaged.add(val);
				}

				//matter aussi si elle etait en input
				Ident idCallerInput((*it).fst.getId(), Ident::ID_MEM_VAL_INPUT);
				if (hasIdent(idCallerInput)) {
					Ident idInput(addr.getId(), Ident::ID_MEM_VAL_INPUT);
					WVar callerInput = getVar(idCallerInput);
					if (out.hasIdent(idInput)) {
						int oldAxis = out.id2axis[idInput];
						out.axis2id[oldAxis] = Ident();
					}
					out.id2axis[idInput] = callerInput.id();
					out.axis2id[callerInput.id()] = idInput;
#ifdef POLY_DEBUG
					cout << "injecting input: " << idInput << endl;
#endif
					injected_inputs.add(idInput);
				}
			} else {
#ifdef POLY_DEBUG
				cout << "Memory variable " << (*it).fst << " was overwritten" << endl;
#endif
				// la variable de l'appelant ne sera pas recuperee, mais si elle est dans le dmg alors il 
				// faut la recuperer quand meme sans sa valeur. Sauf si c'est un exact-dmg auquel cas
				// il faut simplement recuperer l'identifier du out.damage
				
				Ident idCallerVal((*it).fst.getId(), Ident::ID_MEM_VAL);
				Ident addr, val;
				if (exact_dmg) {
#ifdef POLY_DEBUG
					cout << "exact_dmg, do not keep variable" << endl;
#endif
				}
				if (_summary && _summary->_damaged.contains(idCallerVal) && !exact_dmg) {
#ifdef POLY_DEBUG
					cout << "represents damage: keep without its value" << endl;
#endif
					out.varCreatePtr(addr, val);
					//TODO
					if (out.hasIdent(addr)) {
						int oldAxis = out.id2axis[addr];
						out.axis2id[oldAxis] = Ident();
					}
					out.id2axis[addr] = callerVar.id();
					out.axis2id[callerVar.id()] = addr;

					out.varNew(val, false, false);
				} 

			}
		} else if ((*it).fst.getType() == Ident::ID_REG) {
			// inject register variables
			bool dmg = false;
			for (elm::genstruct::Vector<Ident>::Iterator it2(out._summary->_damaged); it2; it2++) {
				if ((*it2).getType() != Ident::ID_REG)
					continue;
				if ((*it2).getId() == (*it).fst.getId()) {
					dmg = true;
					break;
				}
			}
			if (!dmg) {
#ifdef POLY_DEBUG
				cout << "Register variable " << (*it).fst << " was not affected by the call" << endl;
#endif
				Ident idCallerVal = (*it).fst;
				WVar callerVal = getVar(idCallerVal);
				if (out.hasIdent((*it).fst)) {
					int oldAxis = out.id2axis[(*it).fst];
					out.axis2id[oldAxis] = Ident();
				}
				out.id2axis[(*it).fst] = callerVal.id();
				if (_summary && _summary->_damaged.contains(idCallerVal)) {
					out._summary->_damaged.add(idCallerVal);
				}
				out.axis2id[callerVal.id()] = (*it).fst;
			} else {
#ifdef POLY_DEBUG
				cout << "Register variable " << (*it).fst << " was overwritten" << endl;
#endif
			}

		}

	}

	// Recuperer les inputs de la fonction appelante
	/*
	genstruct::Vector<Ident> hello;
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		if (((*it).fst.getType() == Ident::ID_MEM_VAL_INPUT) || ((*it).fst.getType() == Ident::ID_REG_INPUT)){
			hello.add((*it).fst);
		} 
	}
	for (genstruct::Vector<Ident>::Iterator it(hello); it; it++) {
		WVar v = getVar(*it);
		//TODO
		if (out.hasIdent(*it)) {
			int oldAxis = out.id2axis[*it];
			out.axis2id[oldAxis] = Ident();
		}
		out.id2axis[*it] = v.id();
		out.axis2id[v.id()] = *it;
	}
	*/

#ifdef POLY_DEBUG
	cout << "after input merge" << endl;
#endif
	// menache
#ifdef POLY_DEBUG
	cout << "Post-composition cleanup: " << endl;
#endif

	if (out.isBottom()) {
		/* Composition resulted in bottom state for some reason. Will re-analyse function */
		return PPLDomain();
	}
	PPL::dimension_type max_axis = 0;
	for (PPL::dimension_type i = 0; i < out.poly.space_dimension(); i++) {
		if (out.axis2id[i].getType() == Ident::ID_INVALID) {
			WVar v(i);
#ifdef POLY_DEBUG
			cout << "Killing variable: " << v << endl;
#endif
			out.trash.set(i);
		}
		if (max_axis < i)
			max_axis = i;
	}
	out.num_axis = max_axis + 1;
	out.doFinalizeUpdate();
	
	
	if (!_summary && out._summary) {
		delete out._summary;
		out._summary = nullptr;
	}	

	return out;
#endif
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

	/*
	ASSERT(!trash.countOnes());
	ASSERT(!r.trash.countOnes());
	ASSERT(compare_reg.getType() == Ident::ID_INVALID);
	ASSERT(r.compare_reg.getType() == Ident::ID_INVALID);
	*/

#ifdef POLY_DEBUG
	cout << "Non-trivial merge, type=" << (widen ? "widening" : "convex-hull") << endl;
#endif

	/* We have a non-trivial merge, so we will need working copies of l/r to perform the substitutions. */
	PPLDomain l1 = *this;
	PPLDomain r1 = r;

	_doUnify(l1, r1);

	for (int i = 0; i < r1.bounds.length(); i++)
		l1.setBound(i, r1.getBound(i));

	l1.poly.poly_hull_assign(r1.poly);
	if (widen)
		l1.poly.bounded_H79_extrapolation_assign(r1.poly);
	l1.doFinalizeUpdate();
	for (int i = 0; i < r1.bounds.length(); i++)
		l1.setBound(i, r1.getBound(i));
	return l1;





#ifdef POLY_DEBUG
	cout << "Joined state:" << endl;
	cout << l1;
	fflush(stdout);
#endif
exit(0);
#ifdef TODO
//	cout << "before " << (widen ? "widening" : "join") << ", l= " << l1.getConsCount() << " r=" << r1.getConsCount() << endl;

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
		WCons_System dummy;
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
#ifdef POLY_DEBUG
				cout << "compare " << (*it2) << " avec: " << (*it) << endl;
#endif
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
#endif
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
			WVar v = s_out.varNew(id, true, true); // damage du registre ecrit
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
			WVar vs = s_out.getVar(id2);

			sem::reg_t dest = si.d();
			Ident id(dest, Ident::ID_REG);
			WVar v = s_out.varNew(id, true, true); // damage du registre ecrit

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

			WVar vs1 = s_out.getVar(id1);
			WVar vs2 = s_out.getVar(id2);
			WVar v = s_out.varNew(id, true, true); // damage du registre destination de l'operation binaire
			s_out._doBinaryOp(si.op, &v, &vs1, &vs2);
			break;
		}
		case sem::STORE: // MEMb(a) <- d
		{

			sem::reg_t addr = si.a();
			Ident idStoreAddr(addr, Ident::ID_REG);

			//TODO move this shit in some handleInput() method, and call it on other register ops, too 
			WVar storeAddr = s_out.getVarOrNew(idStoreAddr, true); //l'adresse d'ecriture peut etre un input

			sem::reg_t src = si.d();
			Ident idStoreValue(src, Ident::ID_REG);
			bool isInput = !s_out.hasIdent(idStoreValue);
			WVar storeValue = s_out.getVarOrNew(idStoreValue, true); //la valeur d'ecriture peut etre un input
			if (isInput) {
				// we are summarizing
				Ident idSSP(Ident::ID_START_SP, Ident::ID_SPECIAL);
				const WVar &vSSP = s_out.getVar(idSSP);
				s_out.doNewConstraint(storeValue >= vSSP);
			}

/*
			for (MyHTable<Ident, int, HashIdent>::PairIterator it(s_out.id2axis); it; it++)
			{
				elm::Pair<Ident, int> p = *it;
				if (p.fst.getType() == Ident::ID_LOOP) {
					MyHTable<int,int> map;
					Ident id_frame(Ident::ID_START_SP, Ident::ID_SPECIAL);
					WVar v_frame = s_out.getVar(id_frame);
					WVar v_bound = s_out.getVar(p.fst);
					map[storeAddr.id()] = 0;
					map[v_frame.id()] = 1;
					map[v_bound.id()] = 2;
					PPLDomain tmp = s_out;
					tmp.doMapPoly(MapWithHash(map));
					const WCons_System &cons = tmp.poly.minimized_constraints();
					for (WCons_System::const_iterator it2 = cons.begin(); it2 != cons.end(); it2++) {
						const WCons &c = *it2;
						const PPL::Coefficient &coef = c.coefficient(WVar(2));
						const PPL::Coefficient &coef2 = c.coefficient(WVar(0));
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

			for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = s_out.idmap.getPairIter(); it; it++) {
				const Ident &idCurrent = (*it).fst;
				if (idCurrent.getType() == Ident::ID_MEM_ADDR) {
					const WVar &current = WVar((*it).snd);
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
				const WVar &equiv = s_out.getVar(idEquiv);
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
				const WVar &overlap = s_out.getVar((*it));
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
			WVar loadAddr = s_out.getVar(idLoadAddr);
			WVar loadReg = s_out.varNew(idLoadReg, true, true); // damage du registre destination du LOAD
			bool found = false;

			/*
			 * Looking for existing abstract location equivalent to load address.
			 */
			for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = s_out.idmap.getPairIter(); it; it++) {
				if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
					WVar current = WVar((*it).snd);

					if (mustAlias(loadAddr, current)) {
#ifdef POLY_DEBUG
						cout << "LOAD: Found equivalent abstract location: " << (*it).fst << endl;
#endif
						Ident idExistingValue((*it).fst.getId(), Ident::ID_MEM_VAL);
						WVar existingValue = s_out.getVar(idExistingValue);
						s_out.doNewConstraint(loadReg == existingValue);
						found = true;
						break;
					}
				}
			}

			if (!found) {

				uint32_t concreteAddress, initialValue;
				if (s_out.memGetInitial(idLoadAddr, concreteAddress, initialValue)) {
					if ( _ws->process()->program()->findSegmentAt(concreteAddress)->isWritable()) {
						s_out.memCreate(loadAddr, loadReg, false);
#ifdef POLY_DEBUG
						cout << "LOAD: Writable initial value exists, creating new ptr..." << endl;
#endif
					}
#ifdef POLY_DEBUG
					else {
						cout << "LOAD: Initial value is constant, do not create ptr..." << endl;
					}
#endif
					s_out.doNewConstraint(loadReg == initialValue);
				} else {
					const WVar &v = s_out.memCreate(loadAddr, loadReg, false);
#ifdef POLY_DEBUG
					cout << "LOAD: Not found, creating new ptr..." << endl;
#endif
					if (s_out._summary != nullptr) {
						// Unknown LOAD value. If we are summarizing, create an input.
						Ident idInputAddr = s_out.getIdent(v);
						Ident idInputVal = Ident(idInputAddr.getId(), Ident::ID_MEM_VAL_INPUT);
						Ident idCurrentVal = Ident(idInputAddr.getId(), Ident::ID_MEM_VAL);
						WVar currentVal = s_out.getVar(idCurrentVal);
						WVar inputVal = s_out.varNew(idInputVal); // c'est une variable qu'on lit donc pas de damaged
#ifdef POLY_DEBUG
						cout << "Summarizing: creating new input memory: " << " what= " << idInputVal << " where=" << idInputAddr << endl;
#endif
	//					s_out._summary->_inputs.add(idInputVal);
						s_out.doNewConstraint(currentVal == inputVal);

						
					}
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
	if (poly.variable_count() > 0) {
		MapHelper<F> a(pfunc, poly.highest_guid() - 1);
		poly.map_vars(a);
	}
}

template <class F> void PPLDomain::doMapIdents(F pfunc) {
	Mapping new_idmap;

#ifdef POLY_DEBUG
	cout << "Remapping: ";
#endif
	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		guid_t old_guid = (*it).snd;
		guid_t new_guid = (*it).snd;
		if (pfunc.maps(old_guid, new_guid)) {
			new_idmap.add((*it).fst, new_guid);

			if (old_guid != new_guid) {
#ifdef POLY_DEBUG
				cout << (*it).fst  << "[" << WVar(old_guid) << "->" << WVar(new_guid) << "] ";
#endif
			}
		}
	}
#ifdef POLY_DEBUG
	cout << endl;
#endif
	idmap = new_idmap;
	// TODO take care of damage
}

template <class F> void PPLDomain::doMap(F pfunc, bool noproj) {
	doMapPoly(pfunc, noproj);
	doMapIdents(pfunc);
}

void PPLDomain::doScratch(Ident &id) {
	if (!hasIdent(id)) {
		return;
	}

	WVar v = getVar(id);
	poly.unconstrain(v);
}

void PPLDomain::doIntegerWrap() {
	// TODO(clement): integer wrap not supported yet
}

void PPLDomain::doKillTemporaries() {
	Vector<Ident> toDel;

	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++)
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

	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++)
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

	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		elm::Pair<Ident, guid_t> p = *it;
		WVar v = WVar((*it).snd);

		if (p.fst.getType() == Ident::ID_MEM_ADDR) {
			Ident idSp(13, Ident::ID_REG);
			WVar sp = getVar(idSp);

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
	if (victims.count()  == 0) {
		cout << "Nothing to clean" << endl;
		_sanityChecks();
		return;
	}
#endif
	for (Vector<guid_t>::Iter it(victims); it; it++)
		poly.unconstrain(WVar(*it));

	victims.clear();
	_sanityChecks();
}

WVar PPLDomain::varNew(const Ident &ident, bool allow_replace, bool create_damaged) {
	WVar v;
	if (idmap.has1(ident)) {
		ASSERT(allow_replace);
		victims.add(idmap.find1(ident));
#ifdef POLY_DEBUG
		cout << "Variable " << idmap.find1(ident) << ", formerly associated with ident " << ident << ", will be replaced. " << endl;
#endif
	}
	idmap.add(ident, v.guid());
#ifdef TODO
	if (_summary != nullptr && create_damaged) {
		if ((ident.getType() == Ident::ID_REG) && ((ident.getId() >= 0) && ident.getId() <= 3)) {
			// TODO tester les registres qu'il faut garder en fonction de la convention d'appel
			_summary->_damaged.add(ident);
#ifdef POLY_DEBUG
			cout << "Add " << ident << " to the damaged set" << endl;
#endif
		}
		if ((ident.getType() == Ident::ID_MEM_VAL)) { 
			WVar v = getVar(Ident(ident.getId(), Ident::ID_MEM_ADDR));

			Ident idSsp(Ident::ID_START_SP, Ident::ID_SPECIAL);
			WVar ssp = getVar(idSsp);

			if (poly.relation_with(v < ssp).implies(PPL::Poly_Con_Relation::is_included()) &&
				poly.relation_with(v >= int(stackconf_t::STACK_TOP - stackconf_t::STACK_SIZE)).implies(PPL::Poly_Con_Relation::is_included())) {
				// is local variable.. do not add in damaged set
#ifdef POLY_DEBUG
				cout << "WVar " << ident << " not added to damaged set because it is local var." << endl;
#endif
			} else {
#ifdef POLY_DEBUG
				cout << "WVar " << ident << " added to damaged set because it may be a non-local var." << endl;
#endif
				_summary->_damaged.add(ident);
			}
		}
	}
#endif
	return v;
}

WVar PPLDomain::getVar(const Ident &ident) const { return WVar(idmap.find1(ident)); }

WVar PPLDomain::getVarOrNew(const Ident &ident, bool create_input) {
	if (!hasIdent(ident)) {
		WVar v = varNew(ident, false);
		if (create_input && _summary != nullptr) {
			/* Read from untracked register. If we are summarizing, create a new input register */
			ASSERT(ident.getType() == Ident::ID_REG);
			Ident idInput(ident.getId(), Ident::ID_REG_INPUT);
//			_summary->_inputs.add(idInput);
			WVar input = varNew(idInput);
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

bool PPLDomain::hasIdent(const Ident &ident) const { return idmap.has1(ident); }

WVar PPLDomain::memReplace(const WVar &address, const WVar &valueSource) {
	const Ident &idOldAddress = getIdent(address);
	const Ident &idOldValue = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL);
	WVar oldValue = getVar(idOldValue);

	Ident idNewAddress, idNewValue;
	varCreatePtr(idNewAddress, idNewValue);

	const WVar &newAddress = varNew(idNewAddress, false);
	doNewConstraint(address == newAddress);

	const WVar &newValue = varNew(idNewValue, false, true); //memReplace donc on cree un damaged
	doNewConstraint(newValue == valueSource);

#ifdef TODO
	const Ident &idOldInput = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL_INPUT);
	if (hasIdent(idOldInput)) {
		ASSERT(_summary);
		const WVar &oldInput = getVar(idOldInput);
		const Ident idNewInput(idNewAddress.getId(), Ident::ID_MEM_VAL_INPUT);
		const WVar &newInput= varNew(idNewInput, false, false);
		doNewConstraint(newInput == oldInput);
/*		_summary->_inputs.remove(idOldInput);
		_summary->_inputs.add(idNewInput); */ 
#ifdef POLY_DEBUG
		cout << "Summarizing: migrating input from " << idOldInput << " to " << idNewInput << endl;
#endif
		varKill(oldInput);
	}
#endif
	varKill(oldValue);
	varKill(address);
	return newAddress;
}

WVar PPLDomain::memCreate(const WLinExpr & address , const WLinExpr &valueSource, bool damage) {
	Ident idNewAddress, idNewValue;
	varCreatePtr(idNewAddress, idNewValue);

	const WVar &newAddress = varNew(idNewAddress, false);
	doNewConstraint(newAddress == address);

	const WVar &newValue = varNew(idNewValue, false, damage); //memCreate le damage ca depend
	doNewConstraint(newValue == valueSource);

	


	return newAddress;
}

WVar PPLDomain::memMerge(const WVar &address, const WVar &newValue) {
	const Ident &idOldAddress = getIdent(address);
	const Ident &idOldValue = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL);
	const WVar oldValue = getVar(idOldValue);

	PPLDomain tempState = *this;
	ASSERT(idmap == tempState.idmap);

	WVar newAddr1 = tempState.memReplace(address, newValue);
	WVar::_guid_generator -= 2; // TODO HACK FIXME 
	WVar newAddr2 = memReplace(address, oldValue);
	ASSERT(idmap == tempState.idmap);
	ASSERT(newAddr1.id() == newAddr2.id());
	poly.poly_hull_assign(tempState.poly);
	return newAddr1;
}

bool PPLDomain::memGetInitial(const Ident &id, uint32_t &address, uint32_t &value, bool force) {
	PPL::Coefficient num, den;
	/* TODO(clement) : use correct size  */
	if (getConstant(id, num, den)) {
		address = PPL::raw_value(num).get_ui() / PPL::raw_value(den).get_ui();
#ifdef POLY_DEBUG
		cout << "Address is statically known (" 
			<< hex(address) 
			<< "), attempting to read value from initial state" << endl;
#endif

		bool iswriteable = _ws->process()->program()->findSegmentAt(address)->isWritable();
//		cout << "Is writable?" << iswriteable << endl;
		if (_summary && !force && iswriteable) {
			//TODO
#ifdef POLY_DEBUG
			cout << "Writeable address, and we are summarizing... mark as input" << endl;
#endif
			return false;
		}
		try {
			dfa::INITIAL_STATE(_ws)->get(address, value);
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
void PPLDomain::_sanityChecks(bool allow_holes) {
	int max_axis = -1;
	if (isBottom()) {
		ASSERT(poly.is_empty());
		return;
	}
	ASSERT(!poly.is_empty());
#ifdef TODO
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
		if (axis2id[i].getType() == Ident::ID_INVALID) {
			if (!allow_holes) {
				cout << "State has \"hole\" at axis " << i << endl;
				abort();
			}
			continue;
		}
		Ident &ident = axis2id[i];
		ASSERT(id2axis[ident] == i);
	}
#endif
	/*
	ASSERT(max_axis + 1 <= num_axis);
	ASSERT(max_axis + 1 + trash.countOnes() >= num_axis);
	ASSERT(poly.space_dimension() <= (unsigned)num_axis); 
	ASSERT(trash.size() >= num_axis);
	ASSERT(trash.countOnes() <= num_axis);
	*/
}
#endif

/*
void PPLDomain::_doFreeAxis(int axis) {
	ASSERT(axis2id[axis].getType() != Ident::ID_INVALID);
#ifdef POLY_DEBUG
	cout << "WVar " << WVar(axis) << ", mapped to identifier " << axis2id[axis]
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
	cout << "obsolete method" << endl;
	abort();
	ASSERT(!isBottom());
	ASSERT(num_axis < trash.size())
	ASSERT(allow_replace || !id2axis.hasKey(ident));
	if (id2axis.hasKey(ident)) {
		int axis = id2axis[ident];
#ifdef POLY_DEBUG
		cout << "The identifier " << ident << " was mapped to variable " << WVar(axis) << endl;
#endif
		_doFreeAxis(axis);
	}
	id2axis[ident] = num_axis;
	if (axis2id.length() <= num_axis) {
		axis2id.setLength(num_axis + 1);
	}
	axis2id[num_axis] = ident;
#ifdef POLY_DEBUG
	cout << "New variable " << WVar(num_axis) << " created for identifier " << ident << endl;
#endif
	num_axis++;
	return num_axis - 1;
}
*/
// Return the first constraint in poly for which the coef of specified variable axis is non-zero
const WCons *PPLDomain::_getConstraintFor(int axis) const {
	for (WPoly::ConsIterator it(poly); it; it++) {
		const WCons &c = *it;
		if (!c.is_equality()) {
			continue;
		}
		if (c.coefficient(WVar(axis)) != 0) {
			return new WCons(c);
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
	cout << "not implemented" << endl; abort();
#ifdef TODO
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
#endif
}

/*
 * Indexes the pointer in dom, by their expression in terms of registers referenced in map_regs.
 * Stores the result in map_ptr.
 */
void PPLDomain::_indexPointersByExpr(MyHTable<WCons, int, HashCons> &map_ptr,
                                     MyHTable<int, int> &commonRegs) const {
	cout << "not implemented" << endl; abort();
#ifdef TODO
	int axis = commonRegs.count();
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
			commonRegs.put((*it).snd, axis);
			PPLDomain dom(*this); /* make a working copy to do the projections */
			dom.doMapPoly(MapWithHash(commonRegs));

			const WCons *cons = dom._getConstraintFor(axis);
			if (cons != nullptr) {
				map_ptr[*cons] = (*it).fst.getId();
				delete cons;
			}
			commonRegs.remove((*it).snd);
		}
	}
#endif
}

void PPLDomain::_doMatchSummaries(PPLDomain &l1, PPLDomain &r1, unsigned int& axis, 
		MyHTable<int,int> &mappingL, MyHTable<int,int> &mappingR,
		MyHTable<WCons, int, HashCons> &indexPtrsL,
		MyHTable<WCons, int, HashCons> &indexPtrsR) const {

#ifdef TODO

	for (MyHTable<Ident, int, HashIdent>::PairIterator it(l1.id2axis); it; it++) {
		guid_t old_axis = (*it).snd;
		guid_t new_axis = (*it).snd;

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
					WVar addrL = l1.getVar(idAddrL);
					mappingL.put(addrL.id(), axis);
					WVar addrR = r1.varNew(idAddrR);
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
					WCons c;
					WLinExpr le;
					bool found = false;
					for (MyHTable<WCons, int, HashCons>::PairIterator it(indexPtrsL); it; it++) {
						if ((*it).snd == idAddrL.getId()) {
							c = (*it).fst;
							found = true;
							break;
						}
					}
					if (!found) {
						cout << "STATE: " << endl;
						cout << "left=" << endl;
						cout << l1;
						cout << "right=" << endl;
						cout << r1;

						cout << "For the identifier " << idAddrL << " we didn't find any mapped variable " <<endl;
						ASSERT(false);
					}
					char buf[128];

					for (guid_t i = 0; i < c.space_dimension(); i++) {
						WVar v(i);
						WVar vc(0);
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
								vc = WVar(common_axis);
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
							WVar v2(axis_in_R);
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

					WCons c2;
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
					WVar regVar = r1.varNew((*it).fst);
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
#endif
}

void PPLDomain::_doMatchGlobals(PPLDomain &l1, PPLDomain &r1, 
		MyHTable<guid_t, Vector<PPL::Coefficient> > &leftVMap, 
		MyHTable<Vector<PPL::Coefficient> , guid_t, VectCoefIdent> &invRightVMap) const {
		// Look for unmatched global variables
		for (MyHTable<guid_t, Vector<PPL::Coefficient> >::PairIterator it(leftVMap); it; it++) {
			Vector<PPL::Coefficient> vect = (*it).snd;
			int i;
			for (i = 0; (i < vect.length() - 2) && (vect[i] == 0); i++);
			if (i == vect.length() - 2) {
#ifdef POLY_DEBUG				
				cout << "GLOBAL: " << WVar((*it).fst) << endl;
#endif
				if (!invRightVMap.hasKey((*it).snd)) {
#ifdef POLY_DEBUG				
					cout << "Global does not exists in other state." << endl;
#endif
					uint32_t staticAddr = 
						PPL::raw_value(- vect[vect.length() - 1] / vect[vect.length() - 2]).get_ui();
					uint32_t staticVal;
					dfa::INITIAL_STATE(_ws)->get(staticAddr, staticVal);
#ifdef POLY_DEBUG				
					cout << "Static adress: " << hex(staticAddr) << " value: " << hex(staticVal) << endl;
#endif

					Ident leftIdAddr = l1.getIdent(WVar((*it).fst));
					Ident leftIdVal = Ident(leftIdAddr.getId(), Ident::ID_MEM_VAL);
					WVar val = l1.getVar(leftIdVal);

					r1.poly.add_constraint(WVar((*it).fst) == staticAddr);
					r1.poly.add_constraint(val == staticVal);

					Ident rightIdAddr, rightIdVal;
					r1.varCreatePtr(rightIdAddr, rightIdVal);
					r1.idmap.add(rightIdAddr, (*it).fst);
					r1.idmap.add(rightIdVal, val.guid());
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

	MyHTable<guid_t,guid_t> rename;
	if (!noPtr) {
		std::set<guid_t> leftVars = _collectPolyVars(l1.poly); // aka V1 dans l'algo
		std::set<guid_t> rightVars = _collectPolyVars(r1.poly); // aka V2 dans l'algo
		std::set<guid_t> commonVars /* C dans l'algo */ , leftOnlyVars /* V1' dans l'algo */, rightOnlyVars /* V2' dans l'algo */;
		std::set_intersection(leftVars.begin(), leftVars.end(), rightVars.begin(), rightVars.end(), std::inserter(commonVars, commonVars.begin()));
		std::set_difference(leftVars.begin(), leftVars.end(), rightVars.begin(), rightVars.end(), std::inserter(leftOnlyVars, leftOnlyVars.begin()));
		std::set_difference(rightVars.begin(), rightVars.end(), leftVars.begin(), leftVars.end(), std::inserter(rightOnlyVars, rightOnlyVars.begin()));


		WPoly commonPoly = l1.poly;
		commonPoly.poly_hull_assign(r1.poly); /* TODO PERF : maybe we can avoid this operation */
#ifdef POLY_DEBUG
		cout << "Common vars: ";
		for (std::set<guid_t>::const_iterator it = commonVars.begin(); it != commonVars.end(); it++)
			cout << "v" << *it << " ";

		cout << endl;
#endif

		std::set<guid_t> indepVars; /* C' dans l'algo */
		for (std::set<guid_t>::const_iterator it = commonVars.begin(); it != commonVars.end(); it++) {
			WPoly temp = commonPoly;
			WVar v(*it);
			temp.filter_lambda([v, &indepVars](guid_t guid) {
				return ((guid == v.guid()) || (indepVars.find(guid) != indepVars.end()));
			});

			WPoly::ConsIterator it2(temp);
			for (; it2 && !(*it2).is_equality(); it2++);
			if (!it2) {
				if (v.guid() < 20)
				indepVars.insert(*it);
			}
		}

#ifdef POLY_DEBUG
		cout << "Independant common vars: ";
		for (std::set<guid_t>::const_iterator it = indepVars.begin(); it != indepVars.end(); it++)
			cout << "v" << *it << " ";

		cout << endl;
#endif
		MyHTable<guid_t, Vector<PPL::Coefficient> > leftVMap;
		MyHTable<guid_t, Vector<PPL::Coefficient> > rightVMap;
		_identifyPolyVars(l1, leftOnlyVars, indepVars, leftVMap);
		_identifyPolyVars(r1, rightOnlyVars, indepVars, rightVMap);

		MyHTable<Vector<PPL::Coefficient> , guid_t, VectCoefIdent> invLeftVMap, invRightVMap;

		for (MyHTable<guid_t, Vector<PPL::Coefficient> >::PairIterator it(leftVMap); it; it++) {
			invLeftVMap.put((*it).snd, (*it).fst);
		}

		for (MyHTable<guid_t, Vector<PPL::Coefficient> >::PairIterator it(rightVMap); it; it++) {
			invRightVMap.put((*it).snd, (*it).fst);
			if (invLeftVMap.hasKey((*it).snd)) {
#ifdef POLY_DEBUG
				cout << "Variable " << WVar(invLeftVMap[(*it).snd]) << "(left) == " << WVar((*it).fst) << "(right)" << endl;
#endif
				// rename memory address variable
				rename.add(invLeftVMap[(*it).snd], (*it).fst);

				// also rename memory value variable
				Ident leftIdAddr = l1.getIdent(WVar(invLeftVMap[(*it).snd]));
				Ident rightIdAddr = r1.getIdent(WVar((*it).fst));
				Ident leftIdVal = Ident(leftIdAddr.getId(), Ident::ID_MEM_VAL);
				Ident rightIdVal = Ident(rightIdAddr.getId(), Ident::ID_MEM_VAL);
				rename.add(l1.getVar(leftIdVal).guid(), r1.getVar(rightIdVal).guid());
			}
		}

		// _doMatchGlobals(l1, r1, leftVMap, invRightVMap);
//		_doMatchGlobals(r1, l1, rightVMap, invLeftVMap);


		// take care of memory variables that were already same()
		for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = l1.idmap.getPairIter(); it; it++) {
			for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it2 = r1.idmap.getPairIter(); it2; it2++) {
				WVar x1((*it).snd);
				WVar x2((*it2).snd);
				if (((*it).fst.getType() == Ident::ID_MEM_ADDR) || ((*it).fst.getType() == Ident::ID_MEM_VAL)) {
					if (x1.guid() == x2.guid()) {
						rename.add(x1.guid(), x2.guid());
					}
				}
			}
		}
	}

	// rename register variables
	for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it = l1.idmap.getPairIter(); it; it++) {
		if (((*it).fst.getType() != Ident::ID_MEM_ADDR) && ((*it).fst.getType() != Ident::ID_MEM_VAL))
			if (r1.idmap.has1((*it).fst)) {
				rename.add((*it).snd, r1.idmap.find1((*it).fst));
				// cout << "[R] mapping " << (*it).snd << " to " << r1.idmap.find1((*it).fst) << endl;
			}
	}
	//_doMatchGlobals(l1, r1, axis, mappingL, mappingR);
	//

	MapGuid mg(rename);
	l1.doMap(mg);

#ifdef POLY_DEBUG
	cout << "Unified:" << endl;
	cout << "Left state: " << endl;
	cout << l1;
	fflush(stdout);

	cout << "Right state: " << endl;
	cout << r1;
#endif
	return;
}

void PPLDomain::_doBinaryOp(int op, WVar *v, WVar *vs1, WVar *vs2) {
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

void PPLDomain::_identifyPolyVars(const PPLDomain &d, const std::set<guid_t> &vars, const std::set<guid_t> &indep, MyHTable<guid_t, Vector<PPL::Coefficient> > &vmap) const {
	const WPoly &poly = d.poly;
#ifdef POLY_DEBUG
	cout << "Begin _identifyPolyVars" << endl;
#endif
	for (std::set<guid_t>::const_iterator it = vars.begin(); it != vars.end(); it++) {
		WVar v(*it);
#ifdef POLY_DEBUG
		cout << "trying for: " << v << endl;
#endif
		if (!d.isVarMapped(v) || d.getIdent(v).getType() != Ident::ID_MEM_ADDR)
			continue;
		WPoly temp = poly;
		temp.filter_lambda([&indep, v](guid_t guid) {
				return ((guid == v.guid()) || (indep.find(guid) != indep.end()));
		});

		WPoly::ConsIterator it2(temp);
		for (; it2 && !((*it2).is_equality() && (*it2).has_var(v)); it2++);
#ifdef POLY_DEBUG
		cout << "Found equality: " << (*it2) << endl;
#endif

		if (it2) {
			WCons c = *it2;
			Vector<PPL::Coefficient> vect;
			vect.setLength(indep.size() + 2); /* vector format: [Indep. vars coefs, Current var (v) coef, Constant] */
			for (int i = 0; i < vect.length(); i++)
				vect[i] = 0;
			bool seen_v = false;
			for (WCons::TermIterator it3(c); it3; it3++) {
				WVar v2(*it3);
				if ((v2.guid() != v.guid()) && (c.coefficient(v2) != 0)) {
					ASSERT(indep.find(v2.guid()) != indep.end()); // must be true because of projection, and because v2!=v
					PPL::Coefficient coef = c.coefficient(v2);
					vect[std::distance(indep.begin(), indep.find(v2.guid()))] = coef;
				}
				if (v2.guid() == v.guid()) {
					seen_v = true;
					vect[vect.length() - 2] = c.coefficient(v);
					ASSERT(c.coefficient(v) != 0);
				}
			}
			if (!seen_v) {
				ASSERT(false);
#ifdef POLY_DEBUG
				cout << "nvm. " << endl;
#endif
				continue;
			}

			vect[vect.length() - 1] = c.inhomogeneous_term();

			PPL::Coefficient pgcd = vect[vect.length() - 1];

			for (int i = 0; i < vect.length() - 1; i++) {
				PPL::Coefficient coef2 = vect[i];
				while (coef2 != 0) {
					PPL::Coefficient tmp = pgcd;
					pgcd = coef2;
					coef2 = tmp % coef2;
				}
			}

			if (pgcd != 1) {
				for (int i = 0; i < vect.length(); i++) {
					vect[i] /= pgcd;
				}
			}

#ifdef POLY_DEBUG
			cout << "variable " << v << " has vect: ";
			for (int i = 0; i < vect.length(); i++)
				cout << vect[i] << ", ";
			cout << endl;
#endif
			vmap.put(v.guid(), vect);
		}
	}
}

std::set<guid_t> PPLDomain::_collectPolyVars(const WPoly &poly) const {
	std::set<guid_t> result;
	for (WPoly::ConsIterator it(poly); it; it++) {
		WCons c = *it;
		for (WCons::TermIterator it2(c); it2; it2++) {
			if (c.coefficient(*it2) != 0)
			result.insert((*it2).guid());
		}
	}
	return result;
}


void PPLDomain::enableSummary() {
	_summary = new PPLSummary();
	/*
	const WVar &spInput = varNew(Ident(13, Ident::ID_REG_INPUT));
	 doNewConstraint(spInput == getVar(Ident(13, Ident::ID_REG)));
	 */
}
p::feature POLY_ANALYSIS_FEATURE("otawa::poly::POLY_ANALYSIS_FEATURE", new Maker<PolyAnalysis>());

Identifier<int> LOC_VAR_SIZE("otawa::poly::LOC_VAR_SIZE", 4);
Identifier<int> NUM_LOC_VARS("otawa::poly::NUM_LOC_VARS", 8);
Identifier<int> MAX_AXIS("otawa::poly::MAX_AXIS", 512);
Identifier<PPLDomain*> SUMMARY("otawa::poly::SUMMARY", nullptr);
Identifier<MyHTable<int, PPLDomain> * > MAX_LINEAR("otawa::poly::MAX_LINEAR", nullptr);
Identifier<bool> SUMMARIZE("otawa::poly::SUMMARIZE", false);

} // namespace poly
} // namespace otawa
