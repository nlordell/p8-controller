CFLAGS := -std=c11 -Wall

SRC := $(wildcard src/*.c)
INC := $(wildcard src/*.h)
OBJ := $(patsubst src/%,target/%,$(SRC:.c=.o))

.PHONY: all
all: target/pico8-controller

target/pico8-controller: $(OBJ)
	$(CC) -std=c11 -Wall -o $@ $^

target/%.o: src/%.c $(INC)
	mkdir -p target
	$(CC) -c $(CFLAGS) $< -o $@
