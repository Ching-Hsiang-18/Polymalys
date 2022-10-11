#include <otawa/otawa.h>
#include <elm/sys/System.h>
#include <otawa/app/Application.h>
#include <otawa/script/Script.h>
#include <otawa/ipet/IPET.h>
#include <otawa/util/FlowFactLoader.h>
#include <otawa/ilp/System.h>
#include <otawa/stats/StatInfo.h>
#include <otawa/cfg/features.h>
#include <otawa/proc/DynFeature.h>   
#include <otawa/proc/DynProcessor.h>  
#include <otawa/cfg/Virtualizer.h>
#include <otawa/prop/DynIdentifier.h>
#include <otawa/oslice/features.h>



using namespace otawa;
using namespace elm::option;

int main(int argc, char **argv) {
	WorkSpace *ws;
	otawa::Manager manager;
	PropList props;

	NO_SYSTEM(props) = true; //No operating system (standalone program)
	otawa::Processor::VERBOSE(props) = true; //Verbose display
	TASK_ENTRY(props) = ((argc > 2) ? argv[2] : "entry"); //Target program entry point
	VERBOSE(props) = true;
	otawa::Processor::TIMED(props) = true; //Show analysis time
	ws = manager.load((argc > 1) ? argv[1] : "./target", props); //Load target binary

	ws->require(COLLECTED_CFG_FEATURE, props); //Launch CFG construction
	ws->require(DynFeature("otawa::poly::POLY_ANALYSIS_FEATURE"), props); //Launch polyhedra analysis
//	ws->require(DynFeature("otawa::ipet::WCET_FEATURE"), props); //Compute WCET

}
