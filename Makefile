
# CXX=clang++
# CXX=/usr/bin/g++-6

INCLUDEPATH=$(shell pkg-config --cflags eigen3)
CXXFLAGS=-std=c++11 $(INCLUDEPATH) -Wall -g # -fuse-ld=gold #-Xlinker -icf=all

ALL=rtti graph flow pcg sparse gc
SUB=pouf lisp pybind

LDLIBS += -lstdc++ -lm

first: all

all: $(ALL) $(SUB)

clean:
	rm -f $(ALL) *.o



debug: CXXFLAGS += -g
debug: all

release: CXXFLAGS += -O3 -DNDEBUG
release: all

pouf: FORCE
	ninja -C $@/build/

lisp: FORCE
	ninja -C $@/build/

pybind: FORCE
	ninja -C $@/build/


FORCE:

