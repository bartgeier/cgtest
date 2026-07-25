CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic
CFLAGS  += -Isrc

BUILD_DIR := build

TEST_CTESTSCANNER_BIN  := $(BUILD_DIR)/test_ctestscanner
TEST_CPREPROCESSOR_BIN := $(BUILD_DIR)/test_cpreprocessor
TEST_CPATH_BIN         := $(BUILD_DIR)/test_cpath
TEST_CPATHLIST_BIN     := $(BUILD_DIR)/test_cpathlist

.PHONY: all test check-c89 clean

all: test

test: check-c89 $(TEST_CTESTSCANNER_BIN) $(TEST_CPREPROCESSOR_BIN) $(TEST_CPATH_BIN) $(TEST_CPATHLIST_BIN)
	@echo "== test_ctestscanner =="
	@$(TEST_CTESTSCANNER_BIN)
	@echo
	@echo "== test_cpreprocessor =="
	@$(TEST_CPREPROCESSOR_BIN)
	@echo
	@echo "== test_cpath =="
	@$(TEST_CPATH_BIN)
	@echo
	@echo "== test_cpathlist =="
	@$(TEST_CPATHLIST_BIN)

$(TEST_CTESTSCANNER_BIN): tests/test_ctestscanner.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c src/ctestscanner.h src/cpreprocessor.h src/clexer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_ctestscanner.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c -o $@

$(TEST_CPREPROCESSOR_BIN): tests/test_cpreprocessor.c src/cpreprocessor.c src/clexer.c src/cpreprocessor.h src/clexer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpreprocessor.c src/cpreprocessor.c src/clexer.c -o $@

$(TEST_CPATH_BIN): tests/test_cpath.c src/cpath.c src/cpath.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpath.c src/cpath.c -o $@

$(TEST_CPATHLIST_BIN): tests/test_cpathlist.c src/cpathlist.c src/cpath.c src/cpathlist.h src/cpath.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpathlist.c src/cpathlist.c src/cpath.c -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# cgtest.exe itself must stay strict C89 (see specification.md). This
# compiles and links every src/*.c together as a shared library (no
# main() exists yet) purely to catch C89 violations and cross-file
# link errors, same check used while developing clexer/cpreprocessor.
check-c89: | $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -pedantic -Werror -fPIC -shared -Wl,--no-undefined src/*.c -o $(BUILD_DIR)/libcgtest_src_check.so

clean:
	rm -rf $(BUILD_DIR)
