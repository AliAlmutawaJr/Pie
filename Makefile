# Compiler configuration

.DEFAULT_GOAL := main

CC = g++
# CC = g++-15
WEBCC = emcc
VER = -std=c++23
OPT = -O2
ARGS = -Wall -Wextra -Wpedantic -Wno-missing-braces #-Wnrvo

# -sENVIRONMENT=node -sNODERAWFS=1
WEB_ARGS = -sWASM=1 -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS='["callMain"]' \
	 -sASSERTIONS -sENVIRONMENT=web -INVOKE_RUN_AT_START=0 -sEXIT_RUNTIME=0 -sNO_DISABLE_EXCEPTION_CATCHING

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
LIB_FFI_DIR        = $(REMOTE_INCLUDE_DIR)/libffi


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


# ============================================================

define BUILD_CONFIG

$(1)_OBJ_DIR := build/$(1)
$(1)_OBJS    := $$(SRC:src/%.cxx=$$($(1)_OBJ_DIR)/%.o)

$$($(1)_OBJ_DIR)/%.o: src/%.cxx
	@mkdir -p $$(dir $$@)
	$$(CC) $$(ARGS) $$(VER) $$(INCLUDE) $(2) -MMD -MP -c $$< -o $$@

-include $$($(1)_OBJS:.o=.d)

endef

$(eval $(call BUILD_CONFIG,release,$(OPT) -DNO_ERR_LOC))
$(eval $(call BUILD_CONFIG,debug,-O0 $(SAN)))
$(eval $(call BUILD_CONFIG,test,-O0 $(SAN) -DNO_ERR_LOC))
$(eval $(call BUILD_CONFIG,gh-actions,-O0 -DNO_ERR_LOC))
$(eval $(call BUILD_CONFIG,web,$(OPT) -DWEB_PIE))


# ============================== Main target ==============================

$(release_OBJ_DIR)/main.o: src/main.cc
	@mkdir -p $(dir $@)
	$(CC) $(ARGS) $(VER) $(INCLUDE) $(OPT) -DNO_ERR_LOC -MMD -MP -c $< -o $@

-include $(release_OBJ_DIR)/main.d

$(OUTPUT_NAME): $(release_OBJS) $(release_OBJ_DIR)/main.o
	$(CC) $(VER) $(release_OBJS) $(release_OBJ_DIR)/main.o $(INCLUDE) $(LIBS) $(OPT) -o $(OUTPUT_NAME)

main: checklibs $(OUTPUT_NAME)


# ============================== Debug target ==============================

$(debug_OBJ_DIR)/main.o: src/main.cc
	@mkdir -p $(dir $@)
	$(CC) $(ARGS) $(VER) $(INCLUDE) -O0 -MMD -MP -c $< -o $@

-include $(debug_OBJ_DIR)/main.d

$(DEBUG_OUTPUT_NAME): $(debug_OBJS) $(debug_OBJ_DIR)/main.o
	$(CC) $(VER) $(debug_OBJS) $(debug_OBJ_DIR)/main.o $(INCLUDE) $(LIBS) -O0 -o $(DEBUG_OUTPUT_NAME) $(SAN)

debug: checklibs $(DEBUG_OUTPUT_NAME)


# ============================== Test target ==============================

$(test_OBJ_DIR)/Test.o: Tests/Test.cc
	@mkdir -p $(dir $@)
	$(CC) $(ARGS) $(VER) $(INCLUDE) -O0 -MMD -MP -c $< -o $@

$(test_OBJ_DIR)/catch.o: Tests/catch.cpp
	@mkdir -p $(dir $@)
	$(CC) $(ARGS) $(VER) $(INCLUDE) -O0 -MMD -MP -c $< -o $@

-include $(test_OBJ_DIR)/Test.d $(test_OBJ_DIR)/catch.d

run_tests: $(test_OBJS) $(test_OBJ_DIR)/Test.o $(test_OBJ_DIR)/catch.o
	$(CC) $(VER) $(test_OBJS) $(test_OBJ_DIR)/Test.o $(test_OBJ_DIR)/catch.o $(INCLUDE) $(LIBS) -O0 -o run_tests $(SAN) -DNO_ERR_LOC

test: checklibs run_tests
	./run_tests && rm run_tests


# ============================== Web target ==============================

$(web_OBJ_DIR)/main.o: src/main.cc
	@mkdir -p $(dir $@)
	$(WEBCC) $(ARGS) $(VER) $(INCLUDE) $(OPT) -DWEB_PIE -MMD -MP -c $< -o $@

-include $(web_OBJ_DIR)/main.d

$(WEB_OUTPUT_NAME): $(web_OBJS) $(web_OBJ_DIR)/main.o
	$(WEBCC) $(VER) $(web_OBJS) $(web_OBJ_DIR)/main.o $(WEB_ARGS) $(INCLUDE) $(LIBS) $(OPT) -o $(WEB_OUTPUT_NAME)

web: checklibs $(WEB_OUTPUT_NAME)


# ============================== GH Actions ==============================

test_dylib_mac:
	$(CC) $(VER) -dynamiclib Tests/ffi_test.cpp -o Tests/dylib

test_dylib_lnx:
	$(CC) $(VER) -fPIC -shared Tests/ffi_test.cpp -o Tests/dylib

gh-actions: checklibs test_dylib_lnx run_tests_gh

$(gh-actions_OBJ_DIR)/Test.o: Tests/Test.cc
	@mkdir -p $(dir $@)
	$(CC) $(ARGS) $(VER) $(INCLUDE) -O0 -MMD -MP -c $< -o $@

$(gh-actions_OBJ_DIR)/catch.o: Tests/catch.cpp
	@mkdir -p $(dir $@)
	$(CC) $(ARGS) $(VER) $(INCLUDE) -O0 -MMD -MP -c $< -o $@

-include $(gh-actions_OBJ_DIR)/Test.d $(gh-actions_OBJ_DIR)/catch.d

run_tests_gh: $(gh-actions_OBJS) $(gh-actions_OBJ_DIR)/Test.o $(gh-actions_OBJ_DIR)/catch.o
	$(CC) $(VER) $(gh-actions_OBJS) $(gh-actions_OBJ_DIR)/Test.o $(gh-actions_OBJ_DIR)/catch.o $(INCLUDE) $(LIBS) -O0 -o run_tests_gh -DNO_ERR_LOC
	./run_tests_gh



# ============================== Misc ==============================

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
	rm -f $(OUTPUT_NAME) $(DEBUG_OUTPUT_NAME) run_tests run_tests_gh $(WEB_OUTPUT_NAME) Pie.wasm
	rm -rf build
	rm -rf remote_includes/*

.PHONY: checklibs clean main debug test web gh-actions test_dylib_lnx

