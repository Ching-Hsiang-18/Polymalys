#include <stdio.h>

#include <iostream>
#include <fstream>
#include <unistd.h>
#include <ppl.hh>
#include <fcntl.h>
#include "poly.h"

using namespace std;
namespace PPL = Parma_Polyhedra_Library;

typedef unsigned long long guid_t;
typedef GMP_Integer coef_t;
typedef PPL::dimension_type dim_t;
typedef std::ostream output_t;

class ChoupiVar;
class ChoupiCons;
class ChoupiLinExpr;
class ChoupiPoly;
output_t& operator<< (output_t& stream, const coef_t&);
output_t& operator<< (output_t& stream, const ChoupiLinExpr&);
output_t& operator<< (output_t& stream, const ChoupiCons&);
output_t& operator<< (output_t& stream, const ChoupiPoly&);

class ChoupiVar {
public:
  ChoupiVar() {
    _guid = _guid_generator;
    _guid_generator++;
  }
  ChoupiVar(guid_t guid) { _guid = guid; }
  guid_t guid() const { return _guid; }
  

private:
  static guid_t _guid_generator;
  guid_t _guid;
};

class ChoupiCons;
class ChoupiLinExpr {
  friend ChoupiLinExpr operator+(const ChoupiLinExpr &a, const ChoupiLinExpr &b);
  friend ChoupiLinExpr operator-(const ChoupiLinExpr &a, const ChoupiLinExpr &b);
  friend ChoupiLinExpr operator*(const ChoupiLinExpr &a, const coef_t b);
  friend ChoupiLinExpr operator*(const ChoupiLinExpr &a, const coef_t b);
  friend ChoupiCons operator==(const ChoupiLinExpr &a, const ChoupiLinExpr &b);

public:
  ChoupiLinExpr(const coef_t c = 0) { cst = c; }
  ChoupiLinExpr(const int c) { cst = coef_t(c); }
  ChoupiLinExpr(const ChoupiVar &v) { coefs[v.guid()] = 1; cst = 0; }
  
  const PPL::Linear_Expression toPPL(ChoupiPoly &poly) const;
  const PPL::Linear_Expression toPPL(const ChoupiPoly &poly) const;

  void print(output_t &out) const {
    bool first = true;
    for (std::map<guid_t,coef_t>::const_iterator it=coefs.begin(); it!=coefs.end(); ++it) {
      if (!first && (it->second >= 0)) {
        out << "+ ";
      } else if (it->second < 0) out << "- ";
      out << (it->second > 0 ? it->second : -it->second) << ".v" << it->first << " ";
      first = false;
    }
    if (cst != 0) {
      if (cst >= 0) {
        out << "+ ";
      } else out << "- ";
      out << (cst > 0 ? cst : -cst);
    } 
  }
private:
  map<guid_t,coef_t> coefs;
  coef_t cst;
};

guid_t ChoupiVar::_guid_generator = 0;

ChoupiLinExpr operator+(const ChoupiLinExpr &a, const ChoupiLinExpr &b) {
  ChoupiLinExpr res;
  res.cst = a.cst + b.cst;
  
  for (std::map<guid_t,coef_t>::const_iterator it=a.coefs.begin(); it!=a.coefs.end(); ++it) {
    std::map<guid_t,coef_t>::const_iterator it2 = b.coefs.find(it->first);
    coef_t v = it->second;
    if (it2 != b.coefs.end()) {
      v += b.coefs.at(it->first);
    } 
    if (v != 0) 
      res.coefs[it->first] = v;
  }
  for (std::map<guid_t,coef_t>::const_iterator it=b.coefs.begin(); it!=b.coefs.end(); ++it) {
    if (a.coefs.find(it->first) == a.coefs.end())
      res.coefs[it->first] = it->second;
  }

  return res;
}



ChoupiLinExpr operator*(const ChoupiLinExpr &a, const coef_t b) {
  ChoupiLinExpr res;
  if (b == 0)
    return res;
  res.cst = a.cst * b;
  for (std::map<guid_t,coef_t>::const_iterator it=a.coefs.begin(); it!=a.coefs.end(); ++it) {
    res.coefs[it->first] = it->second * b;
  }
  return res;
}

ChoupiLinExpr operator*(const coef_t b, const ChoupiLinExpr &a) {
  return a * b;
}

