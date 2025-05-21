CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

# SDL flags - different for Linux and macOS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    SDL_FLAGS = -lSDL2 -lSDL2_ttf
else ifeq ($(UNAME_S),Darwin)
    SDL_FLAGS = -F/Library/Frameworks -framework SDL2 -lSDL2_ttf
else
    $(error Unsupported operating system)
endif

# JSON library flags
JSON_FLAGS = -ljson-c

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
MULTI_DRONE_TEST = tests/multi_drone_test

# Client drone executable
CLIENT_DRONE = drone_client

# Default target
all: $(MAIN) $(LIST_TEST) $(SDL_TEST) $(CLIENT_DRONE) $(MULTI_DRONE_TEST)

# Main program
$(MAIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(SDL_FLAGS) $(JSON_FLAGS)

# Build object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build test programs
$(LIST_TEST): tests/listtest.o list.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SDL_TEST): tests/sdltest.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(SDL_FLAGS)

# Client drone program
$(CLIENT_DRONE): clientDrone.o map.o list.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(JSON_FLAGS)

# Multi drone test program
$(MULTI_DRONE_TEST): tests/multi_drone_test.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(JSON_FLAGS)

# Run the simulator
run: $(MAIN)
	./$(MAIN)

# Run list test
test_list: $(LIST_TEST)
	./$(LIST_TEST)

# Run SDL test
test_sdl: $(SDL_TEST)
	./$(SDL_TEST)

# Run the client drone
run_client: $(CLIENT_DRONE)
	./$(CLIENT_DRONE)

# Run multi drone test
run_multi_drone: $(MULTI_DRONE_TEST)
	./$(MULTI_DRONE_TEST)

# Run Valgrind on main program
valgrind_main: $(MAIN)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(MAIN)

# Run Valgrind on list test
valgrind_list: $(LIST_TEST)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(LIST_TEST)

# Run Valgrind on SDL test
valgrind_sdl: $(SDL_TEST)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(SDL_TEST)

# Run Valgrind on multi drone test
valgrind_multi_drone: $(MULTI_DRONE_TEST)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(MULTI_DRONE_TEST)

# Run Valgrind on all
valgrind_all: valgrind_main valgrind_list valgrind_sdl valgrind_multi_drone

# Clean up
clean:
	rm -f $(MAIN) $(OBJ) $(LIST_TEST) $(SDL_TEST) $(CLIENT_DRONE) $(MULTI_DRONE_TEST) clientDrone.o tests/*.o

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
clientDrone.o: clientDrone.c headers/drone.h headers/globals.h headers/map.h

.PHONY: all clean run test_list test_sdl run_client run_multi_drone valgrind_main valgrind_list valgrind_sdl valgrind_multi_drone valgrind_all