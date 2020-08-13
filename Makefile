
BUILD=build
CMAKECACHE=$(BUILD)/CMakeCache.txt
GENERATOR=Ninja
CONFIG=
CMAKE_DEFINITIONS=

ifeq ($(CONFIG), release)
CMAKE_DEFINITIONS+= -DCMAKE_BUILD_TYPE=Release
else ifeq ($(CONFIG), debug)
CMAKE_DEFINITIONS+= -DCMAKE_BUILD_TYPE=Debug -DASAN=ON
endif


first: all

all: compile

$(BUILD):
	mkdir $(BUILD)
	cmake -S . -B $(BUILD) -G $(GENERATOR) $(CMAKE_DEFINITIONS)

cmake: $(BUILD)

$(CMAKECACHE): cmake

compile: $(CMAKECACHE)
	cmake --build $(BUILD)

test: compile
	cd $(BUILD) && ctest

distclean:
	rm -rf $(BUILD)

