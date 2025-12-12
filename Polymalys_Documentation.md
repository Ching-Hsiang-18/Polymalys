# Polymalys Documentation

## Overview

**Polymalys** (Polyhedra-based Memory Analysis) is a static analysis tool for binary code that discovers linear relations between data locations (memory locations and registers). The tool leverages abstract interpretation using a polyhedral abstract domain to compute upper bounds on loop iterations, which is essential for Worst-Case Execution Time (WCET) analysis in safety-critical embedded systems.

- **Version**: 1.1 (December 2023)
- **License**: GNU Lesser General Public License v2.1 or higher
- **Target Architecture**: ARM (binary analysis)
- **Framework**: Built as a plugin for OTAWA 2

### Authors

- Clément Ballabriga (Université de Lille)
- Julien Forget (Université de Lille)
- Sandro Grebant (Université de Lille)
- Giuseppe Lipari (Université de Lille)

### Academic Reference

> Ballabriga, C., Forget, J., Gonnord, L., Lipari, G.  and Ruiz, J., 2019. *Static Analysis Of Binary Code With Memory Indirections Using Polyhedra*. In International Conference on Verification, Model Checking, and Abstract Interpretation (pp. 114-135).

---

## Core Concepts

### Polyhedral Abstract Domain

Polymalys uses the **Parma Polyhedra Library (PPL)** to represent program states as convex polyhedra. Each polyhedron captures linear constraints between variables:

```
Example: For a loop with counter i and bound n
Polyhedron constraints:  { i ≥ 0, i < n, i + bound = 42 }
```

This representation enables: 
- **Precise tracking** of linear relationships between variables
- **Automatic inference** of loop invariants
- **Sound over-approximation** of program behavior

### Loop Bound Analysis

The primary goal of Polymalys is to compute **maximum loop iteration counts**.  The analysis works by:

1. **Creating virtual loop counter variables** (`ID_LOOP`) at loop entry
2. **Incrementing counters** at each back-edge traversal
3. **Applying branch conditions** as polyhedron constraints
4. **Extracting upper bounds** via polyhedral projection

---

## Analysis Capabilities

### What Polymalys Can Analyze Successfully

#### 1. Constant Bound Loops
```c
for (int i = 0; i < 10; i++) {
    // Result: MAX_ITERATION = 10
}
```

The analyzer creates constraints `{ bound ≥ 0, bound ≤ 9 }` and extracts the upper bound directly. 

#### 2. Loops with Linear Intra-loop Modifications
```c
int bound = 42;
for (int i = 0; i < bound; i++) {
    bound--;
    // Result: MAX_ITERATION = 21
}
```

Polymalys tracks the invariant `i + bound = 42` and derives:
- From `i < bound` and `i + bound = 42`: `2i < 42` → `i ≤ 20`
- Maximum iterations = 21

#### 3. Constant Global Variables
```c
const int LIMIT = 100;
for (int i = 0; i < LIMIT; i++) {
    // Result: MAX_ITERATION = 100
}
```

#### 4. Parameter-Dependent Loops (with Parametric Bounds)
```c
void process(int n) {
    for (int i = 0; i < n; i++) {
        // Result:  Parametric bound expressed as function of n
    }
}
```

When `LINBOUND` is enabled, Polymalys can export linear expressions like `b: 0` (representing parameter 0) as symbolic bounds.

#### 5. Nested Loops
```c
for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 20; j++) {
        // Analyzes both loops independently
    }
}
```

#### 6. Memory Access Patterns
Polymalys tracks:
- Stack variable accesses relative to stack pointer (SP)
- Global variable accesses at constant addresses
- Array accesses with linear index expressions

---

## Special Return Values

### Loop Bound Types

The `bound_t` enumeration defines special values for loop analysis results:

```cpp
enum bound_t :  signed long {
    UNREACHABLE = -1,  // Loop is never reached
    UNBOUNDED = -2,    // Cannot determine finite upper bound
};
```

### UNREACHABLE (-1)

**Meaning**: The abstract state at the loop header is *bottom* (empty set), indicating the loop can never be executed.

**Trigger Conditions**:
```cpp
bound_t PPLDomain::getLoopBound(int loopId) const {
    if (isBottom()) {
        return bound_t::UNREACHABLE;
    }
    // ... 
}
```

**Common Causes**:
- Contradictory path conditions leading to the loop
- Dead code elimination scenarios
- Unreachable branches

**Example**:
```c
void example(int x) {
    if (x > 10 && x < 5) {  // Always false
        for (int i = 0; i < 100; i++) {
            // UNREACHABLE:  This loop never executes
        }
    }
}
```

### UNBOUNDED (-2)

**Meaning**: The polyhedral analysis cannot determine a finite upper bound for the loop counter variable.

**Trigger Conditions**:
```cpp
bound_t PPLDomain::getLoopBound(int loopId) const {
    // ...
    getRange(id, binf_n, binf_d, bsup_n, bsup_d);
    if (PPL::raw_value(bsup_d).get_ui() != 0) {
        return static_cast<bound_t>(... );  // Finite bound
    }
    return bound_t::UNBOUNDED;  // Denominator is 0 → infinity
}
```

**Common Causes**:
- Unconstrained external parameters
- Non-linear loop conditions
- Infinite loops (`while(1)`)
- Complex control flow that loses precision during widening

**Examples**:
```c
// Case 1: Unconstrained parameter
void func(int n) {
    for (int i = 0; i < n; i++) {
        // UNBOUNDED: n has no constraints
    }
}

// Case 2: True infinite loop
while (1) {
    // UNBOUNDED: No exit condition
}

// Case 3: Non-linear iteration
for (int i = 1; i < n; i *= 2) {
    // UNBOUNDED: i *= 2 is non-linear
}
```

---

## Analysis Pipeline

### Processing Flow

```
┌──────────────────────────────────────────────────────────────┐
│                    Binary Input (ARM ELF)                    │
└──────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────────────┐
│                    OTAWA Framework                           │
│  ┌─────────────┐  ┌──────────────┐  ┌─────────────────────┐  │
│  │ CFG Builder │→ │Loop Detection│→ │  Dominance Analysis │  │
│  └─────────────┘  └──────────────┘  └─────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                  Polymalys Analysis                         │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Abstract Interpretation                 │   │
│  │  ┌────────────┐  ┌────────────┐  ┌────────────────┐  │   │
│  │  │ onLoopEntry│→ │ onLoopIter │→ │ onBranch       │  │   │
│  │  │ (init=0)   │  │ (counter++)│  │ (constraints)  │  │   │
│  │  └────────────┘  └────────────┘  └────────────────┘  │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Widening & Fixed Point                  │   │
│  └──────────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              Bound Extraction (getRange)             │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                    Output                                   │
│  • MAX_ITERATION per loop                                   │
│  • Parametric bounds (if LINBOUND enabled)                  │
│  • Memory state analysis                                    │
│  • loop_bounds.csv export                                   │
└─────────────────────────────────────────────────────────────┘
```

### Key Analysis Functions

| Function | Purpose |
|----------|---------|
| `onLoopEntry(loop)` | Initialize loop counter to 0 |
| `onLoopIter(loop)` | Increment loop counter by 1 |
| `onBranch(taken, block)` | Apply branch condition constraints |
| `onMerge(state, widen)` | Join/widen states at control flow merge points |
| `getLoopBound(loopId)` | Extract upper bound from polyhedron |
| `getRange(var, ...)` | Get min/max values for a variable |

---

## Architecture

### Source Code Structure

