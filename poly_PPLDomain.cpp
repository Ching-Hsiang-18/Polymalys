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
	case ID_MEM_COUNT: /* clp */
		out << "count" << _id;
		break;
	case ID_MEM_VAL:
		out << "*ptr" << _id;
		break;
    case ID_MEM_VAL_PLUS:
        out << "*ptr+" << _id;
        break;
    case ID_MEM_VAL_MINUS:
        out << "*ptr-" << _id;
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
			out << "FP";
			break;
		case ID_START_SP:
			out << "SP";
			break;
		case ID_START_LR:
			out << "LR";
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
	for (WPoly::ConsIterator it(poly); it; it++) {
		const WCons &c = *it;
		ncons++;

//         cout << "this cons:" << (*it) << endl;
/*        for (WLinExpr::TermIterator it2((*it).getLE()); it2; it2++) {
            cout << "var in LE: " << (*it2) << endl;

        } */
        cout << color::Dim << "█" << color::RCol;
		bool firstTerm = true;
		for (WCons::TermIterator it2(c); it2; it2++) {
			WVar v = *it2;
            //cout << "var in cons: " << v << endl;
			const PPL::Coefficient &coef = c.coefficient(v);

			if (coef == -1) {
				if (!firstTerm)
					out << "− ";
				else
					out << "-";
			} else {
				if (!firstTerm)
					out << "+ ";
				if (coef != 1) {
					gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(coef));
					buf[sizeof(buf) - 1] = 0;
					out << buf << ".";
					
				}
			}

			if (isVarMapped(v, NO_STEP)) {
				Ident i = getIdent(v, NO_STEP);
				if(i.getType() == Ident::ID_MEM_VAL)
					out << color::IBlu;
				else if(i.getType() == Ident::ID_SPECIAL)
					out << color::Cya;
				else if(i.getType() == Ident::ID_LOOP)
					out << color::IYel;
				out << i << color::RCol;
			} else {
				if (isVarMapped(v, 0))
                    out << color::Gre << getIdent(v, 0) << color::RCol;
                else if (isVarMapped(v, 4)) /* FIXME */
                    out << color::Gre << getIdent(v, 4) << color::RCol;
                else if (isVarMapped(v, 1)) /* FIXME */
                    out << color::Gre << getIdent(v, 1) << color::RCol;
				else
					out << color::IGre << v << "[?]" << color::RCol; // TODO pretty-print CLP
			}

			out << " ";
			firstTerm = false;
			//cout << "je suis v : "<< v << endl;
		}

		if (c.is_equality()) {
			out << color::IPur << "= ";
		} else {
			out << color::IPur << "≥ ";
		}

		const PPL::Coefficient &cst = -c.inhomogeneous_term(); // Constraints form is: coef*X + ... + cst = 0
		gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(cst));
		buf[sizeof(buf) - 1] = 0;
		out << buf;

		out << color::RCol << "; ";
	}
	if (is_loop_bound)
		return;
	out << endl;

	out << "Variable count: " << poly.variable_count() << ", Constraints count: " << ncons << color::NoDim << endl;
	displayIdentMap(out);
	displayLocVars(out);
	displayGlobVars(out);
	out << endl;

	if (_summary != nullptr) {
#ifdef INTER_PROCEDURAL
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
#endif
		
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
		PDBG("c est pas egal pcq: pas le meme nombre de variables" << endl)
		return false;
	}

	if (compare_reg != b.compare_reg) {
		PDBG("c est pas egal pcq: pas le meme compare reg" << endl)
		return false;
	}

	if (compare_op != b.compare_op) {
		PDBG("c est pas egal pcq: pas le meme compare op" << endl)
		return false;
	}

	if (idmap.count() != b.idmap.count()) {
		PDBG("c est pas egal pcq: pas le meme idmap count" << endl)
		return false;
	}

	if (victims != b.victims) {
		PDBG("c est pas egal pcq: pas le meme victim map" << endl)
		return false;
	}

	if (bounds != b.bounds) {
		PDBG("c est pas egal pcq: pas le meme bounds" << endl)
		return false;
	}

	/*
	 * Try to show that the states are equals when ignoring memory locations (faster)
	 */
	PPLDomain r = b;
	PPLDomain l = *this;

	int expectedVarCount = 0;
	for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		const Pair<Ident, idval_t> &p = *it;
		if ((p.fst.getType() == Ident::ID_MEM_VAL)
            || (p.fst.getType() == Ident::ID_MEM_VAL_PLUS)
            || (p.fst.getType() == Ident::ID_MEM_VAL_MINUS)
			|| (p.fst.getType() == Ident::ID_MEM_COUNT)
			|| (p.fst.getType() == Ident::ID_MEM_ADDR)
			|| (p.fst.getType() == Ident::ID_MEM_VAL_INPUT)) {
			continue;
		}
		PDBG("expect: " << p.fst << endl)
		expectedVarCount++;
	}

	_doUnify(l, r, NULL, true);

	if ((l.idmap.count() != expectedVarCount)) {
		/* There was some unmatched registers */ 
		PDBG("c est pas egal pcq: different ensemble de registres" << endl)
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
    //cout << "Unify for equality testing" << endl;
    _doUnify(l, r, NULL);

	if ((l.idmap.count() != idmap.count()) || (r.idmap.count() != b.idmap.count())) {
		/* There was some unmatched memory locations */
		PDBG("c est pas egal pcq: different ensemble de memory locations" << endl)
		return false;
	}

	if (l.poly != r.poly) {
		PDBG("c est pas egal pcq: poly pas egal (mem)" << endl)
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
	
	/*
	int localcompt=0;
	const WVar variable_temp;
	const WVar Varlist[MAX_VAR];
	*/
	
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

	WVar ssp = getVar(id_ssp, nullptr); //ssp is the bottom of the execution stack
	out << "Local variables: " << endl;
	
	/*On initialise des variables pour afficher après si une variable ( au sens de c par exemple) pointe sur une autre
	Myvariables contient les values des variables ( au sens de notre prgramme, pas de c) et Myvariablesadresses contient leurs adresses
	Les arrays sont remplis plus bas -- FOrgot to comment in english, uups*/

	#ifdef showpointertovar
	/*Should use std::vectors instead of static arrays*/
	int isThereARelation = 0;
	WVar Myvariables[NUM_LOC_VARS(_props)];
	//std::vector<Wvar> Myvariables;
	WVar Myvariablesadresses[NUM_LOC_VARS(_props)];
	Ident idaddresses[NUM_LOC_VARS(_props)];
	Ident idvalues[NUM_LOC_VARS(_props)];
	WVar Myvarval;
	WVar Myvaraddr;
	int Mycounter=0;
	int Mystep;
	int Mystep2;
	#endif

    for (int i = 0; i < (NUM_LOC_VARS(_props) + 10) * LOC_VAR_SIZE(_props); i += LOC_VAR_SIZE(_props)) {
		//out << i << endl;
		WPoly poly_copy = poly;
		WVar v(num_axis);
		poly_copy.add_constraint(v == ssp - i);
        out << " [SSP - " << hex(i) << "] ∈ ";
		bool found = false;
		for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
			elm::Pair<Ident, idval_t> p = *it;
            if ((p.snd.s != 0) && (p.snd.s != 4)) /* FIXME */
				continue;
			WVar vsnd = WVar(p.snd.g);
			if (p.fst.getType() == Ident::ID_MEM_ADDR) {
				PPL::Coefficient infNumAddr, infDenAddr, supNumAddr, supDenAddr;
				bool maximum, minimum;
				poly_copy.maximize(v - vsnd, supNumAddr, supDenAddr, maximum);
				poly_copy.minimize(v - vsnd, infNumAddr, infDenAddr, minimum);
				if ((supNumAddr == 0) && (infNumAddr == 0) && (supDenAddr != 0) && (infDenAddr != 0)) {
					found = true;
					Ident idval(p.fst.getId(), Ident::ID_MEM_VAL);
					

					#ifdef showpointertovar

					/*Getting the variable needed for a mustalias
					Filling 2 arrays w values and adresses so we can compare them*/

					/*TODO : make thing prettier, in fact Myvarval and Myvaraddr are already known as p.snd.g and p.fst so this whole tab thing is not cool
					moreover instead of tabs, should use std::vectors ( dynamical)*/
					 	Ident idaddr(p.fst.getId(), Ident::ID_MEM_ADDR);
						Myvarval = getVar(idval,nullptr);
						//Myvarval = p.snd.g;
						Myvaraddr = getVar(idaddr,&Mystep); 
						//Myvariables.push_back(Myvarval);
						Myvariables[Mycounter]=Myvarval;
						Myvariablesadresses[Mycounter]=Myvaraddr;
						//Myvariablesadresses[Mycounter]=p.fst.getVar();
						idaddresses[Mycounter]=idaddr;
						idvalues[Mycounter]=idval;
						Mycounter++;
						
					#endif 
					
                    if (hasIdent(idval)) {
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

                        out << color::Dim << " (aka " << idval << ")" << color::NoDim ;

                    } else {
                        Ident idvalPlus(p.fst.getId(), Ident::ID_MEM_VAL_PLUS);
                        Ident idvalMinus(p.fst.getId(), Ident::ID_MEM_VAL_MINUS);

                        {
                            PPL::Coefficient supNumVal, supDenVal, infNumVal, infDenVal;
                            getRange(idvalPlus, infNumVal, infDenVal, supNumVal, supDenVal);

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
                        }
                        out << " ∩ ";
                        {
                            PPL::Coefficient supNumVal, supDenVal, infNumVal, infDenVal;
                            getRange(idvalMinus, infNumVal, infDenVal, supNumVal, supDenVal);

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
                            Ident idCount(p.fst.getId(), Ident::ID_MEM_COUNT);

                            out << color::Dim << " (aka " << idvalPlus << " and " << idvalMinus << ") when " << idCount << " > 0 " << color::NoDim;

                        }

                    }
					break;
				}
			}
		}
		if (!found) {
			out << "N/A";
		}

		out << endl;
	}
			/*affichage des pointeurs sur variables dans le code c par ex 
			*/
	#ifdef showpointertovar
		out << endl << "Relations between program variables :" << endl;
		for (int valc=1 ; valc<Mycounter ; valc++){
			for (int adrc=1; adrc<Mycounter ; adrc++){
				if (valc != adrc){
					if (mustAlias(Myvariables[valc],Myvariablesadresses[adrc],0)){
						isThereARelation++;
						out <<  idvalues[valc] << " = " <<  idaddresses[adrc]<< " i.e " << idvalues[valc] << " points on *" << idaddresses[adrc] <<endl;
					}
				}
			}
		}
		if (isThereARelation==0) out << endl << "No relations found" << endl ;
		else out << endl << "A total of "<< isThereARelation << " Relations were found" << endl;
		
	#endif
	out << endl;
}

void PPLDomain::displayGlobVars(io::Output &out) const {
	const PropList _props;
	if (isBottom()) {
		out << "diplayGlobVars: Nothing to display (state is BOTTOM)" << endl;
		return;
	}
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	out << "Global variables: " << endl;

	for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		elm::Pair<Ident, idval_t> p = *it;
		if (p.fst.getType() == Ident::ID_MEM_ADDR) {
			PPL::Coefficient num, den;
			if (getConstant(p.fst, num, den)) {
				out << " @addr 0x";
				displayFrac(out, num, den, true);
				out << ", value = ";
                Ident idval(p.fst.getId(), Ident::ID_MEM_VAL);
                if (hasIdent(idval)) {
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

                    out << color::Dim << " (aka " << idval << ")"<< color::NoDim;



				} else {
                    Ident idvalPlus(p.fst.getId(), Ident::ID_MEM_VAL_PLUS);
                    Ident idvalMinus(p.fst.getId(), Ident::ID_MEM_VAL_MINUS);

                    {
                        PPL::Coefficient supNumVal, supDenVal, infNumVal, infDenVal;
                        getRange(idvalPlus, infNumVal, infDenVal, supNumVal, supDenVal);

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
                    }
                    out << " ∩ ";
                    {
                        PPL::Coefficient supNumVal, supDenVal, infNumVal, infDenVal;
                        getRange(idvalMinus, infNumVal, infDenVal, supNumVal, supDenVal);

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
                        Ident idCount(p.fst.getId(), Ident::ID_MEM_COUNT);

                        out << color::Dim << " (aka " << idvalPlus << " and " << idvalMinus << ") when " << idCount << " > 0 " << color::NoDim;

                    }

                }
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
	int step;
	WVar v = getVar(id, &step);
	return getConstant(v, cst_n, cst_d);
}
void PPLDomain::getRange(const Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n,
                         PPL::Coefficient &bsup_d) const {
	int step;
	WVar v = getVar(id, &step);
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
bool PPLDomain::mayRange(const WVar &base, const WVar &count, const WVar &target, int step) const {
    if (!poly.isTracked(base.id()) || !poly.isTracked(target.id()) || !poly.isTracked(count.id())) return false;

    if (step == STEP_BOT) {
        return mayAlias(base, target);
    }
    return !poly.relation_with((base - 1) >= target).implies(PPL::Poly_Con_Relation::is_included()) &&
            !poly.relation_with(base + count*step <= target).implies(PPL::Poly_Con_Relation::is_included());
}

bool PPLDomain::mustAlias(const WVar &v1, const WVar &v2, int offset) const {
    if (!poly.isTracked(v1.id()) || !poly.isTracked(v2.id())) return false;
	return poly.relation_with(v1 == v2 + offset).implies(PPL::Poly_Con_Relation::is_included());
}

void PPLDomain::displayIdentMap(io::Output &out) const {
	out << "Mapping: " << color::Dim;
	for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		const Ident &ident = (*it).fst;
		WVar v = WVar((*it).snd.g);
		out << ident << ":" << v;
		if ((*it).snd.s != NO_STEP)
		{
			Ident id_n(ident.getId(), Ident::ID_MEM_COUNT);
			out << "/" << (*it).snd.s;
			if(idmap.has1(id_n)) {
				out << "/" << id_n;
			}
		}
	   	out << ", ";
	}
	out << color::NoDim << endl;
}

//comme getLoopBound() mais renvoie une surapproximation de l'expression lineaire de la variable d'induction
PPLDomain PPLDomain::getLinearExpr(const Ident &id) {
#ifdef INTER_PROCEDURAL
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
#else
	crash();
#endif
}
void PPLDomain::doFold(WVar &s, WVar &v1, WVar &v2) {

    WPoly tmp = poly;
    poly.add_constraint(s == v1);
    tmp.add_constraint(s == v2);
    poly.poly_hull_assign(tmp);
}

void PPLDomain::doExpand(WVar &s, WVar &v1) {
    WPoly tmp = poly;
    tmp.map_lambda([&s, &v1](guid_t i, guid_t &j) {
        if (i == s.id()) {
            j = v1.id();
        } else if (i == v1.id()) {
            j = s.id();
        } else j = i;
        return true;
    });
    poly.intersection_assign(tmp);
}

void PPLDomain::doAvExpand(WVar &splus, WVar &sminus, WVar &vplus, WVar &vminus) {
    WPoly tmp = poly;
    tmp.map_lambda([&splus, &vplus, &sminus, &vminus](guid_t i, guid_t &j) {
        if (i == splus.id()) {
            j = vplus.id();
        } else if (i == vplus.id()) {
            j = splus.id();
        } else if (i == vminus.id()) {
            j = sminus.id();
        } else if (i == sminus.id()) {
            j = vminus.id();
        } else j = i;
        return true;
    });
    poly.intersection_assign(tmp);
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
#ifdef LINBOUND
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
#endif
	return s_out;

}

PPLDomain PPLDomain::onLoopExit(int loop, int bound) const {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	ASSERT(s_out.hasIdent(id)); /* You are supposed to be already inside the loop when you call onLoopExit() */
	WVar v = s_out.getVar(id, nullptr);
	if (bound >= 0) {
        //cout << "bounding loop: " << bound << endl;
		s_out.poly.add_constraint(v <= bound);
	}
	s_out.varKill(v, NO_STEP);
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
	WVar v_old = s_out.getVar(id, nullptr);
	WVar v_new = s_out.varNew(id, NO_STEP, true, false); //pas de damage car la boucle appartient forcement a la fonction
	s_out.poly.add_constraint(v_new == v_old + 1);

	return s_out;
}

PPLDomain PPLDomain::onLoopEntry(int loop) const {
	PPLManager::t s_out = *this;
	Ident id(loop, Ident::ID_LOOP);
	WVar v = s_out.varNew(id, NO_STEP, true, false); // idem que sur onLoopIter
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
	cout << "Filtering, compare_reg is: " << compare_reg << ", compare_op is: " << compare_op << ", taken=" << taken << endl;
#endif


	switch (this_op) {
		case sem::NE: {
			WPoly poly2 = res.poly;
			res.poly.add_constraint(res.getVar(compare_reg, nullptr) <= -1);
			poly2.add_constraint(res.getVar(compare_reg, nullptr) >= 1);
			res.poly.poly_hull_assign(poly2);
			break;
			PDBG("NE!" << endl)
		}
		case sem::EQ:
			PDBG("EQ!" << endl)
			res.poly.add_constraint(res.getVar(compare_reg, nullptr) == 0);
			break;
		case sem::GE:
		case sem::UGE:
			res.poly.add_constraint(res.getVar(compare_reg, nullptr) >= 0);
			PDBG("(U)GE!" << endl)
			break;
		case sem::GT:
		case sem::UGT:
			res.poly.add_constraint(res.getVar(compare_reg, nullptr) >= 1);
			PDBG("(U)GT!" << endl)
			break;
		case sem::LE:
		case sem::ULE:
			res.poly.add_constraint(res.getVar(compare_reg, nullptr) <= 0);
			PDBG("(U)LE!" << endl)
			break;
		case sem::LT:
		case sem::ULT:
			res.poly.add_constraint(res.getVar(compare_reg, nullptr) <= -1);
			PDBG("(U)LT!" << endl)
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
			PDBG("Link input register: " << (*it).fst << endl;)
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
			PDBG("Killing variable: " << v << endl)
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
			PDBG("Link input register: " << (*it).fst << endl;)
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
				PDBG("Link input memory variable: " << (*it).fst << endl)
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
					PDBG("Not found... Try initial data." << endl)
					Ident idFormalAddr((*it).fst.getId(), Ident::ID_MEM_ADDR);
					WVar formalArg = out.getVar((*it).fst);
					uint32_t address, value;
					bool ok = out.memGetInitial(idFormalAddr, address, value, false); //TODO force?
					/*
					 * Don't force. Maybe we are summarizing caller function too.
					 */
					if (ok) {
						PDBG("Found initial data. Value= " << hex(value) << endl)
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

	PDBG("Link special inputs " << endl)
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

	PDBG("Inject loop bounds" << endl)
	for (MyHTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
		if ((*it).fst.getType() == Ident::ID_LOOP) {
			WVar v = getVar((*it).fst);
			ASSERT(!out.isVarMapped(v));
			out.id2axis[(*it).fst] = v.id();
			out.axis2id[v.id()] = (*it).fst;
		}
	}

	PDBG("Merge inputs" << endl)
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

	PDBG("Taking care of side-effects..." << endl)
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
					PDBG("Don't keep " << (*it).fst << " because it matches an input" << endl)
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
				PDBG("Memory variable " << (*it).fst << " was not affected by the call" << " (now known as: " << addr << ")" << endl)
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
					PDBG("injecting input: " << idInput << endl)
					injected_inputs.add(idInput);
				}
			} else {
				PDBG("Memory variable " << (*it).fst << " was overwritten" << endl)
				// la variable de l'appelant ne sera pas recuperee, mais si elle est dans le dmg alors il 
				// faut la recuperer quand meme sans sa valeur. Sauf si c'est un exact-dmg auquel cas
				// il faut simplement recuperer l'identifier du out.damage
				
				Ident idCallerVal((*it).fst.getId(), Ident::ID_MEM_VAL);
				Ident addr, val;
				if (exact_dmg) {
					PDBG("exact_dmg, do not keep variable" << endl)
				}
				if (_summary && _summary->_damaged.contains(idCallerVal) && !exact_dmg) {
					PDBG("represents damage: keep without its value" << endl)
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
				PDBG("Register variable " << (*it).fst << " was not affected by the call" << endl)
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
				PDBG("Register variable " << (*it).fst << " was overwritten" << endl)
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

	PDBG("after input merge" << endl)
	// menache
	PDBG("Post-composition cleanup: " << endl)

	if (out.isBottom()) {
		/* Composition resulted in bottom state for some reason. Will re-analyse function */
		return PPLDomain();
	}
	PPL::dimension_type max_axis = 0;
	for (PPL::dimension_type i = 0; i < out.poly.space_dimension(); i++) {
		if (out.axis2id[i].getType() == Ident::ID_INVALID) {
			WVar v(i);
			PDBG("Killing variable: " << v << endl)
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

/* widening: backState,headerState, avec headerState := headerState \/ (backState U headerState) */
PPLDomain PPLDomain::onMerge(const PPLDomain &r /* headerstate */ , bool widen, bool avcreate) {
     //cout << "widen: " << widen << endl;
	/*
	 * The merge works in two phases:
	 *
	 * 1. Unification phase: we perform a substitution (variable renumbering), so that variables representing the same
	 * object (i.e.
	 * register, or memory location) in both states, have the same number.
	 * 2. Merge phase: this is the actual convex hull or widening, performed on the unified states.
	 */


	if (r.isBottom()) {
		PDBG("Trivial merge (r is bottom)" << endl)
		return *this;
	}
	if (isBottom()) {
		PDBG("Trivial merge (l is bottom)" << endl)
		return r;
	}

	/*
	ASSERT(!trash.countOnes());
	ASSERT(!r.trash.countOnes());
	ASSERT(compare_reg.getType() == Ident::ID_INVALID);
	ASSERT(r.compare_reg.getType() == Ident::ID_INVALID);
	*/

	PDBG("Non-trivial merge, type=" << (widen ? "widening" : "convex-hull") << endl)
	DBG("Non-trivial merge, type=" << (widen ? "widening" : "convex-hull"))

	/* We have a non-trivial merge, so we will need working copies of l/r to perform the substitutions. */
	PPLDomain l1 = *this;
	PPLDomain r1 = r;

    _doUnify(l1, r1, this, false, avcreate);

	DBG("Merge's _doUnify done, l1=" << PPLManager::t(l1) << ", r1 = " << PPLManager::t(r1))

	for (int i = 0; i < r1.bounds.length(); i++)
		l1.setBound(i, r1.getBound(i));

	l1.poly.poly_hull_assign(r1.poly);
	if (widen)
		l1.poly.bounded_H79_extrapolation_assign(r1.poly); // a lenvers dans la ppl 
	l1.doFinalizeUpdate();
	for (int i = 0; i < r1.bounds.length(); i++)
		l1.setBound(i, r1.getBound(i));
#ifdef POLY_DEBUG
    cout << "Joined state:" << endl;
    cout << l1;
    fflush(stdout);
#endif
    // if (avcreate) cout << "JOIN WITH AVATAR CREATION";
	return l1;



/*

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
				PDBG("compare " << (*it2) << " avec: " << (*it) << endl)
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

		//for (elm::genstruct::Vector<Ident>::Iterator it(r1._summary->_inputs); it; it++) {
		//	if (!l1._summary->_inputs.contains(*it)) {
		//		l1._summary->_inputs.add(*it);
		//	}
		//}
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
#endif*/
}

PPLDomain PPLDomain::onSemInst(const BasicBlock *bb, const sem::inst &si, int instaddr) const {
	PPLDomain s_out = *this;
	ASSERT(!hasFilter() || (si.op == sem::BRANCH));

	DBG("\t\t" << color::Dim << si)

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
			WVar v = s_out.varNew(id, NO_STEP, true, true); // damage du registre ecrit
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
			WVar vs = s_out.getVar(id2, nullptr);

			sem::reg_t dest = si.d();
			Ident id(dest, Ident::ID_REG);
			WVar v = s_out.varNew(id, NO_STEP, true, true); // damage du registre ecrit

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

			WVar vs1 = s_out.getVar(id1, nullptr);
			WVar vs2 = s_out.getVar(id2, nullptr);
			WVar v = s_out.varNew(id, NO_STEP, true, true); // damage du registre destination de l'operation binaire
			s_out._doBinaryOp(si.op, &v, &vs1, &vs2);
			break;
		}
		case sem::STORE: // MEMb(a) <- d
		{

			sem::reg_t addr = si.a();
	
			Ident idStoreAddr(addr, Ident::ID_REG);
			//TODO move this shit in some handleInput() method, and call it on other register ops, too 
			WVar storeAddr = s_out.getVarOrNew(idStoreAddr, true); //l'adresse d'ecriture peut etre un input
			

			#ifdef CheckWritingArea

			cout << "Analysis of store addr: " << storeAddr << ": " << endl;
			if(PPLDomain::isInStack(storeAddr)) {
				cout <<"In stack (for sure) " ;
			} 
			else {
				int check=segmentChecking(storeAddr);
				if(!PPLDomain::mayBeInStack(storeAddr)) cout <<"Out of stack (for sure) ";	
				else cout << " We don't know if the var is in the stack ¯\\_(ツ)_/¯ ";
						switch (check) {

							case CASE_SAFE: {
								cout << "but the segment in which the store occurs is writable (SAFE)"<< endl	;
								break;}
							case CASE_UNSAFE: {
								cout << "and writing here is illegal ( not writable segment or writing outside of segments) (ISSUE)"<< endl;
								break;}
							case CASE_UNKNOWN:{
								cout << "and no idea if the target area is writable"<< endl;
								break;}
						}		
			}
			

			#endif

			



            bool isArray = _isArrayStore(bb, storeAddr);

			sem::reg_t src = si.d();
			Ident idStoreValue(src, Ident::ID_REG);
            PPL::Coefficient num, den;

            /* bool isInput = !s_out.hasIdent(idStoreValue); */
            WVar storeValue = s_out.getVarOrNew(idStoreValue, true); //la valeur d'ecriture peut etre un input
            if (getConstant(idStoreAddr, num, den)) {
                uint64_t address = PPL::raw_value(num).get_ui() / PPL::raw_value(den).get_ui();

                if (address == DEBUG_REG_COMMAND) {
                    cout << "\nInst addr: " << hex(instaddr) << " ";
                    if (getConstant(idStoreValue, num, den)) {
                        uint64_t value = PPL::raw_value(num).get_ui() / PPL::raw_value(den).get_ui();
                        cout << "Executing debug command with ID " << value << endl;
                        handleCommand(value);
                    } else {
                        cout << "WARNING: Debug command was ignored because command id could not be determined" << endl;
                    }
		    break;

                } else if (address == DEBUG_REG_ASSERT) {
                    if (!poly.relation_with(storeValue == 0).implies(PPL::Poly_Con_Relation::is_included())) {
                        cout << "\nAssertion failed at instr " << hex(instaddr) << endl;
                        cout << "State is: " << endl;
                        this->print(cout);
                        abort();
                    }
		    break;
                }
            }
            /*
			if (isInput) {
				// we are summarizing
				Ident idSSP(Ident::ID_START_SP, Ident::ID_SPECIAL);
				const WVar &vSSP = s_out.getVar(idSSP, nullptr);
				s_out.doNewConstraint(storeValue >= vSSP);
			}
            */

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

            /* compute must / may alias sets */
			Ident idEquiv;                          /* Identifier equivalent to the store addr, if any. */
			elm::genstruct::Vector<Ident> overlaps; /* List of identifiers overlapping the store addr. */

            //cout << "STORE\n";
			for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = s_out.idmap.getPairIter(); it; it++) {
				const Ident &idCurrent = (*it).fst;
				if (idCurrent.getType() == Ident::ID_MEM_ADDR) {
                    // ASSERTP((*it).snd.s == 0, "todo"); // TODO tableaux
                    const WVar &base = WVar((*it).snd.g);
                    Ident idCount((*it).fst.getId(), Ident::ID_MEM_COUNT);

                    if (s_out.mustAlias(base, storeAddr) && (*it).snd.s == 0) {
						ASSERT(idEquiv.getType() == Ident::ID_INVALID); /* must be unique */
						idEquiv = idCurrent;
                    } else {
                        const WVar &count = getVar(idCount, nullptr);
                        if (s_out.mayRange(base, count, storeAddr, (*it).snd.s) && (delta(base, storeAddr, (*it).snd.s) <= 0)) {
                            overlaps.add(idCurrent);                            
                        }
                    }
				}
			}


            /* WEAK UPDATE */          

            /* Merge with overlapping abstract locations */
            for (elm::genstruct::Vector<Ident>::Iterator it(overlaps); it; it++) {
                int step = -1;
                const WVar &overlap = s_out.getVar((*it), &step); // tableaux
                ASSERTP((*it).getType() == Ident::ID_MEM_ADDR && step > -1,
                    "jordy: not addr type (step=" << step << "), why?")
                PDBG("STORE: Merging existing location " << (*it) << " with new value." << endl)
                DBG("STORE: Merging existing location " << (*it) << " with new value.")
                s_out.memMerge(idval_t(overlap.guid(), step), storeValue);
            }



			if (idEquiv.getType() != Ident::ID_INVALID) {

                /* STRONG UPDATE */


				/* Replace existing equivalent abstract location */
				int step = -1;
				const WVar &equiv = s_out.getVar(idEquiv, &step); // tableaux
				ASSERTP(idEquiv.getType() == Ident::ID_MEM_ADDR && step > -1, 
					"jordy: equiv (" << equiv << ") is not addr type (step=" << step << "), why?")
				PDBG("STORE: Replacing existing location " << equiv << " with new value." << endl)
                DBG("STORE: Replacing existing location " << equiv << " with new value.")
                Ident idCount(idEquiv.getId(), Ident::ID_MEM_COUNT);
                const WVar count = getVar(idCount, nullptr);

                s_out.memTabReplace(idval_t(equiv.guid(), step), storeValue, count);
			} else {

                /* CREATE */

				/* No exact match: Create new abstract location */
				PDBG("STORE: Creating new memory location. " << endl)
                DBG("STORE: Creating new memory location. ");
                s_out.memTabCreate(storeAddr, storeValue, STEP_BOT, 1);
                if (isArray) {
                    Ident idAddrNew(s_out.mem_ref - 1, Ident::ID_MEM_ADDR);
                    int step;
                    WVar newAddr = s_out.getVar(idAddrNew, &step);

                    cout << "Create var with arraystore, attempt to find existing array" << endl;
                    for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = s_out.idmap.getPairIter(); it; it++) {
                        if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
                            if ((*it).fst == idAddrNew) break;
                            cout << "Checking if tab " << (*it).fst << " is the start of " << idAddrNew << " \n";
                            WVar tabAddr = s_out.getVar((*it).fst, &step);
                            Ident idCountAddr((*it).fst.getId(), Ident::ID_MEM_COUNT);
                            WVar count = s_out.getVar(idCountAddr, nullptr);

                            // test if tabAddr + count*STEP == newAddr
                            bool fold = false;

                            if (step == STEP_BOT) {
                                bool bounded = false;
                                PPL::Coefficient num, den;
                                s_out.poly.maximize(newAddr - tabAddr, num, den, bounded);
                                if (num != 0 && den != 0) {
                                    //cout << "num:" << num << endl;
                                    //cout << "den:" << den << endl;
                                    int tmp = num.get_si() / den.get_si();
                                    if (tmp > 0) {                                        
                                        step = tmp;
                                        cout << "Guessed step: " << step << endl;
                                    }
                                }
                            }
                            if (step != STEP_BOT && s_out.shouldFold(tabAddr, step, count, newAddr)) {

                                fold = true;
                                 cout << "yes (keep existing step: " << step << ") " << endl;
                            } else {
                                 cout << "no." << endl;
                            }


                            Ident idValPlus((*it).fst.getId(), Ident::ID_MEM_VAL_PLUS);
                            // cout << step << " " << fold << " " << s_out.hasIdent(idValPlus) << "\n";
                            if (fold && s_out.hasIdent(idValPlus)) {

                                cout << "Folding store " << idAddrNew << " into existing array " << (*it).fst << endl;

				s_out.showTab((*it).fst);

                                Ident idValMinus((*it).fst.getId(), Ident::ID_MEM_VAL_MINUS);
                                Ident idValNew(idAddrNew.getId(), Ident::ID_MEM_VAL);
                                Ident idCountNew(idAddrNew.getId(), Ident::ID_MEM_COUNT);


                                WVar newVal = s_out.getVar(idValNew, nullptr);
                                WVar oldValPlus = s_out.getVar(idValPlus, nullptr);
                                WVar oldValMinus = s_out.getVar(idValMinus, nullptr);
                                s_out.memTabCreate(tabAddr, 0, step, count + 1, true);
                                Ident idFoldedVal(s_out.mem_ref - 1, Ident::ID_MEM_VAL);
                                WVar foldedVal = s_out.getVar(idFoldedVal, nullptr);

                                WPoly empty_case = s_out.poly;

                                s_out.doNewConstraint(oldValPlus == oldValMinus);
                                s_out.doFold(foldedVal, newVal, oldValPlus);

                                empty_case.add_constraint(foldedVal == newVal);
                                s_out.poly.add_constraint(count > 0);
                                empty_case.add_constraint(count == 0);

                                s_out.poly.poly_hull_assign(empty_case);

                                s_out.varKill((*it).fst);
                                s_out.varKill(idCountAddr);
                                s_out.varKill(idValPlus);
                                s_out.varKill(idValMinus);
                                s_out.varKill(idAddrNew);
                                s_out.varKill(idValNew);
                                s_out.varKill(idCountNew);

                                s_out.doFinalizeUpdate();

                                //cout << "STATE AFTER FOLD: " << s_out << endl;
                                break;

                            }
                        }
                    }
                }
			}

			break;
		}
		case sem::LOAD: // d <- MEMb(a)
		{
            //cout << "LOAD: " << s_out << endl;
			sem::reg_t dst = si.d();
			sem::reg_t addr = si.a();
			Ident idLoadReg(dst, Ident::ID_REG);
			Ident idLoadAddr(addr, Ident::ID_REG);
			WVar loadAddr = s_out.getVar(idLoadAddr, nullptr);
			WVar loadReg = s_out.varNew(idLoadReg, NO_STEP, true, true); // damage du registre destination du LOAD
			bool found = false;

			/*
			 * Looking for existing abstract location equivalent to load address.
			 */
			for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = s_out.idmap.getPairIter(); it; it++) {
				if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
                    //ASSERT((*it).snd.s == 0); // TODO tableaux

                    WVar current = WVar((*it).snd.g);

                    Ident idBase((*it).fst.getId(), Ident::ID_MEM_ADDR);
                    Ident idCount((*it).fst.getId(), Ident::ID_MEM_COUNT);
                    Ident idExistingValue((*it).fst.getId(), Ident::ID_MEM_VAL);

                    int step;
                    WVar base = getVar(idBase, &step);
                    WVar count = getVar(idCount, nullptr);
                    WVar one;


                    /* FIXME we also do the mustAlias because mustSupseteq behaves incorrectly with STEP_BOT
                     * Btw, need to fix inconsistent usage of STEP_BOT vs. NO_STEP
                     */
                    bool mustalias = mustAlias(loadAddr, current);
                    bool supseteq = false;
                    if ((step != NO_STEP) && (step != STEP_BOT)) { /* FIXME */
                        supseteq = mustSupseteq(base, step, count, loadAddr, NO_STEP, 1);
                    }
                    if (mustalias || supseteq) {
                        PDBG("LOAD: Found equivalent abstract location: " << (*it).fst << endl);
                        // bool supseteq = mustSupseteq(base, step, count, loadAddr, NO_STEP, 1);
                        ASSERT((step == NO_STEP) || (step == STEP_BOT) || !mustalias || supseteq); /* FIXME remove this once mustsupseteq is completed */

                        //cout << "btw, supseteq = " << supseteq << endl;


                        if (s_out.hasIdent(idExistingValue)) {
                            /* Non-avatar case */
                            if (mustalias && step == STEP_BOT) {
                                /* STEP_BOT implies that array is limited to 1 element */
                                /* do not perform expansion in this case */
                                WVar existingValue = s_out.getVar(idExistingValue, nullptr);
                          /*      WVar expanded;
                                s_out.doExpand(existingValue, expanded);
                                s_out.victims.push(expanded.id());
*/
                                //cout << "match acces singulier: " << idExistingValue << endl;
                                //cout << "avant contrainte de load: " << s_out << endl;
                                //cout << "ajout contrainte: " << loadReg << " == " << existingValue << endl;
                                s_out.doNewConstraint(loadReg == existingValue);
                                //cout << "apres contrainte de load: " << s_out << endl;

                            } else {
                                WVar existingValue = s_out.getVar(idExistingValue, nullptr);
                                WVar expanded;
                                s_out.doExpand(existingValue, expanded);
                                s_out.victims.push(expanded.id());
                                s_out.doNewConstraint(loadReg == expanded);
                            }
                        } else {
                            //cout << "match load avec avatar: " << idExistingValue << endl;

                            /*
                             * Avatar case.
                             *
                             * mustAlias() returning true implies that val+ == val-
                             */
                            Ident idExistingValPlus((*it).fst.getId(), Ident::ID_MEM_VAL_PLUS);
                            Ident idExistingValMinus((*it).fst.getId(), Ident::ID_MEM_VAL_MINUS);
                            WVar existingValPlus = s_out.getVar(idExistingValPlus, nullptr);
                            WVar existingValMinus = s_out.getVar(idExistingValMinus, nullptr);

                            WVar expandedPlus;
                            WVar expandedMinus;
                            s_out.doAvExpand(existingValPlus, existingValMinus, expandedPlus, expandedMinus);

                            s_out.doNewConstraint(loadReg == expandedPlus);
                            s_out.doNewConstraint(loadReg == expandedMinus);
                            s_out.victims.push(expandedPlus.id());
                            s_out.victims.push(expandedMinus.id());
                        }
                        found = true;
                        //break;
					}
				}
			}

			if (!found) {
				uint32_t concreteAddress, initialValue;
				if (s_out.memGetInitial(idLoadAddr, concreteAddress, initialValue)) {
                    Segment *seg = _ws->process()->program()->findSegmentAt(concreteAddress);
                    if (seg &&  seg->isWritable()) {
                        //s_out.memSingleCreate(loadAddr, loadReg, false);
                        s_out.memTabCreate(loadAddr, loadReg, STEP_BOT, 1);
						PDBG("LOAD: Writable initial value exists, creating new ptr..." << endl)
					}
					else {
						PDBG("LOAD: Initial value is constant, do not create ptr...")
					}
					s_out.doNewConstraint(loadReg == initialValue);
				} else {
					PDBG("LOAD: Not found, creating new ptr..." << endl)
#ifdef INTER_PROCEDURAL
					const WVar &v = s_out.memSingleCreate(loadAddr, loadReg, false);
					if (s_out._summary != nullptr) {
						// Unknown LOAD value. If we are summarizing, create an input.
						Ident idInputAddr = s_out.getIdent(v);
						Ident idInputVal = Ident(idInputAddr.getId(), Ident::ID_MEM_VAL_INPUT);
						Ident idCurrentVal = Ident(idInputAddr.getId(), Ident::ID_MEM_VAL);
						WVar currentVal = s_out.getVar(idCurrentVal);
						WVar inputVal = s_out.varNew(idInputVal); // c'est une variable qu'on lit donc pas de damaged
						PDBG("Summarizing: creating new input memory: " << " what= " << idInputVal << " where=" << idInputAddr << endl)
	//					s_out._summary->_inputs.add(idInputVal);
						s_out.doNewConstraint(currentVal == inputVal);
					}
#else
                    s_out.memTabCreate(loadAddr, loadReg, STEP_BOT, 1);

                    //s_out.memSingleCreate(loadAddr, loadReg, false); /* FIXME */
#endif
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

	PDBG("Remapping: ")
	DBG("Remapping: ")
	for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		guid_t old_guid = (*it).snd.g;
		guid_t new_guid = (*it).snd.g;
		if (pfunc.maps(old_guid, new_guid)) {
			new_idmap.add((*it).fst, idval_t(new_guid, (*it).snd.s));

            // if (old_guid != new_guid) {
				PDBG((*it).fst  << "[" << WVar(old_guid) << "->" << WVar(new_guid) << "] ")
				DBG((*it).fst  << "[" << WVar(old_guid) << "->" << WVar(new_guid) << "] ")
            // }
		}
	}
	PDBG(endl)
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

	int step;
	WVar v = getVar(id, &step);
	poly.unconstrain(v);
}

void PPLDomain::doIntegerWrap() {
	// TODO(clement): integer wrap not supported yet
}

void PPLDomain::doKillTemporaries() {
	Vector<Ident> toDel;

	for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		if ((*it).snd.s != NO_STEP)
			continue;
		if ((((*it).fst.getType() == Ident::ID_REG) && (*it).fst.getId() < 0)) {
			toDel.add((*it).fst);
			PDBG("Killing temporary register: " << (*it).fst << endl)
		}
	}

	for (Vector<Ident>::Iter it(toDel); it; it++) {
		varKill(*it);
	}
}

void PPLDomain::doKillRegisters(BitVector bv) {
	Vector<Ident> toDel;

	for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		if ((*it).snd.s != NO_STEP)
			continue;
		if (((*it).fst.getType() == Ident::ID_REG) && 
			(*it).fst.getId() >= 0 &&
			(*it).fst.getId() < bv.size() &&
			!bv.bit((*it).fst.getId())) {

			if ((*it).fst == compare_reg)
				continue; // skip compare_reg, as this will be handled in PPLDomain::filter()

			if ((*it).fst.getId() == 13)
				continue; // never kill SP, as we need it to detect out-of-scope stack variables

			#ifdef CheckReturnAddress
			if ((*it).fst.getId() == 14) //Do not kill LR otherwise it breaks Saved-RIP checking
				continue;
			#endif

			if ((*it).fst.getId() == 0)
				continue; // avoid killing R0 as it holds the return value 
			// TODO performance: preserver R0 uniquement s'il atteint la fin de la fonction

			PDBG("Killing dead register: " << (*it).fst << endl)
			toDel.add((*it).fst);
		}
	}

	for (Vector<Ident>::Iter it(toDel); it; it++) {
		varKill(*it);
	}
}

bool PPLDomain::isInStack(WVar v) const{
	Ident idSp(13 /* TODO */ , Ident::ID_REG);
	WVar sp = getVar(idSp, nullptr); 
	Ident idssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	WVar ssp=getVar(idssp,nullptr);
	if (poly.relation_with(v > sp - 5).implies(PPL::Poly_Con_Relation::is_included()) 
	&& poly.relation_with(v < ssp).implies(PPL::Poly_Con_Relation::is_included()))
	//&& poly.relation_with(v < int(stackconf_t::STACK_TOP - stackconf_t::STACK_SIZE)).implies(PPL::Poly_Con_Relation::is_included()))
	return true;
	else return false;
}

/* Careful*/
bool PPLDomain::mayBeInStack(WVar v) const {
	Ident idSp(13 /* TODO */ , Ident::ID_REG);
	WVar sp = getVar(idSp, nullptr); 
	Ident idssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	WVar ssp=getVar(idssp,nullptr);
	/*Check whether we are below the sp or overthe ssp - */
	
	if (poly.relation_with(v < sp - 4).implies(PPL::Poly_Con_Relation::is_included()) &&
		poly.relation_with(v >= int(stackconf_t::STACK_TOP - stackconf_t::STACK_SIZE)).implies(PPL::Poly_Con_Relation::is_included()) ||
		poly.relation_with(v > ssp).implies(PPL::Poly_Con_Relation::is_included())){
			return false; /* Here we are sure we aren't in the stack*/
		}
	return true;	/* Here we may be in the stack, but not sure*/
}

int PPLDomain::segmentChecking(WVar v) const {
	int endAdress=0; 
	for(otawa::File::SegIter it=otawa::File::SegIter(_ws->process()->program());it;it++){
		int beginAdress = (int) *it->address();
		
		if (poly.relation_with(v > endAdress).implies(PPL::Poly_Con_Relation::is_included()) 
		&& poly.relation_with(v < beginAdress).implies(PPL::Poly_Con_Relation::is_included())){
			return CASE_UNSAFE; // We are between 2 segments ( or outside of segments)
		}
		if (mustBeInSegment(v,*it)){
			if(it->isWritable()){
				cout << endl << endl << "Writing in segment :  " << it->name()<< endl;
				return CASE_SAFE; // case in writable segment

			}
			else {
				cout << endl << endl << "Writing in segment :  "<< it->name() << endl;
				return CASE_UNSAFE; // case in not writable segment
			}
		}
		endAdress=(int)*it->address()+(int)(*it)->size();



	}
	return CASE_UNKNOWN; // could not prove anything

}



bool PPLDomain::mustBeInSegment(WVar v, Segment* seg) const { 
	return (poly.relation_with(v >= (int)seg->address()).implies(PPL::Poly_Con_Relation::is_included()) 
	&& poly.relation_with(v < (int)seg->address()+(int)seg->size()).implies(PPL::Poly_Con_Relation::is_included())) ;
	//exclude the upper adress because it may be the beginning adress of another segment 
}


void PPLDomain::doEnterFunction()  {
	int pot_depth=Ident::ID_START_LR; // contains depth+ID_START_LR
	int pot_depth_init=pot_depth;
	WVar var_slr_temp;
	while(1){	
		Ident id_slr_temp(pot_depth,Ident::ID_SPECIAL);
		if(hasIdent(id_slr_temp)){pot_depth++;}
		else {	var_slr_temp = varNew(id_slr_temp,NO_STEP,true,false);
				break;}
	}
	cout << " Entering depth :  " << pot_depth-pot_depth_init << endl ;

	Ident id_lr =Ident(14,Ident::ID_REG);
	cout << "Ident created : enter " << endl;
	WVar var_lr =getVarOrNew(id_lr, false);
	cout << " LR var added : enter " << endl;
	doNewConstraint(var_slr_temp==var_lr);
	cout << "slr var added : enter " << endl;
	//had to add a line in dokillregister method to prevent the LR reg from being destroyed before being checked by doLeaveFunction method, thus making the execution not working 
	}

void PPLDomain::doLeaveFunction(elm::string name) {

	#ifdef CheckReturnAddress
	int pot_depth=Ident::ID_START_LR; // contains depth+ID_START_LR
	int pot_depth_init=pot_depth;
	while(1){	
		Ident id_slr_temp(pot_depth,Ident::ID_SPECIAL);
		if(hasIdent(id_slr_temp)){pot_depth++;}
		else {  pot_depth--;
				break;}
	}
	cout << "Leaving depth : " << pot_depth-pot_depth_init << endl;

	Ident id_slr_real(pot_depth,Ident::ID_SPECIAL);
	WVar var_slr_real=getVar(id_slr_real,nullptr);
	Ident id_lr =Ident(14,Ident::ID_REG);
	WVar var_lr =getVar(id_lr,nullptr);
	//cout << endl << " Getting var worked (Leavefunction) "<< endl << endl;
	if(mustAlias(var_slr_real,var_lr)){cout<< "[" << name << "] CheckReturnAddress: safe"<< endl;} else { cout << "[" << name << "] CheckReturnAddress: maybe unsafe" << endl; }
	varKill(id_slr_real);
	#endif

	Vector<Ident> toDel;
	for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
		elm::Pair<Ident, idval_t> p = *it;
		WVar v = WVar((*it).snd.g); 
		if (p.fst.getType() == Ident::ID_MEM_ADDR) {
            //ASSERT((*it).snd.s == 0);
			Ident idSp(13 /* TODO */ , Ident::ID_REG);
			WVar sp = getVar(idSp, nullptr);

			if (poly.relation_with(v < sp).implies(PPL::Poly_Con_Relation::is_included()) &&
				poly.relation_with(v >= int(stackconf_t::STACK_TOP - stackconf_t::STACK_SIZE)).implies(PPL::Poly_Con_Relation::is_included())) {
				toDel.add(p.fst);
                Ident idVal(p.fst.getId(), Ident::ID_MEM_VAL);
                if (hasIdent(idVal)) {
                    /* no avatar */
                    toDel.add(Ident(p.fst.getId(), Ident::ID_MEM_VAL));
                } else {
                    /* avatar */
                    toDel.add(Ident(p.fst.getId(), Ident::ID_MEM_VAL_PLUS));
                    toDel.add(Ident(p.fst.getId(), Ident::ID_MEM_VAL_MINUS));
                }
		Ident idCount(p.fst.getId(), Ident::ID_MEM_COUNT);
		if (hasIdent(idCount)) {
			toDel.add(idCount);
		}
                PDBG("Killing out-of-scope stack variable or array: " << (*it).fst << endl);
			}
		}
	}

	for (Vector<Ident>::Iter it(toDel); it; it++)
		varKill(*it);


}
void PPLDomain::doRemoveAllAvatars() {
    Vector<Ident> toKill;

    for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
        elm::Pair<Ident, idval_t> p = *it;

        if (p.fst.getType() == Ident::ID_MEM_ADDR) {
            Ident idVal(p.fst.getId(), Ident::ID_MEM_VAL);
            Ident idCount(p.fst.getId(), Ident::ID_MEM_COUNT);

            if (!hasIdent(idVal)) {
                Ident idValPlus(p.fst.getId(), Ident::ID_MEM_VAL_PLUS);
                Ident idValMinus(p.fst.getId(), Ident::ID_MEM_VAL_MINUS);
                toKill.add(p.fst);
                toKill.add(idValPlus);
                toKill.add(idValMinus);
                toKill.add(idCount);
            }
        }
    }
    for (Vector<Ident>::Iter it(toKill); it; it++) {
        varKill(*it);
    }
}
void PPLDomain::listMemoryVariables() {
    doRemoveUselessAvatars();
    doFinalizeUpdate();
    int num_tab = 0;
    int num_av = 0;
    int num_sing = 0;
    int total = 0;
    int num_reg = 0;
    int num_spe = 0;
    int num_loop = 0;

    for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {

        total++;
        elm::Pair<Ident, idval_t> p = *it;

        if (p.fst.getType() == Ident::ID_MEM_ADDR) {
            Ident idVal(p.fst.getId(), Ident::ID_MEM_VAL);
            Ident idCount(p.fst.getId(), Ident::ID_MEM_COUNT);
            WVar count = getVar(idCount, nullptr);

            if (!hasIdent(idVal)) {
                num_av++;
            } else {
                if (poly.relation_with(count == 1).implies(PPL::Poly_Con_Relation::is_included())) {
                    num_sing++;
                } else {
                    num_tab++;
                }
            }
        }
	if (p.fst.getType() == Ident::ID_REG) {
		num_reg++;
	}
	if (p.fst.getType() == Ident::ID_SPECIAL) {
		num_spe++;
	}
	if (p.fst.getType() == Ident::ID_LOOP) {
		num_loop++;
	}
    }
    cout << (num_av + num_sing + num_tab) << " var mem, dont " << num_av << " avatars (x4), " << num_tab << " tab (x3), et " << num_sing << " acces singuliers (x3), " << num_reg << " regs, " << num_loop << " boucles, et " << num_spe << "spe" << "(tot: " << total << ")\n";

    if ((num_av*4 + (num_sing + num_tab)*3 + num_reg + num_loop + num_spe) != total) {
	    print(cout);
	    abort();
    }
}

void PPLDomain::doRemoveUselessAvatars() {
    Vector<Ident> toKill;

    for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = idmap.getPairIter(); it; it++) {
        elm::Pair<Ident, idval_t> p = *it;

        if (p.fst.getType() == Ident::ID_MEM_ADDR) {
            Ident idVal(p.fst.getId(), Ident::ID_MEM_VAL);
            Ident idCount(p.fst.getId(), Ident::ID_MEM_COUNT);

            if (!hasIdent(idVal)) {
                WVar count = getVar(idCount, nullptr);
                if (poly.relation_with(count > 0).implies(PPL::Poly_Con_Relation::is_included())) {
                    Ident idValPlus(p.fst.getId(), Ident::ID_MEM_VAL_PLUS);
                    Ident idValMinus(p.fst.getId(), Ident::ID_MEM_VAL_MINUS);
                    WVar valPlus = getVar(idValPlus, nullptr);
                    WVar valMinus = getVar(idValMinus, nullptr);
                    doNewConstraint(valPlus == valMinus);
                    toKill.add(idValMinus);
                    idmap.del1(idValPlus);
                    idmap.add(idVal, idval_t(valPlus.id(), NO_STEP));


                } else if (poly.relation_with(count == 0).implies(PPL::Poly_Con_Relation::is_included())) {
                    Ident idValPlus(p.fst.getId(), Ident::ID_MEM_VAL_PLUS);
                    Ident idValMinus(p.fst.getId(), Ident::ID_MEM_VAL_MINUS);
                    toKill.add(p.fst);
                    toKill.add(idValPlus);
                    toKill.add(idValMinus);
                    toKill.add(idCount);

                }
            }
        }
    }
    for (Vector<Ident>::Iter it(toKill); it; it++) {
        varKill(*it);
    }
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
	for (Vector<guid_t>::Iter it(victims); it; it++) {
		poly.unconstrain(WVar(*it));
	}

	victims.clear();
	_sanityChecks();
}

WVar PPLDomain::varNew(const Ident &ident, int step, bool allow_replace, bool create_damaged) {
	WVar v;
	if (ident.getType() == Ident::ID_MEM_ADDR) {
		ASSERT(step != NO_STEP);
	} else {
		ASSERT(step == NO_STEP);
	}

	if (idmap.has1(ident)) {
		ASSERT(allow_replace);
		ASSERT(step == NO_STEP);
		victims.add(idmap.find1(ident).g);
		PDBG("Variable " << idmap.find1(ident) << ", formerly associated with ident " << ident << ", will be replaced. " << endl)
	}
	idmap.add(ident, idval_t(v.guid(), step));
#ifdef TODO
	if (_summary != nullptr && create_damaged) {
		if ((ident.getType() == Ident::ID_REG) && ((ident.getId() >= 0) && ident.getId() <= 3)) {
			// TODO tester les registres qu'il faut garder en fonction de la convention d'appel
			_summary->_damaged.add(ident);
			PDBG("Add " << ident << " to the damaged set" << endl)
		}
		if ((ident.getType() == Ident::ID_MEM_VAL)) { 
			WVar v = getVar(Ident(ident.getId(), Ident::ID_MEM_ADDR));

			Ident idSsp(Ident::ID_START_SP, Ident::ID_SPECIAL);
			WVar ssp = getVar(idSsp);

			if (poly.relation_with(v < ssp).implies(PPL::Poly_Con_Relation::is_included()) &&
				poly.relation_with(v >= int(stackconf_t::STACK_TOP - stackconf_t::STACK_SIZE)).implies(PPL::Poly_Con_Relation::is_included())) {
				// is local variable.. do not add in damaged set
				PDBG("WVar " << ident << " not added to damaged set because it is local var." << endl)
			} else {
				PDBG("WVar " << ident << " added to damaged set because it may be a non-local var." << endl)
				_summary->_damaged.add(ident);
			}
		}
	}
#endif
	return v;
}

WVar PPLDomain::getVar(const Ident &ident, int *step) const { 
	if (ident.getType() == Ident::ID_MEM_ADDR)
		ASSERT(step != nullptr);
	idval_t iv = idmap.find1(ident);
	if (step != nullptr)
		*step = iv.s;
	return WVar(iv.g);
}

/* TODO: DEPRECATED */
WVar PPLDomain::getVarOrNew(const Ident &ident, bool create_input) {
	ASSERT(ident.getType() == Ident::ID_REG);
	if (!hasIdent(ident)) {
		WVar v = varNew(ident, NO_STEP, false, false);
#ifdef INTER_PROCEDURAL
		if (create_input && _summary != nullptr) {
			/* Read from untracked register. If we are summarizing, create a new input register */
			ASSERT(ident.getType() == Ident::ID_REG);
			Ident idInput(ident.getId(), Ident::ID_REG_INPUT);
//			_summary->_inputs.add(idInput);
			WVar input = varNew(idInput);
			doNewConstraint(input == v);
			PDBG("Summarizing: creating new input register " << input << endl)
		}
#endif
		return v;
	}
	return getVar(ident, nullptr);
}

void PPLDomain::varCreatePtr(Ident &addr, Ident &val) {
	addr = Ident(mem_ref, Ident::ID_MEM_ADDR);
	val = Ident(mem_ref, Ident::ID_MEM_VAL);
	mem_ref++;
}

void PPLDomain::varCreateClp(Ident &addr, Ident &val, Ident &clp) {
	addr = Ident(mem_ref, Ident::ID_MEM_ADDR);
	val = Ident(mem_ref, Ident::ID_MEM_VAL);
	clp = Ident(mem_ref, Ident::ID_MEM_COUNT);
	mem_ref++;
}

void PPLDomain::varCreateAvClp(Ident &addr, Ident &val_plus, Ident &val_minus, Ident &clp) {
    addr = Ident(mem_ref, Ident::ID_MEM_ADDR);
    val_plus = Ident(mem_ref, Ident::ID_MEM_VAL_PLUS);
    val_minus = Ident(mem_ref, Ident::ID_MEM_VAL_MINUS);

    clp = Ident(mem_ref, Ident::ID_MEM_COUNT);
    mem_ref++;
}


bool PPLDomain::hasIdent(const Ident &ident) const { return idmap.has1(ident); }

WVar PPLDomain::memTabReplace(idval_t address, const WVar &valueSource, const WVar &count) {
    // int step;
    const Ident &idOldAddress = getIdent(address); // tableaux
    Ident idNewAddress, idNewValue, idNewCount;
    varCreateClp(idNewAddress, idNewValue, idNewCount);

    const WVar &newAddress = varNew(idNewAddress, address.s, false, false);
    doNewConstraint(WVar(address.g) == newAddress);

    const WVar &newValue = varNew(idNewValue, NO_STEP, false, true); //memReplace donc on cree un damaged
    doNewConstraint(newValue == valueSource);

    const WVar &newCount = varNew(idNewCount, NO_STEP, false, true); //memReplace donc on cree un damaged
    doNewConstraint(newCount == count);
    varKill(address); // tableaux
    varKill(count, NO_STEP);

    const Ident &idOldValue = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL);
    const Ident &idOldValPlus = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL_PLUS);
    const Ident &idOldValMinus = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL_MINUS);

    if (hasIdent(idOldValue)) {
        /* non-avatar */
        WVar oldValue = getVar(idOldValue, nullptr);
        varKill(oldValue, NO_STEP); // tableaux
    } else {
        /* avatar */
        WVar oldValPlus = getVar(idOldValPlus, nullptr);
        WVar oldValMinus = getVar(idOldValMinus, nullptr);
        varKill(oldValPlus, NO_STEP); // tableaux
        varKill(oldValMinus, NO_STEP); // tableaux
    }
    return newAddress;
}

WVar PPLDomain::memReplace(idval_t address, const WVar &valueSource) {
	// int step;
	const Ident &idOldAddress = getIdent(address); // tableaux
	const Ident &idOldValue = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL);
	WVar oldValue = getVar(idOldValue, nullptr);

	Ident idNewAddress, idNewValue;
	varCreatePtr(idNewAddress, idNewValue);

	const WVar &newAddress = varNew(idNewAddress, address.s, false, false);
	doNewConstraint(WVar(address.g) == newAddress);

    const WVar &newValue = varNew(idNewValue, NO_STEP, false, true); //memReplace donc on cree un damaged
	doNewConstraint(newValue == valueSource);

#ifdef TODO
	const Ident &idOldInput = Ident(idOldAddress.getId(), Ident::ID_MEM_VAL_INPUT);
	if (hasIdent(idOldInput)) {
		ASSERT(_summary);
		const WVar &oldInput = getVar(idOldInput);
		const Ident idNewInput(idNewAddress.getId(), Ident::ID_MEM_VAL_INPUT);
		const WVar &newInput= varNew(idNewInput, false, false); // TO*DO tableaux (disabled code)
		doNewConstraint(newInput == oldInput);
/*		_summary->_inputs.remove(idOldInput);
		_summary->_inputs.add(idNewInput); */ 
		PDBG("Summarizing: migrating input from " << idOldInput << " to " << idNewInput << endl)
		varKill(oldInput);
	}
#endif
	varKill(oldValue, NO_STEP); // tableaux
	varKill(address); // tableaux
	return newAddress;
}

WVar PPLDomain::memSingleCreate(const WLinExpr & address , const WLinExpr &valueSource, bool damage) {
	Ident idNewAddress, idNewValue;
	varCreatePtr(idNewAddress, idNewValue);

	const WVar &newAddress = varNew(idNewAddress, 0, false, false); // tableaux
	doNewConstraint(newAddress == address);

	const WVar &newValue = varNew(idNewValue, NO_STEP, false, damage); //memCreate le damage ca depend
	doNewConstraint(newValue == valueSource);

	return newAddress;
}

WVar PPLDomain::memTabCreate(const WLinExpr & address , const WLinExpr &valueSource, int step, const WLinExpr &count, bool unconstrained) {
    Ident idNewAddress, idNewValue, idNewCount;
    varCreateClp(idNewAddress, idNewValue, idNewCount);

    const WVar &newAddress = varNew(idNewAddress, step, false, false); // tableaux
    doNewConstraint(newAddress == address);

    const WVar &newValue = varNew(idNewValue, NO_STEP, false, false); //memCreate le damage ca depend
    if (!unconstrained)
        doNewConstraint(newValue == valueSource);

    const WVar &newCount = varNew(idNewCount, NO_STEP, false, false); //memCreate le damage ca depend
    doNewConstraint(newCount == count);

    return newAddress;
}


WVar PPLDomain::memMerge(idval_t base, const WVar &newValue) {
    const Ident &idOldBase = getIdent(base); // tableaux
    const Ident &idOldValue = Ident(idOldBase.getId(), Ident::ID_MEM_VAL);
    const Ident &idOldCount = Ident(idOldBase.getId(), Ident::ID_MEM_COUNT);
    const Ident &idOldValPlus = Ident(idOldBase.getId(), Ident::ID_MEM_VAL_PLUS);
    const Ident &idOldValMinus = Ident(idOldBase.getId(), Ident::ID_MEM_VAL_MINUS);


    WVar count = getVar(idOldCount, nullptr);
    if (hasIdent(idOldValue)) {
        /* No avatar */
        const WVar oldValue = getVar(idOldValue, nullptr);
        PPLDomain tempState = *this;
        WVar newAddr1 = tempState.memTabReplace(base, newValue, count);
        WVar::_guid_generator -= 3; /* pour compenser les 3 varNews dans memTabReplace */
        WVar newAddr2 = memTabReplace(base, oldValue, count);
        if (!(idmap == tempState.idmap)) {
            cout << "tempState idmap mismatch" << endl;
            cout << "this:" << *this << endl;
            cout << "tempState:" << tempState << endl;
            ASSERT(false);
        }
        ASSERT(newAddr1.id() == newAddr2.id());
        poly.poly_hull_assign(tempState.poly);
        return newAddr1;
    } else {
        /* Avatar */
        const WVar oldValPlus = getVar(idOldValPlus, nullptr);
        const WVar oldValMinus = getVar(idOldValMinus, nullptr);
        PPLDomain tempState = *this;
        Ident idNewBase, idNewValPlus, idNewValMinus, idNewCount;

        varCreateAvClp(idNewBase, idNewValPlus, idNewValMinus, idNewCount);

        const WVar &newBase = varNew(idNewBase, base.s, false, false);
        tempState.doNewConstraint(newBase == WVar(base.g));
        doNewConstraint(newBase == WVar(base.g));

        const WVar &newValPlus = varNew(idNewValPlus, NO_STEP, false, false);
        tempState.doNewConstraint(newValPlus == oldValPlus);
        doNewConstraint(newValPlus >= newValue);

        const WVar &newValMinus = varNew(idNewValMinus, NO_STEP, false, false);
        tempState.doNewConstraint(newValMinus == oldValMinus);
        doNewConstraint(newValMinus <= newValue);

        const WVar &newCount = varNew(idNewCount, NO_STEP, false, false);
        tempState.doNewConstraint(newCount == count);
        doNewConstraint(newCount == count);

        //cout << "Merge tempState: " << tempState << endl;
        //cout << "Merge this: " << *this << endl;

        poly.poly_hull_assign(tempState.poly);
        varKill(idOldValPlus);
        varKill(idOldValMinus);
        varKill(idOldBase);
        varKill(idOldCount);
        doFinalizeUpdate();
        //cout << "after merge:" << *this << endl;
        //abort();
        return newBase;
    }
}

bool PPLDomain::memGetInitial(const Ident &id, uint32_t &address, uint32_t &value, bool force) {
	PPL::Coefficient num, den;
	/* TODO(clement) : use correct size  */
	if (getConstant(id, num, den)) {
		address = PPL::raw_value(num).get_ui() / PPL::raw_value(den).get_ui();
		PDBG("Address is statically known (" 
			<< hex(address) 
			<< "), attempting to read value from initial state" << endl)
        Segment *seg = _ws->process()->program()->findSegmentAt(address);
        bool iswriteable = seg && seg->isWritable();

//		 << "Is writable?" << iswriteable << endl;
		if (_summary && !force && iswriteable) {
			//TODO
			PDBG("Writeable address, and we are summarizing... mark as input" << endl)
			return false;
		}
		try {
			dfa::INITIAL_STATE(_ws)->get(address, value);
			PDBG("Initial state contains a value for this address: " << value << endl)
			return true;
		} catch (Exception ex) { 
			/* TODO(clement): Find the exact exception that is thrown in this case, it appears to be undocumented */
			PDBG("Could not find value (address out of bounds?)" << endl)
			return false;
		}
	}
	return false;
}

