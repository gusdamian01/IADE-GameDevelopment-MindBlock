# Commands
# make        -> builds debug (default)
# make debug  -> debug build
# make release-> optimized build
# make run    -> builds debug then runs ./app (or .exe)
# make clean  -> deletes .o, .d, and app

# ---------------- Platform detection ----------------
UNAME_S := $(shell uname -s)

# Assume MSYS2 MinGW64 if MSYSTEM is set (e.g., "MINGW64")
ifdef MSYSTEM
    IS_MSYS2 := 1
endif

# ---------------- Compiler ----------------
ifeq ($(UNAME_S),Darwin)
    CC := cc
else ifdef IS_MSYS2
    CC := x86_64-w64-mingw32-gcc
else
    CC := gcc
endif

# ---------------- pkg-config (SDL3) ----------------
SDL_PKG  := sdl3 sdl3-image

ifeq ($(UNAME_S),Darwin)
    PKGCONF := pkg-config
else ifdef IS_MSYS2
    PKGCONF := x86_64-w64-mingw32-pkg-config
else
    PKGCONF := pkg-config
endif

PKG_CFLAGS := $(shell $(PKGCONF) --cflags $(SDL_PKG) 2>/dev/null)
PKG_LIBS   := $(shell $(PKGCONF) --libs   $(SDL_PKG) 2>/dev/null)

# Fallback if pkg-config isn't found/configured (shouldn't be needed if you installed it)
ifeq ($(PKG_LIBS),)
    PKG_LIBS := -lSDL3 -lSDL3_image
endif

# ---------------- Common flags ----------------
CSTD := -std=c17
WARN := -Wall -Wextra -Wpedantic
DEPS := -MMD -MP

# Debug and Release flags
DBG := -g -O0 -fno-omit-frame-pointer
REL := -O2 -DNDEBUG

# Project structure
SRC := $(wildcard *.c) $(shell find src -name '*.c')
OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)
BIN := app

.PHONY: all debug release run clean

# Default: debug
all: debug

debug: CFLAGS := $(CSTD) $(WARN) $(DBG) $(DEPS) $(PKG_CFLAGS)
debug: LDFLAGS := $(PKG_LIBS)
debug: $(BIN)

release: CFLAGS := $(CSTD) $(WARN) $(REL) $(DEPS) $(PKG_CFLAGS)
release: LDFLAGS := $(PKG_LIBS)
release: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: debug
	./$(BIN)

clean:
	rm -f $(OBJ) $(DEP) $(BIN)

# Auto-include header dependencies
-include $(DEP)
