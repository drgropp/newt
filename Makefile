CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic

ifeq ($(OS),Windows_NT)
TARGET := newt.exe
else
TARGET := newt
endif

.PHONY: all test

all: $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c

test: $(TARGET)
ifeq ($(OS),Windows_NT)
	cmd /c test_newt.bat
else
	./$(TARGET) examples/args.nt hello world
	./$(TARGET) examples/read_file.nt
	./$(TARGET) examples/write_file.nt
	./$(TARGET) examples/append_file.nt
	./$(TARGET) examples/ghostlog.nt
endif
