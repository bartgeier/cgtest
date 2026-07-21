CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic
CFLAGS  += -Isrc

BUILD_DIR := build

TEST_CLEXER_BIN        := $(BUILD_DIR)/test_clexer
TEST_CPREPROCESSOR_BIN := $(BUILD_DIR)/test_cpreprocessor

.PHONY: all test check-c89 clean

all: test

test: check-c89 $(TEST_CLEXER_BIN) $(TEST_CPREPROCESSOR_BIN)
	@echo "== test_clexer =="
	@$(TEST_CLEXER_BIN)
	@echo
	@echo "== test_cpreprocessor =="
	@$(TEST_CPREPROCESSOR_BIN)

$(TEST_CLEXER_BIN): tests/test_clexer.c src/clexer.c src/clexer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_clexer.c src/clexer.c -o $@

$(TEST_CPREPROCESSOR_BIN): tests/test_cpreprocessor.c src/cpreprocessor.c src/clexer.c src/cpreprocessor.h src/clexer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpreprocessor.c src/cpreprocessor.c src/clexer.c -o $@

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
