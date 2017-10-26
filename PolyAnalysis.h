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

// test
#include <ppl.hh>

namespace PPL = Parma_Polyhedra_Library;

namespace otawa { namespace poly {

using namespace otawa;
using namespace otawa::util;

extern Identifier<int> NUM_LOC_VARS;
extern Identifier<int> LOC_VAR_SIZE;
extern Identifier<int> MAX_AXIS;
extern p::feature POLY_ANALYSIS_FEATURE;

class PPLManager;
class HashIdent;
class PPLDomain;
class Ident;

inline Output& operator<<(Output& o, const PPL::Variable pv);

class Variable : public PPL::Variable {
	public:
		Variable(int axis, const PPLDomain &dom);
		Variable(const Ident &ident, const PPLDomain &dom);

		const PPLDomain &getDom() const { return _dom; }
		const Ident &getIdent() const { return _ident; }
	private:
		const PPLDomain &_dom;
		const Ident &_ident;
};

class Ident {

	public:
		friend class HashIdent;
		friend class PPLManager;
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
		Ident() : _type(ID_INVALID) { }
		Ident(int id, IdentType typ, const PPLDomain *dom = NULL) : _id(id), _type(typ) { }
		~Ident() { } 
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
		static t::hash hash(const Ident& key) { 
			return key._id;
		};
		static inline bool equals(const Ident& key1, const Ident& key2) {   
			return (key1._id == key2._id) && (key1._type == key2._type);  
		}
};

class HashCons {
	public:
		static t::hash hash(const PPL::Constraint& key) { 
			int kind;
			unsigned int prime = 16777619;
			t::hash result = 2166136261;
			for (PPL::dimension_type i = 0; i < key.space_dimension(); i++) {
				result ^= key.coefficient(PPL::Variable(i)).get_ui();
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
		static inline bool equals(const PPL::Constraint& key1, const PPL::Constraint& key2) { 
			return key1.is_equivalent_to(key2);
		}
};



class PPLDomain {
		// TODO remove friend as much as possible
		friend class PPLManager;
		friend class Variable;
		friend class PolyAnalysis; 

	private:
		PPL::C_Polyhedron poly;

		genstruct::HashTable<Ident , int, HashIdent> id2axis; ///< Mapping from identifier (register/pointers) to polyhedron variable
		genstruct::Vector<Ident> axis2id; ///< Reverse identifier mapping

		Ident compare_reg; ///< Register holding the last comparison result
		sem::cond_t compare_op; ///< Last comparison semantics

		/* Not really part of the abstract state */
		static BitVector trash; 

	public:
		inline void print(io::Output & out) const {
			PPL::Constraint_System cons = poly.minimized_constraints();
			cons.print();
			out << "";
		}

		/**
		 * Builds a bottom state
		 */
		PPLDomain() {
			num_axis = -1;
			poly = PPL::C_Polyhedron(0, PPL::EMPTY);
		}

		/**
		 * Builds a top state
		 * @param maxAxis Maximum number of variables this state can hold
		 */
		PPLDomain(int max_axis) {
			num_axis = 0;
			mem_ref = 0;
			trash = BitVector(max_axis);
			poly = PPL::C_Polyhedron(0, PPL::UNIVERSE);

		}

		PPLDomain (const PPLDomain &src)  {
			poly = src.poly;
			num_axis = src.num_axis;
			id2axis = src.id2axis;
			axis2id = src.axis2id;
			mem_ref = src.mem_ref;
			compare_reg = src.compare_reg;
			compare_op = src.compare_op;
			trash = src.trash;
		}

		~PPLDomain() { 
			// TODO
		}
		PPLDomain loopEntry(int loop, bool inner=false);
		PPLDomain loopIter(int loop, bool inner=false);
		PPLDomain loopExit(int loop, int bound);
		PPLDomain loopTotal(int loop, int bound);

		void binary_operation_helper(int op, PPL::Variable *v, PPL::Variable *vs1, PPL::Variable *vs2);
		void integer_wrap();
		void bring_out_your_dead();
		bool may_be_equal(PPL::Variable &v1, PPL::Variable &v2, int offset = 0);
		bool must_be_equal(PPL::Variable &v1, PPL::Variable &v2, int offset = 0);
		void scratch(Ident &id);
		void get_range(Ident &id, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display = false);
		void get_range(PPL::Variable &var, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d, bool display = false);
		bool get_constant(Ident &id, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d, bool display = false);
		bool get_constant(PPL::Variable &var, PPL::Coefficient &cst, PPL::Coefficient &cst_d, bool display = false);
		void display_loc_vars();

		void displayIdentMap() {
			cout << "IDMAP: " ;
			for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(id2axis); it; it++) {
				const Ident &ident = (*it).fst;
				cout << ident << ":" << PPL::Variable((*it).snd) << ", ";
			}
			cout << endl;
		}


		/* 
		 * Wrapper around partial mapping functions (for PPL poly map), i
		 * computes max-in-domain & empty automatically 
		 */
		template <class F> class MapHelper {
			public:
				MapHelper(F &pfunc, int max_in_domain) : _pfunc(pfunc), _max_in_domain(max_in_domain), _empty(true) {
					for (PPL::dimension_type i = 0; i <= _max_in_domain; i++) {
						PPL::dimension_type j;
						if (_pfunc.maps(i, j) && (_empty || (_max_in_codomain < j)))  {
							_max_in_codomain = j;
							_empty = false;
						}
					}
				}
				PPL::dimension_type max_in_codomain() const { return _max_in_codomain; }
				bool maps(PPL::dimension_type i, PPL::dimension_type &j) const { return _pfunc.maps(i, j); }
				bool has_empty_codomain() const { return _empty; }
			private:
				F &_pfunc;
				PPL::dimension_type _max_in_domain;
				PPL::dimension_type _max_in_codomain;
				bool _empty;
		};

		template <class F> void map_only_poly(F pfunc) {
			MapHelper<F> a(pfunc, poly.space_dimension() - 1);
			poly.map_space_dimensions(a);
		}

		template <class F> void map_only_idents(F pfunc) {
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
						cout << it.key() << "[" << PPL::Variable(old_axis) << "->" << PPL::Variable(new_axis) << "] ";
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
		template <class F> void map_poly_and_idents(F pfunc) {
			map_only_poly(pfunc);
			map_only_idents(pfunc);
		}

		// Return the first constraint in poly for which the coef of specified variable axis is non-zero
		const PPL::Constraint *getConstraintFor(int axis) {
			PPL::Constraint *cons = NULL;
			PPL::Constraint_System cons_sys = poly.minimized_constraints();
			for (PPL::Constraint_System::const_iterator it = cons_sys.begin(); it != cons_sys.end(); it++) {
				const PPL::Constraint &c = *it;
				if (!c.is_equality())
					continue;
				if (c.coefficient(PPL::Variable(axis)) != 0) {
					return new PPL::Constraint(c);
				}
			}
			return NULL;
		}

		/* Variable manipulations */
		int allocAxis(const Ident&, bool allow_replace = false);
		void freeAxis(int axis);
		bool exists(int);

		void destroy(const Ident&);
		void destroy(PPL::Variable&);
		Variable lookup(const Ident&, bool allow_create = false);
		void rename(const Ident &ident, const Ident &newident, bool allow_replace);
		Variable lookup(int);
		bool exists(const Ident&);
		bool exists(PPL::Variable&);
		Variable create(const Ident&, bool allow_replace = false);
		void create_ptr(Ident&, Ident&);

		int mem_ref; /* number of pointerse in this domain object */

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
		bool isBottom() {
			return num_axis == -1;
		}
		void sanity_checks() {
#ifdef POLY_DEBUG
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
#endif
		}




	friend bool operator==(const PPLDomain &a, const PPLDomain &b);

	/* Partial mapping function that removes variables in set (bitvector) */
	class RemoveMarked {
		public:
			~RemoveMarked() {
			}
			RemoveMarked(BitVector &bv, int size) : _bv(bv), _size(size) { }
			bool has_empty_codomain() const { return _bv.countBits() == _size; }
			PPL::dimension_type max_in_codomain() const { 
				return _size - _bv.countBits() - 1; // FIXME
			}
			bool maps(PPL::dimension_type i, PPL::dimension_type &j) const {
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
		private:
			BitVector &_bv;
			int _size;
	};

	class MapWithHash {
		public:
			MapWithHash(genstruct::HashTable<int,int> &map) : _map(map) { }
			bool has_empty_codomain() const { return false; }
			PPL::dimension_type max_in_codomain() const { 
				return 0;
			}
			bool maps(PPL::dimension_type i, PPL::dimension_type &j) const {
				if (_map.hasKey(i)) {
					j = _map[i];
					return true;
				} else return false;
			}
		private:
			genstruct::HashTable<int,int> &_map;
	};



	static void displayIdentMap(const PPLDomain &dom) {
		cout << "IDMAP: " ;
		for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(dom.id2axis); it; it++) {
			const Ident &ident = (*it).fst;
			cout << ident << ":" << PPL::Variable((*it).snd) << ", ";
		}
		cout << endl;
	}

	/**
	 * Indexes the pointer in dom, by their expression in terms of registers referenced in map_regs. 
	 * Stores the result in map_ptr.
	 */
	void indexPointersByExpr(genstruct::HashTable<PPL::Constraint, int, HashCons> &map_ptr, genstruct::HashTable<int, int> map_regs) {
		int axis = map_regs.count();
		for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(id2axis); it; it++) {
			if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
				map_regs.put((*it).snd, axis);
				PPLDomain dom(*this); /* make a working copy to do the projections */
				dom.map_only_poly(MapWithHash(map_regs));

				const PPL::Constraint *cons = dom.getConstraintFor(axis);
			    if (cons) {
					map_ptr[*cons] = (*it).fst.getId();
					delete cons;
				}
				map_regs.remove((*it).snd);
			}
		}
	}
	inline PPLDomain widening(const PPLDomain& r) {
		return join_or_widening(r, false); 
	}
	inline PPLDomain join(const PPLDomain& r) {
		return join_or_widening(r, false); 
	}
	inline PPLDomain narrowing(const PPLDomain& src) {
		PPLDomain s_out = src;
		return s_out;
	}

