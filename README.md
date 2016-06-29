NeonGoby Alias Analysis Checker
===============================

**Jia: after migrating to LLVM 3.6, I revise this codebase completely and get rid of all the stuffs I don't need. Now the project builds with CMake, is no longer required to be intalled in the same directory as LLVM, and also needs not to use any submodules.**

NeonGoby is a system for effectively detecting errors in alias analysis, one of
the most important and widely used program analysis. It currently checks alias
analyses implemented on the LLVM framework. We have used it to find 29 bugs in
two popular alias analysis implementations: [Data Structure Alias
Analysis](http://llvm.org/docs/AliasAnalysis.html#the-ds-aa-pass)(`ds-aa`)
and Andersen's Alias Analysis (`anders-aa`).

Publications
------------

[Effective Dynamic Detection of Alias Analysis
Errors](http://www.cs.columbia.edu/~jingyue/docs/wu-fse13.pdf). In Proc.
ESEC/FSE 2013.

Building NeonGoby
-----------------

To build NeonGoby, you need to have a C++ compiler (e.g., g++ or clang)
installed. Python 2.7 or later are also required to run all the provided scripts. It should compile without trouble on most recent Linux or MacXOS
machines.

1. Download, build and install the source code of LLVM 3.6 and clang 3.6 from [LLVM Download Page](http://llvm.org/releases/download.html) or from your OS's software repo. Other version of LLVM and clang are not guaranteed to work with NeonGoby. If you want to compile LLVM by yourself, CMake is the prefered way to build it.

2. Checkout NeonGoby's source code

3. Build NeonGoby
```bash
cd <your-prefered-build-dir>
cmake <source-dir-of-neongoby> -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<your-prefered-install-dir>
make
make install
```

Using NeonGoby
----------------

Given a test program and a test workload, NeonGoby dynamically observes the
pointer addresses in the test program, and then checks these addresses against
an alias analysis for errors.

NeonGoby provides two modes to check an alias analysis: the offline mode and the
online mode. The offline mode checks more thoroughly, whereas the online mode
checks only intraprocedural alias queries. **Jia: I removed the online mode from the original NeonGoby codebase. Offline mode is the only option now.**

**Offline Mode**

To check an alias analysis (say `buggyaa`) with a test program (say
`example.cpp`) using the offline
mode of NeonGoby, first compile the code into `example.bc` in LLVM’s
intermediate representation (IR), and run the following three commands:

```bash
ng_hook_mem.py --hook-all example.bc
./example.inst
ng_check_aa.py --check-all example.bc <log-file> buggyaa
```

The first command instruments the program for checking, and outputs the
instrumented executable as `example.inst`. The second command runs the
instrumented program, which logs information to
`/tmp/ng-<date>-<time>/log.pts`. You can change the location by specifying
environment variable `LOG_DIR`. The third command checks this log against
`buggyaa` for errors.

Our scripts currently work with all the builtin alias analyses in LLVM (e.g.,
`basicaa` and `scev-aa`), and some third-party alias analyses (e.g., `anders-aa`
and `ds-aa`). To check more third-party alias analyses, you need to build the
alias analysis as an LLVM loadable module (a `.so` file), and manually add extra
configuration in `tools/ng_utils.py`.

**Dumping Logs**

Use `ng_dump_log` to dump `.pts` files to a readable format.

```bash
ng_dump_log -log-file <log-file>
```

Bugs Detected
-------------

See table 2 in our paper [Effective Dynamic Detection of Alias Analysis
Errors](http://www.cs.columbia.edu/~jingyue/docs/wu-fse13.pdf) for the bugs we
found so far using NeonGoby.

We fixed all the 13 bugs we found in `anders-aa`; we included the patches in the
`bugs` folder. We reported 8 bugs to `ds-aa` developers, and 4 of them have been
confirmed and fixed.

Fixed bugs in `ds-aa`:
- [#12744](http://llvm.org/bugs/show_bug.cgi?id=12744)
- [#12786](http://llvm.org/bugs/show_bug.cgi?id=12786)
- [#14147](http://llvm.org/bugs/show_bug.cgi?id=14147)
- [#14190](http://llvm.org/bugs/show_bug.cgi?id=14190)

Reported bugs in `ds-aa`:
- [#14075](http://llvm.org/bugs/show_bug.cgi?id=14075)
- [#14179](http://llvm.org/bugs/show_bug.cgi?id=14179)
- [#14401](http://llvm.org/bugs/show_bug.cgi?id=14401)
- [#14496](http://llvm.org/bugs/show_bug.cgi?id=14496)

People
------
- [Jingyue Wu](http://www.cs.columbia.edu/~jingyue/)
- Gang Hu
- [Yang Tang](http://ytang.com/)
- Junyang Lu
- [Junfeng Yang](http://www.cs.columbia.edu/~junfeng/)

- The is is a modified version of the [original NeonGoby project](https://github.com/wujingyue/neongoby). Modifier: [Jia Chen](http://www.cs.utexas.edu/~jchen/)
