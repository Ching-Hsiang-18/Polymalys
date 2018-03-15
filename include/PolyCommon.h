#ifndef POLYCOMMON_H
#define POLYCOMMON_H 1

#include <elm/util/BitVector.h>
#include <otawa/cfg.h>
#include <otawa/cfg/features.h>
#include <otawa/ipet.h>
#include <otawa/otawa.h>
#include <otawa/prog/sem.h>
#include <ppl.hh>

#include "MyHTable.h"

// #define POLY_DEBUG 1

namespace otawa {
namespace poly {
using namespace otawa;
using namespace otawa::util;

class PPLDomain;
extern Identifier<int> NUM_LOC_VARS;
extern Identifier<int> LOC_VAR_SIZE;
extern Identifier<int> MAX_AXIS;
extern Identifier<PPLDomain*> SUMMARY;
extern p::feature POLY_ANALYSIS_FEATURE;
} // namespace poly
} // namespace otawa
#endif
