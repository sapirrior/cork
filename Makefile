CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
SRC = src/main.c src/utils.c src/lexer.c src/expander.c src/parser.c src/runner.c
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
OBJ = $(patsubst src/%.c, $(OBJ_DIR)/%.o, $(SRC))
TARGET = $(BUILD_DIR)/cork

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

$(OBJ_DIR)/%.o: src/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
