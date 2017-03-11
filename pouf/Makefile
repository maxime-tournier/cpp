
PKGCONFIG=eigen3 python3
CXXFLAGS=`pkg-config --cflags $(PKGCONFIG)`
LIBS=-lboost_python-py35
LDFLAGS=-shared -fPIC

all: main.o



pouf.so: python.cpp
	$(CXX) $< $(CXXFLAGS)  -o $@ $(LIBS) $(LDFLAGS)
