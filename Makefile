# OCFX - Optimal Computing Framework X
# Makefile

# Compiler and flags
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c11 -O3 -fPIC
CFLAGS += -Iinclude
CFLAGS += $(shell pkg-config --cflags wayland-client wayland-egl xkbcommon egl glesv2 freetype2)

# Libraries
LIBS = $(shell pkg-config --libs wayland-client wayland-egl xkbcommon egl glesv2 freetype2)

# Directories
SRC_DIR = src
BUILD_DIR = build
LIB_DIR = lib
INCLUDE_DIR = include
EXAMPLES_DIR = examples
PROTOCOL_DIR = protocols

# Wayland protocols
WAYLAND_PROTOCOLS_DIR = /usr/share/wayland-protocols
XDG_SHELL_XML = $(WAYLAND_PROTOCOLS_DIR)/stable/xdg-shell/xdg-shell.xml
XDG_SHELL_PROTOCOL = $(PROTOCOL_DIR)/xdg-shell-protocol

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
PROTOCOL_OBJECTS = $(BUILD_DIR)/xdg-shell-protocol.o

# Target library
TARGET_STATIC = $(LIB_DIR)/libocfx.a
TARGET_SHARED = $(LIB_DIR)/libocfx.so

# Default target
.PHONY: all
all: static

# Static library (recommended for suckless builds)
.PHONY: static
static: $(TARGET_STATIC)

# Shared library (optional)
.PHONY: shared
shared: $(TARGET_SHARED)

# Create directories
$(BUILD_DIR) $(LIB_DIR) $(PROTOCOL_DIR):
	mkdir -p $@

# Generate Wayland protocol code
$(XDG_SHELL_PROTOCOL).c $(XDG_SHELL_PROTOCOL).h: $(XDG_SHELL_XML) | $(PROTOCOL_DIR)
	wayland-scanner private-code $< $(XDG_SHELL_PROTOCOL).c
	wayland-scanner client-header $< $(XDG_SHELL_PROTOCOL).h

# Compile protocol object
$(BUILD_DIR)/xdg-shell-protocol.o: $(XDG_SHELL_PROTOCOL).c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(PROTOCOL_DIR) -c $< -o $@

# Compile source files (depend on protocol headers)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(XDG_SHELL_PROTOCOL).h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(PROTOCOL_DIR) -c $< -o $@

# Build static library
$(TARGET_STATIC): $(OBJECTS) $(PROTOCOL_OBJECTS) | $(LIB_DIR)
	$(AR) rcs $@ $^
	@echo "Built static library: $(TARGET_STATIC)"
	@size $@

# Build shared library
$(TARGET_SHARED): $(OBJECTS) | $(LIB_DIR)
	$(CC) -shared $^ $(LIBS) -o $@
	@echo "Built shared library: $(TARGET_SHARED)"
	@ls -lh $@

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR)
	rm -f $(PROTOCOL_DIR)/xdg-shell-protocol.*
	@echo "Clean complete"

# Install library and headers
.PHONY: install
install: static
	install -d /usr/local/lib
	install -d /usr/local/include/ocfx
	install -m 644 $(TARGET_STATIC) /usr/local/lib/
	install -m 644 $(INCLUDE_DIR)/ocfx/*.h /usr/local/include/ocfx/
	@echo "Installed to /usr/local"

# Uninstall
.PHONY: uninstall
uninstall:
	rm -f /usr/local/lib/libocfx.a
	rm -f /usr/local/lib/libocfx.so
	rm -rf /usr/local/include/ocfx
	@echo "Uninstalled"

# Build examples (TODO)
.PHONY: examples
examples: static
	@echo "Examples not yet implemented"

# Check dependencies
.PHONY: check-deps
check-deps:
	@echo "Checking dependencies..."
	@pkg-config --exists wayland-client || echo "Missing: wayland-client"
	@pkg-config --exists wayland-egl || echo "Missing: wayland-egl"
	@pkg-config --exists xkbcommon || echo "Missing: xkbcommon"
	@pkg-config --exists egl || echo "Missing: egl"
	@pkg-config --exists glesv2 || echo "Missing: glesv2"
	@pkg-config --exists freetype2 || echo "Missing: freetype2"
	@echo "Dependency check complete"

# Help
.PHONY: help
help:
	@echo "OCFX Makefile targets:"
	@echo "  all        - Build static library (default)"
	@echo "  static     - Build static library (libocfx.a)"
	@echo "  shared     - Build shared library (libocfx.so)"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install library and headers to /usr/local"
	@echo "  uninstall  - Remove installed files"
	@echo "  examples   - Build example programs"
	@echo "  check-deps - Verify all dependencies are installed"
	@echo "  help       - Show this help message"
