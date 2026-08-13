ifeq ($(OS),Windows_NT)
    CC := gcc
else
    CC ?= cc
endif
CFLAGS     ?= -std=c99 -Wall -Wextra -pedantic
CFLAGS     += -Isrc -Ithird_party/arq -Ithird_party/jsmn
AMALGAMATE ?= amalgamate

BUILD_DIR := build

AMALGAMATE_STUB  := amalgamate_cgtest.c
AMALGAMATED_SRC  := cgtest.c
CHECK_AMALGAMATE_BIN := $(BUILD_DIR)/check_amalgamate

TEST_CTESTSCANNER_BIN  := $(BUILD_DIR)/test_ctestscanner
TEST_CPREPROCESSOR_BIN := $(BUILD_DIR)/test_cpreprocessor
TEST_CPATH_BIN         := $(BUILD_DIR)/test_cpath
TEST_CPATHLIST_BIN     := $(BUILD_DIR)/test_cpathlist
TEST_CGTEST_PROJECT_BIN := $(BUILD_DIR)/test_cgtest_project
TEST_CTESTFILES_BIN    := $(BUILD_DIR)/test_ctestfiles
TEST_CGTEST_ARQ_BIN    := $(BUILD_DIR)/test_cgtest_arq
TEST_CGTEST_CREATE_BIN := $(BUILD_DIR)/test_cgtest_create
TEST_CGTEST_RUNNER_BIN := $(BUILD_DIR)/test_cgtest_runner
TEST_CTIMER_BIN        := $(BUILD_DIR)/test_ctimer
CGTEST_BIN             := $(BUILD_DIR)/cgtest

.PHONY: all test check-c89 check-amalgamate clean

all: test $(CGTEST_BIN) $(AMALGAMATED_SRC)

# Binary paths below are quoted: unquoted, cmd.exe (mingw32-make's
# fallback SHELL when no POSIX shell is on PATH) stops parsing the
# command at the first unquoted "/", treating it as a switch character
# rather than a path separator - e.g. "build/test_ctestscanner" gets
# read as the command "build". Quoting is a no-op under a POSIX shell.
test: check-c89 check-amalgamate $(TEST_CTESTSCANNER_BIN) $(TEST_CPREPROCESSOR_BIN) $(TEST_CPATH_BIN) $(TEST_CPATHLIST_BIN) $(TEST_CGTEST_PROJECT_BIN) $(TEST_CTESTFILES_BIN) $(TEST_CGTEST_ARQ_BIN) $(TEST_CGTEST_CREATE_BIN) $(TEST_CGTEST_RUNNER_BIN) $(TEST_CTIMER_BIN)
	@echo "== test_ctestscanner =="
	@"$(TEST_CTESTSCANNER_BIN)"
	@echo
	@echo "== test_cpreprocessor =="
	@"$(TEST_CPREPROCESSOR_BIN)"
	@echo
	@echo "== test_cpath =="
	@"$(TEST_CPATH_BIN)"
	@echo
	@echo "== test_cpathlist =="
	@"$(TEST_CPATHLIST_BIN)"
	@echo
	@echo "== test_cgtest_project =="
	@"$(TEST_CGTEST_PROJECT_BIN)"
	@echo
	@echo "== test_ctestfiles =="
	@"$(TEST_CTESTFILES_BIN)"
	@echo
	@echo "== test_cgtest_arq =="
	@"$(TEST_CGTEST_ARQ_BIN)"
	@echo
	@echo "== test_cgtest_create =="
	@"$(TEST_CGTEST_CREATE_BIN)"
	@echo
	@echo "== test_cgtest_runner =="
	@"$(TEST_CGTEST_RUNNER_BIN)"
	@echo
	@echo "== test_ctimer =="
	@"$(TEST_CTIMER_BIN)"

$(TEST_CTESTSCANNER_BIN): tests/test_ctestscanner.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c src/ctestscanner.h src/cpreprocessor.h src/clexer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_ctestscanner.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c -o $@

$(TEST_CPREPROCESSOR_BIN): tests/test_cpreprocessor.c src/cpreprocessor.c src/clexer.c src/cpreprocessor.h src/clexer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpreprocessor.c src/cpreprocessor.c src/clexer.c -o $@

$(TEST_CPATH_BIN): tests/test_cpath.c src/cpath.c src/cpath.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpath.c src/cpath.c -o $@

$(TEST_CPATHLIST_BIN): tests/test_cpathlist.c src/cpathlist.c src/cpath.c src/cpathlist.h src/cpath.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cpathlist.c src/cpathlist.c src/cpath.c -o $@

$(TEST_CGTEST_PROJECT_BIN): tests/test_cgtest_project.c src/cgtest_project.c src/cpathlist.c src/cpath.c src/cmsg.c src/cgtest_project.h src/cpathlist.h src/cpath.h src/cmsg.h third_party/jsmn/jsmn.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cgtest_project.c src/cgtest_project.c src/cpathlist.c src/cpath.c src/cmsg.c -o $@

$(TEST_CTESTFILES_BIN): tests/test_ctestfiles.c src/ctestfiles.c src/cpathlist.c src/cpath.c src/cmsg.c src/ctestfiles.h src/cpathlist.h src/cpath.h src/cmsg.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_ctestfiles.c src/ctestfiles.c src/cpathlist.c src/cpath.c src/cmsg.c -o $@

$(TEST_CGTEST_ARQ_BIN): tests/test_cgtest_arq.c src/cgtest_arq.c src/cmsg.c src/cgtest_arq.h src/cmsg.h third_party/arq/arq.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cgtest_arq.c src/cgtest_arq.c src/cmsg.c -o $@

$(TEST_CGTEST_CREATE_BIN): tests/test_cgtest_create.c src/cgtest_create.c src/cgtest_project.c src/cpathlist.c src/cpath.c src/cmsg.c src/cgtest_create.h src/cgtest_project.h src/cpathlist.h src/cpath.h src/cmsg.h third_party/jsmn/jsmn.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cgtest_create.c src/cgtest_create.c src/cgtest_project.c src/cpathlist.c src/cpath.c src/cmsg.c -o $@

$(TEST_CGTEST_RUNNER_BIN): tests/test_cgtest_runner.c src/cgtest_runner.c src/ctestfiles.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c src/cpathlist.c src/cpath.c src/cmsg.c src/ctimer.c src/cgtest_runner.h src/ctestfiles.h src/ctestscanner.h src/cpreprocessor.h src/clexer.h src/cpathlist.h src/cpath.h src/cmsg.h src/ctimer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_cgtest_runner.c src/cgtest_runner.c src/ctestfiles.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c src/cpathlist.c src/cpath.c src/cmsg.c src/ctimer.c -o $@

$(TEST_CTIMER_BIN): tests/test_ctimer.c src/ctimer.c src/ctimer.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) tests/test_ctimer.c src/ctimer.c -o $@

$(CGTEST_BIN): src/cgtest_main.c src/cgtest_arq.c src/cgtest_create.c src/cgtest_project.c src/cgtest_runner.c src/ctestfiles.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c src/cpathlist.c src/cpath.c src/cmsg.c src/ctimer.c | $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -pedantic -Ithird_party/arq -Ithird_party/jsmn src/cgtest_main.c src/cgtest_arq.c src/cgtest_create.c src/cgtest_project.c src/cgtest_runner.c src/ctestfiles.c src/ctestscanner.c src/cpreprocessor.c src/clexer.c src/cpathlist.c src/cpath.c src/cmsg.c src/ctimer.c -o $@

# mkdir -p / rm -rf below assume a POSIX shell. mingw32-make picks its
# SHELL by probing for sh.exe on PATH, which is unreliable on Windows -
# plain cmd.exe has no `rm` at all, and its own `mkdir` doesn't support
# -p (it creates a literal "-p" directory instead). Force cmd.exe with
# native commands for just these two targets on Windows so behavior
# doesn't depend on whether a POSIX shell happens to be on PATH.
ifeq ($(OS),Windows_NT)
$(BUILD_DIR): SHELL := cmd.exe
clean: SHELL := cmd.exe
endif

$(BUILD_DIR):
ifeq ($(OS),Windows_NT)
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
else
	mkdir -p $(BUILD_DIR)
endif

# cgtest.exe itself must stay strict C89 (see specification.md). This
# compiles and links every src/*.c together as a shared library purely
# to catch C89 violations and cross-file link errors, same check used
# while developing clexer/cpreprocessor. cgtest_main.c's main() ends up
# in the .so too, which is harmless - it's just another symbol.
check-c89: | $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -pedantic -Werror -fPIC -shared -Wl,--no-undefined -Ithird_party/arq -Ithird_party/jsmn src/*.c -o $(BUILD_DIR)/libcgtest_src_check.so

# cgtest.c - a single-file amalgamation of every src/*.c and the
# third_party/ headers it needs (see amalgamate_cgtest.c), produced by
# the `amalgamate` CLI (https://github.com/rindeal/Amalgamate) so a
# downstream developer can grab exactly one file, compile it, and have
# a working cgtest.exe - no Makefile, no src/ tree, no third_party/ to
# fetch separately. Checked into version control (not a build
# artifact under $(BUILD_DIR)) since that one file IS the deliverable
# this target exists for - re-run `make cgtest.c` (or plain `make`)
# after changing anything under src/ or third_party/ to keep it in
# sync, the same way a generated cgtest-runner.c is regenerated, never
# hand-edited.
$(AMALGAMATED_SRC): $(AMALGAMATE_STUB) src/*.c src/*.h third_party/arq/*.h third_party/jsmn/*.h
	$(AMALGAMATE) -i src -i third_party/arq -i third_party/jsmn $(AMALGAMATE_STUB) $(AMALGAMATED_SRC)

# cgtest.c must compile standalone under the same strict flags
# cgtest.exe itself does, and actually run - not just parse without
# error - since a broken single-file build defeats the entire point of
# shipping it. --version is enough of a smoke test to catch a botched
# amalgamation (e.g. a genuine cross-file symbol collision, unlikely
# given check-c89 already compiles every src/*.c file - see its own
# comment - but that check compiles each as its own translation unit,
# not merged into one the way amalgamation does).
check-amalgamate: $(AMALGAMATED_SRC) | $(BUILD_DIR)
	$(CC) -std=c89 -Wall -Wextra -pedantic -Werror $(AMALGAMATED_SRC) -o $(CHECK_AMALGAMATE_BIN)
	"$(CHECK_AMALGAMATE_BIN)" --version

clean:
ifeq ($(OS),Windows_NT)
	if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
else
	rm -rf $(BUILD_DIR)
endif
