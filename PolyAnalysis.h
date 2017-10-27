// #define POLY_DEBUG 1
//
#ifndef OTAWA_POLY_ANALYSIS_FEATURE_H
#define OTAWA_POLY_ANALYSIS_FEATURE_H

#include <otawa/otawa.h>
#include <otawa/ipet.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/prog/sem.h>
#include <elm/util/BitVector.h>
#include <ppl.hh>


namespace otawa { namespace poly {

using namespace otawa;
using namespace otawa::util;

namespace PPL = Parma_Polyhedra_Library;
using Variable = PPL::Variable;

extern Identifier<int> NUM_LOC_VARS;
extern Identifier<int> LOC_VAR_SIZE;
extern Identifier<int> MAX_AXIS;
extern p::feature POLY_ANALYSIS_FEATURE;

class PPLManager;
class HashIdent;
class PPLDomain;
class Ident;

class Ident {

	public:
		friend class HashIdent;
		enum IdentType {
			ID_REG=0,
			ID_MEM_ADDR,
			ID_MEM_VAL,
			ID_SPECIAL,
			ID_LOOP,
			ID_INVALID,
			ID_MAX_TYPE,
		};
		enum IdentSpecial {
			ID_START_SP=0,
			ID_START_FP=1,
			ID_START_LR=2,
		};
		inline Ident() : _type(ID_INVALID) { }
		inline Ident(int id, IdentType typ, const PPLDomain *dom = NULL) : _id(id), _type(typ) { }
		inline ~Ident() { } 
		inline int getId() const { return _id; }
		inline IdentType getType() const { return _type; }

		inline void print(io::Output &out) const {
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
		inline bool operator==(const Ident &i) const {
			return (_id == i._id) && (_type == i._type);
		}
		inline bool operator!=(const Ident &i) const {
			return (_id != i._id) || (_type != i._type);
		}

	private:
		int _id;
		IdentType _type;
};

class HashIdent {
	public:
		static inline t::hash hash(const Ident& key) { 
			return key._id;
		};
		static inline bool equals(const Ident& key1, const Ident& key2) {   
			return (key1._id == key2._id) && (key1._type == key2._type);  
		}
};

class HashCons {
	public:
		static t::hash hash(const PPL::Constraint& key);
		static inline bool equals(const PPL::Constraint& key1, const PPL::Constraint& key2) { 
			return key1.is_equivalent_to(key2);
		}
};

class PPLDomain {
	friend class PPLManager;

private:
	/* Abstract state */
	PPL::C_Polyhedron poly;

	genstruct::HashTable<Ident , int, HashIdent> id2axis; ///< Mapping from identifier (register/pointers) to polyhedron variable
	genstruct::Vector<Ident> axis2id; ///< Reverse identifier mapping

	Ident compare_reg; ///< Register holding the last comparison result
	sem::cond_t compare_op; ///< Last comparison semantics

	int mem_ref; ///< Highest pointer ID + 1
	int num_axis; ///<Highest poly variable ID + 1

	/* Not really part of the abstract state */
	static BitVector trash; 

	/* Nested classes */
	template <class F> class MapHelper {
		public:
			MapHelper(F &pfunc, int max_in_domain);
			inline PPL::dimension_type max_in_codomain() const { return _max_in_codomain; }
			inline bool maps(PPL::dimension_type i, PPL::dimension_type &j) const { return _pfunc.maps(i, j); }
			inline bool has_empty_codomain() const { return _empty; }
		private:
			F &_pfunc;
			PPL::dimension_type _max_in_domain;
			PPL::dimension_type _max_in_codomain;
			bool _empty;
	};

	/* Partial mapping function that removes variables in set (bitvector) */
	class RemoveMarked {
		public:
			inline ~RemoveMarked() { }
			inline RemoveMarked(BitVector &bv, int size) : _bv(bv), _size(size) { }
			inline bool has_empty_codomain() const { return _bv.countBits() == _size; }
			inline PPL::dimension_type max_in_codomain() const { 
				return _size - _bv.countBits() - 1;
				/* FIXME not always correct due to PPL weirdness */ 
			}
			bool maps(PPL::dimension_type i, PPL::dimension_type &j) const;
		private:
			BitVector &_bv;
			int _size;
	};

	class MapWithHash {
		public:
			inline MapWithHash(genstruct::HashTable<int,int> &map) : _map(map) { }
			inline bool has_empty_codomain() const { return false; }
			inline PPL::dimension_type max_in_codomain() const { return 0; } 
			inline bool maps(PPL::dimension_type i, PPL::dimension_type &j) const {
				if (_map.hasKey(i)) {
					j = _map[i];
					return true;
				} else return false;
			}
		private:
			genstruct::HashTable<int,int> &_map;
	};

public:

	/* Basic operations (constructor, destructor, copy, comparison) */

	/**
	 * Builds a bottom state
	 */
	inline PPLDomain() {
		num_axis = -1;
		poly = PPL::C_Polyhedron(0, PPL::EMPTY);
	}

	/**
	 * Builds a top state
	 * @param maxAxis Maximum number of variables this state can hold
	 */
	inline PPLDomain(int max_axis) {
		num_axis = 0;
		mem_ref = 0;
		trash = BitVector(max_axis);
		poly = PPL::C_Polyhedron(0, PPL::UNIVERSE);

	}

	inline PPLDomain (const PPLDomain &src)  {
		poly = src.poly;
		num_axis = src.num_axis;
		id2axis = src.id2axis;
		axis2id = src.axis2id;
		mem_ref = src.mem_ref;
		compare_reg = src.compare_reg;
		compare_op = src.compare_op;
		trash = src.trash;
	}

