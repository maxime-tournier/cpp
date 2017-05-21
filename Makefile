
# CXX=clang++
# CXX=c++

INCLUDEPATH=$(shell pkg-config --cflags eigen3)
CXXFLAGS=-std=c++11 $(INCLUDEPATH) -g # -fuse-ld=gold #-Xlinker -icf=all

ALL=parse rtti graph flow pcg sparse
SUB=pouf

LDLIBS += -lstdc++ -lm

first: all

all: $(ALL) $(SUB)


clean:
	rm -f $(ALL) *.o



debug: CXXFLAGS += -g
debug: all

release: CXXFLAGS += -O3 -DNDEBUG
release: all


parse: LDLIBS += -lreadline

pouf:
	$(MAKE) -C $@


FORCE:
