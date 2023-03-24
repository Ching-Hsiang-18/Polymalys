#include <fstream>
#include<cstdio>

#include "include/BranchConditionner.h"

namespace otawa{
    namespace poly{

        void ArmArgumentsDetector::initFunctionParameters(PPLDomain* s, Block* cfgEntry){
            if(cfgEntry->cfg() != ROOT_CFG(cfgEntry->cfg()))
                return;
            std::map<int,WVar> used = USED_ARGS(cfgEntry);
            // track function arguments stored in registers
            for(int reg=0; reg<=3; reg++){
                WVar r = s->varNew(Ident(reg, Ident::ID_REG), NO_STEP, false, false); //< create register variable
                WVar sr = s->varNew(Ident(-reg-1, Ident::ID_SPECIAL), NO_STEP, false, false); //< create argument variable
                s->doNewConstraint(sr == r); //< mapping initial register value with argument variable
                used.emplace(reg,sr); //< add the function arguments to the set of needed variables for projection
            }
            USED_ARGS(cfgEntry) = used;
        }

        int ArmArgumentsDetector::getParamId(Block* cfgEntry, WVar v){
            std::map<int,WVar> map = USED_ARGS(cfgEntry);
            for(auto i = map.begin(); i != map.end(); i++){
                WVar cmp = (*i).second;
                if(v.guid() == cmp.guid() && v.id() == cmp.id())
                    return (*i).first;
            }
            return -1;
        }

	Conditional::Conditional(Block* b, bool taken, std::string conditions){
		this->b = b;
		this->taken = taken;
		this->conditions = conditions;
	}

	Block* Conditional::getBlock(){
		return b;
	}

	bool Conditional::isTaken(){
		return taken;
	}

	std::string Conditional::getConditions(){
		return conditions;
	}

	OrderedConditional::OrderedConditional(int loopId, Conditional conditional) : conditional(conditional) {
		this->loopId = loopId;
		this->conditional = conditional;
	}

	OrderedConditional::OrderedConditional(int loopId) : conditional(nullptr,false,"") {
		this->loopId = loopId;
	}

	int OrderedConditional::getLoopId(){
		return loopId;
	}

	Conditional OrderedConditional::getConditional(){
		return conditional;
	}

	OrderedElements::OrderedElements(){
		order = std::map<int,OrderedConditional>();
	}

	void OrderedElements::addElement(OrderedConditional conditional){
		order.emplace(order.size(), conditional);
	}

	std::map<int,OrderedConditional> OrderedElements::getOrder(){
		return order;
	}

        void ConstraintExporter::exportToFile(Conditional c){
            std::ofstream o;
            o.open(CONSTRAINTS_FILE, std::ofstream::app);
            o << c.getBlock()->toBasic()->control()->address() << ";";
            if(c.isTaken())
                o << "true;";
            else
                o << "false;";
            o << c.getConditions() << endl;
            o.close();
        }

        void ConstraintExporter::resetFile(){
            std::ofstream o;
            o.open(CONSTRAINTS_FILE, std::ofstream::trunc);
            o << std::string("Branching Instruction Address;taken;conditions") <<endl;
            o.close();
        }

	void ConstraintExporter::exportOrderedConstraints(OrderedElements order, std::map<int, std::map<bool,Conditional>> loopConstraints){
		std::map<int,OrderedConditional> oc = order.getOrder();
		for(int i = 0; i < oc.size(); i++){
			OrderedConditional wrapper = oc.at(i);
			// manage non loop elements
			if(wrapper.getLoopId() == -1){
				exportToFile(wrapper.getConditional());
			}
			// manage loop elements
			else{
				std::map<bool,Conditional> branching = loopConstraints.at(wrapper.getLoopId());
				for(auto i = branching.begin(); i != branching.end(); i++){
					exportToFile((*i).second);
				}
			}
		}
	}
	
        Identifier<std::map<int,WVar>> FUNCTION_ARGS("otawa::poly::FUNCTION_ARGS", std::map<int,WVar>());
        Identifier<std::map<int,WVar>> USED_ARGS("otawa::poly::USED_ARGS", std::map<int,WVar>());
        Identifier<CFG*> ROOT_CFG("otawa::poly::ROOT_CFG", nullptr);
	Identifier<std::map<int,std::map<string,int>>> CURRENT_INSTANCES("otawa::poly::CURRENT_INSTANCES",std::map<int,std::map<string,int>>());
	Identifier<bool> LOOP_RESET("otawa::poly::LOOP_RESET", false);
	Identifier<std::map<int,std::map<bool,Conditional>>> LOOP_CONDITIONALS("otawa::poly::LOOP_CONDITIONALS", std::map<int,std::map<bool,Conditional>>());
	Identifier<int> CURRENT_LOOP("otawa::poly::CURRENT_LOOP", -1);
	Identifier<int> PREVIOUS_LOOP("otawa::poly::PREVIOUS_LOOP", -1);
	Identifier<OrderedElements> EXPORT_ORDER("otawa::poly::EXPORT_ORDER", OrderedElements());
} // namespace poly
} // namespace otawa
