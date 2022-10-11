CXXFLAGS=`LD_LIBRARY_PATH=../otawa/lib/otawa/otawa ../otawa/bin/otawa-config otawa/poly otawa/oslice --cflags`
LIBS=`LD_LIBRARY_PATH=../otawa/lib/otawa/otawa ../otawa/bin/otawa-config otawa/poly otawa/oslice --libs`

all: poly

poly: poly.cpp
	c++ -o poly poly.cpp $(CXXFLAGS) $(LIBS) -std=c++14 -O3 -march=native
	
clean:
	rm -f *~ core* target poly *.o target.ffx *.dot *.orange `cat tests.txt` tmp

tabtest: poly
	@./tabtest.sh

douter: poly
	@./douter.sh

test: douter

tests: douter

.PHONY: clean douter test tests

