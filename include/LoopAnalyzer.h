#ifndef LA
#define LA 1

#include <fstream>
#include <otawa/otawa.h>
#include "PolyWrap.h"
#include "PPLDomain.h"

#define LOOP_BOUNDS_FILE "loop_bounds.csv" // the file containing loop bounds
//#define LINBOUND // exports linear bounds when needed
//#define INTER_PROCEDURAL // required by LINBOUND

namespace otawa{
namespace poly{

	using namespace otawa;

	/**
	 * A state analysis to detect induction variables
	 */
	class LoopAnalyzer{
		public:
			/**
			 * Try to compute a linear loop bound from a projected polyhedron
			 * @param b the header of the loop
			 * @param proj the result of the projection
			 */
			void computeLinearBound(Block* b, std::vector<WCons> cs, MyHTable<int, int> mapping);
	};

	/**
	 * Represent a loop bound, which can be either static or linear
	 */
	class LoopBound{
		private:
			bool linear; //< gives the kind of bound
			int staticBound; //< static bound if exists
			std::string linearBound; //< linear bound f exists
		public:
			/**
			 * Constructor
			 * @param linear true = linear, false = static
			 * @param staticBound the static bound if exists
			 * @param linearBouns the linear bound if exists
			 */
			LoopBound(bool linear, int staticBound, std::string linearBound);
			/**
			 * Exports the constraint into the giver file
			 * @param o the output file stream
			 * @param lid the loop id corresponding to that bound
			 */
			void exportToFile(std::ofstream *o, int lid);
	};

	/**
	 * A simple class to export all the loop bounds at once
	 */
	class LoopBoundsExporter{
		public:
			/**
			 * Exports all, the loop bounds into a file
			 * @param bounds the loop bounds to export
			 */
			void exportToFile(std::map<int,LoopBound> bounds);
	};

	/**
	 * Identifier to store loop bounds
	 * Attached to root cfg entry
	 */
	extern Identifier<std::map<int,LoopBound>> LOOP_BOUNDS;
	
	/**
	 * Identifier to store the constraints detected at each loop iteration
	 * Attached to the loop header block
	 */
	extern Identifier<std::vector<WCons>> CONSTRAINTS;

}
}

#endif