ChoupiLinExpr operator-(const ChoupiLinExpr &a, const ChoupiLinExpr &b) {
  return a + -1*b;
}

ChoupiLinExpr operator+=(ChoupiLinExpr &a, const ChoupiLinExpr &b) {
	a = a + b;
	return a;
}

ChoupiLinExpr operator-=(ChoupiLinExpr &a, const ChoupiLinExpr &b) {
	a = a - b;
	return a;
}

enum ctype_t {CONS_EQ, CONS_GE};
class ChoupiCons {
public:
  ChoupiCons(const ChoupiLinExpr &l, ctype_t t) { expr = l; ctype = t; }
  void print(output_t &out) const {
    if (ctype == CONS_EQ) {
      out << "0 == ";
    } else if (ctype == CONS_GE) {
		out << "0 <= ";
	} else abort();
    expr.print(out);
  }
  const ChoupiLinExpr& getLE() const { return expr; }
  ctype_t getType() const { return ctype; }
private:
  ctype_t ctype;
  ChoupiLinExpr expr;

};


ChoupiCons operator==(const ChoupiLinExpr &a, const ChoupiLinExpr &b) {
  ChoupiLinExpr expr = a - b;
  ChoupiCons c(expr, CONS_EQ);
  return c;
}

ChoupiCons operator>=(const ChoupiLinExpr &a, const ChoupiLinExpr &b) {
  ChoupiLinExpr expr = a - b;
  ChoupiCons c(expr, CONS_GE);
  return c;
}

ChoupiCons operator<=(const ChoupiLinExpr &a, const ChoupiLinExpr &b) {
  return (b >= a);
}


class ChoupiPoly {
	private:
		class Eliminator {
			public:
				Eliminator(const vector<PPL::dimension_type> &victims, 
						PPL::C_Polyhedron &p) : _victims(victims), _p(p) { 
				}
			
				inline PPL::dimension_type max_in_codomain() const { return _p.space_dimension() - 2; }
				inline PPL::dimension_type max_in_domain() const { return _p.space_dimension() - 1; }
				inline bool maps(PPL::dimension_type i, PPL::dimension_type &j) const { 
					PPL::dimension_type k = 0;
					for (vector<PPL::dimension_type>::const_iterator it = _victims.begin(); it != _victims.end(); it++) {
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
				const vector<PPL::dimension_type> &_victims;
				const PPL::C_Polyhedron & _p;
		};
		class MapHash {
			public:
				MapHash(const map<dim_t,dim_t> remap) : _remap(remap) {
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
				map<dim_t,dim_t> _remap;
				dim_t _max_co, _max_dom;
		};
		void combine(ChoupiPoly &src, bool keep) {

		  //remove variable appearing one side

			vector<dim_t> victim1;
			int add = 0; 
			int src_add = 0;
		  
		  for (std::map<guid_t,dim_t>::const_iterator it=adapter.begin(); it!=adapter.end(); ++it) {
			  if (src.adapter.find(it->first) == src.adapter.end()) {
				  if (keep) {
					  src.adapter[it->first] = src.next;
					  src.next++;
					  src_add++;
//					  src.poly.add_space_dimensions_and_embed(1);
				  } else 
					  victim1.push_back(it->second);
			  }
		  }
			vector<dim_t> victim2;
		  for (std::map<guid_t,dim_t>::const_iterator it=src.adapter.begin(); it!=src.adapter.end(); ++it) {
			  if (adapter.find(it->first) == adapter.end()) {
				  if (keep) {
					  adapter[it->first] = next;
					  next++;
					  add++;
//					  src.poly.add_space_dimensions_and_embed(1);
				  } else  {

					  victim2.push_back(it->second);
				  }
			  }
		  }
		  poly.add_space_dimensions_and_embed(add);
		  src.poly.add_space_dimensions_and_embed(src_add);
		  /*
		  cout << "eliminating, this = " << *this << endl;
		  cout << "eliminating,  src = " << src << endl;
		  */
		  if (!keep) {
			  map_with_dim(Eliminator(victim1, poly));
			  src.map_with_dim(Eliminator(victim2, src.poly));
		  }
		  /*
		  cout << "eliminated, this = " << *this << endl;
		  cout << "eliminated,  src = " << src << endl;
		  */


		  std::map<dim_t,dim_t> srcmap;
		  for (std::map<guid_t,dim_t>::const_iterator it=adapter.begin(); it!=adapter.end(); ++it) {
			  srcmap[src.adapter[it->first]] = it->second;
		  }


		  //map with hash src with srcmap
		  src.map_with_dim(MapHash(srcmap));
			
		}


public:
	class const_iterator {
		public:
			const_iterator(const ChoupiPoly &p): _p(p), sys(p.poly.minimized_constraints()), real_it(sys.begin()) {
			}
			void operator++() {
				++real_it;
			}
			void operator++(int) {
				real_it++;
			}
			ChoupiCons operator*() const {
				return _p.back_translate(*real_it);
			}
			operator bool() const {
				return real_it != sys.end();
			}

		private:
			const ChoupiPoly &_p;
			PPL::Constraint_System sys;
			PPL::Constraint_System::const_iterator real_it;
	};
  ChoupiPoly() {
    poly = PPL::C_Polyhedron(0, PPL::UNIVERSE);
  }


	// intersection 
	void intersection_assign(const ChoupiPoly &src) {
		ChoupiPoly copie = src;
		combine(copie, true);
		poly.intersection_assign(copie.poly);
	}

	// join
	void poly_hull_assign(const ChoupiPoly &src) {
		ChoupiPoly copie = src;
		combine(copie, false);
		poly.poly_hull_assign(copie.poly);
	}

	// widening	
	void bounded_H79_extrapolation_assign(ChoupiPoly &src) {
		ChoupiPoly copie = src;
		combine(copie, false);
		PPL::Constraint_System dummy;
		poly.bounded_H79_extrapolation_assign(copie.poly, dummy);
	}

  
  PPL::Variable translate(const guid_t g) const {
    return PPL::Variable(adapter.at(g));
  }

  PPL::Variable translate(const guid_t g) {
    if (adapter.find(g) == adapter.end()) {
      adapter[g] = next;
      next++;
    }
    return PPL::Variable(adapter[g]);
  }
  
  PPL::Variable translate(const ChoupiVar &v) {
    return translate(v.guid());
  }
  
  PPL::Linear_Expression translate(const ChoupiLinExpr &le) const {
    return le.toPPL(*this);
  }

  PPL::Linear_Expression translate(const ChoupiLinExpr &le) {
    return le.toPPL(*this);
  }

  PPL::Constraint translate(const ChoupiCons &c) const {
    PPL::Linear_Expression lexpr = translate(c.getLE());
    PPL::Constraint ppl_c = (c.getType() == CONS_EQ) ? (lexpr == 0) : (lexpr >= 0);
    return ppl_c;
  }

  PPL::Constraint translate(const ChoupiCons &c) {
    PPL::Linear_Expression lexpr = translate(c.getLE());
    PPL::Constraint ppl_c = (c.getType() == CONS_EQ) ? (lexpr == 0) : (lexpr >= 0);
    if (poly.space_dimension() < next) {
      poly.add_space_dimensions_and_embed(next - poly.space_dimension());
    }
    return ppl_c;
  }

  map<dim_t,guid_t> invMap() const {
	  map<dim_t,guid_t> res;
	  for (std::map<guid_t,dim_t>::const_iterator it=adapter.begin(); it!=adapter.end(); ++it) {
		  res[it->second] = it->first;
	  }
	  return res;
  }

  void add_constraint(const ChoupiCons &c)  { 
    poly.add_constraint(translate(c));
  }

  ChoupiCons back_translate(const PPL::Constraint &c) const {
	ChoupiLinExpr le;
	map<dim_t,guid_t> inv = invMap();
	for (PPL::dimension_type i = 0; i < poly.space_dimension(); i++) {
		const PPL::Coefficient &coef = c.coefficient(Variable(i));
		if (coef != 0) {
			le = le + coef*ChoupiVar(inv[i]);
		}
	}
	le = le + c.inhomogeneous_term();
	if (c.is_equality()) {
		return le == 0;
	} else {
		return le >= 0;
	}
  }

  void print(output_t &out) const {
	  out << "[";
	  for (ChoupiPoly::const_iterator it(*this); it; it++) {
		  out << (*it) << "; ";
	  }
	  out << "]" << endl;
	  poly.print();
	  cout << endl;
  }
  const PPL::C_Polyhedron& getPoly() { return poly; }

  inline bool maximize(const ChoupiLinExpr&expr, coef_t &sup_n, coef_t &sup_d, bool &maximum) const {
	  return poly.maximize(translate(expr), sup_n, sup_d, maximum);
  }

  inline void unconstrain(const ChoupiVar &var) {
	  eliminate(translate(var).id());
  }

  template <class F> void map_vars(F pfunc) {
	  map<guid_t,dim_t> new_adapter;
	  vector<dim_t> victims;
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
private:
  template <class F> void map_adapter_dim(F pfunc) {
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

  template <class F> void map_with_dim(F pfunc) {
	  poly.map_space_dimensions(pfunc);
	  map_adapter_dim(pfunc);
  }

  void eliminate(PPL::dimension_type id) {
	  vector<PPL::dimension_type> victims;
	  victims.push_back(id);
	  Eliminator el(victims, poly);
	  map_with_dim(el);
  }
  map<guid_t,dim_t> adapter;
  dim_t next = 0;
  PPL::C_Polyhedron poly;
};

output_t& operator<< (output_t& stream, const coef_t& coef) {
	char buf[128];
	gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(coef));
	buf[sizeof(buf) - 1] = 0;
	stream << buf;
	return stream;
}
output_t& operator<< (output_t& stream, const ChoupiLinExpr& le) {
  le.print(stream);
  return stream; 
}
output_t& operator<< (output_t& stream, const ChoupiCons& c) {
  c.print(stream);
  return stream;
}

output_t& operator<< (output_t& stream, const ChoupiPoly& p) {
  p.print(stream);
  return stream;
}
const PPL::Linear_Expression ChoupiLinExpr::toPPL(ChoupiPoly &poly) const {
	PPL::Linear_Expression expr;
	for (std::map<guid_t,coef_t>::const_iterator it=coefs.begin(); it!=coefs.end(); ++it) {
		expr = expr + it->second * poly.translate(it->first);
	}
	expr = expr + cst;
	return expr;
}

const PPL::Linear_Expression ChoupiLinExpr::toPPL(const ChoupiPoly &poly) const {
	PPL::Linear_Expression expr;
	for (std::map<guid_t,coef_t>::const_iterator it=coefs.begin(); it!=coefs.end(); ++it) {
		expr = expr + it->second * poly.translate(it->first);
	}
	expr = expr + cst;
	return expr;
}

class Shift {
private:
	const int _depl;
public:
	Shift(int depl) : _depl(depl) { }
	bool maps(const guid_t i, guid_t &j) {
		if ((_depl < 0) && (i < ((unsigned)-_depl)))
			return false;
		j = i + _depl;
		return true;
	}
};

class Bye {
private:
	const int _bye;
public:
	Bye(int bye) : _bye(bye) { }
	bool maps(const guid_t i, guid_t &j) {
		if (i == _bye)
			return false;
		j = i;
		return true;
	}
};
int main(void) {

  ChoupiVar x;
  ChoupiVar y;
  ChoupiVar z;
  
  ChoupiPoly p;
  p.add_constraint(x >= 0);
  p.add_constraint(x <= 5);
  p.add_constraint(y == 42);
  p.add_constraint(z <= 10);
  coef_t bsup_n, bsup_d;
  bool maximum;
  cout << "poly: " << p << endl;
  cout << "Testage maximization de v2\n";
  p.maximize(z, bsup_n, bsup_d, maximum);
  cout << bsup_n << "/" << bsup_d << endl;
//  cout << c << endl;;
  cout << p << endl;
  cout << "cylindrification v1\n";
  p.unconstrain(y);
  cout << p << endl;

  cout << "cylindrification v0\n";
  Bye a(0);
  p.map_vars(a);
  cout << p << endl;

  cout << "Testage convex-hull" << endl;  
  ChoupiPoly p1;
  ChoupiPoly p2;

  p1.add_constraint(x >= 0);
  p1.add_constraint(x <= 5);
  p2.add_constraint(y >= 0);
  p2.add_constraint(x >= 2);
  p2.add_constraint(x <= 8);
  cout << "p1 " << p1 << endl;
  cout << "p2 " << p2 << endl;
  ChoupiPoly p3 = p1;
  p3.poly_hull_assign(p2);
  cout << "p1 \\/ p2 " << p3 << endl;

  cout << "Testage intersection" << endl;  
  p3 = p1;
  p3.intersection_assign(p2);
  cout << "p1 /\\ p2 " << p3 << endl;

  exit(0);
}
