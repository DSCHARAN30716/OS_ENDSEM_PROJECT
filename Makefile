CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude

TARGET = sigreactor

SRC = src/main.c src/sigreactor.c

all: $(TARGET)

$(TARGET): $(SRC) include/sigreactor.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
