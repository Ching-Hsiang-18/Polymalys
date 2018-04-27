#ifndef POLYWRAP_H
#define POLYWRAP_H 1

#include <ppl.hh>
#include <elm/io/Output.h>

namespace otawa { 
namespace poly {

namespace PPL = Parma_Polyhedra_Library;

typedef unsigned long long guid_t;
typedef std::GMP_Integer coef_t;
typedef PPL::dimension_type dim_t;
typedef elm::io::Output output_t;

class WVar;
class WCons;
class WLinExpr;
class WPoly;

output_t& operator<< (output_t& stream, const coef_t&);
output_t& operator<< (output_t& stream, const WLinExpr&);
output_t& operator<< (output_t& stream, const WCons&);
output_t& operator<< (output_t& stream, const WPoly&);
WCons operator==(const WLinExpr &a, const WLinExpr &b);
bool operator==(const WPoly &a, const WPoly &b);
bool operator!=(const WPoly &a, const WPoly &b);
WCons operator>=(const WLinExpr &a, const WLinExpr &b);
WCons operator<=(const WLinExpr &a, const WLinExpr &b);
WCons operator>(const WLinExpr &a, const WLinExpr &b);
WCons operator<(const WLinExpr &a, const WLinExpr &b);
WLinExpr operator+(const WLinExpr &a, const WLinExpr &b);
WLinExpr operator*(const WLinExpr &a, const coef_t b);
WLinExpr operator*(const coef_t b, const WLinExpr &a);
WLinExpr operator-(const WLinExpr &a, const WLinExpr &b);
WLinExpr operator+=(WLinExpr &a, const WLinExpr &b);
WLinExpr operator-=(WLinExpr &a, const WLinExpr &b);

class WVar {
	public:
		inline WVar() {
			_guid = _guid_generator;
			_guid_generator++;
		}
		inline WVar(guid_t guid) { _guid = guid; }
		inline guid_t guid() const { return _guid; }
		inline guid_t id() const { return _guid; }

	private:
		static guid_t _guid_generator;
		guid_t _guid;
};

// WVar
//
class WLinExpr {
	public:
		friend WLinExpr operator+(const WLinExpr &a, const WLinExpr &b);
		friend WLinExpr operator-(const WLinExpr &a, const WLinExpr &b);
		friend WLinExpr operator*(const WLinExpr &a, const coef_t b);
		friend WLinExpr operator*(const WLinExpr &a, const coef_t b);
		friend WCons operator==(const WLinExpr &a, const WLinExpr &b);

		WLinExpr(const coef_t c = 0) { cst = c; }
		WLinExpr(const int c) { cst = coef_t(c); }
		WLinExpr(const WVar &v) { coefs[v.guid()] = 1; cst = 0; }

		const PPL::Linear_Expression toPPL(WPoly &poly) const;
		const PPL::Linear_Expression toPPL(const WPoly &poly) const;
		
		inline coef_t inhomogeneous_term() { return cst; }
		inline coef_t coefficient(const WVar &v) { return coefs[v.guid()]; }
		inline guid_t space_dimension() {
			std::cout << "warning: WLinExpr::space_dimension() is deprecated" << std::endl;
			guid_t res = 0;
			for (std::map<guid_t,coef_t>::const_iterator it=coefs.begin(); it!=coefs.end(); ++it) {
				if (res < it->first)
					res = it->first;
			}
			return res + 1;
		}

		void print(output_t &out) const;

	private:
		std::map<guid_t,coef_t> coefs;
		coef_t cst;
};

// WLinExpr 
//


enum ctype_t {CONS_EQ, CONS_GE};
class WCons {
	public:
		inline WCons(const WLinExpr &l, ctype_t t) { expr = l; ctype = t; }
		inline WCons() {
			ctype = CONS_EQ;
		}
		inline void print(output_t &out) const {
			if (ctype == CONS_EQ) {
				out << "0 == ";
			} else if (ctype == CONS_GE) {
				out << "0 <= ";
			} else abort();
			expr.print(out);
		}
		inline coef_t inhomogeneous_term() { return expr.inhomogeneous_term(); }
		inline coef_t coefficient(const WVar &v) { return expr.coefficient(v); }
		inline guid_t space_dimension() { return expr.space_dimension(); }
		inline const WLinExpr& getLE() const { return expr; }
		inline ctype_t getType() const { return ctype; }
		inline bool is_equality() const { return ctype == CONS_EQ; }
	private:
		ctype_t ctype;
		WLinExpr expr;

};

class WPoly {
	private:
		class Eliminator {
			public:
				inline Eliminator(const std::vector<PPL::dimension_type> &victims, 
						PPL::C_Polyhedron &p) : _victims(victims), _p(p) { 
				}

				inline PPL::dimension_type max_in_codomain() const { return _p.space_dimension() - 2; }
				inline PPL::dimension_type max_in_domain() const { return _p.space_dimension() - 1; }
				inline bool maps(PPL::dimension_type i, PPL::dimension_type &j) const { 
					PPL::dimension_type k = 0;
					for (std::vector<PPL::dimension_type>::const_iterator it = _victims.begin(); it != _victims.end(); it++) {
						if ((*it) == i)
							return false;
						if ((*it) < i)
							k++;
					}
					j = i - k;
					return true;
				}
				inline bool has_empty_codomain() const { return _p.space_dimension() == _victims.size(); }
			private:
				const std::vector<PPL::dimension_type> &_victims;
				const PPL::C_Polyhedron & _p;
		};
		class MapHash {
			public:
				inline MapHash(const std::map<dim_t,dim_t> remap) : _remap(remap) {
					_max_co = 0;
					_max_dom = 0;

					for (std::map<dim_t,dim_t>::const_iterator it=_remap.begin(); it!=_remap.end(); ++it) {
						if (it->first > _max_dom)
							_max_dom = it->first;
						if (it->second > _max_co)
							_max_co = it->second;
					}
				}

				inline PPL::dimension_type max_in_codomain() const { return _max_co; }
				inline PPL::dimension_type max_in_domain() const { return _max_dom; }
				inline bool maps(PPL::dimension_type i, PPL::dimension_type &j) const { 
					if (_remap.find(i) == _remap.end())
						return false;
					j = _remap.at(i);
					return true;
				}
				inline bool has_empty_codomain() const { return false; }

			private:
				std::map<dim_t,dim_t> _remap;
				dim_t _max_co, _max_dom;
		};
		void combine(WPoly &src, bool keep);


	public:
		class const_iterator {
			public:
				inline const_iterator(const WPoly &p): _p(p), sys(p.poly.minimized_constraints()), real_it(sys.begin()) {
				}
				inline void operator++() {
					++real_it;
				}
				inline void operator++(int) {
					real_it++;
				}
				inline WCons operator*() const {
					return _p.back_translate(*real_it);
				}
				inline operator bool() const {
					return real_it != sys.end();
				}

			private:
				const WPoly &_p;
				PPL::Constraint_System sys;
				PPL::Constraint_System::const_iterator real_it;
		};

		inline WPoly(bool empty = false) {
			poly = PPL::C_Polyhedron(0, empty ? PPL::EMPTY : PPL::UNIVERSE);
		}


		inline PPL::Variable translate(const guid_t g) const {
			return PPL::Variable(adapter.at(g));
		}

		inline PPL::Variable translate(const guid_t g) {
			if (adapter.find(g) == adapter.end()) {
				adapter[g] = next;
				next++;
			}
			return PPL::Variable(adapter[g]);
		}

		inline PPL::Variable translate(const WVar &v) {
			return translate(v.guid());
		}

		inline PPL::Linear_Expression translate(const WLinExpr &le) const {
			return le.toPPL(*this);
		}

		inline PPL::Linear_Expression translate(const WLinExpr &le) {
			return le.toPPL(*this);
		}

		inline PPL::Constraint translate(const WCons &c) const {
			PPL::Linear_Expression lexpr = translate(c.getLE());
			PPL::Constraint ppl_c = (c.getType() == CONS_EQ) ? (lexpr == 0) : (lexpr >= 0);
			return ppl_c;
		}

		inline PPL::Constraint translate(const WCons &c) {
			PPL::Linear_Expression lexpr = translate(c.getLE());
			PPL::Constraint ppl_c = (c.getType() == CONS_EQ) ? (lexpr == 0) : (lexpr >= 0);
			if (poly.space_dimension() < next) {
				poly.add_space_dimensions_and_embed(next - poly.space_dimension());
			}
			return ppl_c;
		}

		std::map<dim_t,guid_t> invMap() const;

		void add_constraint(const WCons &c)  { 
			poly.add_constraint(translate(c));
		}

		WCons back_translate(const PPL::Constraint &c) const;
		void print(output_t &out) const;

		inline const PPL::C_Polyhedron& getPoly() { return poly; }

		inline bool maximize(const WLinExpr&expr, coef_t &sup_n, coef_t &sup_d, bool &maximum) const {
			return poly.maximize(translate(expr), sup_n, sup_d, maximum);
		}

		inline bool minimize(const WLinExpr&expr, coef_t &sup_n, coef_t &sup_d, bool &maximum) const {
			return poly.minimize(translate(expr), sup_n, sup_d, maximum);
		}

		inline void unconstrain(const WVar &var) {
			eliminate(translate(var).id());
		}


		/* Wrapper around PPL methods */
		PPL::Poly_Con_Relation relation_with(const WCons &c) const {
			return poly.relation_with(translate(c));
		}

		inline bool is_empty() const {
			return poly.is_empty();
		}

		inline bool is_universe() const {
			return poly.is_universe();
		}

		inline dim_t space_dimension() const {
			return poly.space_dimension();
		}

		// Mapping (projection)
		template <class F> void map_vars(F pfunc);

		// equality
		inline bool equals(const WPoly &src) const {
			WPoly copie1 = src;
			WPoly copie2 = *this;
			copie1.combine(copie2, true);
			return copie1.poly == copie2.poly;
		}

		// intersection 
		inline void intersection_assign(const WPoly &src) {
			WPoly copie = src;
			combine(copie, true);
			poly.intersection_assign(copie.poly);
		}

		// join
		inline void poly_hull_assign(const WPoly &src) {
			WPoly copie = src;
			combine(copie, false);
			poly.poly_hull_assign(copie.poly);
		}

		// widening	
		inline void bounded_H79_extrapolation_assign(WPoly &src) {
			WPoly copie = src;
			combine(copie, false);
			PPL::Constraint_System dummy;
			poly.bounded_H79_extrapolation_assign(copie.poly, dummy);
		}


	private:
		template <class F> void map_adapter_dim(F pfunc);

		template <class F> inline void map_with_dim(F pfunc) {
			poly.map_space_dimensions(pfunc);
			map_adapter_dim(pfunc);
		}

		inline void eliminate(PPL::dimension_type id) {
			std::vector<PPL::dimension_type> victims;
			victims.push_back(id);
			Eliminator el(victims, poly);
			map_with_dim(el);
		}

		std::map<guid_t,dim_t> adapter;
		dim_t next = 0;
		PPL::C_Polyhedron poly;
};


/* Template implementations */
template <class F> void WPoly::map_adapter_dim(F pfunc) {
	std::vector<guid_t> bye;

	for (std::map<guid_t,dim_t>::iterator it=adapter.begin(); it!=adapter.end(); ++it) {
		PPL::dimension_type j = it->second;
		bool m = pfunc.maps(it->second, j);
		if (m == false) {
			bye.push_back(it->first);
		} else it->second = j;
	}
	for (std::vector<guid_t>::const_iterator it = bye.begin(); it != bye.end(); it++) {
		adapter.erase(*it);
	}
}

template <class F> void WPoly::map_vars(F pfunc) {
	std::map<guid_t,dim_t> new_adapter;
	std::vector<dim_t> victims;
	for (std::map<guid_t,dim_t>::iterator it=adapter.begin(); it!=adapter.end(); ++it) {
		guid_t i = it->first;
		guid_t j = i;
		bool b = pfunc.maps(i, j);
		if (b) {
			new_adapter[j] = it->second;
		} else {
			victims.push_back(it->second);
		}
	}
	adapter = new_adapter;
	Eliminator el(victims, poly);
	map_with_dim(el);
}

} // end namespace poly 
} // end namespace otawa
#endif
