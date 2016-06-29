NeonGoby Alias Analysis Checker
===============================

**Jia2: after migrating to LLVM 3.9, I get rid of even more stuffs I don't need. Now the project is super clean and ready to test cfl-aa. Yay!**

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

To build NeonGoby, you need to have a C++ compiler (e.g., g++ or clang) that supports C++14
installed. Python 2.7 or later are also required to run all the provided scripts. It should compile without trouble on most recent Linux or MacXOS machines.

1. Download, build and install the source code of LLVM 3.9 and clang 3.9 from [LLVM Download Page](http://llvm.org/releases/download.html) or from your OS's software repo. Other version of LLVM and clang are not guaranteed to work with NeonGoby. If you want to compile LLVM by yourself, CMake is the required way to build it.

2. Checkout NeonGoby's source code

3. Build NeonGoby
```bash
cd <your-prefered-build-dir>
cmake <source-dir-of-neongoby> -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=<build-dir-of-llvm>
make
```
The definition of LLVM_DIR is not strictly necessary. Please refer to [this page](http://llvm.org/docs/CMake.html#embedding-llvm-in-your-project) for more info on when and why to use it. 

Using NeonGoby
----------------

Given a test program and a test workload, NeonGoby dynamically observes the
pointer addresses in the test program, and then checks these addresses against
an alias analysis for errors.

**Offline Mode**

To check an alias analysis (say `buggyaa`) with a test program (say
`example.cpp`) using the offline
mode of NeonGoby, first compile the code into `example.bc` in LLVM’s
intermediate representation (IR), and run the following four commands:

```bash
bin/instrument example.bc -o example.inst.bc
clang example.inst.bc runtime/libRuntime.a -o example.inst
LOG_DIR=<log-dir> ./example.inst
bin/aa-check example.bc <log-file> -buggyaa
```

The first command instruments the program for checking, and outputs the
instrumented bitcode as `example.inst.bc`. The second command compiles the bitcode and links it with our runtime hook. The third command runs the
instrumented program, which logs information to
`<log-dir>/pts.log`. You can change the location by specifying
environment variable `LOG_DIR`. The fourth command checks this log against
`buggyaa` for errors.

Our scripts currently work with only cfl-aa in LLVM (e.g.,
`-cfl-aa`). 

**Dumping Logs**

Use `bin/log-dump` to dump `pts.log` files to a readable format.

```bash
bin/log-dump <log-file>
```

- The is is a modified version of the [original NeonGoby project](https://github.com/wujingyue/neongoby). Modifier: [Jia Chen](http://www.cs.utexas.edu/~jchen/)
