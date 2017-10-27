CC=clang
CXX=clang++

CXXFLAGS=`otawa-config --cflags`
LIBS=`otawa-config --libs`
LIBS+=-lppl

CXXFLAGS+=-fPIC -Wall -DUSE_CLANG_COMPLETER -std=c++11 -g -O0

all: poly.so

poly.so: poly_PolyAnalysis.o poly_PlugHook.o poly_PPLDomain.o poly_PPLManager.o
	$(CC) -shared -o poly.so poly_PolyAnalysis.o poly_PlugHook.o poly_PPLDomain.o poly_PPLManager.o $(LIBS)
	

poly_PolyAnalysis.o: poly_PolyAnalysis.cpp PolyAnalysis.h PPLDomain.h PPLManager.h PolyCommon.h
	$(CXX) $(CXXFLAGS) -c poly_PolyAnalysis.cpp -o poly_PolyAnalysis.o

poly_PPLManager.o: poly_PPLManager.cpp PolyAnalysis.h PPLDomain.h PPLManager.h PolyCommon.h
	$(CXX) $(CXXFLAGS) -c poly_PPLManager.cpp -o poly_PPLManager.o

poly_PPLDomain.o: poly_PPLDomain.cpp PolyAnalysis.h PPLDomain.h PPLManager.h PolyCommon.h
	$(CXX) $(CXXFLAGS) -c poly_PPLDomain.cpp -o poly_PPLDomain.o

poly_PlugHook.o: poly_PlugHook.cpp
	$(CXX) $(CXXFLAGS) -c poly_PlugHook.cpp -o poly_PlugHook.o

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

uninstall:
	rm -f $(HOME)/.otawa/proc/otawa/poly.eld
	rm -f $(HOME)/.otawa/proc/otawa/poly.so

clang-tidy:
	rm -f tidy.txt
	clang-tidy -header-filter='.*' -checks='*' poly_PolyAnalysis.cpp >> tidy.txt
	clang-tidy -header-filter='.*' -checks='*' poly_PlugHook.cpp >> tidy.txt
	clang-tidy -header-filter='.*' -checks='*' poly_PPLDomain.cpp >> tidy.txt
	clang-tidy -header-filter='.*' -checks='*' poly_PPLManager.cpp >> tidy.txt
.PHONY: clean install douter test tests compildb
