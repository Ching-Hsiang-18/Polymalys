#include <otawa/proc/ProcessorPlugin.h>

namespace otawa {
namespace poly {

using namespace otawa;

// Plugin declaration
class Plugin : public ProcessorPlugin {
  public:
	Plugin() : ProcessorPlugin("otawa::poly::PolyAnalysis", Version(1, 0, 0), OTAWA_PROC_VERSION) {}
};

} // namespace poly
} // namespace otawa

otawa::poly::Plugin OTAWA_PROC_HOOK;
otawa::poly::Plugin &otawa_poly = OTAWA_PROC_HOOK;
