CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O3 -Iinclude -D_FILE_OFFSET_BITS=64
LDFLAGS = -lpthread

TARGET = MI
SRC_DIR = .
LIB_DIR = lib
UTILS_DIR = lib/utils
MISC_DIR = lib/misc
OBJ_DIR = build

SOURCES = main.cpp \
          $(LIB_DIR)/persistence.cpp \
          $(LIB_DIR)/persistence_fallback.cpp \
          $(LIB_DIR)/iso_burner.cpp \
          $(LIB_DIR)/iso_analyzer.cpp \
          $(LIB_DIR)/smart_burner.cpp \
          $(LIB_DIR)/dev_handler.cpp \
          $(LIB_DIR)/errors.cpp \
          $(LIB_DIR)/fs_supports.cpp \
          $(LIB_DIR)/fs_creator.cpp \
          $(LIB_DIR)/mbr_gpt.cpp \
          $(LIB_DIR)/bootloader.cpp \
          $(UTILS_DIR)/colors.cpp \
          $(UTILS_DIR)/logs.cpp \
          $(UTILS_DIR)/progress_bar.cpp \
          $(MISC_DIR)/version.cpp

OBJECTS = $(patsubst %.cpp,$(OBJ_DIR)/%.o,$(SOURCES))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Linking $@..."
	@$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	@echo "Cleaning build files..."
	@rm -rf $(OBJ_DIR) $(TARGET)
	@echo "Clean complete"

install: $(TARGET)
	@echo "Installing $(TARGET) to /usr/local/bin..."
	@install -m 755 $(TARGET) /usr/local/bin/
	@echo "Installation complete"

uninstall:
	@echo "Uninstalling $(TARGET)..."
	@rm -f /usr/local/bin/$(TARGET)
	@echo "Uninstallation complete"

.PHONY: all clean install uninstall