```
polymalys/
├── include/
│   ├── PPLDomain.h          # Core polyhedral domain class
│   ├── LoopAnalyzer.h       # Loop bound computation
│   ├── PolyAnalysis.h       # Main analysis processor
│   ├── PolyWrap.h           # PPL wrapper utilities
│   └── BranchConditionner.h # Branch condition handling
├── poly_PPLDomain.cpp       # Domain operations (148KB, main implementation)
├── poly_PolyAnalysis.cpp    # Analysis entry point
├── poly_LoopAnalyzer.cpp    # Linear bound extraction
├── poly_PPLManager.cpp      # PPL interface management
├── poly_PolyWrap.cpp        # Wrapper implementations
├── poly_BranchConditionner.cpp
├── poly_PlugHook.cpp        # OTAWA plugin registration
├── otawa/                   # Bundled OTAWA 2 framework
├── tests/                   # Test cases
└── Makefile
```

### Key Classes

#### `PPLDomain`
The central class representing an abstract state as a polyhedron with variable mappings.

**Key Members**:
```cpp
class PPLDomain {
    WPoly poly;                    // PPL polyhedron
    Mapping idmap;                 // Identifier → Variable mapping
    Vector<bound_t> bounds;        // Cached loop bounds
    sem:: cond_t compare_op;        // Pending branch condition
    Ident compare_reg;             // Register holding comparison result
    // ... 
};
```

**Key Operations**:
- `isBottom()`: Check if state is empty
- `onSemInst(...)`: Process semantic instruction
- `onMerge(...)`: Join/widen two states
- `getLoopBound(...)`: Extract loop bound

#### `Ident`
Identifies program locations (registers, memory, loop counters).

```cpp
enum IdentType {
    ID_REG,           // Hardware register
    ID_REG_INPUT,     // Input register (for summaries)
    ID_MEM_ADDR,      // Memory address variable
    ID_MEM_VAL,       // Memory value variable
    ID_MEM_COUNT,     // Array element count
    ID_LOOP,          // Loop counter
    ID_SPECIAL,       // SP, FP, LR markers
    // ...
};
```

#### `LoopBound`
Represents computed loop bounds (static or parametric).

```cpp
class LoopBound {
    bool linear;           // true = parametric, false = constant
    int staticBound;       // Constant bound value
    std::string linearBound;  // Parametric expression (e.g., "b:0 - 1")
};
```

---

## Installation

### Prerequisites

```bash
# Debian/Ubuntu
sudo apt install build-essential python2 git cmake flex bison \
                 libxml2-dev libxslt-dev ocaml gcc-arm-none-eabi

# Parma Polyhedra Library
# Install from:  http://bugseng.com/products/ppl/
```

### Building

```bash
# Clone repository
git clone <repository-url>
cd polymalys

# Build
make

# Install (to home directory, no root required)
make install

# Run tests (optional)
make test
```

---

## Usage

### Analyzing C Source Files

```bash
cd tests
./do. sh <filename>    # Analyzes <filename>. c
```

This script:
1. Cross-compiles the C file to ARM binary
2. Runs Polymalys analysis
3. Outputs loop bounds and memory state

### Analyzing Pre-compiled ARM Binaries

```bash
cd tests
./poly. sh <binary> [<function_name>]
# Default function: main
```

### Programmatic Usage

```cpp
// See tests/poly. cpp for API usage example
#include "PPLDomain.h"
#include "PolyAnalysis. h"

// Register the analysis feature
p:: feature POLY_ANALYSIS_FEATURE("otawa:: poly:: POLY_ANALYSIS_FEATURE", 
                                  new Maker<PolyAnalysis>());
```

### Output Files

- **Console**:  Real-time analysis progress and final state
- **`loop_bounds.csv`**: Exported loop bounds in CSV format
  ```
  Loop id;isLinear;bound
  2;false;21
  5;true;b:0 - 1
  ```

---

## Limitations

### Current Limitations

| Limitation | Description |
|------------|-------------|
| **32-bit Memory Model** | Assumes 32-bit aligned memory accesses |
| **No Integer Overflow** | Does not model integer wrap-around behavior |
| **Equality Conditions** | Loop conditions with `==` may produce pessimistic bounds |
| **Single Exit Bounds** | Loops with multiple bounds may not be fully supported |
| **Intra-procedural** | Inter-procedural analysis requires `INTER_PROCEDURAL` flag |
| **ARM Only** | Currently targets ARM architecture |

