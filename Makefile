
BUILD=build
CMAKECACHE=$(BUILD)/CMakeCache.txt
GENERATOR=Ninja

first: all

all: compile

$(BUILD):
	mkdir $(BUILD)
	cmake -S . -B $(BUILD) -G $(GENERATOR)

cmake: $(BUILD)

$(CMAKECACHE): cmake

compile: $(CMAKECACHE)
	cmake --build $(BUILD)

test: compile
	cd $(BUILD) && ctest

distclean:
	rm -rf $(BUILD)

