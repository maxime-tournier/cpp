

INCLUDEPATH=$(shell pkg-config --cflags eigen3)
CXXFLAGS=-std=c++11 $(INCLUDEPATH) # -g # -fuse-ld=gold #-Xlinker -icf=all

ALL=parse rtti graph flow pouf


first: release

all: $(ALL) 


clean:
	rm $(ALL)


debug: CXXFLAGS += -g
debug: all

release: CXXFLAGS += -O3 -DNDEBUG
release: all
