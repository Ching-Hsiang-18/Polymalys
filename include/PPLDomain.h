#ifndef PPLDOMAIN_H
#define PPLDOMAIN_H 1

#include <elm/util/BitVector.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/ipet.h>
#include <otawa/otawa.h>
#include <otawa/prog/sem.h>
#include <otawa/dfa/ai.h>
#include <otawa/dfa/State.h>
#include <ppl.hh>

#include "PolyCommon.h"
#include "PolyWrap.h"

namespace otawa {
namespace poly {

using namespace otawa;
using namespace otawa::util;

namespace PPL = Parma_Polyhedra_Library;
using Variable = PPL::Variable;

enum bound_t : signed long {
	UNREACHABLE = -1,
	UNBOUNDED = -2,
};

/* TODO(clement): detect stack conf. from OTAWA */ 
enum stackconf_t : int32_t {
	STACK_TOP = -0x60000000,
	STACK_SIZE = 0x10000000,
};

/**
 * @class Ident
 *
 * Identifiers can represent registers, memory address/values, loop bounds, or special values (such as starting stack
 * frame address).
 * They are associated to polyhedron variables in abstract states.
 *
 * Identifiers are uniquely identified by a type, and an id number.
 *
 */
class Ident {
  public:
	enum IdentType {
		ID_REG = 0,
		ID_REG_INPUT,
		ID_MEM_ADDR,
		ID_MEM_VAL,
		ID_MEM_VAL_INPUT,
		ID_SPECIAL,
		ID_LOOP,
		ID_INVALID,
		ID_MAX_TYPE,
	};
	enum IdentSpecial {
		ID_START_SP = 0,
		ID_START_FP = 1,
		ID_START_LR = 2,
	};
	inline Ident() : _type(ID_INVALID) {}
	inline Ident(int id, IdentType typ) : _id(id), _type(typ) {}
	inline ~Ident() = default;
	inline int getId() const { return _id; }
	inline IdentType getType() const { return _type; }
	inline bool equals(const Ident &b) const { return (_id == b._id) && (_type == b._type); }

	void print(io::Output &out) const;

	inline bool operator==(const Ident &i) const { return (_id == i._id) && (_type == i._type); }
	inline bool operator!=(const Ident &i) const { return (_id != i._id) || (_type != i._type); }

  private:
	int _id{};
	IdentType _type;
};
inline Output &operator<<(Output &o, const Ident &i) {
	i.print(o);
	return o;
}

class HashIdent {
  public:
	static inline t::hash hash(const Ident &key) { return key.getId(); };
	static inline bool equals(const Ident &key1, const Ident &key2) { return key1.equals(key2); }
};

class VectCoefIdent {
	public:
		static inline t::hash hash(const Vector<PPL::Coefficient> &v) { 
			int prime = 31;
			t::hash result = 1;
			for (int i = 0; i < v.length(); i++) {
				result = prime * result + PPL::raw_value(v[i]).get_ui();
			}
			return 0; 
		}
		static inline bool equals(const Vector<PPL::Coefficient> &v1, const Vector<PPL::Coefficient> &v2) {
			return v1 == v2;
		}
};

class HashCons {
  public:
	static t::hash hash(const PPL::Constraint &key);
	static inline bool equals(const PPL::Constraint &key1, const PPL::Constraint &key2) {
		return key1.is_equivalent_to(key2);
	}
};


/**
 * @class PPLInput
 *
 * Represents an input (register, or memory location) to a summarized function
 *
 */
/*
class PPLInput {
	public:
		Ident _input;
		Ident _where;
	bool operator==(const PPLInput &a) const {
		return false;
	}
};
*/

/**
 * @class PPLSummary
 *
 * Represents a partial result (summary) computed for a function.
 */
class PPLSummary {
	public:
		genstruct::Vector<Ident> _damaged; ///< The list of output (or side-effects) variables (registers or pointers)
//		genstruct::Vector<Ident> _inputs; ///< The list of inputs for the function
		inline bool equals(const PPLSummary &b) const {
			return true;
		}
};

// TODO make this a template and put in elm/whatever
class Mapping {
	public:
		bool includes(const Mapping &src) const {
			for (MyHTable<Ident, guid_t, HashIdent>::PairIterator it(id2guid); it; it++) {
				if (!src.id2guid.hasKey((*it).fst))
				   return false;	
				if (src.id2guid[(*it).fst] != (*it).snd)
					return false;
			}
			for (MyHTable<guid_t, Ident>::PairIterator it(guid2id); it; it++) {
				if (!src.guid2id.hasKey((*it).fst))
				   return false;	

				const Ident &id1 = (*it).snd;
				const Ident &id2 = src.guid2id[(*it).fst];
				if (id1 != id2) 
					return false;
			}
			return true;
		}

