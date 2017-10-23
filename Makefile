CXXFLAGS=`otawa-config --cflags`
LIBS=`otawa-config --libs`
LIBS+=-lppl

CXXFLAGS+=-fPIC

all: poly.so

poly.so: poly_PolyAnalysis.o poly_PlugHook.o
	$(CC) -shared -o poly.so poly_PolyAnalysis.o poly_PlugHook.o $(LIBS)
	
poly_PolyAnalysis.o: poly_PolyAnalysis.cpp PolyAnalysis.h

poly_PlugHook.o: poly_PlugHook.cpp

clean:
	rm -f *~ core* poly.so *.o
	make -C tests clean

install: poly.so
	mkdir -p $(HOME)/.otawa/proc/otawa
	cp poly.eld $(HOME)/.otawa/proc/otawa/
	cp poly.so $(HOME)/.otawa/proc/otawa/

test: douter

tests: douter

douter: install
	make -C tests douter


.PHONY: clean install douter test tests
