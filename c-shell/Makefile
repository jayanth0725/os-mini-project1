# Use gcc as the C compiler.
CC := gcc

# Compiler flags for strict C23 POSIX compliance, warnings, and include directory mapping.
CFLAGS := -std=c23 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
          -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm \
          -Iinclude

# Linker flags.
LDFLAGS :=

# Define directories for source code and compiled object files.
SRC_DIR := src
BUILD_DIR := build
TARGET := shell.out

# Find all .c files in the source directory.
SRCS := $(wildcard $(SRC_DIR)/*.c)

# Map the found .c source files to corresponding .o object files inside the build directory.
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Declare 'all' and 'clean' as phony targets so Make doesn't confuse them with actual files.
.PHONY: all clean

# The default target invoked when running 'make'.
all: $(TARGET)

# Links all the compiled object files together to generate the target executable.
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Pattern rule to compile each .c source file into a .o object file. 
# The '| $(BUILD_DIR)' ensures the build directory exists before compiling.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Creates the build directory if it does not already exist.
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# On 'make clean', deletes the entire build directory and the target executable.
clean:
	rm -rf $(BUILD_DIR) $(TARGET)