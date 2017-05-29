#define POLY_DEBUG 1


/*
 *	GlobalAnalysis class interface
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2009, IRIT UPS.
 *
 *	OTAWA is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	OTAWA is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with OTAWA; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#ifndef OTAWA_POLY_ANALYSIS_FEATURE_H
#define OTAWA_POLY_ANALYSIS_FEATURE_H

#include <otawa/otawa.h>
#include <otawa/ipet.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/prog/sem.h>

#include <ppl.hh>

namespace PPL = Parma_Polyhedra_Library;

namespace otawa { namespace poly {

using namespace otawa ;
using namespace otawa::util ;

#define NUM_LOC_VARS 8
#define LOC_VAR_SIZE 4

#define MAX_AXIS 1024

class PPLManager;
extern PPLManager *beurk;
//class Ident { 
/* 
	public:
		friend class PPLManager;
		Ident(String name = "???") : _name(name) { }
		~Ident() {}
		inline void print(io::Output & out) const {  
   */	/* 
			char letter = (pv.id() % 26) + 'A';
			int number = pv.id() / 26;
			char name[32];
			if (number) {
				snprintf(name, sizeof(name), "%c%u", letter, number);
			} else {
				snprintf(name, sizeof(name), "%c", letter);
			}
			out << _name << " (" << name << ")";  */ /* 
			out << _name;
		}  
		inline bool operator==(const Ident& id) const {
			return (_name == id._name);
		}
		inline bool operator!=(const Ident& id) const {
			return (_name != id._name);
		} */ /* 
	private:
		String _name; */ 
//		PPL::Variable pv;
// };
inline Output& operator<<(Output& o, const PPL::Variable pv);

class HashIdent;
class HashIdent2;
class PPLDomain;
class Ident;
template <class T> class IdHolder;

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



// /
// typedef IdHolder<Ident> DomId;
// typedef Variable DomVar;
 /* 
class Variable {
	public:
		Variable(int axis, const PPLDomain &dom) : _var(axis), _dom(dom) { }
	const DomId &getId();
	operator PPL::Variable&() const { return *const_cast<PPL::Variable*>(&_var); } // FIXME
	operator PPL::Variable() const { return *(new PPL::Variable(0)); }
	private:
		const PPL::Variable _var;
		const PPLDomain &_dom;
};
*/
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
		friend class PPLManager;
		Ident() : _type(ID_INVALID) { }
		Ident(int id, IdentType typ, const PPLDomain *dom = NULL) : _id(id), _type(typ) { }
		Ident(int _canonicalAxis) : _id(_canonicalAxis / ID_MAX_TYPE), _type(IdentType(_canonicalAxis % ID_MAX_TYPE)) { }
		~Ident() { } 
		inline int getCanonicalAxis() const {
			int temp_id = _id;
			if (temp_id < 0) {
				temp_id = 16 + _id;
			}
			return temp_id*ID_MAX_TYPE + _type;
		}
		inline int getId() const { return _id; }
		inline IdentType getType() const { return _type; }
		// const PPL::Variable &getVar();

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

// FIXME
//
/*
class HashIdent2 {
	public:
		static t::hash hash(const DomId& key) { 
			return key._id;
		};
		static inline bool equals(const DomId& key1, const DomId& key2) {   
			ASSERT((key1._dom == key2._dom));
			return (key1._id == key2._id) && (key1._type == key2._type);  
			return true;
		}
};
*/

