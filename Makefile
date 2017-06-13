
# CXX=clang++
# CXX=/usr/bin/g++-6

INCLUDEPATH=$(shell pkg-config --cflags eigen3)
CXXFLAGS=-std=c++11 $(INCLUDEPATH) -Wall -g # -fuse-ld=gold #-Xlinker -icf=all

ALL=rtti graph flow pcg sparse gc dynamic_sized

LDLIBS += -lstdc++ -lm

first: all

all: $(ALL) ninja

clean:
	rm -f $(ALL) *.o

debug: CXXFLAGS += -g
debug: all

release: CXXFLAGS += -O3 -DNDEBUG
release: all

ninja: $(shell find . -name build.ninja)

%/build.ninja: FORCE
	ninja -C $*


FORCE:

