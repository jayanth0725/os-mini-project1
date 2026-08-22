#  Compiles with all the POSIX-compliant flags
CC = gcc
CFLAGS = -std=c23 \
		 -D_POSIX_C_SOURCE=200809L \
		 -D_XOPEN_SOURCE=700 \
		 -Wall -Wextra -Werror \
		 -Wno-unused-parameter \
		 -fno-asm \
		 -Iinclude

# Includes all the .c files in /src and names the compiled executable as shell.out
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = shell.out

# The default target invoked when simply running 'make'. It builds the shell executable.
all: $(TARGET)

# Links all the compiled object files together to generate the target executable.
$(TARGET): $(OBJS)
		$(CC) $(CFLAGS) -o $@ $^

# Pattern rule to compile each .c source file into a .o object file.
src/%.o: src/%.c
		$(CC) $(CFLAGS) -c $< -o $@

# On 'make clean', deletes all object files and the target shell.out executable
clean:
		rm -f $(OBJS) $(TARGET)

# Declares 'all' and 'clean' as phony targets so Make doesn't confuse them with files of the same name.
.PHONY: all clean
