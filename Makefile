CXXFLAGS=`otawa-config otawa/display --cflags`
LIBS=`otawa-config otawa/display --libs`

CXXFLAGS+=-fPIC

all: poly.so

poly.so: poly_PolyAnalysis.o /home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/clp.so
	$(CC) -shared -o poly.so poly_PolyAnalysis.o /home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/clp.so
	
poly_PolyAnalysis.o: poly_PolyAnalysis.cpp PolyAnalysis.h

clean:
	rm -f *~ core* poly.so *.o

douter: poly
	@./douter.sh


.PHONY: clean
