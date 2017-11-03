CXXFLAGS=`otawa-config otawa/poly otawa/oslice --cflags`
LIBS=`otawa-config otawa/poly otawa/oslice --libs`

all: poly

poly: poly.cpp
	c++ -o poly poly.cpp $(CXXFLAGS) $(LIBS) -march=native -O3
	
clean:
	rm -f *~ core* target poly *.o target.ffx *.dot *.orange `cat tests.txt` tmp

douter: poly
	@./douter.sh

test: douter

tests: douter

.PHONY: clean douter test tests

