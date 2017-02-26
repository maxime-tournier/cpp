

INCLUDEPATH=$(shell pkg-config --cflags eigen3)
CXXFLAGS=-std=c++11 $(INCLUDEPATH) -g

ALL=parse rtti graph flow sofa

first: all

all: $(ALL)


clean:
	rm $(ALL)


