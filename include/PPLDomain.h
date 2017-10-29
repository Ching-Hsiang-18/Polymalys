// #define POLY_DEBUG 1
//
#ifndef OTAWA_POLY_DOMAIN_H
#define OTAWA_POLY_DOMAIN_H 1

#include <otawa/otawa.h>
#include <otawa/ipet.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/prog/sem.h>
#include <elm/util/BitVector.h>
#include <ppl.hh>

#include "PolyCommon.h"

namespace otawa { namespace poly {

using namespace otawa;
using namespace otawa::util;

namespace PPL = Parma_Polyhedra_Library;
using Variable = PPL::Variable;

enum bound_t : signed long {
	UNREACHABLE = -1,
	UNBOUNDED = -2,
};

class Ident {
	public:
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
		inline Ident(int id, IdentType typ) : _id(id), _type(typ) { }
		inline ~Ident() { } 
		inline int getId() const { return _id; }
		inline IdentType getType() const { return _type; }
		inline bool equals(const Ident &b) const {
			return (_id == b._id) && (_type == b._type);  
		}

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
inline Output& operator<<(Output& o, const Ident &i) { i.print(o); return o; }

class HashIdent {
	public:
		static inline t::hash hash(const Ident& key) { 
			return key.getId();
		};
		static inline bool equals(const Ident& key1, const Ident& key2) {
			return key1.equals(key2);	
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
	BitVector trash; 

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
		compare_reg = Ident();
		compare_op = sem::EQ;
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
		compare_reg = Ident();
		compare_op = sem::EQ;
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

	inline int getVarIDCount() { return poly.space_dimension(); }
	inline bool isBottom() const { return (num_axis == -1) || poly.is_empty(); } 
	inline void setBottom() { *this = PPLDomain(); }
	inline bool hasFilter() { return (compare_reg.getType() != Ident::ID_INVALID); }
	bool mayAlias(const Variable &v1, const Variable &v2, int offset = 0) const;
	bool mustAlias(const Variable &v1, const Variable &v2, int offset = 0) const;
	bool getConstant(Ident &id, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d, bool display = false);
	bool getConstant(Variable &var, PPL::Coefficient &cst, PPL::Coefficient &cst_d, bool display = false);
	bound_t getLoopBound(int loopId);

	/* High-level update operations. They return the modified state. */
	PPLDomain onSemInst(sem::inst si, int instaddr); ///< Process an OTAWA semantic instruction
	PPLDomain onBranch(bool taken); ///< Process a branch, taking care of filtering
	PPLDomain onMerge(const PPLDomain& r, bool widen=false); ///< Process join and widening
	PPLDomain onLoopEntry(int loop, bool inner=false); ///< Process loop entry edge
	PPLDomain onLoopIter(int loop, bool inner=false); ///< Process loop back-edge
	PPLDomain onLoopExit(int loop, int bound); ///< Process loop exit-edge

	/* Operations that modify the state in-place */
	template <class F> void doMapPoly(F pfunc);
	template <class F> void doMapIdents(F pfunc);
	template <class F> void doMap(F pfunc);
	void doIntegerWrap();
	void doScratch(Ident &id);
	void doNewConstraint(const PPL::Constraint &c) { poly.add_constraint(c); }

	/* Variable/Idents handling operations */
	Variable varNew(const Ident&, bool allow_replace = false);
	inline void varKill(const Ident& id) { return _doFreeAxis(id2axis[id]); }
	inline void varKill(const Variable& v) { return _doFreeAxis(v.id()); }
	void varRename(const Ident &ident, const Ident &newident, bool allow_replace);
	Variable getVar(const Ident&, bool allow_varNew = false);
	inline bool isVarMapped(const Variable& v) { return axis2id[v.id()].getType() != Ident::ID_INVALID; }
	inline Ident& getIdent(const Variable& v) { return axis2id[v.id()]; }
	bool hasIdent(const Ident&);
	bool hasVar(Variable&);
	void doFinalizeUpdate();

	/* Pointers/Memory-related operations */

	/**
	 * Create new memory address/value variable and identifiers.
	 */
	void varCreatePtr(Ident&, Ident&);


	/**
	 * Associate a new value to the address variable, replacing existing value.
	 *
	 * The variable representing the address is scheduled to be destroyed. It is replaced by another variable representing
	 * the same address, but associated with another value.
	 *
	 * The variable representing the old value is scheduled to be destroyed.
	 *
	 * @param address A variable representing a memory address. 
	 * @param newValue A variable representing the new memory value.
	 * @return new address variable
	 */
	Variable memReplace(const Variable& /* address */, const Variable& /* newValue */);

	/**
	 * Create a new abstract memory location at specified address, with the specified value.
	 *
	 * @param address A variable representing the memory address.
	 * @param value A variable representing the memory value.
	 * @return new address variable
	 */
	Variable memCreate(const Variable& /* address */, const Variable& /* newValue */);

	/**
	 * Associate a new value to the address variable, merging with existing value.
	 *
	 * The variable representing the address is scheduled to be destroyed. It is replaced by another variable representing
	 * the same address, but associated with another value.
	 *
	 * @param address A variable representing a memory address. 
	 * @param newValue A variable representing the new memory value.
	 * @return new address variable
	 */
	Variable memMerge(const Variable& /* address */, const Variable& /* newValue */);

private: /* Private helper functions */
	int _doAllocAxis(const Ident&, bool allow_replace = false);
	void _doFreeAxis(int axis); 
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

	/**
	 * Computes the convex hull of two polyhedron of different space dimension, extending the smaller if needed.
	 */
	void _extendAndHull(PPL::C_Polyhedron &poly1, PPL::C_Polyhedron &poly2) const;
	/**
	 * Computes the join or widening of two abstract states
	 * @param this The first abstract state (will not be modified)
	 * @param r The second abstract state (will not be modified)
	 * @return Join or widening result
	 */
	void _doBinaryOp(int op, Variable *v, Variable *vs1, Variable *vs2);

};

inline bool operator==(const PPLDomain &a, const PPLDomain &b) { return a.equals(b); }
inline bool operator!=(const PPLDomain &a, const PPLDomain &b) { return !(a == b); }
inline Output& operator<<(Output& o, const PPLDomain &dom) { dom.print(o); return o; }





} } // namespace otawa::poly
#endif