		bool operator==(const Mapping &src) const {
			return this->includes(src) && src.includes(*this);
		}

		inline void add(const Ident &id, guid_t guid) {
			if (id2guid.hasKey(id))
				guid2id.remove(id2guid[id]);

			if (guid2id.hasKey(guid))
				id2guid.remove(guid2id[guid]);

			id2guid[id] = guid;
			guid2id[guid] = id;
		}

		inline guid_t find1(const Ident &id) const {
			return id2guid[id];
		}

		inline const Ident& find2(guid_t guid) const {
			return guid2id[guid];
		}

		inline void del1(const Ident &id) {
			guid2id.remove(id2guid[id]);
			id2guid.remove(id);

		}

		inline void del2(guid_t guid) {
			id2guid.remove(guid2id[guid]);
			guid2id.remove(guid);
		}

		inline bool has1(const Ident &id) const {
			return id2guid.hasKey(id);
		}

		inline bool has2(guid_t guid) const {
			return guid2id.hasKey(guid);
		}

		inline MyHTable<Ident, guid_t, HashIdent>::PairIterator getPairIter() const {
			return MyHTable<Ident, guid_t, HashIdent>::PairIterator(id2guid);
		}

		inline MyHTable<Ident, guid_t, HashIdent>::MutableIter getMutableIter() {
			return MyHTable<Ident, guid_t, HashIdent>::MutableIter(id2guid);
		}
		
		inline int count() const {
			ASSERT(id2guid.count() == guid2id.count());
			return id2guid.count();
		}

	private:
		MyHTable<Ident, guid_t, HashIdent> id2guid;
		MyHTable<guid_t, Ident> guid2id;

};

class PPLDomain {

private:
	/* Abstract state */
	WPoly poly;

	Mapping idmap; ///< Mapping from identifiers (registers/pointers) to polyhedron variables
	Vector<guid_t> victims; ///< Set of variables scheduled to be destroyed

	Ident compare_reg;      ///< Register holding the last comparison result
	sem::cond_t compare_op; ///< Last comparison semantics

	int mem_ref{}; ///< Highest pointer ID + 1
	int num_axis;  ///<Highest poly variable ID + 1

	Vector<bound_t> bounds;
	Vector<PPLDomain> *linbounds = NULL;

	WorkSpace *_ws;

	PPLSummary *_summary; ///< Summarize currently analyzed function

	/* Nested classes */

	/**
	 * @class MapHelper
	 *
	 * Wrapper for the PPL poly mapping partial functions, that computes automatically
	 * max_in_codomain/has_empty_codomain.
	 * See PPL documentation for details.
	 */
	template <class F> class MapHelper {
	  public:
		MapHelper(F &pfunc, int max_in_domain);
		inline bool maps(guid_t i, guid_t &j) const { return _pfunc.maps(i, j); }

	  private:
		F &_pfunc;
	};

	/**
	 * @class RemoveMarked
	 *
	 * Partial mapping function that removes variables in set (bitvector)
	 * Needs to be wrapped with MapHelper before usage with PPL.
	 */
	class RemoveMarked {
	  public:
		inline ~RemoveMarked() = default;
		inline RemoveMarked(BitVector &bv, int size) : _bv(bv), _size(size) {}
		bool maps(guid_t i, guid_t &j) const;

	  private:
		BitVector &_bv;
		int _size;
	};


	/**
	 * @class MapWithHash
	 *
	 * Partial mapping function accoring to passed hashtable.
	 * Needs to be wrapped with MapHelper before usage with PPL.
	 */
	class MapGuid {
	  public:
		inline explicit MapGuid(MyHTable<guid_t, guid_t> &map) : _map(map) {}
		inline bool maps(guid_t i, guid_t &j) const {
			if (_map.hasKey(i)) {
				j = _map[i];
				return true;
			}
			{ return false; }
		}

	  private:
		MyHTable<guid_t, guid_t> &_map;
	};
	class MapWithHash {
	  public:
		inline explicit MapWithHash(MyHTable<int, int> &map) : _map(map) {}
		inline bool maps(guid_t i, guid_t &j) const {
			if (_map.hasKey(i)) {
				j = _map[i];
				return true;
			}
			{ return false; }
		}

	  private:
		MyHTable<int, int> &_map;
	};

	class MapShift {
	  public:
		inline explicit MapShift(guid_t domsize, guid_t shift) : _domsize(domsize), _shift(shift) {}
		inline bool maps(guid_t i, guid_t &j) const {
			if (i < _domsize) {
				j = i + _shift;
				return true;
			} else return false;
		}

	  private:
		guid_t _domsize;
		guid_t _shift;
	};

  public:
	/* Basic operations (constructor, destructor, copy, comparison) */

	/**
	 * Builds a bottom state
	 */
	inline PPLDomain() : poly(true) {
		num_axis = -1;
		compare_reg = Ident();
		compare_op = sem::EQ;
		_ws = nullptr;
		_summary = nullptr;
	}

	/**
	 * Builds a top state
	 * @param maxAxis Maximum number of variables this state can hold
	 */
	inline explicit PPLDomain(int maxAxis, WorkSpace *ws, PPLSummary *summary = nullptr) : poly(false) {
		num_axis = 0;
		mem_ref = 0;
		_ws = ws;
		compare_reg = Ident();
		compare_op = sem::EQ;
		_summary = summary;
	}

	inline PPLDomain(const PPLDomain &src) : poly(src.poly){
		num_axis = src.num_axis;
		idmap = src.idmap;
		ASSERT(idmap == src.idmap);
		mem_ref = src.mem_ref;
		compare_reg = src.compare_reg;
		compare_op = src.compare_op;
		victims = src.victims;
		bounds = src.bounds;
		if (src.linbounds != nullptr) {
			linbounds = new Vector<PPLDomain>(*src.linbounds);
		}
		_ws = src._ws;
		if (src._summary != nullptr) {
			_summary = new PPLSummary(*src._summary);
		} else {
			_summary = nullptr;
		}
	}

	~PPLDomain();

	inline PPLDomain &operator=(const PPLDomain &dom) {
		poly = dom.poly;
		num_axis = dom.num_axis;
		idmap = dom.idmap;
		ASSERT(idmap == dom.idmap);
		compare_reg = dom.compare_reg;
		compare_op = dom.compare_op;
		mem_ref = dom.mem_ref;
		victims = dom.victims;
		bounds = dom.bounds;
		_ws = dom._ws;
		if (linbounds != nullptr) {
			if (dom.linbounds != nullptr) {
				*linbounds = *dom.linbounds;
			} else {
				delete linbounds;
				linbounds = nullptr;
			}
		} else {
			if (dom.linbounds != nullptr)
				linbounds = new Vector<PPLDomain>(*dom.linbounds);
		}
		if (_summary != nullptr) {
			if (dom._summary != nullptr) {
				*_summary = *dom._summary;
			} else {
				delete _summary;
				_summary = nullptr;
			}
		} else {
			if (dom._summary != nullptr)
				_summary = new PPLSummary(*dom._summary);
		}
		return *this;
	}

	bool equals(const PPLDomain & /*b*/) const;

	/* Operations that reads the state and returns information about it */

	/**
	 * Gets summary information 
	 */
	PPLSummary *getSummary() {
		return _summary;
	}

	/**
	 * Prints this state (constraints, mappings, and local variables)
	 */
	void print(io::Output &out) const;

	/**
	 * Display local variables in this state
	 */
	void displayLocVars(io::Output &out) const;

