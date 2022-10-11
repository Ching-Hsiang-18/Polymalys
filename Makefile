CXXFLAGS=`otawa/bin/otawa-config otawa/oslice --cflags`
LIBS=`otawa/bin/otawa-config otawa/oslice --libs`
LIBS+=-lppl

CC=gcc
CXX=g++


CXXFLAGS+=-fPIC -Wall -DUSE_CLANG_COMPLETER -std=c++14 -O3 -march=native
#CXXFLAGS+=-fPIC -Wall -DUSE_CLANG_COMPLETER -std=c++14 -O0 -g

all: poly.so

dbg: CXXFLAGS += -DELM_LOG
dbg: poly.so

debug: CXXFLAGS += -DPOLY_DEBUG
debug: poly.so

poly.so: poly_PolyAnalysis.o poly_PlugHook.o poly_PPLDomain.o poly_PPLManager.o poly_PolyWrap.o
	$(CC) -shared -o poly.so poly_PolyAnalysis.o poly_PlugHook.o poly_PPLDomain.o poly_PPLManager.o poly_PolyWrap.o $(LIBS)
	

poly_PolyAnalysis.o: poly_PolyAnalysis.cpp include/PolyAnalysis.h include/PPLDomain.h include/PolyWrap.h include/PPLManager.h include/PolyCommon.h include/MyHTable.h include/OrderedDriver.h
	$(CXX) $(CXXFLAGS) -c poly_PolyAnalysis.cpp -o poly_PolyAnalysis.o

poly_PPLManager.o: poly_PPLManager.cpp include/PolyAnalysis.h include/PPLDomain.h include/PolyWrap.h include/PPLManager.h include/PolyCommon.h include/MyHTable.h include/OrderedDriver.h
	$(CXX) $(CXXFLAGS) -c poly_PPLManager.cpp -o poly_PPLManager.o

poly_PPLDomain.o: poly_PPLDomain.cpp include/PolyAnalysis.h include/PPLDomain.h include/PolyWrap.h include/PPLManager.h include/PolyCommon.h include/MyHTable.h include/OrderedDriver.h
	$(CXX) $(CXXFLAGS) -c poly_PPLDomain.cpp -o poly_PPLDomain.o

poly_PlugHook.o: poly_PlugHook.cpp
	$(CXX) $(CXXFLAGS) -c poly_PlugHook.cpp -o poly_PlugHook.o

poly_PolyWrap.o: poly_PolyWrap.cpp include/PolyWrap.h
	$(CXX) $(CXXFLAGS) -c poly_PolyWrap.cpp -o poly_PolyWrap.o
	

clean:
	rm -f *~ core* poly.so *.o
	rm -rf doc
	make -C tests clean

install: poly.so
	mkdir -p $(HOME)/.otawa/proc/otawa
	cp poly.eld $(HOME)/.otawa/proc/otawa/
	cp poly.so $(HOME)/.otawa/proc/otawa/

tabtest: install
	make -C tests tabtest

test: douter

tests: douter

douter: install
	make -C tests douter

uninstall:
	rm -f $(HOME)/.otawa/proc/otawa/poly.eld
	rm -f $(HOME)/.otawa/proc/otawa/poly.so

doc:
	doxygen

clang-tidy:
	rm -f tidy.txt
	clang-tidy -header-filter=^./include -checks='*' poly_PolyAnalysis.cpp >> tidy.txt
	clang-tidy -header-filter=^./include -checks='*' poly_PlugHook.cpp >> tidy.txt
	clang-tidy -header-filter=^./include -checks='*' poly_PPLDomain.cpp >> tidy.txt
	clang-tidy -header-filter=^./include -checks='*' poly_PPLManager.cpp >> tidy.txt
.PHONY: clean install douter test tests compildb