/*
template <class T> class IdHolder {
	public:
		IdHolder(T &value, PPLDomain &dom) : _value(value), _dom(dom) {  }
		const DomVar &getVar();

	private:
		T _value;
		PPLDomain &_dom;
};
 */ 


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
	public:
		friend class PPLManager;
		inline void print(io::Output & out) const {
			out << "[" << serial << "]";
			cons.print();
			out << "";
		}
		PPLDomain() { 
//			cons.print();
			magic = 0xDEADBEEF;
			serial = 666;
			num_axis = 0;
			mem_ref = 0;
			gen++;
		}
		PPLDomain (const PPLDomain &src) /*  : cong(src.cong), cons(src.cons) */  {
			ASSERT(src.magic == 0xDEADBEEF);
//			src.cons.print();
			cong = src.cong;
			cons = src.cons;
			magic = src.magic;
			num_axis = src.num_axis;
			id2axis = src.id2axis;
			axis2id = src.axis2id;
			serial = src.serial;
			mem_ref = src.mem_ref;
		}
		~PPLDomain() { 
			// TODO
		}

		template <class F> void map_identifiers(F pfunc) {
			genstruct::Vector<Ident> todel;
			int old_length = axis2id.length();
			axis2id.clear();
			axis2id.setLength(old_length);
			// cout << "trash bitvector:" << PPLDomain::trash << endl;
			cout << "REMAP: ";
			for (elm::genstruct::HashTable<Ident, int, HashIdent>::MutableIter it(id2axis); it; it++) {
				int &n = it.item();
				PPL::dimension_type old_axis = n;
				PPL::dimension_type new_axis = n;
				if (pfunc.maps(old_axis, new_axis)) {
				   	if (old_axis != new_axis) {
						cout << it.key() << "[" << PPL::Variable(old_axis) << "->" << PPL::Variable(new_axis) << "] ";
						n = new_axis;
					}
					axis2id[new_axis] = it.key();
				} else todel.add(it.key());
			}
			cout << endl;
			for (genstruct::Vector<Ident>::Iterator it(todel); it; it++) {
				id2axis.remove(*it);
			}
		}
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

		int mem_ref;

		inline void operator=(const PPLDomain& dom) {
			ASSERT(dom.magic == 0xDEADBEEF);
			cong = dom.cong;
			cons = dom.cons;
			magic = dom.magic;
			serial = dom.serial;
			num_axis = dom.num_axis;
			id2axis = dom.id2axis;
			axis2id = dom.axis2id;
			mem_ref = dom.mem_ref;
		
		}
		bool isBottom() {
			return num_axis == -1;
		}
		void sanity_checks() {
			int max_axis = -1;
			PPL::C_Polyhedron poly(cons);
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
			ASSERT(cons.space_dimension() <= num_axis); // unused axis can exist at the end
			ASSERT(trash.size() >= num_axis);
			ASSERT(trash.countOnes() <= num_axis);
		}


	// new mapping
	genstruct::HashTable<Ident , int, HashIdent> id2axis;
	genstruct::Vector<Ident> axis2id;
	static BitVector trash;

	friend bool operator==(const PPLDomain &a, const PPLDomain &b);
		int serial;
	private:
		int num_axis;
		uint32_t magic;
		static int gen;
		PPL::Congruence_System cong;
		PPL::Constraint_System cons;

};

class RemoveAllButOne {
	public:
		RemoveAllButOne(PPL::dimension_type selected) : _selected(selected) { }
		bool has_empty_codomain() const { return false; }
		PPL::dimension_type max_in_codomain() const { return 0; }
		bool maps(PPL::dimension_type i, PPL::dimension_type &j) const {
			if (i == _selected) {
				j = 0;
				return true;
			}
			return false;
		}
	private:
		PPL::dimension_type _selected;
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

/*
 * 1) identifier les axes communs avec getpointerexpr etc)
 * 2) pour les registres garder que ce qiu existe dans l'id2axis des deux
 */
class PPLManager {
public:
	typedef PPLDomain t;
private:
	/*
	 * Remap domain axis before a join()/widening() operation, so that corresponding values are on the same
	 * axis. Maps REGs using the canonical axis map, and PTR using the user-provided ptrmap hash.
	 * Also update the new idmap accordingly, if provided (i.e. non-NULL)
	 */
	class RemapBeforeJoin {
		public:
			RemapBeforeJoin(const PPLManager::t &dom, genstruct::HashTable<Ident, int> &_ptrmap, elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent> *newmap = NULL) : _dom(dom) , ptrmap(_ptrmap) {  
				/*
				int num_addr = 0;
				int num_val = 0;
				PPL::C_Polyhedron us_poly(_us.cons);
				PPL::C_Polyhedron them_poly(_them.cons);
				_idx = us_poly.space_dimension();
				*/
				int highest_axis = 0;
				/* Count REG/SPECIAL axis */
				/*
				for (elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent>::PairIterator it(_dom.ids); it; it++) {
					const Ident &kaka = (*it).fst;
					ASSERT(kaka.getType() != -1);
					const Ident &id = *new Ident((*it).fst); 
					printf("ADDR: 0x%x\n", &id);
					PPL::Variable *var = (*it).snd;
					if (revmap.length() < var->id() + 1) {
						for (int k = revmap.length(); k < var->id() + 1; k++) {
							revmap.push(NULL);
						}
					}
					revmap.set(var->id(), &id);
					if ((id.getType() == Ident::ID_REG) || (id.getType() == Ident::ID_SPECIAL)) {
						if (newmap) {
							(*newmap)[id] = new PPL::Variable(id.getCanonicalAxis());
						}
						if (highest_axis < id.getCanonicalAxis()) {
							highest_axis = id.getCanonicalAxis();
						}
						cout << "processing a REG" << endl;
					} else {
						//
					}
				}
				_base_ptr_axis = highest_axis + 1;
				cout << "BASE: " << _base_ptr_axis << endl;
				*/
				/* Count ptrmap axis */
				for (elm::genstruct::HashTable<Ident, int>::PairIterator it(_ptrmap); it; it++) {
					int remapped_axis = (*it).snd + _base_ptr_axis;
					if (highest_axis < remapped_axis)
						highest_axis = remapped_axis;
					if (newmap) {
						(*newmap)[(*it).fst] = new PPL::Variable(remapped_axis);
					} 
					cout << "processing a pointer: " << remapped_axis << endl;
				}
				_codomain_size = highest_axis + 1;
				cout << "REG/SPECIAL mapped from 0 to " << (_base_ptr_axis - 1) << ", PTR mapped from " << _base_ptr_axis << " to " << max_in_codomain() << endl;
			}
			bool has_empty_codomain() const { return false; }
			PPL::dimension_type max_in_codomain() const { return _codomain_size - 1; }
			bool maps(PPL::dimension_type i, PPL::dimension_type &j) const {
				if (revmap.count() <= i)
					return false;
				const Ident *id = revmap.get(i);
				if (!id)
					return false;
				if ((id->getType() == Ident::ID_REG) || (id->getType() == Ident::ID_SPECIAL)) {
					j = id->getCanonicalAxis();
					id->print(cout);
					cout << endl;
				} else if ((id->getType() == Ident::ID_MEM_ADDR) || (id->getType() == Ident::ID_MEM_VAL)) {
					if (!ptrmap.hasKey(*id))
						return false;
					j = ptrmap[*id] + _base_ptr_axis;
					cout << "ptrmap" << endl;
				}
				cout << j << " <= " << max_in_codomain() << endl;
				ASSERT(j <= max_in_codomain());
				return true;
			}
			int getBasePtrAxis() {
				return _base_ptr_axis;
			}
		private:
			genstruct::HashTable<Ident, int> &ptrmap;
			PPLManager::t _dom;
			int _codomain_size;
			int _idx;
			int _base_ptr_axis;
			genstruct::Vector<const Ident*> revmap;
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
	/* workaround for PPL crazyness */
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
	class GetPointerExpr {
		public:
			~GetPointerExpr() {
				free(tab);
			}
			/*
			 * Destination axis layout:
			 * Axis 0..(n-1) for common ancestor axis
			 * Axis n for target pointer axis
			 */
			GetPointerExpr(const t &dom, const Ident &ptr) {
				ASSERT(ptr.getType() == Ident::ID_MEM_ADDR);
				int map_dest = 0;
				PPL::C_Polyhedron poly(dom.cons);
				int count = poly.space_dimension();
				tab = (int*)malloc(sizeof(int)*count);
				for (int i = 0; i < count; i++)
					tab[i] = -1;
				_ptr_orig_axis = -1;
				/*
				for (elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent>::PairIterator it(dom.ids); it; it++) {
					const Ident &id = (*it).fst;
					if (id.getType() == Ident::ID_SPECIAL) {
						PPL::Variable *var = (*it).snd;
						if (map_dest < id.getCanonicalAxis())
							map_dest = id.getCanonicalAxis();
						tab[var->id()] = map_dest;
					} else if (id == ptr) {
						PPL::Variable *var = (*it).snd;
						_ptr_orig_axis = var->id();
					}
				}
				*/
				_codomain_size = map_dest + 2; /* +1 space for pointer axis */
				ASSERT(_ptr_orig_axis != -1);
			}
			bool has_empty_codomain() const { return false; }
			PPL::dimension_type max_in_codomain() const { 
				cout << "max in codomain is: " << (_codomain_size - 1) << endl;
				return _codomain_size - 1 + 1;  // FIXME TODO
			}
			bool maps(PPL::dimension_type i, PPL::dimension_type &j) const {
				if (i == _ptr_orig_axis) {
					j = max_in_codomain();
					cout << char('A' + i) << "[PTR] mapped to: axis " << char('A' + j) << endl;
					return true;
				}
				if (tab[i] == -1) {
					cout << char('A' + i) << " mapped to: the dark and empty nothingness of the void" << endl;
					return false;
				}
				j = tab[i];
				cout << char('A' + i) << "[ETC] mapped to: axis " << char('A' + j) << endl;
				return true;
			}

		private:
				int *tab;
				int _codomain_size;
				int _ptr_orig_axis;
	};

public:
	genstruct::Vector<PPL::Variable*> to_remove;

	PPLManager() { 
		_init.num_axis = 0;
		_bot.num_axis = -1;
		_top.num_axis = 0;
		Variable var_sp = _init.create(Ident(13, Ident::ID_REG));
		Variable var_fp = _init.create(Ident(11, Ident::ID_REG));
		Variable var_lr = _init.create(Ident(14, Ident::ID_REG));
		Variable var_ssp = _init.create(Ident(Ident::ID_START_SP, Ident::ID_SPECIAL));
		Variable var_sfp = _init.create(Ident(Ident::ID_START_FP, Ident::ID_SPECIAL));
		Variable var_slr = _init.create(Ident(Ident::ID_START_LR, Ident::ID_SPECIAL));
		_init.cons.insert(var_ssp == var_sp);
		_init.cons.insert(var_sfp == var_fp);
		_init.cons.insert(var_slr == var_lr);

		_top.cons.clear();
		PPL::Variable v(0);
		_bot.cons.insert(0*v == 1);
		cout << "Initial state: " ;
		_init.cons.print();
		cout << endl;
		cout << "TOP state: " ;
		_top.cons.print();
		cout << endl;
		cout << "BOTTOM state: " ;
		_bot.cons.print();
		cout << endl;
	} 
	~PPLManager() { }


	inline t& init(void) { 
		return _init; 
	}
	inline t& bot(void) { return _bot; }
	inline t& top(void) { return _top; }

	static void displayIdentMap(const t &dom) {
		cout << "IDMAP: " ;
		for (elm::genstruct::HashTable<Ident, int,HashIdent>::PairIterator it(dom.id2axis); it; it++) {
			const Ident &ident = (*it).fst;
			cout << ident << ":" << PPL::Variable((*it).snd) << ", ";
		}
		cout << endl;
	}
	void fill_hashmap(const t &dom, genstruct::HashTable<PPL::Constraint, elm::Pair<int,int>, HashCons> &map, bool snd) {
		cout << "compute ptr equivalence:" << endl;
		/*
		for (elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent>::PairIterator it(dom.ids); it; it++) {
			const Ident &id = (*it).fst;
			if (id.getType() == Ident::ID_MEM_ADDR) {
				PPL::C_Polyhedron p(dom.cons);
				GetPointerExpr gpexpr(dom, id);
				p.map_space_dimensions(gpexpr);
				PPL::Constraint_System cons = p.minimized_constraints();
				cout << "testing for: ";
				id.print(cout);
				cout << endl;
				bool ok = false;
				for (PPL::Constraint_System::const_iterator it = cons.begin(); it != cons.end(); it++) {
					const PPL::Constraint &c = *it;
					c.print();
					fflush(stdout);
					cout << endl;
					if (!c.is_equality())
						continue;
					if (c.coefficient(PPL::Variable(p.space_dimension() - 1)) != 0) {
						cout << "successfully got ptr expr!" << endl;
						if (!map.hasKey(c)) {
							elm::Pair<int,int> p;
							p.fst = -1;
							p.snd = -1;
							map[c] = p;
							cout << "adding some stuff to the map" << endl;
						} else cout << "already existed" << endl;
						elm::Pair<int,int> &p = map[c];
						int &val = snd ? p.snd : p.fst;
						//ASSERT(val == -1);
						val = id.getId();
						ok = true;
						break;
					} 
				}
				if (!ok)  {
					cout << "ptr expr is not statically known." << endl;
				}
			}
		} */
	}

	// Return the first constraint in poly for which the coef of specified axis is non-zero
	bool get_constraint_for(PPL::C_Polyhedron &poly, PPL::Constraint *cons, int axis) {
		PPL::Constraint_System cons_sys = poly.minimized_constraints();
		for (PPL::Constraint_System::const_iterator it = cons_sys.begin(); it != cons_sys.end(); it++) {
			const PPL::Constraint &c = *it;
			if (!c.is_equality())
				continue;
			if (c.coefficient(PPL::Variable(axis)) != 0) {
				*cons = c;
				return true;
			}
		}
		/*
		if (!ok)  {
			cout << "ptr expr is not statically known." << endl;
		}
		*/
		return false;
	}
	template <class F> void map_space_dimensions(F pfunc, PPL::C_Polyhedron &poly) {
		MapHelper<F> a(pfunc, poly.space_dimension() - 1);
		poly.map_space_dimensions(a);
	}

	/*
	 * map_ptr : reception de l'association Contrainte => numero de pointeur 
	 * map_regs : mapping ancien axe registre => nouvel axe registre commun
	 * axis : numero max d'axe registre commun + 1
	 * poly : copie du poly d'entree (pas remappe)
	 */
	void collect_pointer_expr(genstruct::HashTable<PPL::Constraint, int, HashCons> &map_ptr, genstruct::HashTable<int, int> map_regs, int axis, t &dom, PPL::C_Polyhedron &spoly) {
		for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(dom.id2axis); it; it++) {
			if ((*it).fst.getType() == Ident::ID_MEM_ADDR) {
				map_regs.put((*it).snd, axis);
				// mapper tout les registres, et 1 pointeur
				PPL::C_Polyhedron poly(spoly);
				map_space_dimensions(MapWithHash(map_regs), poly);
				PPL::Constraint cons;
			    if (get_constraint_for(poly, &cons, axis)) {
					/*
					cout << " constraint for " << (*it).fst << " is: ";
				    cons.print();
					cout << endl;
					*/
					map_ptr[cons] = (*it).fst.getId();
				}
				map_regs.remove((*it).snd);
			}
		}
	}
	inline bool hasFilter() {
		return (compare_reg.getType() != Ident::ID_INVALID);
	}
	inline void removeFilter() {
		compare_reg = Ident::ID_INVALID;
	}

	t filter(t &before, bool taken) {
		if (before.isBottom())
			return _bot;
		sem::cond_t this_op;
		ASSERT(hasFilter());
		t res = before;
		this_op = taken ? compare_op : sem::invert(compare_op);
		cout << "compare_reg is: " << compare_reg << endl;
		switch (this_op) {
			case sem::NE: {
				PPL::Constraint_System cons2 = res.cons;
				res.cons.insert(res.lookup(compare_reg) <= -1);
				cons2.insert(res.lookup(compare_reg) >= 1);
				PPL::C_Polyhedron poly(res.cons);
				PPL::C_Polyhedron poly2(cons2);
				poly.poly_hull_assign(poly2);
				res.cons = poly.minimized_constraints();
				break;
			}
			case sem::EQ:
				res.cons.insert(res.lookup(compare_reg) == 0);
				break;
			case sem::GE:
			case sem::UGE:
				res.cons.insert(res.lookup(compare_reg) >= 0);
				break;
			case sem::GT:
			case sem::UGT:
				res.cons.insert(res.lookup(compare_reg) >= 1);
				break;
			case sem::LE:
			case sem::ULE:
				res.cons.insert(res.lookup(compare_reg) <= 0);
				break;
			case sem::LT:
			case sem::ULT:
				res.cons.insert(res.lookup(compare_reg) <= -1);
				break;
			default:
				break;
		};
		PPL::C_Polyhedron poly(res.cons);
		cout << "empty? " << poly.is_empty() << endl;
		return res;
	}

	inline t widening(t& l, const t& r) {
		return join(l, r, true); // FIXME TODO 
	}
	inline t narrowing(const t& src) {
		t s_out = src;
		return s_out;
	}


	inline t join(t& l, const t& r, bool widen=false) { 
		int axis = 0;
		ASSERT(!l.trash.countOnes());
		ASSERT(!r.trash.countOnes());
		PPL::C_Polyhedron poly_r(r.cons);
		if (poly_r.is_empty()) {
			cerr << "trivial join (r empty)" << endl;
			return l;
			}
		PPL::C_Polyhedron poly_l(l.cons);
		if (poly_l.is_empty()) {
			cerr << "trivial join (l empty)" << endl;
			return r;
		}

		if (widen) {
			cerr << "================= WIDENING ==================" << endl;
		} else cerr << "=================== JOIN ====================" << endl;
		cerr << "=== prepare phase ===" << endl;
		cerr << l.serial << ": left hand term dimension: " << poly_l.space_dimension() << endl;
		l.cons.print();
		cerr << endl;
		display_loc_vars((PPLManager::t&)l);
		displayIdentMap(l);
		cout << endl;
		cerr << r.serial << ": right hand term dimension: " << poly_r.space_dimension() << endl;
		r.cons.print();
		cerr << endl;
		display_loc_vars((PPLManager::t&)r);
		displayIdentMap(r);

		genstruct::HashTable<PPL::Constraint, elm::Pair<int,int>, HashCons> map;
		genstruct::HashTable<PPL::Constraint, int, HashCons> mapl_ptr;
		genstruct::HashTable<PPL::Constraint, int, HashCons> mapr_ptr;
		/* maps pointers, from linear expr, to ident number in D and S */ 
		/*
		fill_hashmap(d, map, false);
		fill_hashmap(s, map, true);
		*/
		genstruct::HashTable<int,int> mapl;
		genstruct::HashTable<int,int> mapr;

		genstruct::HashTable<int,int> mapl2;
		genstruct::HashTable<int,int> mapr2;

		t r1 = r;
		t l1 = l;

		// mapl/mapr : on map tout les id SPECIAL vers une numerotation commune
		cerr << "Mapping common ancestors to common destination axis\n";
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
	
		cerr << "Identifying address expressions appearing on both sides\n";	
		collect_pointer_expr(mapl_ptr, mapl, axis, l1, poly_l);
		collect_pointer_expr(mapr_ptr, mapr, axis, r1, poly_r);
		

		// mapl/mapr: on ajoute tout les pointeurs communs, on map vers une numerotation commune
		cout << "COMMON PTRs: " ;
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
				cout << "ptr" << ptrl << "/ptr" << ptrr << ", ";
				axis += 2;
			}
		}
		cout << endl;

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

		map_space_dimensions(MapWithHash(mapl), poly_l);
		map_space_dimensions(MapWithHash(mapr), poly_r);
		l1.cons = poly_l.minimized_constraints();
		r1.cons = poly_r.minimized_constraints();

		l1.map_identifiers(MapWithHash(mapl));
		r1.map_identifiers(MapWithHash(mapr));

		// Fin preparation
		cerr << "=== prepare done ===" << endl;
		cerr << l.serial << ": left hand term dimension: " << poly_l.space_dimension() << endl;
		l1.cons.print();
		cerr << endl;
		display_loc_vars((PPLManager::t&)l1);
		displayIdentMap(l1);
		cout << endl;
		cerr << r.serial << ": right hand term dimension: " << poly_r.space_dimension() << endl;
		r1.cons.print();
		cerr << endl;
		display_loc_vars((PPLManager::t&)r1);
		displayIdentMap(r1);

		cerr << "=== convex-hull phase ===" << endl;

		poly_l.poly_hull_assign(poly_r);
		if (widen) {
			cerr << "before widening: " << endl;
		l1.cons = poly_l.minimized_constraints();
		r1.cons = poly_r.minimized_constraints();
		cerr << l.serial << ": left hand term dimension: " << poly_l.space_dimension() << endl;
		l1.cons.print();
		cerr << endl;
		display_loc_vars((PPLManager::t&)l1);
		displayIdentMap(l1);
		cout << endl;
		cerr << r.serial << ": right hand term dimension: " << poly_r.space_dimension() << endl;
		r1.cons.print();
		cerr << endl;
		display_loc_vars((PPLManager::t&)r1);
		displayIdentMap(r1);
		cerr << endl;
		cerr << "---" << endl;
		PPL::Constraint_System dummy;
			poly_l.bounded_BHRZ03_extrapolation_assign(poly_r, dummy);
			//poly_l.BHRZ03_widening_assign(poly_r);
			ASSERT(poly_l.contains(poly_r));
			static unsigned int p = 5;
			//poly_l.widening_assign(poly_r, &p);
		}
		l1.cons = poly_l.minimized_constraints();
		l1.num_axis = -1;
		for (elm::genstruct::HashTable<Ident, int, HashIdent>::PairIterator it(l1.id2axis); it; it++) {
			if ((*it).snd > l1.num_axis)
				l1.num_axis = (*it).snd;
		}
		l1.num_axis++;
		cerr << "=== all done. ===" << endl;
		cerr << l.serial << ": result dimension: " << poly_l.space_dimension() << endl;
		l1.cons.print();
		cerr << endl;
		display_loc_vars((PPLManager::t&)l1);
		displayIdentMap(l1);
		cerr << "=====================================" << endl;
		return l1;


#ifdef UNDEF

		PPL::C_Polyhedron poly2_l(poly_l);
		PPL::C_Polyhedron poly2_r(poly_r);