#ifdef POLY_DEBUG
void PPLDomain::_sanityChecks(bool allow_holes) {
	if (isBottom()) {
		ASSERT(poly.is_empty());
		return;
	}
	ASSERT(!poly.is_empty());
#ifdef TODO
	int max_axis = -1;
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
			PDBG("Also destroying from damaged set." << endl)
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
			PDBG("Using: " << ident << endl)
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
				PDBG("On doit garder " << (*it).fst << " car il est dans l'ensemble Damaged" << endl)
				keep = true;
			} 
			if ((*it).fst.getType() == Ident::ID_REG_INPUT || (*it).fst.getType() == Ident::ID_MEM_VAL_INPUT) {
				PDBG("On doit garder " << (*it).fst << " car c'est un Input" << endl)
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

					PDBG(idAddrR << " <==> " << idAddrL << endl)
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
						PDBG(" == 0")
					} else {
						c2 = (le >= 0);
						PDBG(" >= 0 ")
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

void PPLDomain::clpFold(WVar &base1, int step1, WVar &base2, int step2, WVar &baseMerged, int stepMerged, WVar &countMerged) {

    Ident idBase1 = getIdent(base1, step1);
    Ident idBase2 = getIdent(base2, step2);


    Ident idVal1 = Ident(idBase1.getId(), Ident::ID_MEM_VAL);
    WVar val1 = getVar(idVal1, nullptr);

    Ident idVal2 = Ident(idBase2.getId(), Ident::ID_MEM_VAL);
    WVar val2 = getVar(idVal2, nullptr);

    WVar summarized;
    doFold(summarized, val1, val2);

    this->memTabCreate(baseMerged, summarized, step1, countMerged);
}

bool PPLDomain::shouldFold(WVar &base, int step, WVar &count, WVar &newVar) {

    bool straight = poly.relation_with(base + step*count == newVar).implies(PPL::Poly_Con_Relation::is_included());
    bool reverse = poly.relation_with(newVar + step == base).implies(PPL::Poly_Con_Relation::is_included());
    return straight || reverse;
}
void PPLDomain::_doMatchGlobals(PPLDomain &l1, PPLDomain &r1, 
    const MyHTable<guid_t, LEVect > &leftVMap,
    const MyHTable<LEVect , guid_t, HashLEVect> &invRightVMap) const {
	// Look for unmatched global variables
    for (MyHTable<guid_t, LEVect>::PairIterator it(leftVMap); it; it++) {
		WVar v = WVar((*it).fst);
		Vector<PPL::Coefficient> vect = (*it).snd.cv;
		int i;
        for (i = 0; (i < vect.length() - 2) && (vect[i] == 0); i++) {};
        if (i == vect.length() - 2) { /* Ensure variable address is a constant */
			PDBG("GLOBAL: " << v << endl)
			DBG("GLOBAL: " << v)
			if (!invRightVMap.hasKey((*it).snd)) {
				PDBG("Global does not exists in other state." << endl)
				DBG("Global does not exists in other state.")
				uint32_t staticAddr = PPL::raw_value(- vect[vect.length() - 1] / vect[vect.length() - 2]).get_ui();
				uint32_t staticVal;
                dfa::INITIAL_STATE(_ws)->get(staticAddr, staticVal); // get static constant value from OTAWA
				PDBG("Static adress: " << hex(staticAddr) << " value: " << hex(staticVal) << endl)
				cout << "Static adress: " << hex(staticAddr) << " value: " << hex(staticVal) << endl;

                Ident leftIdAddr = l1.getIdent(v, STEP_BOT); /* TODO get initial values for arrays */
				Ident leftIdVal = Ident(leftIdAddr.getId(), Ident::ID_MEM_VAL);
                Ident leftIdCount = Ident(leftIdAddr.getId(), Ident::ID_MEM_COUNT);
                if (!l1.hasIdent(leftIdVal)) {
                    /* FIXME tempfix: do not fetch initial state when doing avatar creation
                     * TODO think of a clean way to handle this
                     */
                    continue;
                }
				WVar val = l1.getVar(leftIdVal, nullptr);
                WVar count = l1.getVar(leftIdCount, nullptr);
                if (!l1.poly.relation_with(count == 1).implies(PPL::Poly_Con_Relation::is_included())) {
                    continue;
                }


				r1.poly.add_constraint(WVar((*it).fst) == staticAddr);
				r1.poly.add_constraint(val == staticVal);

                Ident rightIdAddr, rightIdVal, rightIdCount;
                r1.varCreateClp(rightIdAddr, rightIdVal, rightIdCount);
                r1.poly.add_constraint(count == 1);
                r1.idmap.add(rightIdAddr, idval_t((*it).fst, STEP_BOT)); // tableaux
				r1.idmap.add(rightIdVal, idval_t(val.guid(), NO_STEP));
                r1.idmap.add(rightIdCount, idval_t(count.guid(), NO_STEP));
			}
		}
	}
}

/* unify((p1, R1, ∗1), (p2, R2, ∗2))
begin
	(p2', R2', ∗2') ← (p2, R2, ∗2)
	C ← vars(p1) ∩ vars(p2)								// common variables
	V1 ← vars(p1) \ C
	V2 ← vars(p2) \ C
	B ← base(p1, p2)

	M1 ← []
	M2 ← []
	for all xi ∈ V1
		M1[xi] = norm(xi, p1, B)
	end for
	for all xj ∈ V2
		M2[xj] = norm(xj, p2, B)
	end for

	for all (xi, xj) ∈ V1 × V2
		vi = M1[xi]
		vj = M2[xj]
		if vi ≠ [] and vj ≠ [] and vi = vj					// variables are equivalent
			Replace xj by xi in p2', R2', and ∗2'
			Replace ∗(xj) by ∗(xi) in p2', R2', and ∗2'
		end if
	end for

	for all r ∈ Dom(R1) ∩ Dom(R2) 							// variables are trivially equivalent
		Replace R2(r) by R1(r) in p2', R2', and ∗2'
    end for
	return (p2', R2', ∗2')
end
*/
// TODO modifier pour garder quand meme des deux cotés ce qui est dans DAMAGED
// TODO unifier le summary aussi ici
// TODO faire la projection sur le damaged
void PPLDomain::_doUnify(PPLDomain &l1 /* back */, PPLDomain &r1 /* header */, PPLDomain* this_dom, bool noPtr, bool avcreate) const { // no longer a const for tabcode

#ifdef POLY_DEBUG
	cout << "Unify phase." << endl;
    cout << "Left state (back if widening): " << endl;
	cout << l1;
    cout << "Right state (header if widening): " << endl;
    cout << r1;
    if (avcreate)
        cout << "This unify is subjected to avatar-creation. " << endl;
#endif
    if (avcreate) {
        l1.doRemoveUselessAvatars();
        l1.doRemoveAllAvatars();
        l1.doFinalizeUpdate();
    }

    /*
     * When unifying before a widening, order is important, bc of the avatar creation phase.
     *
     * The l1 state contains back-state, the r1 state contains header-state.
     *
     * Avatar creation is for the headerState. Rename is for the backstate.
     *
     * We keep the idmap from state l1 (back state), subjected to renaming.
     */

    MyHTable<guid_t,guid_t> rename; /* renaming for back state */

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
		printAll(commonVars, "Common vars:");
#endif

		std::set<guid_t> indepVars; /* B dans l'algo (C') */
		for (std::set<guid_t>::const_iterator it = commonVars.begin(); it != commonVars.end(); it++) {
			WPoly temp = commonPoly;
			WVar v(*it);
			temp.filter_lambda([v, &indepVars](guid_t guid) {
				return ((guid == v.guid()) || (indepVars.find(guid) != indepVars.end()));
			});

			WPoly::ConsIterator it2(temp);
			for (; it2 && !(*it2).is_equality(); it2++); // look for the next equality
			if (!it2) {
                // if (v.guid() < 20)
				indepVars.insert(*it);
			}
		}
#ifdef POLY_DEBUG
		printAll(indepVars, "Independant common vars:");
#endif
        MyHTable<guid_t, LEVect> leftCSMap; // M1, linexpr vars de gauche
        MyHTable<guid_t, LEVect> rightCSMap; // M2, linexpr vars de droite

        /* Express BASE variables in terms of linear combination of independant vars */
        _identifyPolyVars(l1, leftOnlyVars, indepVars, leftCSMap);
        _identifyPolyVars(r1, rightOnlyVars, indepVars, rightCSMap);

        // Enable inverse lookup (i.e. find right (i.e. header) var from linexpr)
        MyHTable<LEVect, guid_t, HashLEVect> invRightCSMap;
        for (MyHTable<guid_t, LEVect >::PairIterator it(rightCSMap); it; it++) {
            invRightCSMap.put((*it).snd, (*it).fst);
        }
        MyHTable<LEVect, guid_t, HashLEVect> invLeftCSMap;
        for (MyHTable<guid_t, LEVect >::PairIterator it(leftCSMap); it; it++) {
            invLeftCSMap.put((*it).snd, (*it).fst);
        }

	/*
        _doMatchGlobals(l1, r1, leftCSMap, invRightCSMap);
        _doMatchGlobals(r1, l1, rightCSMap, invLeftCSMap);
	*/
        /* Detect variable in left (i.e. backstate) subjected to avatar creation */
        if (avcreate) {
            for (MyHTable<guid_t, LEVect >::PairIterator it(leftCSMap); it; it++) {
                const LEVect& vj = (*it).snd;
                if (!invRightCSMap.hasKey(vj) && !r1.isVarMapped((*it).fst, STEP_BOT)) {
                    int step;
                    Ident idBase = l1.getIdent((*it).fst, &step);
                    Ident idVal = Ident(idBase.getId(), Ident::ID_MEM_VAL);
                    if (!l1.hasIdent(idVal)) {
                       continue;
                    }

                    /* guid of variables in header state */
                    guid_t newBase = 42, newCount = 42;
                    guid_t newValPlus = 42, newValMinus = 42;

                    /* cout << "Creating avatar for backstate base variable: " << (*it).fst << endl; */

                    bool created = PPLDomain::_avcreate_helper(r1, l1, rightVars, idBase, WVar((*it).fst), step, newBase, newCount, newValPlus, newValMinus);
                    if (!created) {
                        continue;
                    }
                    Ident idCount = Ident(idBase.getId(), Ident::ID_MEM_COUNT);
                    Ident idPlus = Ident(idBase.getId(), Ident::ID_MEM_VAL_PLUS);
                    Ident idMinus = Ident(idBase.getId(), Ident::ID_MEM_VAL_MINUS);

                    WVar backVarCount = l1.getVar(idCount, nullptr);
                    WVar backVarPlus = l1.getVar(idPlus, nullptr);
                    WVar backVarMinus = l1.getVar(idMinus, nullptr);
                    rename.add((*it).fst, newBase);
                    rename.add(backVarCount.id(), newCount);
                    rename.add(backVarPlus.id(), newValPlus);
                    rename.add(backVarMinus.id(), newValMinus);
                }
            }
            avcreate = false;
        }

        for (MyHTable<guid_t, LEVect >::PairIterator it(leftCSMap); it; it++) {
            const guid_t& xj = (*it).fst;
            const LEVect& vj = (*it).snd;

            if (!invRightCSMap.hasKey(vj))
                continue;

            int left_step;
            int right_step;
            // invLeftCSMap.put(vj, xj);
            Ident leftIdBase = l1.getIdent(xj, &left_step);
            Ident rightIdBase = r1.getIdent(invRightCSMap[vj], &right_step);

            bool step_ok = (left_step == right_step) || (left_step == STEP_BOT) || (right_step == STEP_BOT);
            if (!step_ok)
                continue;

            if (!((right_step == STEP_BOT) || (left_step != STEP_BOT)))
                continue;

            Ident leftIdCount = Ident(leftIdBase.getId(), Ident::ID_MEM_COUNT);
            ASSERT(l1.hasIdent(leftIdCount));

            Ident rightIdCount = Ident(rightIdBase.getId(), Ident::ID_MEM_COUNT);
            ASSERT(r1.hasIdent(rightIdCount));

            Ident leftIdVal = Ident(leftIdBase.getId(), Ident::ID_MEM_VAL);
            Ident rightIdVal = Ident(rightIdBase.getId(), Ident::ID_MEM_VAL);
            bool leftHasAvatar = !l1.hasIdent(leftIdVal);
            bool rightHasAvatar = !r1.hasIdent(rightIdVal);

            if (leftHasAvatar && !rightHasAvatar) {
                WVar vminus = r1.getVar(rightIdVal, nullptr);
                WVar vplus = r1._doAvatarSplit(vminus);
                Ident rightIdValPlus = Ident(rightIdBase.getId(), Ident::ID_MEM_VAL_PLUS);
                Ident rightIdValMinus = Ident(rightIdBase.getId(), Ident::ID_MEM_VAL_MINUS);
                r1.idmap.del1(rightIdVal);
                r1.idmap.add(rightIdValPlus, idval_t(vplus.id(), NO_STEP));
                r1.idmap.add(rightIdValMinus, idval_t(vminus.id(), NO_STEP));
                r1.doNewConstraint(vplus == vminus);
                rightHasAvatar = true;
            }


            if (rightHasAvatar && !leftHasAvatar) {
                WVar vminus = l1.getVar(leftIdVal, nullptr);
                WVar vplus = l1._doAvatarSplit(vminus);
                Ident leftIdValPlus = Ident(leftIdBase.getId(), Ident::ID_MEM_VAL_PLUS);
                Ident leftIdValMinus = Ident(leftIdBase.getId(), Ident::ID_MEM_VAL_MINUS);
                l1.idmap.del1(leftIdVal);
                l1.idmap.add(leftIdValPlus, idval_t(vplus.id(), NO_STEP));
                l1.idmap.add(leftIdValMinus, idval_t(vminus.id(), NO_STEP));
                l1.doNewConstraint(vplus == vminus);
                leftHasAvatar = true;
            }
            /* rename BASE and COUNT */
            rename.add(xj, invRightCSMap[vj]);
            rename.add(l1.getVar(leftIdCount, nullptr).guid(), r1.getVar(rightIdCount, nullptr).guid());

            /* Rename Vplus and Vminus (avatar case), or Value (non-avatar case) */
            if (leftHasAvatar && rightHasAvatar) {
                Ident leftIdValPlus = Ident(leftIdBase.getId(), Ident::ID_MEM_VAL_PLUS);
                Ident rightIdValPlus = Ident(rightIdBase.getId(), Ident::ID_MEM_VAL_PLUS);
                Ident leftIdValMinus = Ident(leftIdBase.getId(), Ident::ID_MEM_VAL_MINUS);
                Ident rightIdValMinus = Ident(rightIdBase.getId(), Ident::ID_MEM_VAL_MINUS);
                rename.add(l1.getVar(leftIdValPlus, nullptr).guid(), r1.getVar(rightIdValPlus, nullptr).guid());
                rename.add(l1.getVar(leftIdValMinus, nullptr).guid(), r1.getVar(rightIdValMinus, nullptr).guid());
            } else {
                rename.add(l1.getVar(leftIdVal, nullptr).guid(), r1.getVar(rightIdVal, nullptr).guid());
            }
        }

        Vector<Ident> toUnMapLeft;
        Vector<Ident> toUnMapRight;
        Vector<Pair<Ident, idval_t> > toReMapLeft;
        Vector<Pair<Ident, idval_t> > toReMapRight;

		// take care of memory variables that were already same()
		for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = l1.idmap.getPairIter(); it; it++) {
			for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it2 = r1.idmap.getPairIter(); it2; it2++) {
				WVar x1((*it).snd.g);
				WVar x2((*it2).snd.g);
         //       cout << "testing same: " << x1 << " == " << x2 << endl;

                /*
				if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
					ASSERT((*it).snd.s == 0);
				}
				if ((*it2).fst.getType() == Ident::ID_MEM_ADDR) {
					ASSERT((*it2).snd.s == 0);
				}
                */
                if (((*it).fst.getType() == Ident::ID_MEM_ADDR) || ((*it).fst.getType() == Ident::ID_MEM_COUNT)|| ((*it).fst.getType() == Ident::ID_MEM_VAL_PLUS)|| ((*it).fst.getType() == Ident::ID_MEM_VAL_MINUS)|| ((*it).fst.getType() == Ident::ID_MEM_VAL)) {
                    // cout << "correct type" << endl;
                    if (x1.guid() == x2.guid()) {
                     //   cout << "same!" << endl;
                        if ((*it).fst.getType() != (*it2).fst.getType()) {
                            if ((*it).fst.getType() == Ident::ID_MEM_VAL && (*it2).fst.getType() == Ident::ID_MEM_VAL_PLUS) {
                                Ident rightIdMinus = Ident((*it2).fst.getId(), Ident::ID_MEM_VAL_MINUS);
                                WVar rvminus = r1.getVar(rightIdMinus, nullptr);
                                WVar rvplus((*it2).snd.g);
                                WVar lvminus((*it).snd.g);
                                WVar lvplus = l1._doAvatarSplit(lvminus);
                                l1.doNewConstraint(lvplus == lvminus);
                                Ident leftIdValPlus = Ident((*it).fst.getId(), Ident::ID_MEM_VAL_PLUS);
                                Ident leftIdValMinus = Ident((*it).fst.getId(), Ident::ID_MEM_VAL_MINUS);

                                toUnMapLeft.push((*it).fst);

                                toReMapLeft.push(Pair<Ident, idval_t>(leftIdValPlus, idval_t(lvplus.id(), NO_STEP)));
                                toReMapLeft.push(Pair<Ident, idval_t>(leftIdValMinus, idval_t(lvminus.id(), NO_STEP)));

                                rename.add(lvminus.id(), rvminus.id());
                                rename.add(lvplus.id(), rvplus.id());

                            } else if ((*it).fst.getType() == Ident::ID_MEM_VAL_PLUS && (*it2).fst.getType() == Ident::ID_MEM_VAL) {
                                Ident leftIdMinus = Ident((*it2).fst.getId(), Ident::ID_MEM_VAL_MINUS);
                                WVar lvminus = r1.getVar(leftIdMinus, nullptr);
                                WVar lvplus((*it).snd.g);
                                WVar rvminus((*it2).snd.g);
                                WVar rvplus = l1._doAvatarSplit(rvminus);

                                l1.doNewConstraint(rvplus == rvminus);
                                Ident rightIdValPlus = Ident((*it2).fst.getId(), Ident::ID_MEM_VAL_PLUS);
                                Ident rightIdValMinus = Ident((*it2).fst.getId(), Ident::ID_MEM_VAL_MINUS);

                                toUnMapRight.push((*it2).fst);

                                toReMapRight.push(Pair<Ident, idval_t>(rightIdValPlus, idval_t(rvplus.id(), NO_STEP)));
                                toReMapRight.push(Pair<Ident, idval_t>(rightIdValMinus, idval_t(rvminus.id(), NO_STEP)));

                                rename.add(lvminus.id(), rvminus.id());
                                rename.add(lvplus.id(), rvplus.id());
                            } else {
                                cout << "type1: " << int((*it).fst.getType()) << endl;
                                cout << "type2: " << int((*it2).fst.getType()) << endl;
                                ASSERT(false);
                            }
                        } else {
                            rename.add(x1.guid(), x2.guid());
                        }
					}
				}
			}
		}

        for (Vector<Ident>::Iter it(toUnMapLeft); it; it++)
            l1.idmap.del1(*it);

        for (Vector<Ident>::Iter it(toUnMapRight); it; it++)
            r1.idmap.del1(*it);

        for (Vector<Pair<Ident, idval_t> >::Iter it(toReMapLeft); it; it++)
            l1.idmap.add((*it).fst, (*it).snd);

        for (Vector<Pair<Ident, idval_t> >::Iter it(toReMapRight); it; it++)
            l1.idmap.add((*it).fst, (*it).snd);


	}



	// rename register variables
	for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = l1.idmap.getPairIter(); it; it++) {
        if (((*it).fst.getType() != Ident::ID_MEM_ADDR) && ((*it).fst.getType() != Ident::ID_MEM_VAL_PLUS )&& ((*it).fst.getType() != Ident::ID_MEM_VAL_MINUS)&& ((*it).fst.getType() != Ident::ID_MEM_COUNT)&& ((*it).fst.getType() != Ident::ID_MEM_VAL))
			if (r1.idmap.has1((*it).fst)) {
				ASSERT((*it).snd.s == NO_STEP);
				ASSERT(r1.idmap.find1((*it).fst).s == NO_STEP);
				rename.add((*it).snd.g, r1.idmap.find1((*it).fst).g);
				// cout << "[R] mapping " << (*it).snd << " to " << r1.idmap.find1((*it).fst) << endl;
			}
	}



	// do it!
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