	inline ~PPLDomain() { 
		// TODO
	}

	inline void operator=(const PPLDomain& dom) {
		poly = dom.poly;
		num_axis = dom.num_axis;
		id2axis = dom.id2axis;
		axis2id = dom.axis2id;
		compare_reg = dom.compare_reg;
		compare_op = dom.compare_op;
		mem_ref = dom.mem_ref;
		trash = dom.trash;
	
	}

	bool equals(const PPLDomain &) const;

	/* Operations that reads the state and returns information about it */
	inline void print(io::Output & out) const {
		PPL::Constraint_System cons = poly.minimized_constraints();
		cons.print();
		out << "";
	}
	void displayLocVars();
	void displayIdentMap();
	void getRange(Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display = false);
	void getRange(Variable &var, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display = false);

	inline int getVarCount() { return poly.space_dimension(); }
	inline bool isBottom() { return num_axis == -1; }
	inline bool hasFilter() { return (compare_reg.getType() != Ident::ID_INVALID); }
	bool mayEqual(Variable &v1, Variable &v2, int offset = 0);
	bool mustEqual(Variable &v1, Variable &v2, int offset = 0);
	bool getConstant(Ident &id, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d, bool display = false);
	bool getConstant(Variable &var, PPL::Coefficient &cst, PPL::Coefficient &cst_d, bool display = false);
	bool hasVar(int);
	Variable lookup(const Ident&, bool allow_create = false);
	Variable getVar(int);
	bool hasIdent(const Ident&);
	bool exists(Variable&);

	/* Update-like operations, that returns a modified new state */
	PPLDomain onSemInst(sem::inst si, int instaddr);
	PPLDomain onBranch(bool taken);
	PPLDomain onMerge(const PPLDomain& r, bool widen=false);

	PPLDomain onLoopEntry(int loop, bool inner=false);
	PPLDomain onLoopIter(int loop, bool inner=false);
	PPLDomain onLoopExit(int loop, int bound);

	/* Operations that modify the state in-place */
	template <class F> void doMapPoly(F pfunc);
	template <class F> void doMapIdents(F pfunc);
	template <class F> void doMap(F pfunc);
	void doIntegerWrap();
	void doScratch(Ident &id);
	void doDestroy(const Ident&);
	void doDestroy(Variable&);
	void doRename(const Ident &ident, const Ident &newident, bool allow_replace);
	Variable create(const Ident&, bool allow_replace = false);
	void doCreatePtr(Ident&, Ident&);
	Variable *make_var(Ident &id);
	void doFinalizeUpdate();


	int doAllocAxis(const Ident&, bool allow_replace = false);
	void doFreeAxis(int axis);
private: /* Private helper functions */
#ifdef POLY_DEBUG
	void _sanityChecks();
#else
	inline void _sanityChecks() { }
#endif

	const PPL::Constraint *_getConstraintFor(int axis);
	/**
	 * Indexes the pointer in dom, by their expression in terms of registers referenced in map_regs. 
	 * Stores the result in map_ptr.
	 */
	void _indexPointersByExpr(genstruct::HashTable<PPL::Constraint, int, HashCons> &map_ptr, genstruct::HashTable<int, int> map_regs);

	void _partialMerge(PPL::C_Polyhedron &poly1, PPL::C_Polyhedron &poly2) const;
	/**
	 * Computes the join or widening of two abstract states
	 * @param this The first abstract state (will not be modified)
	 * @param r The second abstract state (will not be modified)
	 * @return Join or widening result
	 */
	void _doBinaryOp(int op, Variable *v, Variable *vs1, Variable *vs2);

};


class PPLManager {
public:
	using t = PPLDomain;
	private:

public:
	/**
	 * Create PPLManager for existing init state.
	 */
	inline PPLManager(t &init, const PropList &props) :  _init(init), _bot(), _top(MAX_AXIS(props)) { }


	/**
	 * Create PPLManager using a fresh init state.
	 */
	PPLManager(const PropList &props);

	inline ~PPLManager() { }

	inline t& init(void) { return _init; }
	inline t& bot(void) { return _bot; }
	inline t& top(void) { return _top; }

	inline t join(t& v1, const t& v2) { return v1.onMerge(v2, false); }
	inline t widening(t& v1, const t& v2) { return v1.onMerge(v2, true); }
	inline bool equals(const t& v1, const t& v2) { return v1.equals(v2); }

private:
	t _init;
	t _bot;
	t _top;
};

class PolyAnalysis: public Processor {
public:
	static p::declare reg;
	PolyAnalysis(p::declare& r = reg);

protected:
	void processWorkSpace(WorkSpace*) ;
	void configure(const PropList &props) ;
private:
	using state_t = PPLManager::t;
	void analyzeGraph(CFG &cfg, state_t &s, bool do_init); 
	const PropList* _props;
};

inline bool operator==(const PPLDomain &a, const PPLDomain &b) { return a.equals(b); }
inline bool operator!=(const PPLDomain &a, const PPLDomain &b) { return !(a == b); }


inline Output& operator<<(Output& o, const PPLDomain &dom) { dom.print(o); return o; }
inline Output& operator<<(Output& o, const Ident &i) { i.print(o); return o; }

Output& operator<<(Output& o, const Variable pv);



} } // namespace otawa::poly
#endif	// OTAWA_POLY_ANALYSIS_FEATURE_H

