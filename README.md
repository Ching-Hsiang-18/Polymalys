# Polymalys

Polymalys: Polyhedra-based Memory Analysis.

* Version XX for Otawa XX

----
## Author

Clement Ballabriga <Clement.Ballabriga@nospamuniv-lille.fr>

License: GPL 2.1 or higher.

Please remove "nospam" from the email address.

----

## Introduction

Polymalys is a tool for static analysis of *binary* code. It discovers
linear relations between data locations (i.e. memory locations as well
as registers) of the code. The analysis relies on abstract
interpretation using a polyhedra-based abstract domain. The current
implementation exploits the discovered linear relations to compute upper
bounds to loop iterations. For more information on the underlying
theory, refer to [1].

## Requirements

* Parma Polyhedra Library (PPL): http://bugseng.com/products/ppl/

* For testing, you will need an ARM cross-compiler

First, install all required dependencies. On debian/ubuntu:

```
sudo apt install build-essential python2 git cmake flex bison libxml2-dev libxslt-dev ocaml gcc-arm-none-eabi
```

To prevent compatibility issues, Polymalys is linked to a specific
version of OTAWA 2, which is bundled in the Polymalys directory.

## Compiling: 

```
make
```

# Installing (in home directory, no root required):

```
make install
```

## Automated tests (optionnal)

```
make test
```

## Testing with a source code target program: 

```
cd tests
./do.sh <filename> # where <filename>.c is a C program
```

# Testing with a binary target program:

```
cd tests
./poly.sh <binary> [<optional function name, defaults to main>] # where <binary> is an ARM binary program
```

## Using in your program:

Read `tests/poly.cpp`


## Future works

 * Use discovered linear relations for other static analyses:
   out-of-stack accesses detection, dead-code detection, ...
 * Inter-procedural analysis
 * Support for integer wrap/overflow
 * Loops conditions with equality tests (instead of inequality)
   currently have pessimistic bounds
 * We currently assume that each memory access has 32bits-size, and is 32bits-aligned
 * Lots of speed optimization needs to be done

----
## References

[1] Ballabriga, C., Forget, J., Gonnord, L., Lipari, G. and Ruiz, J.,
2019, January. Static Analysis Of Binary Code With Memory Indirections
Using Polyhedra. In International Conference on Verification, Model
Checking, and Abstract Interpretation (pp. 114-135).
