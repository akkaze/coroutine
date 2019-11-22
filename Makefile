ifndef CXX
CXX = g++
endif

INCPATH = -I./include
CFLAGS = -std=c++11 -fPIC -O3 -finline-functions
LDFLAGS = -pthread
all: example
% : %.cc
	$(CXX) $(CFLAGS) -MM -MT $* $< >$*.d
	$(CXX) $(CFLAGS) -o $@ $(filter %.cc, $^) $(LDFLAGS)

