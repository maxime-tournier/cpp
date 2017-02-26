

INCLUDEPATH=-I/usr/include/eigen3
CXXFLAGS=-std=c++11 $(INCLUDEPATH) -g

ALL=parse graph flow sofa

first: all

all: $(ALL)


clean:
	rm $(ALL)


