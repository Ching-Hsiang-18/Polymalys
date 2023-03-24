#include "include/LoopAnalyzer.h"

namespace otawa{
namespace poly{

	LoopBound::LoopBound(bool linear, int staticBound, std::string linearBound){
		this->linear = linear;
		this->staticBound = staticBound;
		this->linearBound = linearBound;
	}

	void LoopBound::exportToFile(std::ofstream *o, int lid){
		*o << lid;
		if(linear)
			*o << ";true;" << linearBound;
		else
			*o << ";false;" << staticBound;
		*o << endl;
	}

	void LoopBoundsExporter::exportToFile(std::map<int,LoopBound> bounds){
		// prepare the file
		std::ofstream o;
		o.open(LOOP_BOUNDS_FILE, std::ofstream::trunc);
		o << std::string("Loop id;isLinear;bound") << endl;
		// exports all the loop bunds
		for(auto i = bounds.begin(); i != bounds.end(); i++){
			int lid = (*i).first;
			LoopBound lb = (*i).second;
			lb.exportToFile(&o, lid);
		}
		// closing stream
		o.close();
	}

	void LoopAnalyzer::computeLinearBound(Block* b, std::vector<WCons> cs, MyHTable<int, int> mapping){

		// drop constraints that do not contain any parameter (e.g. lower bound)
		std::vector<WCons> kept;
		for(auto c = cs.begin(); c != cs.end(); c++){
			WCons constraint = *c;
			// cout << "CONSTRAINT: " << constraint << endl;
			bool hasBound = false;
			bool hasPA = false;
			for(WLinExpr::TermIterator i(constraint.getLE()); i; i++){
				WVar v = *i;
				// v0 = loop bound variable
				if(v.guid() == 0)
					hasBound = true;
				// vx = entry parameters
				if(v.guid() != 0)
					hasPA = true;
				// push only if the two are present
				if(hasBound && hasPA){
					kept.push_back(constraint);
					break;
				}
			}
			
		}

		// cout << "Number of constraints: " << kept.size() << endl;

		// extract remaining constraints as a loop bound
		if(kept.size() == 1){
			
			// single loop bound
			auto c = kept.begin();
			WCons cons = *c;
			
			// keep coefcicient of the loop bound (division when induction > 1)
			char bufSign[64];

			// coef1 * v1 + ... + coefn * vn + constant
			int first = 1;
			std::string loopBound;

			// pretty printing in a string
			for(WLinExpr::TermIterator i(cons.getLE()); i; i++){
				WVar v = *i;
				coef_t coef = cons.coefficient(v);

				// manage induction and remove loop bound variable from constraint
				if(v.guid() == 0){
					gmp_snprintf(bufSign, sizeof(bufSign), "%Zd", &PPL::raw_value(-coef)); // the bound is negative in the right side of the constraint
					continue;
				}

				// export the loop as an expression
				// sign
				std::string signStr = "";
				if(!first && coef > 0)
					signStr = " + ";
				else if(coef == -1)
					signStr = " - ";

				// coef
				std::string coefStr = "";
				if(coef != 1 && coef != -1){
					char buf[64];
					gmp_snprintf(buf, sizeof(buf), "%Zd", &PPL::raw_value(coef));
					coefStr = std::string(buf);
				}

				// variable mapping
				int paramId = -(mapping[v.guid()] + 1); // get the parameter id from the mapping
				std::string variableStr = "b:" + std::to_string(paramId);

				if(first)
					first = 0;

				// building constraint term
				loopBound = coef != 1 && coef != -1 ? loopBound + signStr + coefStr + " * " + variableStr : loopBound + signStr + variableStr;
			}
			// final bound
			coef_t constant = cons.inhomogeneous_term();
			if(constant != 0){
				char cstBuf[64];
				gmp_snprintf(cstBuf, sizeof(cstBuf), "%Zd", &PPL::raw_value(constant));
				std::string constantStr(cstBuf);
				loopBound = constant > 0 ? loopBound + " + " + constantStr : loopBound + constantStr; // - is in the constant
			}

			int induction = atoi(bufSign);

			// checks
			// check 1 : induction == 0 means that the loop is infinite (or that the coefficient is not an integer value)
			if(induction == 0)
				return;
			// check 2 : induction != 1 means that we must do a division to find the real loop bound
			if(induction != 1)
				loopBound = "(" + loopBound + ")/" + std::string(bufSign);

			// save the bound
			LoopBound lb(true, 0, loopBound);
			std::map<int,LoopBound> lbs = LOOP_BOUNDS(ROOT_CFG(b->cfg())->entry());
			if(lbs.find(b->id()) != lbs.end())
				lbs.erase(b->id());
			lbs.emplace(b->id(), lb);
			LOOP_BOUNDS(ROOT_CFG(b->cfg())->entry()) = lbs;

		}
		else{
			if(kept.size() != 0)
				cout << "[WARN] Loop have more than a single bound, case not supported yet !" << endl;
		}

		// OLD CODE
		// std::vector<WCons> cs_bef = CONSTRAINTS(b);

		// std::map<int,LoopBound> lbs = LOOP_BOUNDS(ROOT_CFG(b->cfg())->entry());
		// // clear old data if exist
		// if(lbs.find(b->id()) != lbs.end())
		// 	lbs.erase(b->id());

		// // filtering needed to remove weird constraints which are not the maximum loop bound
		// std::vector<WCons> cs;
		// int addedIterations = 0;
		// std::map<int,coef_t> maximums;
		// // find the maximum value for each constraint
		// for(auto i = cs_bef.begin(); i != cs_bef.end(); i++){
		// 	WCons c = *i;
		// 	for(WLinExpr::TermIterator i(c.getLE()); i; i++){
		// 		WVar v = *i;
		// 		coef_t coef = c.coefficient(v);
		// 		if(maximums.find(v.guid()) == maximums.end()){
		// 			maximums.emplace(v.guid(),coef);
		// 		}
		// 		else{
		// 			if(maximums.at(v.guid()) > coef){
		// 				maximums.erase(v.guid());
		// 				maximums.emplace(v.guid(), coef);
		// 			}
		// 		}
		// 	}
		// }
		// // remove constraints not representing the maximum value
		// for(auto i = cs_bef.begin(); i != cs_bef.end(); i++){
		// 	bool add = true;
		// 	WCons c = *i;
		// 	for(WLinExpr::TermIterator i(c.getLE()); i; i++){
		// 		WVar v = *i;
		// 		coef_t coef = c.coefficient(v);
		// 		//cout << "var :" << v.guid() <<  ", coef: " << coef << ", should be = to " << maximums.at(v.guid()) << endl;
		// 		if(coef > maximums.at(v.guid())){
		// 			add = false;
		// 			break;
		// 		}
		// 	}
		// 	if(add){
		// 		cs.push_back(c);
		// 		//cout << "added " << c << endl;
		// 	} else {
		// 		if(i == cs_bef.begin() || cs.size() == 0){
		// 			// sometimes the first loop bound is not correctly extracted since polyhedra will simplify the constraint...
		// 			// thus we need to correct the iteration number
		// 			addedIterations++;
		// 		}
		// 	}
		// }

		// // use remaining constraints to compute loop induction
		// WCons previous;
		// WCons current;
		// coef_t current_diff = 0;
		// bool error = cs.size() > 0 ? false : true;
		// for(auto i = cs.begin(); i != cs.end(); i++){
		// 	current = *i;


		// 	//cout << "ANALYZING " << current << endl;

		// 	// detect variace between two constraints
		// 	if(!(i == cs.begin())){
		// 		coef_t const_current = current.inhomogeneous_term();
		// 		coef_t const_previous = previous.inhomogeneous_term();
		// 		coef_t diff = const_current - const_previous;
		// 		//cout << "DIFF = " << diff << ", OLD DIFF = " << current_diff << endl;
		// 		if(current_diff != 0 && current_diff != diff){
		// 			error = true; //< This means that we do not have a constant induction
		// 			cout << "WARN: There is a difference between loop iterations ? diff = " << diff << ", current diff = " << current_diff << endl;
		// 		}
		// 		else{
		// 			cout << "INFO: Found a regular induction:" << diff << ", " << current_diff << endl;
		// 			error = false;
		// 		}
		// 		current_diff = diff;
		// 	}
		// 	previous = current;
		// }
		// if(error || current_diff == 0 || cs.size() < 2){
		// 	// Unable to compute loop induction
		// 	if(error && cs.size() >= 2)
		// 		cout << "WARN: Could not compute a constant loop induction" << endl;
		// 	else if(cs.size() < 2)
		// 		cout << "INFO: Could not compute a loop induction (too few iterations)" << endl;
		// 	else if(current_diff == 0)
		// 		cout << "WARN: Loop induction 0 detected, possibly an infinite loop" << endl;

		// }
		// else{
		// 	// Found a loop induction and thus exports the parametric loop bound
		// 	
		// 	//cout << "COMPUTED DIFFERENCE = " << current_diff << endl;
		// 	WCons c = *(cs.begin()); // use the first loop exit constraint to compute the bound

		// 	// coef_1 * v_1 + ... + coef_n * v_n + constant
		// 	// retrieve constant
		// 	coef_t constant = c.inhomogeneous_term();
		// 	//cout << "constant: " << constant << endl;

		// 	// format constraint as a string, which is easier to export in a file
		// 	// we need to reverse the constraint since we are in the loop exit case
		// 	std::string cString = /*"0 " + op*/"";
		// 	std::string opmult = " * ";
		// 	std::string opadd = " + ";
		// 	std::string opminus = " - ";
		// 	char buf[128];
		// 	int index=0;
		// 	for(WLinExpr::TermIterator i(c.getLE()); i; i++){
		// 		WVar v = *i;
		// 		coef_t coef = c.coefficient(v);
		// 		
		// 		// manage operator
		// 		std::string opv = index == 0 ? "" : " + ";
		// 		opv = -coef > 0 ? opv : " - ";

		// 		// manage coef != 1 && != 1
		// 		if(coef > 1 || coef < -1){
		// 			coef > 0 ? parse(coef, buf) : parse(-coef, buf);
		// 			std::string cs(buf);
		// 			opv = opv + cs + opmult;
		// 		}

		// 		// manage variable name
		// 		std::string var = std::to_string(v.guid());
		// 		opv = opv + "b:" + var;
		// 		cString = cString + opv;

		// 		++index;
		// 	}

		// 	// manage constant value
		// 	// correction of part of bug 1 : - when += >2, even if bugued for now
		// 	/*cout << "CURRENT_DIFF = " << current_diff << endl;

		// 	constant = current_diff > 0 ? constant - current_diff : constant;
		// 	constant = current_diff < 0 ? constant + current_diff : constant;*/
		// 	
		// 	// manage constant sign
		// 	std::string opconst = -constant > 0 ? opadd : opminus;

		// 	// correction of the loop bound
		// 	if(addedIterations != 0){
		// 		constant = current_diff > 0 ? constant - (current_diff*addedIterations) : constant;
		// 		constant = current_diff < 0 ? constant + (current_diff*addedIterations) : constant;
		// 	}

		// 	constant = -constant > 0 ? -constant : constant;

		// 	parse(constant, buf);
		// 	std::string cst(buf);
		// 	if(cst != "0")
		// 		cString = cString + opconst + cst;

		// 	// support for more than i+=1 (i+=x), involves a division operator in the resulting expression
		// 	if(current_diff > 1 || current_diff < -1){
		// 		cout << "WARN: Unsupported loop bound (induction > 1 || induction < -1)" << endl;
		// 		return; // FIXME it does not work in all cases and produces bugs
		// 		std::cout << "cString before: " << cString << endl;
		// 		ASSERT(current_diff > 0); // cannot be < 0 since it is a loop bound ?
		// 		parse(current_diff, buf);
		// 		cString = "(" + cString + ")/" + buf;
		// 	}

		// 	std::cout << "COMPUTED LOOP BOUND(" << b->id() << ") = " << cString << endl;
		// 	// save the loop bound
		// 	LoopBound lb(true, 0, cString);
		// 	lbs.emplace(b->id(), lb);
		// }
		// LOOP_BOUNDS(ROOT_CFG(b->cfg())->entry()) = lbs;
	}

	Identifier<std::map<int,LoopBound>> LOOP_BOUNDS("otawa::poly::LOOP_BOUNDS", std::map<int,LoopBound>());
	Identifier<std::vector<PPLDomain>> LOOP_STATES("otawa::poly::LOOP_STATES", std::vector<PPLDomain>());
	Identifier<std::vector<WCons>> CONSTRAINTS("ortawa::poly::CONSTRAINTS", std::vector<WCons>());

}
}