	/**
	 * Display global variables in this state
	 */
	void displayGlobVars(io::Output &out) const;

	/**
	 * Display mappings in this state
	 */
	void displayIdentMap(io::Output &out) const;

	/**
	 * Attempts to get the range of possible values for a variable associated with an identifier.
	 *
	 * @param id The target identifier
	 * @param binf_n Reference for storing the numerator for the lower bound
	 * @param binf_d Reference for storing the denominator for the lower bound
	 * @param bsup_n Reference for storing the numerator for the upper bound
	 * @param bsup_d eference for storing the denominator for the upper bound
	 */
	void getRange(const Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n,
	              PPL::Coefficient &bsup_d) const;
	/**
	 * Attempts to get the range of possible values for a variable.
	 *
	 * @param var The target variable
	 * @param binf_n Reference for storing the numerator for the lower bound
	 * @param binf_d Reference for storing the denominator for the lower bound
	 * @param bsup_n Reference for storing the numerator for the upper bound
	 * @param bsup_d eference for storing the denominator for the upper bound
	 */
	void getRange(const WVar &var, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n,
	              PPL::Coefficient &bsup_d) const;

	/**
	 * Display a fraction on output stream.
	 *
	 * @param out The output stream
	 * @param num Numerator
	 * @param den Denominator
	 * @param hex display in hex if true
	 */
	void displayFrac(io::Output &out, const PPL::Coefficient &num, const PPL::Coefficient &den, bool hex = false) const;

	/**
	 * Gets the space dimension of the polyhedron in the current state
	 *
	 * @return The space dimension count.
	 */
	inline int getVarIDCount() const { return poly.variable_count(); }

	/**
	 * Gets the number of constraints in the current state
	 *
	 * @return The constraint count.
	 */
	 int getConsCount() const;

	/**
	 * Tests if the current state is equivalent to Bottom.
	 *
	 * @return true if bottom, false otherwise.
	 */
	inline bool isBottom() const { return (num_axis == -1) || poly.is_empty(); }

	/**
	 * Sets the current state to bottom.
	 */
	inline void setBottom() { *this = PPLDomain(); }

	/**
	 * Tests if the current state has a pending filtering operation, resulting from a condition branch
	 *
	 * @return true if pending filtering operation, false otherwise.
	 */
	inline bool hasFilter() const { return (compare_reg.getType() != Ident::ID_INVALID); }

	/**
	 * Tests if two variables may be equal (i.e. exists at least one concrete state in this abstract state where they
	 * are equal)
	 *
	 * @param v1 First variable
	 * @param v2 Second variable
	 * @return true if may be equal, false otherwise
	 */
	bool mayAlias(const WVar &v1, const WVar &v2) const;

	/**
	 * Tests if two variables must be equal (i.e. they are equal for all concrete states in this abstract state)
	 *
	 * @param v1 First variable
	 * @param v2 Second variable
	 * @return true if must be equal, false otherwise
	 */
	bool mustAlias(const WVar &v1, const WVar &v2, int offset = 0) const;

	/**
	 * Attempts to get the value of a a variable mapped to an identifier, if this value can be statically determined,
	 * and is unique.
	 *
	 * @param id The target identifier
	 * @param cst_n Reference for storing the numerator
	 * @param cst_d Reference for storing the denominator
	 * @return true if successful, false if the value cannot be determined.
	 */
	bool getConstant(const Ident &id, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d) const;

	/**
	 * Attempts to get the value of a variable, if this value can be statically determined, and is unique.
	 *
	 * @param var The target variable
	 * @param cst_n Reference for storing the numerator
	 * @param cst_d Reference for storing the denominator
	 * @return true if successful, false if the value cannot be determined.
	 */
	bool getConstant(const WVar &var, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d) const;

	/* TODO documenter */
	PPLDomain getLinearExpr(const Ident &id);

	/**
	 * Attempts to get the current loop bound estimation, in the current state.
	 *
	 * @param loopId The ID of the loop header.
	 * @return loop bound, or bound_t::UNREACHABLE if this state is bottom, or bound_t::UNBOUNDED if loop cannot be
	 * bounded.
	 */
	bound_t getLoopBound(int loopId) const;

