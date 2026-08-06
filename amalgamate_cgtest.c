/* MIT License
 * 
 * Copyright (c) 2026 Bernhard Bertrand (https://github.com/bartgeier/cgtest)
 * Copyright (c) 2010 Serge A. Zaitsev (https://github.com/zserge/jsmn)
 * Copyright (c) 2019 Bernhard Bertrand (https://github.com/bartgeier/arq)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* amalgamate_cgtest.c - entry point for the `amalgamate` CLI tool
 * (https://github.com/rindeal/Amalgamate) to produce cgtest.c, a
 * single-file drop-in build of cgtest.exe: no Makefile, no src/ tree,
 * no third_party/ - just download cgtest.c, compile it, run it. Run
 * `make cgtest.c` to (re)generate it after changing anything under
 * src/ or third_party/ - never edit cgtest.c by hand, it's overwritten
 * every time.
 *
 * Order here doesn't affect correctness (each #include'd .c file pulls
 * in its own prototypes via its own header's #include, and the
 * `amalgamate` tool only inlines any given file once regardless of how
 * many times it's #include'd - see cgtest_project.c's JSMN_STATIC use,
 * pulled in once here despite jsmn.h being reachable from more than
 * one file) - listed bottom-up (leaf modules first, cgtest_main.c
 * last) purely so a reader skimming cgtest.c sees dependencies before
 * their users.
 */
#include "cpath.c"
#include "cpathlist.c"
#include "cmsg.c"
#include "clexer.c"
#include "cpreprocessor.c"
#include "ctestscanner.c"
#include "ctestfiles.c"
#include "ctimer.c"
#include "cgtest_project.c"
#include "cgtest_create.c"
#include "cgtest_runner.c"
#include "cgtest_arq.c"
#include "cgtest_main.c"