	inline bool hasFilter() {
		return (compare_reg.getType() != Ident::ID_INVALID);
	}
	inline void removeFilter() {
		compare_reg = Ident();
	}

	PPLDomain filter(bool taken) {
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
		return res;
	}
	PPLDomain update(sem::inst si, int instaddr);
	PPL::Variable *make_var(Ident &id);
	inline void poly_hull_helper(PPL::C_Polyhedron &poly1, PPL::C_Polyhedron &poly2) const {
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

	/**
	 * Computes the join or widening of two abstract states
	 * @param this The first abstract state (will not be modified)
	 * @param r The second abstract state (will not be modified)
	 * @return Join or widening result
	 */
	inline PPLDomain join_or_widening(const PPLDomain& r, bool widen=false) { 
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
		l1.indexPointersByExpr(mapl_ptr, mapl);
		r1.indexPointersByExpr(mapr_ptr, mapr);
		

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

		l1.map_poly_and_idents(PPLDomain::MapWithHash(mapl));
		r1.map_poly_and_idents(PPLDomain::MapWithHash(mapr)); 

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
		return l1;
	}
	private:
		int num_axis;

};

inline Output& operator<<(Output& o, const Ident &i) {
	i.print(o);
	return o;
}

inline Output& operator<<(Output& o, const PPL::Variable pv) { 
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


class PPLManager {
public:
	typedef PPLDomain t;
	private:

public:
	/**
	 * Create PPLManager for existing init state.
	 */
	PPLManager(t &init, const PropList &props) : _props(props), _init(init), _bot(), _top(MAX_AXIS(props)) {
	} 


	/**
	 * Create PPLManager using a fresh init state.
	 */
	PPLManager(const PropList &props) : _props(props), _init(MAX_AXIS(props)), _bot(), _top(MAX_AXIS(props)) {
		PPL::Constraint_System initcons;

		Variable var_sp = _init.create(Ident(13, Ident::ID_REG));
		Variable var_fp = _init.create(Ident(11, Ident::ID_REG));
		Variable var_lr = _init.create(Ident(14, Ident::ID_REG));
		Variable var_ssp = _init.create(Ident(Ident::ID_START_SP, Ident::ID_SPECIAL));
		Variable var_sfp = _init.create(Ident(Ident::ID_START_FP, Ident::ID_SPECIAL));
		Variable var_slr = _init.create(Ident(Ident::ID_START_LR, Ident::ID_SPECIAL));

		_init.poly.add_constraint(var_ssp == var_sp);
		_init.poly.add_constraint(var_sfp == var_fp);
		_init.poly.add_constraint(var_slr == var_lr);

	} 
	~PPLManager() { }

	inline t& init(void) { return _init; }
	inline t& bot(void) { return _bot; }
	inline t& top(void) { return _top; }

	inline t join(t& v1, const t& v2) { return v1.join(v2); }
	inline t widening(t& v1, const t& v2) { return v1.widening(v2); }

	inline bool equals(const t& v1, const t& v2) { 
		return (v1 == v2);
	}
	inline void set(t& d, const t& s) {  }
	inline void dump(io::Output& out, const t& v) {  }
	inline void dump(io::Output& out, value_t v) {  }

	// TODO migrer


	// fin migrer

private:
	const PropList& _props;
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
	typedef PPLManager::t state_t;
	void analyzeGraph(CFG &cfg, state_t &s, bool do_init); 
	const PropList* _props;
};

bool operator==(const PPLDomain &a, const PPLDomain &b) {
	if (!((a.poly == b.poly) &&
			(a.id2axis.count() == b.id2axis.count()))) {
		return false;
	}
	/*
	if (a.compare_reg != b.compare_reg)
		return false;
		*/

	// FIXME TODO not correct because of same memory location having different names
	for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(a.id2axis); it; it++) {
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
bool operator!=(const PPLDomain &a, const PPLDomain &b) {
	return !(a == b);
}


inline Output& operator<<(Output& o, const PPLDomain &dom) { 
	dom.print(o);
	return o;
}

Variable::Variable(const Ident &ident, const PPLDomain &dom) : PPL::Variable(dom.id2axis[ident]), _dom(dom), _ident(ident) { }
Variable::Variable(int axis, const PPLDomain &dom) 	: PPL::Variable(axis), _dom(dom), _ident(dom.axis2id[axis]) { 
	ASSERT(_ident.getType() != Ident::ID_INVALID);	
}

} } // namespace otawa::poly
#endif	// OTAWA_POLY_ANALYSIS_FEATURE_H

