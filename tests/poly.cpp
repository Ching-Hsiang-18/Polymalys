#include <elm/option/StringList.h>
#include <elm/sys/System.h>
#include <otawa/app/Application.h>
#include <otawa/script/Script.h>
#include <otawa/ipet/IPET.h>
#include <otawa/util/FlowFactLoader.h>
#include <otawa/ilp/System.h>
#include <otawa/stats/StatInfo.h>
#include <otawa/cfg/features.h>
#include <otawa/poly/features.h>
#include <otawa/proc/DynFeature.h>   
#include <otawa/proc/DynProcessor.h>  
#include <otawa/cfg/Virtualizer.h>
#include <otawa/prop/DynIdentifier.h>
#include <otawa/display/CFGDrawer.h>
#include <otawa/display/CFGOutput.h>

using namespace otawa;
using namespace elm::option;
using namespace otawa::poly;

int main(int argc, char **argv) {
	WorkSpace *ws;
	otawa::Manager manager;
	PropList props;
	NO_SYSTEM(props) = true; //Pas de systeme d'exploitation (programme "standalone") 
	otawa::Processor::VERBOSE(props) = true; //Affichage verbose
	TASK_ENTRY(props) = "main"; //C'est le point d'entree du programme a analyser 
	PROCESSOR_PATH(props) = "/home/clement/code/dist/linux-x86_64/otawa-core/share/Otawa/procs/op1.xml";
	otawa::Processor::TIMED(props) = true;
	ws = manager.load("./cible", props);
	ws->require(COLLECTED_CFG_FEATURE, props);
//	ws->require(VIRTUALIZED_CFG_FEATURE, props);
	display::CFGOutput output;
	output.process(ws, props);

	ws->require(DynFeature("otawa::poly::POLY_ANALYSIS_FEATURE"), props);
	ws->require(DynFeature("otawa::ipet::WCET_FEATURE"), props);

}
