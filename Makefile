

CXXFLAGS=-std=c++11

SOURCES=$(wildcard *.cpp)


first: all

all:

bin:
	mkdir -p bin

bin/%: %.cpp %.hpp bin 
	$(CXX) $(CXXFLAGS) $< -o bin/$*

clean:
	rm -r bin/*

