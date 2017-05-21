
# CXX=clang++
# CXX=c++

INCLUDEPATH=$(shell pkg-config --cflags eigen3)
CXXFLAGS=-std=c++11 $(INCLUDEPATH) -Wall -g # -fuse-ld=gold #-Xlinker -icf=all

ALL=lisp rtti graph flow pcg sparse
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

lisp: LDLIBS += -lreadline

pouf:
	$(MAKE) -C $@


FORCE:
