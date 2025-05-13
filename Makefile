CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

# SDL flags - different for Linux and macOS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    SDL_FLAGS = -lSDL2
else ifeq ($(UNAME_S),Darwin)
    SDL_FLAGS = -F/Library/Frameworks -framework SDL2
else
    $(error Unsupported operating system)
endif

# Source files
SRC = controller.c list.c map.c drone.c survivor.c ai.c view.c
OBJ = $(SRC:.c=.o)

# Test source files
TEST_SRC = tests/listtest.c tests/sdltest.c
TEST_OBJ = $(TEST_SRC:.c=.o)

# Main executable
MAIN = drone_simulator

# Test executables
LIST_TEST = tests/listtest
SDL_TEST = tests/sdltest

# Default target
all: $(MAIN) $(LIST_TEST) $(SDL_TEST)

# Main program
$(MAIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(SDL_FLAGS)

# Build object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test programs
$(LIST_TEST): tests/listtest.o list.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SDL_TEST): tests/sdltest.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(SDL_FLAGS)

# Run the simulator
run: $(MAIN)
	./$(MAIN)

# Run list test
test_list: $(LIST_TEST)
	./$(LIST_TEST)

# Run SDL test
test_sdl: $(SDL_TEST)
	./$(SDL_TEST)

# Clean up
clean:
	rm -f $(MAIN) $(OBJ) $(LIST_TEST) $(SDL_TEST) tests/*.o

# Dependencies
controller.o: controller.c headers/globals.h headers/map.h headers/drone.h headers/survivor.h headers/ai.h headers/list.h headers/view.h
list.o: list.c headers/list.h
map.o: map.c headers/map.h headers/list.h
drone.o: drone.c headers/drone.h headers/globals.h
survivor.o: survivor.c headers/survivor.h headers/globals.h headers/map.h
ai.o: ai.c headers/ai.h headers/drone.h headers/survivor.h
view.o: view.c headers/view.h headers/drone.h headers/map.h headers/survivor.h
tests/listtest.o: tests/listtest.c headers/list.h headers/survivor.h
tests/sdltest.o: tests/sdltest.c

.PHONY: all clean run test_list test_sdl