	inline bound_t getBound(int loopId) const { 
		if (bounds.length() <= loopId)
			return bound_t(0);
		return bounds[loopId]; 
	}
	inline PPLDomain getLinBound(int loopId) const { 
		if ((linbounds == nullptr) || (linbounds->length() <= loopId))
			return PPLDomain();
		return (*linbounds)[loopId];
	}

	inline void setLinBound(int loopId, PPLDomain bound) {
		if (bound.isBottom()) 
			return;
		if (linbounds == nullptr)
			linbounds = new Vector<PPLDomain>();
		if (linbounds->length() <= loopId) {
			linbounds->setLength(loopId + 1);
		}
		delete bound.linbounds;
		bound.linbounds = nullptr;
		(*linbounds)[loopId] = (*linbounds)[loopId].onMerge(bound, false);
	}

	inline void setBound(int loopId, bound_t b) { 
		if (b == bound_t(0))
			return;
		if (bounds.length() <= loopId) {
			bounds.setLength(loopId + 1);
			bounds[loopId] = b;
		} else {
			if ((bounds[loopId] != bound_t::UNBOUNDED) &&
			    ((bounds[loopId] < b) || (b == bound_t::UNBOUNDED))) {
				bounds[loopId] = b;
			}
		}
	}

	/* High-level update operations. They return the modified state. */

	/**
	 * Compose current state with function summary
	 *
	 * @param summary Callee function summary
	 * 
	 * @return Composed state
	 */
	PPLDomain onCompose(const PPLDomain &summary) const;
	PPLDomain onComposeBounds(const PPLDomain &summary) const;

	/**
	 * Process an OTAWA semantic instruction
	 *
	 * @param si The semantic instruction
	 * @param instaddr Instruction address
	 *
	 * @return The updated state.
	 */
	PPLDomain onSemInst(const sem::inst &si, int instaddr) const;

	/**
	 * Process a conditional branch instruction, and apply the filtering.
	 *
	 * @param taken true if we are processing the TAKEN edge, false otherwise
	 *
	 * @return The updated state.
	 */

	PPLDomain onBranch(bool taken) const;

	/**
	 * Process join and widening
	 *
	 * @param r Other state to merge with
	 * @param widen true if we perform a widening, false for normal join
	 * @return The updated state.
	 */
	PPLDomain onMerge(const PPLDomain &r, bool widen = false) const;
	/**
	 * Process loop entry
	 *
	 * @param loop The ID of the loop header.
	 * @return The updated state.
	 */
	PPLDomain onLoopEntry(int loop) const;
	/**
	 * Process loop back-edge
	 *
	 * @param loop The ID of the loop header.
	 * @return The updated state.
	 */
	PPLDomain onLoopIter(int loop) const;
	/**
	 * Process loop exit-edge
	 *
	 * @param loop The ID of the loop header.
	 * @param bound The current estimated bound of the loop (or bound_t::UNREACHABLE/bound_t::UNBOUNDED if loop
	 * unreachable/unbounded)
	 * @return The updated state.
	 */
	PPLDomain onLoopExit(int loop, int bound) const;
	PPLDomain onLoopExitLinear(int loop, const PPLDomain &bound) const;

	/* Operations that modify the state in-place */

	/**
	 * Enable summarization of to-be-analyzed function (must be called before the start of the analysis of this function)
	 */
	void enableSummary();


	/**
	 * Map only the polyhedron (without the identifier mappings). This is probably not what you want.
	 *
	 * @param pfunc The partial mapping function (see PPL docs)
	 * @param noproj Prevent PPL from optimizing the projection by classifying it as a permutation
	 */
	template <class F> void doMapPoly(F pfunc, bool noproj = false);

	/**
	 * Map only the identifiers mappings (without the polyhedron). This is probably not what you want.
	 *
	 * @param pfunc The partial mapping function (see PPL docs)
	 */
	template <class F> void doMapIdents(F pfunc);

