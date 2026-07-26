CC      ?= gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic
CFLAGS  += -Isrc -Ithird_party

BUILD_DIR := build

TEST_CTESTSCANNER_BIN  := $(BUILD_DIR)/test_ctestscanner
TEST_CPREPROCESSOR_BIN := $(BUILD_DIR)/test_cpreprocessor
TEST_CPATH_BIN         := $(BUILD_DIR)/test_cpath
TEST_CPATHLIST_BIN     := $(BUILD_DIR)/test_cpathlist
TEST_CGTEST_CONFIG_BIN := $(BUILD_DIR)/test_cgtest_config
TEST_CTESTFILES_BIN    := $(BUILD_DIR)/test_ctestfiles
TEST_CGTEST_ARQ_BIN    := $(BUILD_DIR)/test_cgtest_arq
TEST_CGTEST_CREATE_BIN := $(BUILD_DIR)/test_cgtest_create
CGTEST_BIN             := $(BUILD_DIR)/cgtest

.PHONY: all test check-c89 clean

all: test $(CGTEST_BIN)

test: check-c89 $(TEST_CTESTSCANNER_BIN) $(TEST_CPREPROCESSOR_BIN) $(TEST_CPATH_BIN) $(TEST_CPATHLIST_BIN) $(TEST_CGTEST_CONFIG_BIN) $(TEST_CTESTFILES_BIN) $(TEST_CGTEST_ARQ_BIN) $(TEST_CGTEST_CREATE_BIN)
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
	@echo
	@echo "== test_cgtest_config =="
	@$(TEST_CGTEST_CONFIG_BIN)
	@echo
	@echo "== test_ctestfiles =="
	@$(TEST_CTESTFILES_BIN)
	@echo
	@echo "== test_cgtest_arq =="
	@$(TEST_CGTEST_ARQ_BIN)
	@echo
	@echo "== test_cgtest_create =="
	@$(TEST_CGTEST_CREATE_BIN)

$(TEST_CTESTSCANNER_BIN): tests/test_ctestscanner.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c src/ctestscanner.h src/cpreprocessor.h src/clexer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_ctestscanner.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c -o $@

$(TEST_CPREPROCESSOR_BIN): tests/test_cpreprocessor.c src/cpreprocessor.c src/clexer.c src/cpreprocessor.h src/clexer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpreprocessor.c src/cpreprocessor.c src/clexer.c -o $@

$(TEST_CPATH_BIN): tests/test_cpath.c src/cpath.c src/cpath.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpath.c src/cpath.c -o $@

$(TEST_CPATHLIST_BIN): tests/test_cpathlist.c src/cpathlist.c src/cpath.c src/cpathlist.h src/cpath.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpathlist.c src/cpathlist.c src/cpath.c -o $@

$(TEST_CGTEST_CONFIG_BIN): tests/test_cgtest_config.c src/cgtest_config.c src/cpathlist.c src/cpath.c src/cmsg.c src/cgtest_config.h src/cpathlist.h src/cpath.h src/cmsg.h third_party/jsmn.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cgtest_config.c src/cgtest_config.c src/cpathlist.c src/cpath.c src/cmsg.c -o $@

$(TEST_CTESTFILES_BIN): tests/test_ctestfiles.c src/ctestfiles.c src/cpathlist.c src/cpath.c src/cmsg.c src/ctestfiles.h src/cpathlist.h src/cpath.h src/cmsg.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_ctestfiles.c src/ctestfiles.c src/cpathlist.c src/cpath.c src/cmsg.c -o $@

$(TEST_CGTEST_ARQ_BIN): tests/test_cgtest_arq.c src/cgtest_arq.c src/cmsg.c src/cgtest_arq.h src/cmsg.h third_party/arq.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cgtest_arq.c src/cgtest_arq.c src/cmsg.c -o $@

$(TEST_CGTEST_CREATE_BIN): tests/test_cgtest_create.c src/cgtest_create.c src/cgtest_config.c src/cpathlist.c src/cpath.c src/cmsg.c src/cgtest_create.h src/cgtest_config.h src/cpathlist.h src/cpath.h src/cmsg.h third_party/jsmn.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cgtest_create.c src/cgtest_create.c src/cgtest_config.c src/cpathlist.c src/cpath.c src/cmsg.c -o $@

$(CGTEST_BIN): src/cgtest_main.c src/cgtest_arq.c src/cgtest_create.c src/cgtest_config.c src/cpathlist.c src/cpath.c src/cmsg.c | $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -pedantic -Ithird_party src/cgtest_main.c src/cgtest_arq.c src/cgtest_create.c src/cgtest_config.c src/cpathlist.c src/cpath.c src/cmsg.c -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# cgtest.exe itself must stay strict C89 (see specification.md). This
# compiles and links every src/*.c together as a shared library purely
# to catch C89 violations and cross-file link errors, same check used
# while developing clexer/cpreprocessor. cgtest_main.c's main() ends up
# in the .so too, which is harmless - it's just another symbol.
check-c89: | $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -pedantic -Werror -fPIC -shared -Wl,--no-undefined -Ithird_party src/*.c -o $(BUILD_DIR)/libcgtest_src_check.so

clean:
	rm -rf $(BUILD_DIR)
