# Compiler configuration

.DEFAULT_GOAL := main

CC = g++
# CC = g++-15
WEBCC = emcc
VER = -std=c++23
OPT = -O2
ARGS = -Wall -Wextra -Wpedantic -Wno-missing-braces #-Wnrvo

# -sENVIRONMENT=node -sNODERAWFS=1
WEB_ARGS = -sWASM=1 -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS='["callMain", "ccall"]' \
	 -sASSERTIONS -sENVIRONMENT=web -sSTACK_SIZE=8388608 -INVOKE_RUN_AT_START=0 -sEXIT_RUNTIME=0 -sNO_DISABLE_EXCEPTION_CATCHING

SRC_DIRS = src/Lex src/Parser src/Analysis src/Interp src/Utils src/CLI src/Preprocessor src/Type src/Value
SRC = $(wildcard $(SRC_DIRS:=/*.cxx))
SAN = -fsanitize=undefined -fsanitize=address # -g3

OUTPUT_NAME = Pie
DEBUG_OUTPUT_NAME = Pie_debug
WEB_OUTPUT_NAME = Pie.js

## Library directories

# Pulled from GitHub
REMOTE_INCLUDE_DIR = remote_includes
MP11_DIR           = $(REMOTE_INCLUDE_DIR)/mp11
CPP_STD_EXT_DIR    = $(REMOTE_INCLUDE_DIR)/cpp-std-extensions
# LIB_FFI_DIR        = $(REMOTE_INCLUDE_DIR)/libffi


# Saved locally
LOCAL_INCLUDE_DIR = includes


# Include paths (compile-time only)
INCLUDE = \
	-Isrc                              \
	-I$(MP11_DIR)/include/             \
	-I$(CPP_STD_EXT_DIR)/include/      \
	-I$(LIB_FFI_DIR)/include/          \

# Link-time only
LIBS = -lffi



test_dylib_mac:
	$(CC) $(VER) -dynamiclib -arch arm64 -arch x86_64 Tests/ffi_test.cpp -o Tests/dylib

test_dylib_lnx:
	$(CC) $(VER) -fPIC -shared Tests/ffi_test.cpp -o Tests/dylib

clean:
	rm -f $(OUTPUT_NAME) $(DEBUG_OUTPUT_NAME) run_tests run_tests_gh $(WEB_OUTPUT_NAME) Pie.wasm
	rm -rf build
	rm -rf remote_includes/*

.PHONY: checklibs clean main debug test web gh-actions test_dylib_lnx
