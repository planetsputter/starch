Starch
======

Starch is a theoretical stack-based computer architecture.

See [starch/doc/starch-desc.md](starch/doc/starch-desc.md) for a description of the instruction set.
[starch/inc/starch.h](starch/inc/starch.h) is a C header file which defines all Starch instruction opcodes.

Projects
--------

 * [build.py](build.py) is a general-purpose build script used by Starch. It invokes the compiler to generate dependencies for Starch build targets and then serves as a front-end for make. [build.py.md](build.py.md) documents the build script and its configuration file, [build.cfg](build.cfg).
 * [cloc.py](cloc.py) is a script to count lines of code in Starch. It can count blank, comment, code, and documentation lines in C, Python, and Markdown files.
 * [distasm](distasm) is a Starch disassembler. It takes a stub binary file and produces Starch assembly describing it.
 * [starch](starch) contains code associated with the Starch instruction set.
 * [stasm](stasm) is a Starch assembler. It takes a Starch assembly file and produces a stub binary containing Starch code suitable for emulation. See [stasm/doc/stasm-desc.md](stasm/doc/stasm-desc.md) for more details.
 * [stem](stem) is a Starch emulator. It executes Starch code in a emulated environment on the host machine.
 * [stub](stub) is a custom binary file format used by Starch projects.
 * [util](util) is a collection of utilities used by Starch projects.

How to Build
------------

Run `./build.py` to perform a parallel build of the release configuration. To build the debug configuration run `./build.py BUILDCFG=debug` or change the environment variable "BUILDCFG" to "debug" as in `BUILDCFG=debug ./build.py`. To perform a non-parallel build pass the arguments `-j 1` to build.py.

A build target may be specified on the command line. Typical build targets would be "all" (builds all Starch binaries, the default) or "clean" (removes intermediate files, recommended when switching configurations).

How to Test
-----------

Run `./test.sh`.

Name
----

Starch probably stands for "Stack Architecture" but might also stand for "Smart Things and Really Cool Hacks".
