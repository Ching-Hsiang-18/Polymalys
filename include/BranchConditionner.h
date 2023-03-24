#ifndef BC
#define BC 1

#include <otawa/otawa.h>
#include "PolyWrap.h"
#include "PPLDomain.h"

#define CONSTRAINTS_FILE "constraints.csv" // the file containing parametric conditionals
#define MAX_LOOP 1000000 // macro used to have a better loop management for parametric conditionals

namespace otawa {
namespace poly {
using namespace otawa;
    /**
    * Map the function arguments using register number (0 <= r <= 3)
    * > 3 means that the value is in the stack (i.g. a function with more than 4 arguments)
    * This identifier should be used on the function's CFG entry node
    */
    extern Identifier<std::map<int,WVar>> FUNCTION_ARGS;

    /**
     * Remember which variables are used in functions (useful for polyhedron projections)
     * It maps a variable to the id of its special identifier
     * This identifier marks a function argument variable as used
     * It should be used on the function's CFG entry node
     */
    extern Identifier<std::map<int,WVar>> USED_ARGS;

    /**
     * Stores the root CFG on any CFG
     */
    extern Identifier<CFG*> ROOT_CFG;

    /**
     * Identifies single function calls in loops
     * std::map<LOOP_HEADER_ID,std::map<FUNCTION_NAME,INSTANCE>>
     */
    extern Identifier<std::map<int,std::map<string,int>>> CURRENT_INSTANCES; 
    
    /**
     * Tells if a loop has exited
     */
    extern Identifier<bool> LOOP_RESET;

    /**
     * Each loop header knows his parent loop
     */
    extern Identifier<int> PREVIOUS_LOOP;	

    /**
     * Arm function arguments detector
     * Enables to manage functino arguments and to track function arguments usage
     */
    class ArmArgumentsDetector{
        public:
            /**
             * mark function parameters at the begining of the analysis
             * @param s the initial state of the analysis
             */
            void initFunctionParameters(PPLDomain* s, Block* cfgEntry);
            /**
             * Check if a variable is a marked as used program argument
             * It should be used in order to choose the variable needed to make a polyhedron projection
             * @param cfgEntry the entry point of the CFG
             * @param v the variable to check
             * @return the id of the parameter if exists, -1 otherwise
             */
            int getParamId(Block* cfgEntry, WVar v);
    };

    /**
     * Simple object class to represent the exported conditional statement
     */
    class Conditional{
        private:
	    Block* b;
            bool taken;
            std::string conditions;
        public:
	    /**
	     * Constructor
	     * @param b the basic block corresponding to the condition
	     * @param taken tell if the condition is taken or not
	     * @param conditions the conditions as a string
	     */
            Conditional(Block* b, bool taken, std::string conditions);
            /**
	     * block accessor
	     * @return the basic block of the condition
	     */
	    Block* getBlock();
            /**
	     * taken accessor
	     * @return if the conditional is taken or not
	     */
	    bool isTaken();
	    /**
	     * conditions accessor
	     * @return the conditions as a string
	     */
            std::string getConditions();
    };

    /**
     * Abstract representation of an ordered element to export
     */
    class OrderedConditional{
	private:
		int loopId; //< identifies the order of the custom loopIds
		Conditional conditional; //< if loopId == -1 then conditional != null
	public:
		OrderedConditional(int loopId, Conditional conditional); //< constructor for outer loop elements
		OrderedConditional(int loopId); //< constructor for inner loops element
		int getLoopId(); //< accessor
		Conditional getConditional(); //< accessor
    };

    /**
     * Keep an order for conditional inside/outside loops
     */
    class OrderedElements{
	private:
	    std::map<int,OrderedConditional> order;
	public:
	    OrderedElements();
	    void addElement(OrderedConditional conditional);
	    std::map<int,OrderedConditional> getOrder();
    };

    /**
     * Remember global order form constraints
     * it should be attached on root cfg
     */
    extern Identifier<OrderedElements> EXPORT_ORDER;

    /**
     * Store only the last condition when in a loop context
     */
    extern Identifier<std::map<int,std::map<bool,Conditional>>> LOOP_CONDITIONALS;
    /**
     * Store the current loop context
     */
    extern Identifier<int> CURRENT_LOOP;
    /**
     * A simple class to export constraints
     */
    class ConstraintExporter{
        public:
            /**
             * Export constraints into a file
             * @param branchingBlock the block inside which the branching instruction is
             * @param taken if the condition is in the taken branch or the other
             * @param conditions the conditionnal boolean expression
             */
	    void exportToFile(Conditional c);
            /**
             * Allows to remove the file containing data, this is needed at the begining of an analysis
             * @return true if the file was deleted, false otherwise
             */
            void resetFile();
	    /**
	     * Manage particular case of loops
	     * @param order non loop conditionals + order
	     * @param loopConstraints loop conditionals
	     */
	    //void exportLoopConstraints(std::map<int, std::map<bool,Conditional>> toExport);
	    void exportOrderedConstraints(OrderedElements order, std::map<int, std::map<bool,Conditional>> loopConstraints);
    };

}
}

#endif // !BC