		MapWithHash mwhl(mapl);
		MapWithHash mwhr(mapr);

		poly2_l.map_space_dimensions(mwhl);
		poly2_r.map_space_dimensions(mwhr);

		for (genstruct::HashTable<int,int>::PairIterator it(mapl); it; it++) {
			// get ptr expr for axis 
			PPL::Constraint *c = NULL; // FIXME
			//const Pair<int,int*> &p = map[*c];
			const auto &p = *map[*c];
			if ((p.fst == -1) || (p.snd == -1))
				continue;	

			mapl2.put(p.fst, axisl);
			mapr2.put(p.snd, axisr);
			axisl++;
			axisr++;
		}
		// remap d vers d1 avec mapl2
		// remap s vers s1 avec mapr2
		//
		// ensuite on prends l'idmap d'un des deux au pif osef a
		//
		//
		return l;

		/////////////////////////////////


		genstruct::HashTable<Ident, PPL::Variable*, HashIdent> newids;
		
		genstruct::HashTable<Ident, int> ptrmap1,ptrmap2;
		int idx = 0; 
		/* ptrmap1 (resp.2) maps old ptr (ID_MEM_ADDR and ID_MEM_VAL) axis in D (resp.S) to common new ptr axis offset */
		cout << "debut construction ptrmap" << endl;
		for (genstruct::HashTable<PPL::Constraint, elm::Pair<int,int>, HashCons>::PairIterator it(map); it; it++) {
			const Pair<PPL::Constraint,Pair<int,int> > &p = *it;
			if ((p.snd.fst == -1) || (p.snd.snd == -1))
				continue; // discard unmatched pointers
			/* Put ID_MEM_ADDR identifier type on even new axis, and ID_MEM_VAL type on odd new axis */
			Ident id_addr1(p.snd.fst, Ident::ID_MEM_ADDR);
			Ident id_val1(p.snd.fst, Ident::ID_MEM_VAL);
			Ident id_addr2(p.snd.snd, Ident::ID_MEM_ADDR);
			Ident id_val2(p.snd.snd, Ident::ID_MEM_VAL); 
			/*
			PPL::Variable *var_addr1 = d.ids[id_addr1];
			PPL::Variable *var_val1 = d.ids[id_val1];
			PPL::Variable *var_addr2 = d.ids[id_addr2];
			PPL::Variable *var_val2 = d.ids[id_val2];
			*/
			cout << "New ptr (";
			id_addr1.print(cout);
			cout << ",";
			id_addr2.print(cout);
			cout << ") has expr: ";
			p.fst.print();
			cout << " and axis: " << idx << " + BASE" << endl;
			cout << endl;
			ptrmap1[id_addr1] = idx;
			ptrmap1[id_val1] = idx + 1;
			ptrmap2[id_addr2] = idx;
			ptrmap2[id_val2] = idx + 1; 

			newids[id_addr1] = new PPL::Variable(idx);
			newids[id_val1] = new PPL::Variable(idx + 1);
			/*
			d.ids[id_addr1] = idx;
			d.ids[id_val1] = idx + 1; 
			*/ 
			idx+=2;
		}
		cout << "end construction" << endl;
		RemapBeforeJoin rbj2(d, ptrmap1, &newids);
		cout << "remapjoin D " << endl;
		RemapBeforeJoin rbj1(s, ptrmap2);
		cout << "remapjoin S " << endl;
		poly.map_space_dimensions(rbj1);
		poly2.map_space_dimensions(rbj2);
		cout << "remap done" << endl;
		//d.cons = poly2.minimized_constraints();

		/* Now, REG/SPECIAL are mapped on their canonical axis in d and r,
		 * and MEM_ADDR/MEM_VAL are mapped on the same axis if their expression is equivalent.
		 * Unmatched pointers are discarded ( "cylindrified" ). */
		cout << "before hull, d=" << endl;
		d.cons.print();
		cout << endl;
		cout << "before hull, s=" << endl;
		poly.minimized_constraints().print();
		cout << endl;

		poly2.poly_hull_assign(poly);
		d.cons = poly2.minimized_constraints();
		/*
		for (elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent>::PairIterator it(d.ids); it; it++) {
			PPL::Variable *v = (*it).snd;
			delete v;
		}
		d.ids = newids; */ 
		cout << "joined:" << endl;
		d.cons.print();
		fflush(stdout);
		cout << endl;
		/*
		cout << "Ident: ";
		for (elm::genstruct::HashTable<Ident, PPL::Variable*,HashIdent>::PairIterator it(d.ids); it; it++) {
			elm::Pair<Ident, PPL::Variable *> p = *it;
			cout << p.fst << " = " << *p.snd << ", ";
		}
		cout << endl;
		*/
		displayIdentMap(d);

		cerr << "================ END JOIN ====================" << endl;
		beurk->display_loc_vars((PPLManager::t&)d);
		fflush(stdout);
		cerr << "OK" << endl;

	   
		/*
		Ident id(0,Ident::IdentType(0));
		GetPointerExpr gpexpr(d, id);
	    PPL::C_Polyhedron poly3(d.cons);
		poly3.map_space_dimensions(gpexpr); */ 
		d.num_axis = max(rbj1.max_in_codomain(), rbj2.max_in_codomain()) + 1 ;
		printf("D= %d\n", d.num_axis);
		return d;
#endif

	}
	inline bool equals(const t& v1, const t& v2) { 
		return (v1 == v2);
	}
	inline void set(t& d, const t& s) {  }
	inline void dump(io::Output& out, const t& v) {  }
	inline void dump(io::Output& out, value_t v) {  }
	t update(t s, sem::inst si);
	t loopEntry(PPLManager::t s_in, int loop);
	t loopIter(PPLManager::t s_in, int loop);
	t loopExit(PPLManager::t s_in, int loop, int bound);
	PPL::Variable *make_var(Ident &id, PPLManager::t &dom);
	void bring_out_your_dead(PPLManager::t &dom);
	void binary_operation_helper(PPLManager::t &s, int op, PPL::Variable *v, PPL::Variable *vs1, PPL::Variable *vs2);
	void integer_wrap(PPLManager::t &s);
	bool may_be_equal(PPLManager::t &s, PPL::Variable &v1, PPL::Variable &v2);
	bool must_be_equal(PPLManager::t &s, PPL::Variable &v1, PPL::Variable &v2);
	void scratch(Ident &id, PPLManager::t &dom);
	void get_range(Ident &id, PPLManager::t &dom, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d);
	void get_range(PPL::Variable &var, PPLManager::t &dom, PPL::Coefficient &binf_n, PPL::Coefficient &binf_d, PPL::Coefficient &bsup_n, PPL::Coefficient &bsup_d);
	bool get_constant(Ident &id, PPLManager::t &dom, PPL::Coefficient &cst_n, PPL::Coefficient &cst_d);
	bool get_constant(PPL::Variable &var, PPLManager::t &dom, PPL::Coefficient &cst, PPL::Coefficient &cst_d);
	bool is_constrained(Ident &id, PPLManager::t &dom);
	bool is_constrained(PPL::Variable &var, PPLManager::t &dom);
	void display_loc_vars(PPLManager::t &dom);
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

private:
	Ident compare_reg;
	sem::cond_t compare_op;
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

};

bool operator==(const PPLDomain &a, const PPLDomain &b) {
	/*
	 * cout << "COMPARE: " << endl;
	a.cons.print(); cout << endl;
	PPLManager::displayIdentMap(a);
	cout << "------------" << endl;
	b.cons.print(); cout << endl;
	PPLManager::displayIdentMap(b);
	cout << "------------" << endl;
	*/
	if (!((a.cong == b.cong) &&
		    (a.cons == b.cons) &&
			(a.id2axis.count() == b.id2axis.count()) && 
			(a.magic == b.magic))) {
		return false;
	}

	// FIXME TODO not correct
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

extern p::feature POLY_ANALYSIS_FEATURE;

inline Output& operator<<(Output& o, const PPLDomain &dom) { 
	dom.print(o);
	return o;
}

Variable::Variable(const Ident &ident, const PPLDomain &dom) : PPL::Variable(dom.id2axis[ident]), _dom(dom), _ident(ident) { }
Variable::Variable(int axis, const PPLDomain &dom) 	: PPL::Variable(axis), _dom(dom), _ident(dom.axis2id[axis]) { 
	ASSERT(_ident.getType() != Ident::ID_INVALID);	
}

 /* 
template <class T> const DomVar &IdHolder<T>::getVar() {
	DomVar *tmp = _dom.id2var[*this];
	return *tmp;
}
const DomId &Variable::getId() {
    int axis = _var.id();
    return *(_dom.var2id[axis]);
}
*/
} } // otawa::poly
#endif	// OTAWA_POLY_ANALYSIS_FEATURE_H

