# Compiler configuration

CC = g++
# CC = g++-15
WEBCC = emcc
VER = -std=c++23
OPT = -O2
ARGS = -Wall -Wextra -Wpedantic -Wno-missing-braces #-Wnrvo

# -sENVIRONMENT=node -sNODERAWFS=1
WEB_ARGS = -sWASM=1 -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS='["callMain"]' \
	 -sASSERTIONS -sENVIRONMENT=web -INVOKE_RUN_AT_START=0 -sEXIT_RUNTIME=0 -sNO_DISABLE_EXCEPTION_CATCHING

CPP = $(wildcard src/Lex/*.cxx src/Parser/*.cxx src/Analysis/*.cxx src/Interp/*.cxx src/Utils/*.cxx src/CLI/*.cxx src/Preprocessor/*.cxx src/Type/*.cxx src/Value/*.cxx)
SAN = -fsanitize=undefined -fsanitize=address # -g3

OUTPUT_NAME = Pie
DEBUG_OUTPUT_NAME = Pie_debug
WEB_OUTPUT_NAME = Pie.js

## Library directories

# Pulled from GitHub
REMOTE_INCLUDE_DIR = remote_includes
MP11_DIR           = $(REMOTE_INCLUDE_DIR)/mp11
CPP_STD_EXT_DIR    = $(REMOTE_INCLUDE_DIR)/cpp-std-extensions
LIB_FFI_DIR        = $(REMOTE_INCLUDE_DIR)/libffi


# Saved locally
LOCAL_INCLUDE_DIR = includes


# Include paths
INCLUDE = \
	-Isrc                              \
	-I$(MP11_DIR)/include/             \
	-I$(CPP_STD_EXT_DIR)/include/      \
	-I$(LIB_FFI_DIR)/include/          \
	-lffi 							   \


# Main target
main: checklibs src/main.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) $(OPT) -DNO_ERR_LOC src/main.cc -o $(OUTPUT_NAME)

debug: checklibs src/main.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) -O0 src/main.cc -o $(DEBUG_OUTPUT_NAME) $(SAN)

test: checklibs Tests/Test.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) -O0 Tests/Test.cc Tests/catch.cpp -o run_tests $(SAN) -DNO_ERR_LOC && ./run_tests && rm run_tests

web: checklibs src/main.cc
	$(WEBCC) $(CPP) $(WEB_ARGS) $(VER) $(INCLUDE) $(OPT) src/main.cc -o $(WEB_OUTPUT_NAME) -DWEB_PIE



test_dylib_lnx:
	$(CC) $(VER) -fPIC -shared Tests/ffi_test.cpp -o Tests/dylib

gh-actions: checklibs test_dylib_lnx Tests/Test.cc
	$(CC) $(CPP) $(ARGS) $(VER) $(INCLUDE) -O0 Tests/Test.cc Tests/catch.cpp -o run_tests -DNO_ERR_LOC && ./run_tests


checklibs:
	@mkdir -p $(REMOTE_INCLUDE_DIR)
	@if [ ! -d "$(MP11_DIR)" ]; then \
        echo "Cloning Boost.MP11..."; \
        git clone https://github.com/boostorg/mp11 $(MP11_DIR); \
	fi
	@if [ ! -d "$(CPP_STD_EXT_DIR)" ]; then \
        echo "Cloning cpp-std-extensions...";\
        git clone https://github.com/intel/cpp-std-extensions $(CPP_STD_EXT_DIR); \
	fi

clean:
	rm -f $(OUTPUT_NAME) run_tests && rm -rf remote_includes/*

.PHONY: checklibs clean