	/**
	 * Map the state according to partial function. This is equivalent to calling doMapPoly + doMapIdents
	 *
	 * @param pfunc The partial mapping function (see PPL docs)
	 * @param noproj Prevent PPL from optimizing the projection by classifying it as a permutation
	 */
	template <class F> void doMap(F pfunc, bool noproj = false);

	/**
	 * Handle integer wrap-around (currently not implemented)
	 */
	void doIntegerWrap();

	/**
	 * Unconstrain (scratch) a variable associated with an identifier.
	 *
	 * @param id Target identifier.
	 */
	void doScratch(Ident &id);

	/**
	 * Add a new constraints to the polyhedron
	 *
	 * @param c The new constraint to add.
	 */
	void doNewConstraint(const WCons &c) { poly.add_constraint(c); }

	/* Variable/Idents handling operations */

	/**
	 * create a new variable to represent an identifier.
	 *
	 * @param id The identifier to associate the variable with.
	 * @param allow_replace true if we allow replacing an existing variable that was mapped to id, false otherwise
	 * @return The new variable.
	 */
	WVar varNew(const Ident &id, bool allow_replace = false, bool create_damaged = false);

	/**
	 * Schedule a variable (associated with an identifier) to be destroyed.
	 * The actual variable removal will be done at the next _doFinalizeUpdate()
	 *
	 * @param id Target identifier
	 */
	inline void varKill(const Ident &id) { 
		varKill(WVar(idmap.find1(id)));
	}

	/**
	 * Schedule a variable to be destroyed.
	 * The actual variable removal will be done at the next _doFinalizeUpdate()
	 *
	 * @param v Target variable
	 */
	inline void varKill(const WVar &v) { 
		idmap.del2(v.guid());
		victims.add(v.guid()); 
	}

	/**
	 * Gets the variable associated with an identifier, creating a new variable if it doesn't exists (i.e. lookup).
	 *
	 * @param id The target identifier
	 * @param create_input if true, and the identifier is unknown, and we are summarizing, create an input
	 * @return The variable.
	 */
	WVar getVarOrNew(const Ident &id, bool create_input = false);

	/**
	 * Gets the variable associated with an identifier, aborting if the variables doesn't exists (i.e. lookup).
	 *
	 * @param id The target identifier
	 * @return The variable.
	 */
	WVar getVar(const Ident &id) const;

	/**
	 * Tests if a variable is associated with an identifier.
	 *
	 * @param v Variable to test
	 * @return true if the variable is mapped to an identifier, false otherwise
	 */
	inline bool isVarMapped(const WVar &v) const {
		return idmap.has2(v.guid());
	}

	/**
	 * Returns the identifier associated with a variable
	 *
	 * @param v The variable
	 * @return The identifier
	 */
	inline const Ident &getIdent(const WVar &v) const { return idmap.find2(v.guid()); }

	/**
	 * Tests if an identifier exists
	 *
	 * @param id The identifier
	 * @return true if the identifier exists, false otherwise
	 */
	bool hasIdent(const Ident & id) const;

	/**
	 * Schedule all OTAWA semantic-instruction temporary registers to be destroyed.
	 */
	void doKillTemporaries();

	/**
	 * Schedule all hardware registers in bitset to be destroyed (Ri is destroyed if bit i is NOT set in bv)
	 *
	 * @param bv Bitset indicating which registers to destroy
	 */
	void doKillRegisters(BitVector bv);

	/**
	 * Schedule the local variables to be destroyed, when leaving a function.
	 */
	void doLeaveFunction();

	/**
	 * Performs garbage-collection of variables scheduled to be destroyed.
	 */
	void doFinalizeUpdate();

	/* Pointers/Memory-related operations */

	/**
	 * Create new memory address/value variable and identifiers.
	 */
	void varCreatePtr(Ident & /*addr*/, Ident & /*val*/);

	/**
	 * Associate a new value to the address variable, replacing existing value.
	 *
	 * The variable representing the address is scheduled to be destroyed. It is replaced by another variable
	 * representing
	 * the same address, but associated with another value.
	 *
	 * The variable representing the old value is scheduled to be destroyed.
	 *
	 * @param address A variable representing a memory address.
	 * @param newValue A variable representing the new memory value.
	 * @return new address variable
	 */
	WVar memReplace(const WVar & address, const WVar & newValue);

	/**
	 * Create a new abstract memory location at specified address, with the specified value.
	 *
	 * @param address A variable representing the memory address.
	 * @param newValue A variable representing the memory value.
	 * @param dmg If summarizing, mark address as damaged
	 * @return new address variable
	 */
	WVar memCreate(const WLinExpr & address , const WLinExpr &newValue, bool dmg = true);

	/**
	 * Associate a new value to the address variable, merging with existing value.
	 *
	 * The variable representing the address is scheduled to be destroyed. It is replaced by another variable
	 * representing
	 * the same address, but associated with another value.
	 *
	 * @param address A variable representing a memory address.
	 * @param newValue A variable representing the new memory value.
	 * @return new address variable
	 */
	WVar memMerge(const WVar &address, const WVar &newValue);

	/**
	 * Attempts to use process initial state to discover value associated with a constant address-variable
	 *
	 * @param id The identifier associated with the constant address-varialbe
	 * @param address The constant address (detected from polyhedron)
	 * @param value The value at the address (recovered from process initial state)
	 * @return true if success, false otherwise
	 */
	bool memGetInitial(const Ident &id, uint32_t &address, uint32_t &value, bool force = false);

  private:
	/* Private helper functions. Subject to changes, and should not be used directly. */


	std::set<guid_t> _collectPolyVars(const WPoly &poly) const;

	void _identifyPolyVars(const PPLDomain &d, const std::set<guid_t> &vars, const std::set<guid_t> &indep, MyHTable<guid_t, Vector<PPL::Coefficient> > &vmap) const;

//	int _doAllocAxis(const Ident & /*ident*/, bool allow_replace = false);
//	void _doFreeAxis(int axis);
#ifdef POLY_DEBUG
	void _sanityChecks(bool allow_holes = false);
#else
	inline void _sanityChecks(bool allow_holes = false) {}
#endif

	const WCons *_getConstraintFor(int axis) const;
	/**
	 * Indexes the pointer in dom, by their expression in terms of registers referenced in map_regs.
	 * Stores the result in map_ptr.
	 */
	void _indexPointersByExpr(MyHTable<WCons, int, HashCons> &map_ptr,
	                          MyHTable<int, int> &commonRegs) const;

	void _identifyAncestorVars(PPLDomain &l, MyHTable<int, int> &commonVarsL, PPLDomain &r,
	                           MyHTable<int, int> &commonVarsR) const;

	/**
	 * Computes the join or widening of two abstract states
	 * @param this The first abstract state (will not be modified)
	 * @param r The second abstract state (will not be modified)
	 * @return Join or widening result
	 */
	void _doBinaryOp(int op, WVar *v, WVar *vs1, WVar *vs2);

	/**
	 * Unify the two states so that variables refering to the same object (register, memory location, ...) have the same
	 * number.
	 *
	 * @param l First state to unify
	 * @param r Second state to unify
	 */
	void _doUnify(PPLDomain &l1, PPLDomain &r1, bool noPtre=false) const;

	/**
	 * To be documented
	 */
	void _doMatchGlobals(PPLDomain &l1, PPLDomain &r1, unsigned int &axis, 
			MyHTable <int,int> &mappingL, MyHTable<int,int> &mappingR) const;
	/**
	 * To be documented
	 */
	void _doMatchSummaries(PPLDomain &l1, PPLDomain &r1, unsigned int &axis, 
			MyHTable <int,int> &mappingL, MyHTable<int,int> &mappingR,
			MyHTable<WCons, int, HashCons>&,
			MyHTable<WCons, int, HashCons>&) const;

};

inline Output &operator<<(Output &o, const PPLDomain &dom) {
	dom.print(o);
	return o;
}
inline bool operator==(const PPLDomain &a, const PPLDomain &b) { return a.equals(b); }
inline bool operator!=(const PPLDomain &a, const PPLDomain &b) { return !(a == b); }

} // namespace poly
} // namespace otawa
#endif
