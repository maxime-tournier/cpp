CURRENT=.

BUILD?=build
CMAKE_BUILD_TYPE?=

first: compile

$(BUILD):
	cmake -S $(CURRENT) -B $(BUILD) -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)

compile: $(BUILD) FORCE
	cmake --build $(BUILD)

distclean: FORCE
	rm -rf $(BUILD)

FORCE:

