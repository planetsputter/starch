README.md
=========

Starch is a theoretical stack-based computer architecture.

See [starch-desc.md](starch-desc.md) for a description of the instruction set.
[starch/inc/starch.h](starch/inc/starch.h) is a C header file which defines all Starch instruction opcodes.

Projects
--------

 * [bstr](bstr) is a custom B-string implementation used by other Starch projects.
 * [build.py](build.py) is a custom Python build script used by Starch.
 * [starch](starch) contains code associated with the Starch instruction set.
 * [starg](starg) is a command-line argument parsing library used by other Starch projects.
 * [stasm](stasm) is a Starch assembler.
 * [stem](stem) is a Starch emulator.
 * [utf8](utf8) is a UTF-8 parser used by other Starch projects.

How to Build
------------

Simply run `./build.py`.
