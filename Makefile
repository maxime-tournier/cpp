
build=build
cmakecache=$(BUILD)/CMakeCache.txt
generator=Ninja
config=
cmakedefs=

ifeq ($(config), release)
cmakedefs += -DCMAKE_BUILD_TYPE=Release
else ifeq ($(config), debug)
cmakedefs += -DCMAKE_BUILD_TYPE=Debug -DASAN=ON -DBUILD_TESTS=ON
endif


first: all

all: compile

$(build):
	mkdir $(build)

$(cmakecache): $(build)
	cmake -S . -B $(build) -G $(generator) $(cmakedefs)

cmake: $(cmakecache)

compile: $(cmakecache)
	cmake --build $(build)

test: compile
	cd $(build) && ctest

distclean:
	rm -rf $(build)

