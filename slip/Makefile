
BUILD_PATH=build
TARGET=slip

first: all



$(BUILD_PATH):
	meson $(BUILD_PATH)

$(BUILD_PATH)/$(TARGET): $(BUILD_PATH)
	cd $(BUILD_PATH); ninja

target: $(BUILD_PATH)/$(TARGET)

all: target

clean: $(BUILD_PATH)
	cd $(BUILD_PATH); ninja clean


