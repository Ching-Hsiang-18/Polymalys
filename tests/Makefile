CXXFLAGS=`otawa-config otawa/poly --cflags`
LIBS=`otawa-config otawa/poly --libs`

all: poly

poly: poly.cpp
	c++ -o poly poly.cpp $(CXXFLAGS) $(LIBS) -g -O0
	
clean:
	rm -f *~ core* cible poly *.o cible.ffx *.dot *.orange `cat tests.txt` tmp

douter: poly
	@./douter.sh

test: douter

tests: douter

.PHONY: clean douter test tests