/* join(<>, <(x_b, 0, 1) = x_val>) = <(x_b, 0, x_n) = x_val; 0 ≤ x_n ≤ 1> */
void PPLDomain::doCLPJoin(const WVar& x_b, const PPLDomain &dom, PPLDomain& this_dom) {
	// identify x_b
	Ident id_b = dom.getIdent(x_b, 0);
	// create x_n with the same ID as x_b
	Ident id_n(id_b.getId(), Ident::ID_MEM_COUNT);
	WVar x_n = this_dom.varNew(id_n, NO_STEP, false, false);
	// add the constraint 0 ≤ x_n ≤ 1
	this_dom.doNewConstraint(x_n >= 0);
	this_dom.doNewConstraint(x_n <= 1);
	DBG("join(<(" << id_b << ", 0, 1) = " << Ident(id_b.getId(), Ident::ID_MEM_VAL) << ">, <>) = <(" 
		<< id_b << ", 0, " << x_n << ") = " << Ident(id_b.getId(), Ident::ID_MEM_VAL) << "; 0 ≤ "<< x_n << " ≤ 1>")

	// DBG("doCLPJoin, this_dom=" << PPLManager::t(this_dom));
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

bool PPLDomain::mustSupseteq(const WVar &b1, const int s1, const WVar &n1, const WVar &b2, const int s2, const WLinExpr &n2) const {
    if (s1 == NO_STEP) {
        if (s2 != NO_STEP)
            return false;
        WPoly tmp = poly;
        tmp.add_constraint(b1 == b2);
        tmp.add_constraint(n1 == 1);
        tmp.add_constraint(n2 == 1);
        return tmp == poly;
    } else {
        if (s2 == NO_STEP) {
            // cout << "test simple inclusion" << endl;
            return poly.relation_with(b1 <= b2).implies(PPL::Poly_Con_Relation::is_included()) &&
                   poly.relation_with(b1 + s1*(n1 - 1) >= b2 + s2*(n2 - 1)).implies(PPL::Poly_Con_Relation::is_included()) &&
                   (delta(b1, b2, s1) == 0);
            /*
            WPoly tmp = poly;
            tmp.add_constraint(b1 <= b2);
            tmp.add_constraint(b1 + s1*(n1 - 1) >= b2 + s2*(n2 - 1));

            int v = delta(b1, b2, s1);
            */
            // cout << "delta b1=" << b1 << " b2 = " << b2 << " s1 = " << s1 << endl;
            //cout << "in range? " << (tmp == poly) << " delta: " << v << endl;
            //return (v == 0) && (tmp == poly);
        } else {
            /* TODO */
            cout << "mustSupseteq case not implemented" << endl;
            ASSERT(false);

        }
    }
}

bool PPLDomain::mayIntersect(const WVar &b1, const int s1, const WVar &n1, const WVar &b2, const int s2, const WVar &n2) const {

}



int PPLDomain::delta(const WVar &v1, const WVar &v2, int step) const {
    int d = -1;
    if (step == 1) return 0;
    for (WPoly::VarIterator it(poly); it; it++) { // TODO PERF: skip non-induction variables
        WVar v(*it);
        if (v.id() == v1.id() || v.id() == v2.id()) continue;
        WPoly temp = poly;
        temp.filter_lambda([&v1, &v2, &v](guid_t guid) {
            return guid == v.id() || guid == v1.id() || guid == v2.id();
        });

        for (WPoly::ConsIterator it(temp); it; it++) {
            WCons c = *it;


            if (c.is_equality() && c.has_var(v) && c.coefficient(v) != 0 && c.has_var(v1) && c.coefficient(v1) != 0 && c.has_var(v2) && c.coefficient(v2) != 0) {
                if (c.coefficient(v) / c.coefficient(v1) == step && c.coefficient(v) / c.coefficient(v2) == -step) {
                    d = c.inhomogeneous_term().get_si();
                    goto out;
                }
            }
            if (c.is_equality() && (!c.has_var(v) || c.coefficient(v) == 0) && c.has_var(v1) && c.coefficient(v1) != 0 && c.has_var(v2) && c.coefficient(v2) != 0) {
                if (c.coefficient(v2) / c.coefficient(v1) == -1) {
                    d = ((c.coefficient(v2).get_si() < 0) ? c.inhomogeneous_term().get_si() : -c.inhomogeneous_term().get_si());
                    goto out;
                }
            }
        }
    }
    out:
    if ((d == -1) || (step == 0))
        return d;
    return d % step;
}
void PPLDomain::_euclid(int a, int b, int &gcd, int &inverse) {
    int rest = b;
    int old_rest = a;
    int bezout = 1;
    int old_bezout = 0;
    int quotient, tmp;
    while (rest != 0) {
        quotient = old_rest / rest;

        tmp = rest;
        rest = old_rest - quotient*rest;
        old_rest = tmp;

        tmp = bezout;
        bezout = old_bezout - quotient*bezout;
        old_bezout = tmp;
    }
    gcd = old_rest;
    inverse = old_bezout % a;
}

int PPLDomain::lca(const WVar &b1, const int s1, const WVar &n1, const WVar &b2, const int s2, const WVar &n2) const {
    if (poly.relation_with(b1 <= b2).implies(PPL::Poly_Con_Relation::is_included())) {
        int v = delta(b1, b2, s1);
        if (v >= 0) {
            int gcd, inverse, dummy;
            _euclid(s1, s2, gcd, dummy);
            if ((v % gcd) == 0) {
                int s2p = s2 / gcd;
                int s1p = s1 / gcd;
                int vp = v / gcd;
                _euclid(s1p, s2p, dummy, inverse);
                return ((-v * inverse) % s1p) % s2;
            }

        }
    }
    return -1;
}

static void _report_constraints_containing(const WPoly &source, WPoly &dest, const WVar &v, bool only_eq) {
    //cout << "var to report: " << v << endl;
    for (WPoly::ConsIterator it(source); it; it++) {
        WCons c = *it;
        if (c.has_var(v) && c.coefficient(v) != 0 && (c.is_equality() || !only_eq)) {
            //cout << "report constraint: " << c << endl;
            dest.add_constraint(c);
          }
    }
}

/* static */
bool PPLDomain::_avcreate_helper(PPLDomain &entry, PPLDomain &back, const std::set<guid_t> &entryVars, const Ident &id_base, const WVar &base, int step, guid_t &newBase, guid_t &newCount, guid_t &newValPlus, guid_t &newValMinus) {

   // cout << "back  state before avatar creation: " << back << endl;

    bool found = false;
    //cout << "entry state before avatar creation: " << entry << endl;
 /*   for (MyHTable<Ident, idval_t, HashIdent>::PairIterator it = back.idmap.getPairIter(); it; it++) {
        if ((*it).fst.getType() == Ident::ID_MEM_ADDR) { */
            const Ident &id_count = Ident(id_base.getId(), Ident::ID_MEM_COUNT);

            /* ici on a trouve la variable header qui a le bon id */


            const WVar count = back.getVar(id_count, nullptr);
            if (!entry.isVarMapped(base, step)) {
                //cout << "Base-variable " << base << " exists in backstate but not in entrystate" << endl;
                ASSERT(!entry.isVarMapped(count, NO_STEP));
                if (back.poly.relation_with(count > 0).implies(PPL::Poly_Con_Relation::is_included())) {
                    found = true;
                    //cout << "The associated count variable " << count << " cannot be 0 (good)" << endl;
                    const Ident &id_val = Ident(id_base.getId(), Ident::ID_MEM_VAL);
                    const Ident &id_vplus = Ident(id_base.getId(), Ident::ID_MEM_VAL_PLUS);
                    const Ident &id_vminus = Ident(id_base.getId(), Ident::ID_MEM_VAL_MINUS);

                    WVar vb_minus = back.getVar(id_val, nullptr);
                    //cout << "before avatar split: " << back.poly << "...end" << endl;

                    WVar vb_plus = back._doAvatarSplit(vb_minus); /* function FIXED */
                    //cout << "after avatar split: " << back.poly << "...end" << endl;


                    /* vb_minus and vb_plus represents the properties of the back-edge variable */

                    /* reporter les contraintes sur entry edge */

                    /* First, collect guid of variables appearing in entry state */
                    /* NOTE: guid of unconstrained variables must be collected too, as long as they are in mapping */

                    WPoly temp = back.poly;

                    //cout << "Back poly " << temp << "...end" << endl;

                    //cout << "vb_plus id == " << vb_plus.id() << endl;
                    //cout << "vb_minus id == " << vb_minus.id() << endl;

                    /* TODO PERF reporter les contraintes de base/vplus/vminus ensemble */


                    temp.filter_lambda([&base, &entryVars](guid_t guid) {
                        bool exists_in_entry = entryVars.find(guid) != entryVars.end();
                        bool keep = (guid == base.id()) || exists_in_entry;
                        //cout << "Do we keep variable " << guid << " ? " << keep << endl;
                        return keep;
                    });

                    /* iterate on equality constraints containing base */

                    //cout << "Intersect for base " << temp << "...end" << endl;
                    //cout << "Entry before intersect " << entry.poly << "...end" << endl;

                    _report_constraints_containing(temp, entry.poly, base, true);
                    //entry.poly.intersection_assign(temp);
                    //cout << "Entry after intersect " << entry.poly << "...end" << endl;

                    temp = back.poly;



                    temp.filter_lambda([&vb_plus, &entryVars](guid_t guid) {
                        bool exists_in_entry = entryVars.find(guid) != entryVars.end();
                        bool keep = (guid == vb_plus.id()) || exists_in_entry;
                        //cout << "Do we keep variable " << guid << " ? " << keep << endl;
                        return keep;
                    });
                    //cout << "Intersect for vplus " << temp << "...end" << endl;
                    //cout << "Entry before intersect " << entry.poly << "...end" << endl;

                    _report_constraints_containing(temp, entry.poly, vb_plus, false);
                    //cout << "Entry after intersect " << entry.poly << "...end" << endl;

                    temp = back.poly;

                    temp.filter_lambda([&vb_minus, &entryVars](guid_t guid) {
                        bool exists_in_entry = entryVars.find(guid) != entryVars.end();
                        return (guid == vb_minus.id()) || exists_in_entry;
                    });
                    // cout << "report for vminus " << temp << "...end" << endl;

                    _report_constraints_containing(temp, entry.poly, vb_minus, false);
                    // cout << "Entry after report " << entry.poly << "...end" << endl;


                    /* updater les clp */

                    /* modifier la clp backedge pour que ca pointe sur les avatars */


                    WVar var_vminus = back.varNew(id_vminus, NO_STEP, true, false);
                    WVar var_vplus = back.varNew(id_vplus, NO_STEP, true, false);
                    back.doNewConstraint(var_vminus == vb_minus);
                    back.doNewConstraint(var_vplus == vb_plus);

                    back.varKill(id_val);
                    back.victims.add(vb_plus.guid());
                    back.victims.add(vb_minus.guid());


                    /* rajouter la nouvelle clp pour le entry edge */

                    Ident id_base2, id_vplus2, id_vminus2, id_count2;
                    entry.varCreateAvClp(id_base2, id_vplus2, id_vminus2, id_count2);


                    WVar var_base2, var_vplus2, var_vminus2;
                    var_base2 = entry.varNew(id_base2, step, false, false);
                    var_vplus = entry.varNew(id_vplus2, NO_STEP, false, false);
                    var_vminus = entry.varNew(id_vminus2, NO_STEP, false, false);
                    entry.idmap.add(id_count2, idval_t(count.guid(), NO_STEP));
                    newCount = count.guid();
                    newBase = var_base2.id();
                    newValPlus = var_vplus.id();
                    newValMinus = var_vminus.id();


                    entry.doNewConstraint(count == 0); /* FIXED */
                    /* FIXME pourquoi creer var_base2 / var_vplus / var_vminus ? */
                    /* parce que varNew créé les variables */
                    entry.doNewConstraint(var_base2 == base); /* TODO recopier contraintes comme sur vplus/vminus*/
                    entry.doNewConstraint(var_vplus == vb_plus);
                    entry.doNewConstraint(var_vminus == vb_minus);

                    entry.victims.add(base.guid());
                    entry.victims.add(vb_plus.guid());
                    entry.victims.add(vb_minus.guid());

                } else cout << "avatar creation: count can be 0" << endl;
            } else {
                cout << "avatar creation: entry base var exists even though caller must ensure that it doesn't" << endl;
       /*     }
        } */
    }
    entry.doFinalizeUpdate();
    back.doFinalizeUpdate();
    //cout << "entry state after avatar creation: " << entry << endl;
    //cout << "back state after avatar creation: " << back << endl;
    return found;
}


bool PPLDomain::_isArrayStore(const BasicBlock *bb, const WVar &storeAddr) const {
    return true;
    MyHTable<guid_t,guid_t> map;
    std::vector<const Block* > headers;
    const Block *current = bb;

    if (LOOP_HEADER(current))
        headers.push_back(current);

    for (;;) {
        current = ENCLOSING_LOOP_HEADER(current);
        if (current == nullptr) {
            break;
        } else {
            headers.push_back(current);
        }
    }

    Ident idSsp(Ident::ID_START_SP, Ident::ID_SPECIAL);
    WVar ssp = getVar(idSsp, nullptr);
    map[ssp.guid()] = 0;
    guid_t g = 1;
    for (std::vector<const Block*>::const_iterator it = headers.begin(); it != headers.end(); it++, g++) {
        Ident idBound((*it)->id(), Ident::ID_LOOP);
        if (hasIdent(idBound)) {
            WVar v = getVar(idBound, nullptr);
            map[v.guid()] = g;
        }
    }
    map[storeAddr.guid()] = g;

    PPLDomain tmp(*this);
    //cout << "arraystore var " << storeAddr << " guess domain input: " << tmp << endl;
    tmp.doMap(MapGuid(map));
    //cout << "arraystore guess domain helper: " << tmp << endl;
//    cout <<"\n";
    for (WPoly::ConsIterator it(tmp.poly); it; it++) {
        const WCons &c = (*it);
        bool has_store = false;
        bool has_bound = false;
        if (!c.is_equality()) continue;
        for (WCons::TermIterator it2(c); it2; it2++) {
            const WVar &v = (*it2);
            if (v.guid() == g) {
                has_store = true;
            } else if (v.guid() > 0) {
                has_bound = true;
            }
            if (has_store && has_bound) {
		cout << "found cons: " << c << endl;
                return true;
	    }
        }
    }
    return false;
}





void PPLDomain::_identifyPolyVars(const PPLDomain &d, const std::set<guid_t> &vars, const std::set<guid_t> &indep, MyHTable<guid_t, LEVect > &csmap, Ident::IdentType ident_type) const {
	const WPoly &poly = d.poly;
    PDBG("Begin _identifyPolyVars, type=" << ((int)ident_type) << endl)

    bool is_count = ident_type == Ident::ID_MEM_COUNT;
	for (std::set<guid_t>::const_iterator it = vars.begin(); it != vars.end(); it++) {
		WVar v(*it);
		PDBG("trying for: " << v << endl)

        for(int step = is_count ? -1 : 0; step < ( is_count ? 0 : MAX_STEP); step++) { /* TODO cette boucle est useless */
            if (!d.isVarMapped(v, step) || d.getIdent(v, step).getType() != ident_type /* Ident::ID_MEM_ADDR */) // tableaux
				continue;

			WPoly temp = poly;
			/* on veut calculer linexpr de v */
			/* v == la variable dont on veut calculer le linexpr */ 
			/* indep == l ensemble npiv */ 
			temp.filter_lambda([&indep, v](guid_t guid) {
					return ((guid == v.guid()) || (indep.find(guid) != indep.end()));
			});

			// on attend une egalite
			WPoly::ConsIterator it2(temp);
			for (; it2; it2++)
				if((*it2).is_equality() && (*it2).has_var(v))
					break;

			if (it2) {
				PDBG("Found equality: " << (*it2) << endl)
				WCons c = *it2;
				Vector<PPL::Coefficient> vect;
				vect.setLength(indep.size() + 2); /* vector format: [Indep. vars coefs, Current var (v) coef, Constant] */
				for (int i = 0; i < vect.length(); i++)
					vect[i] = 0;
				bool seen_v = false;
				// parcourt tous les termes
				for (WCons::TermIterator it3(c); it3; it3++) {
					WVar v2(*it3);
					if ((v2.guid() != v.guid()) && (c.coefficient(v2) != 0)) {
						ASSERT(indep.find(v2.guid()) != indep.end()); // must be true because of projection, and because v2!=v
						PPL::Coefficient coef = c.coefficient(v2);
						// on les met dans le vecteur
						vect[std::distance(indep.begin(), indep.find(v2.guid()))] = coef;
					}
					if (v2.guid() == v.guid()) {
						seen_v = true;
						vect[vect.length() - 2] = c.coefficient(v);
						ASSERT(c.coefficient(v) != 0);
					}
				}
				ASSERTP(seen_v, "nvm.");

				vect[vect.length() - 1] = c.inhomogeneous_term();

				// on calcule le pgcd
				PPL::Coefficient pgcd = vect[vect.length() - 1];
				for (int i = 0; i < vect.length() - 1; i++) {
					PPL::Coefficient coef2 = vect[i];
					while (coef2 != 0) {
						PPL::Coefficient tmp = pgcd;
						pgcd = coef2;
						coef2 = tmp % coef2;
					}
				}

				// on divise par le pgcd
				if (pgcd != 1) {
					for (int i = 0; i < vect.length(); i++) {
						vect[i] /= pgcd;
					}
				}

				PDBG("variable " << v << " has vect: " << vect << endl)
                csmap.put(v.guid(), LEVect(vect));
			}
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
WVar PPLDomain::_doAvatarSplit(WVar &vminus) {
    WVar vplus = varNew();
    WPoly result(false);
    //cout << "VPlus is " << vplus << " VMinus is " << vminus << endl;

    for (WPoly::ConsIterator it(poly); it; it++) {
        WCons c = *it;
        // cout << "Processing constraint: " << c << endl;
        if (c.has_var(vminus) && c.coefficient(vminus) != 0) {
            WLinExpr le;
            WLinExpr le2;
            for (WCons::TermIterator it2(c); it2; it2++) {
                if (c.has_var(*it2) && c.coefficient(*it2) != 0) {
                    if ((*it2).id() == vminus.id()) {
                        if (c.coefficient(vminus) > 0) {
                            le = le + c.coefficient(*it2) * vplus;
                            le2 = le2 + c.coefficient(*it2) * (*it2);
                        } else {
                            le = le + c.coefficient(*it2) * (*it2);
                            le2 = le2 + c.coefficient(*it2) * vplus;
                        }
                    } else {
                        le = le + c.coefficient(*it2) * (*it2);
                        le2 = le2 + c.coefficient(*it2) * (*it2);
                    }

                }
            }
            le = le + c.inhomogeneous_term(); /* TODO verifier signe. Pourquoi? */
            le2 = le2 + c.inhomogeneous_term(); /* TODO verifier signe. Pourquoi? */

            result.add_constraint(le >= 0);
            //cout << " Processed constraint: " << (le >= 0) << endl;
            if (c.is_equality()) {
                result.add_constraint(le2 <= 0);
                //cout << " Processed constraint: " << (le2 <= 0) << endl;
            }
        } else {
            //cout << " Processed constraint: " << c << endl;
            result.add_constraint(c);
        }
    }
    this->poly = result;
    //cout << "After avatar split: " << *this << endl;
    return vplus;
}

void PPLDomain::enableSummary() {
	_summary = new PPLSummary();
	/*
	const WVar &spInput = varNew(Ident(13, Ident::ID_REG_INPUT));
	 doNewConstraint(spInput == getVar(Ident(13, Ident::ID_REG)));
	 */
}

extern elm::String cfgname;

void PPLDomain::showTab(const Ident &ident) {
	Ident id_ssp(Ident::ID_START_SP, Ident::ID_SPECIAL);
	int step;
	WVar ssp = getVar(id_ssp, nullptr);
	WVar store = getVar(ident, &step);

	WPoly temp = poly;
	temp.filter_lambda([store, ssp](guid_t guid) {
		return guid == store.guid() || guid == ssp.guid();
	});
	for (WPoly::ConsIterator it(temp); it; it++) {
		const WCons &c = (*it);
		bool has_store = false;
		bool has_ssp = false;
		bool has_other = false;
		if (!c.is_equality()) continue;
		for (WCons::TermIterator it2(c); it2; it2++) {
		    const WVar &v = (*it2);
		    if (v.guid() == ssp.guid()) {
			has_ssp = true;
		    } else if (v.guid() == store.guid()) {
			has_store = true;
		    } else {
			has_other = true;
		    }
		    if (has_store && has_ssp && !has_other && c.coefficient(ssp) == -c.coefficient(store)) {
			int cst = -c.inhomogeneous_term().get_si();
			int coef = -c.coefficient(store).get_si();
			cout << "ARRAY: type=local func=" << cfgname << " addr=START_SP-" << hex(cst/coef) << endl;
		    }
		    if (has_store && !has_ssp && !has_other) {
			int cst = -c.inhomogeneous_term().get_si();
			int coef = c.coefficient(store).get_si();
			
			cout << "ARRAY: type=global addr=" << hex(cst/coef) << endl;
		    }
		}
        }
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
