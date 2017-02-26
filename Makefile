

INCLUDEPATH=$(shell pkg-config --cflags eigen3)
CXXFLAGS=-std=c++11 $(INCLUDEPATH) -O3 -DNDEBUG # -g # -fuse-ld=gold #-Xlinker -icf=all

ALL=parse rtti graph flow pouf


first: all

all: $(ALL)


clean:
	rm $(ALL)


