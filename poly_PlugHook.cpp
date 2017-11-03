#include <otawa/proc/ProcessorPlugin.h>

namespace otawa {
namespace poly {

using namespace otawa;

// Plugin declaration
class Plugin : public ProcessorPlugin {
  public:
	Plugin() : ProcessorPlugin("otawa::poly", Version(1, 0, 0), OTAWA_PROC_VERSION) {}
};

} // namespace poly
} // namespace otawa

otawa::poly::Plugin otawa_poly_plugin;
ELM_PLUGIN(otawa_poly_plugin, OTAWA_PROC_HOOK);
