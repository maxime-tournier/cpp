
# CXX=clang++
# CXX=/usr/bin/g++-6

INCLUDEPATH=$(shell pkg-config --cflags eigen3)
CXXFLAGS=-std=c++11 $(INCLUDEPATH) -Wall -g -O3 # -fuse-ld=gold #-Xlinker -icf=all

ALL=rtti graph flow pcg sparse gc dynamic_sized alloc stl obj nan octree

LDLIBS += -lstdc++ -lm

NINJA_BUILD=$(shell find . -name build.ninja)

first: all

all: $(ALL) ninja

clean: ninja-clean
	rm -f $(ALL) *.o

debug: CXXFLAGS += -g
debug: all

release: CXXFLAGS += -O3 -DNDEBUG
release: all

ninja: $(NINJA_BUILD)

%/build.ninja: FORCE
	ninja -C $*

FORCE:

