CC = gcc
CFLAGS = -std=c23 \
		 -D_POSIX_C_SOURCE=200809L \
		 -D_XOPEN_SOURCE=700 \
		 -Wall -Wextra -Werror \
		 -Wno-unused-parameter \
		 -fno-asm \
		 -Iinclude

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = shell.out

all: $(TARGET)

$(TARGET): $(OBJS)
		$(CC) $(CFLAGS) -o $@ $^

src/%.o: src/%.c
		$(CC) $(CFLAGS) -c $< -o $@

clean:
		rm -f $(OBJS) $(TARGET)

.PHONY: all clean
