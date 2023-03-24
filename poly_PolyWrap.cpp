#include "include/PolyWrap.h"

namespace otawa {
namespace poly {
/* Display operators */

using namespace elm::io;
using namespace PPL;

void parse(const coef_t& coef, char* buf){
	gmp_snprintf(buf, sizeof(buf), "%Zd", &raw_value(coef));
	buf[sizeof(buf) - 1] = 0;
}

output_t& operator<< (output_t& stream, const coef_t& coef) {
	char buf[128];
	gmp_snprintf(buf, sizeof(buf), "%Zd", &raw_value(coef));
	buf[sizeof(buf) - 1] = 0;
	stream << buf;
	return stream;
}
output_t& operator<< (output_t& stream, const WLinExpr& le) {
	le.print(stream);
	return stream; 
}

output_t& operator<< (output_t& stream, const WCons& c) {
	c.print(stream);
	return stream;
}

output_t& operator<< (output_t& stream, const WPoly& p) {
	p.print(stream);
	return stream;
}
output_t& operator<< (output_t& stream, const WVar& p) {
	stream << "v" << p.guid();
	return stream;
}

WLinExpr::TermIterator::TermIterator(const WCons &c): _le(c.getLE()), real_it(_le.coefs.begin()) { }
void WLinExpr::print(output_t &out) const {
	bool first = true;
	for (std::map<guid_t,coef_t>::const_iterator it=coefs.begin(); it!=coefs.end(); ++it) {
		if (!first && (it->second >= 0)) {
			out << "+ ";
		} else if (it->second < 0) out << "− ";
		if(it->second == 1 || it->second == -1)
			out << "v" << it->first << " ";
		else
			out << (it->second > 0 ? it->second : -it->second) << ".v" << it->first << " ";
		first = false;
	}
	if (cst != 0) {
		if (cst >= 0) {
			out << "+ ";
		} else out << "− ";
		out << (cst > 0 ? cst : -cst);
	} 
}

void WPoly::print(output_t &out) const {
	  out << "\nRaw poly: [";
	  fflush(stdout);
	  poly.print();
	  fflush(stdout);
	  out << "]\n" << endl;
	  out << "Map: [";
	  for (std::map<guid_t,dim_t>::const_iterator it=adapter.begin(); it != adapter.end(); it++) {
		  PPL::Variable pv(it->second);
		char letter = (pv.id() % 26) + 'A';
		int number = pv.id() / 26;
		char name[32];
		if (number != 0) {
			snprintf(name, sizeof(name), "%c%u", letter, number);
		} else {
			snprintf(name, sizeof(name), "%c", letter);
		}
		  out << "v" << it->first << " -> " << name << " █";
	  }
	  out << "] " << endl << endl;
	  out << "WPoly: [";
	  for (WPoly::ConsIterator it(*this); it; it++) {
		  out << (*it) << " █";
	  }
	  out << "]" << endl;
	  out << endl;
}

/* Constraint & Linear combination building operators */

WCons operator==(const WLinExpr &a, const WLinExpr &b) {
	WLinExpr expr = a - b;
	WCons c(expr, CONS_EQ);
	return c;
}

WCons operator>=(const WLinExpr &a, const WLinExpr &b) {
	WLinExpr expr = a - b;
	WCons c(expr, CONS_GE);
	return c;
}

WCons operator<=(const WLinExpr &a, const WLinExpr &b) {
	return (b >= a);
}
WCons operator<(const WLinExpr &a, const WLinExpr &b) {
	return (a + 1 <= b);
}
WCons operator>(const WLinExpr &a, const WLinExpr &b) {
	return (a >= b + 1);
}
WLinExpr operator+(const WLinExpr &a, const WLinExpr &b) {
	WLinExpr res;
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



WLinExpr operator*(const WLinExpr &a, const coef_t b) {
	WLinExpr res;
	if (b == 0)
		return res;
	res.cst = a.cst * b;
	for (std::map<guid_t,coef_t>::const_iterator it=a.coefs.begin(); it!=a.coefs.end(); ++it) {
		res.coefs[it->first] = it->second * b;
	}
	return res;
}

WLinExpr operator*(const coef_t b, const WLinExpr &a) {
	return a * b;
}

WLinExpr operator-(const WLinExpr &a, const WLinExpr &b) {
	return a + -1*b;
}

WLinExpr operator+=(WLinExpr &a, const WLinExpr &b) {
	a = a + b;
	return a;
}

WLinExpr operator-=(WLinExpr &a, const WLinExpr &b) {
	a = a - b;
	return a;
}

/* Conversion methods */
const Linear_Expression WLinExpr::toPPL(WPoly &poly) const {
	Linear_Expression expr;
	for (std::map<guid_t,coef_t>::const_iterator it=coefs.begin(); it!=coefs.end(); ++it) {
		expr = expr + it->second * poly.translate(it->first);
	}
	expr = expr + cst;
	return expr;
}

const Linear_Expression WLinExpr::toPPL(const WPoly &poly) const {
	Linear_Expression expr;
	for (std::map<guid_t,coef_t>::const_iterator it=coefs.begin(); it!=coefs.end(); ++it) {
		expr = expr + it->second * poly.translate(it->first);
	}
	expr = expr + cst;
	return expr;
}

/* Constraint methods */

/* Poly methods */
bool operator==(const WPoly &a, const WPoly &b) {
	return a.equals(b);
}
bool operator!=(const WPoly &a, const WPoly &b) {
	return !a.equals(b);
}

std::map<dim_t,guid_t> WPoly::invMap() const {
	std::map<dim_t,guid_t> res;
	for (std::map<guid_t,dim_t>::const_iterator it=adapter.begin(); it!=adapter.end(); ++it) {
		res[it->second] = it->first;
	}
	return res;
}
WCons WPoly::back_translate(const Constraint &c) const {
	WLinExpr le;
	std::map<dim_t,guid_t> inv = invMap();
	for (dimension_type i = 0; i < poly.space_dimension(); i++) {
		const Coefficient &coef = c.coefficient(Variable(i));
		if (coef != 0) {
			le = le + coef*WVar(inv.at(i));
		}
	}
	le = le + c.inhomogeneous_term();
	if (c.is_equality()) {
		return le == 0;
	} else {
		return le >= 0;
	}
  }


void WPoly::combine(WPoly &src, bool keep) {
	int add = 0; 
	int src_add = 0;
	std::vector<dim_t> victim1;
	std::vector<dim_t> victim2;

	for (std::map<guid_t,dim_t>::const_iterator it=adapter.begin(); it!=adapter.end(); ++it) {
		if (src.adapter.find(it->first) == src.adapter.end()) {
			if (keep) {
				src.adapter[it->first] = src.next;
				src.next++;
				src_add++;
			} else 
				victim1.push_back(it->second);
		}
	}

	for (std::map<guid_t,dim_t>::const_iterator it=src.adapter.begin(); it!=src.adapter.end(); ++it) {
		if (adapter.find(it->first) == adapter.end()) {
			if (keep) {
				adapter[it->first] = next;
				next++;
				add++;
			} else 
				victim2.push_back(it->second);
		}
	}
	poly.add_space_dimensions_and_embed(add);
	src.poly.add_space_dimensions_and_embed(src_add);
	if (!keep) {
		map_with_dim(Eliminator(victim1, poly));
		src.map_with_dim(Eliminator(victim2, src.poly));
	}

	std::map<dim_t,dim_t> srcmap;
	for (std::map<guid_t,dim_t>::const_iterator it=adapter.begin(); it!=adapter.end(); ++it) {
		srcmap[src.adapter[it->first]] = it->second;
	}

	src.map_with_dim(MapHash(srcmap));
}
guid_t WVar::_guid_generator = 0;

}

}

