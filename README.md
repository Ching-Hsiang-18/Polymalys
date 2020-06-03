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

* Having otawa-config in $PATH: export PATH=$PATH:/path/to/otawa/binaries/

* Having otawa lib dir in $LD_LIBRARY_PATH: export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(otawa-config --libdir)

* For testing, you will need an ARM cross-compiler

* OTAWA 2

First, install all required dependencies. On debian/ubuntu:

```
sudo apt install build-essential python2 git cmake flex bison libxml2-dev libxslt-dev ocaml gcc-arm-none-eabi
```

To download and install OTAWA v2, first download the following script
and execute it using python 2.7:

```
http://www.tracesgroup.net/otawa/packages/otawa-install.py
```
*Warning*: the script is compatible with python 2.7 (NOT python 3). If
your default python installation is python3, you need to edit the first
line of `otawa-install.py` and replace `#!/usr/bin/python` by
`#!/usr/bin/python2`

You will also need to install the OTAWA plugins for ARM, and
lp-solve. Go to `<otawa dir>/bin` (where otawa dir is the directory
where you just installed otawa) and type:

```
python otawa-install.py otawa-arm otawa-lp_solve5
```

*Warning*: to install plugins, do not use `otawa-install.py` in
`<otawa_dir>`.


## Setting up the environment:

Modify your environment variables as follows:

add `<otawa dir>/bin` to variable `PATH`

add `<otawa dir>/lib:<otawa dir>:/lib/otawa/otawa` to variable `LD_LIBRARY_PATH`


## Compiling: 

```
make
```

# Installing (in home directory, no root required):

```
make install
```

## Automated tests

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
./poly <binary> # where <binary> is an ARM binary program
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
