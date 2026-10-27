CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic
SOURCES := src/main.c src/lexer.c src/parser.c src/interpreter.c src/builtins.c src/numeric.c
HEADERS := $(wildcard src/*.h)

ifeq ($(OS),Windows_NT)
TARGET := newt.exe
else
TARGET := newt
endif

.PHONY: all test

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) -lm

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