### Known Issues (from README)

> - Loops conditions with equality tests (instead of inequality) currently have pessimistic bounds
> - We currently assume that each memory access has 32bits-size, and is 32bits-aligned
> - Lots of speed optimization needs to be done

### What Cannot Be Analyzed

1. **Non-linear arithmetic**:  `i *= 2`, `i = i * i`
2. **Floating-point operations**: Not supported in polyhedral domain
3. **Dynamic memory allocation**: `malloc`/`free` not tracked
4. **Function pointers**:  Indirect calls not resolved
5. **Inline assembly**:  Not interpreted
6. **Concurrency**: Single-threaded analysis only

---

## Configuration Options

### Compile-time Flags

| Flag | Description |
|------|-------------|
| `LINBOUND` | Enable parametric (linear) bound export |
| `INTER_PROCEDURAL` | Enable inter-procedural analysis with function summaries |
| `POLY_DEBUG` | Enable verbose debug output |
| `CheckWritingArea` | Check memory write safety |
| `CheckReturnAddress` | Detect return address overwrites |

### Runtime Identifiers

```cpp
Identifier<int> LOC_VAR_SIZE("otawa::poly::LOC_VAR_SIZE", 4);
Identifier<int> NUM_LOC_VARS("otawa:: poly::NUM_LOC_VARS", 8);
Identifier<int> MAX_AXIS("otawa::poly::MAX_AXIS", 512);
```

---

## Example Analysis

### Input:  `global2.c`
```c
int bound = 42;
int main(int cond) {
  int i;
  for (i = 0; i < bound; i++) {
    bound--;
  }  
}
```

### Analysis Output (Excerpt)
```
Processing basic block:  BB 2 (00008280) spaceDimension=21, numCons=18
Processing basic block: BB 3 (00008260) spaceDimension=25, numCons=22
... 
LOOP BOUNDS:  
[main]MAX_ITERATION(2) = 21

FINAL STATE:  
█count10 = 1; █ptr10 = 100028; █*ptr10 = 21; ... 

Global variables:  
 @addr 0x186bc, value = [21;21] (aka *ptr10)
```

### Interpretation

- **Loop at BB 2**:  Executes exactly **21 times**
- **Invariant detected**: `i + bound = 42`
- **Final bound value**: 21 (stored at global address 0x186bc)

---

## Future Development

According to the project roadmap: 

1. **Extended Linear Relations**: Use discovered relations for: 
   - Out-of-stack access detection
   - Dead-code detection
   
2. **Inter-procedural Analysis**: Full function summary support

3. **Integer Overflow Handling**: Model wrap-around semantics

4. **Improved Loop Conditions**: Better handling of equality tests

5. **Performance Optimization**: Reduce analysis time for large programs

---

## Troubleshooting

### Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| `UNBOUNDED (-2)` | Unconstrained loop bound | Add manual annotations or simplify loop |
| `UNREACHABLE (-1)` | Dead code path | Check control flow logic |
| Missing bounds | Loop not detected | Verify CFG construction |
| Slow analysis | Large polyhedra | Reduce variable count or enable aggressive widening |

### Debug Output

Enable `POLY_DEBUG` to see detailed analysis steps:
```cpp
#define POLY_DEBUG
```

This outputs: 
- Constraint additions
- Variable mappings
- Merge/widening operations
- Branch filtering results

---

## References

1.  Ballabriga et al. (2019). *Static Analysis Of Binary Code With Memory Indirections Using Polyhedra*.  VMCAI 2019.

2. Cousot, P., & Halbwachs, N. (1978). *Automatic discovery of linear restraints among variables of a program*.  POPL 1978.

3.  OTAWA Framework:  https://www.irit.fr/TRACES/site/tools/otawa-2/

4. Parma Polyhedra Library: http://bugseng.com/products/ppl/

---

## License

GNU Lesser General Public License v2.1 or higher. 

See `COPYING` file for full license text. 