Starch (Stack Architecture)
===========================

Starch is a theoretical stack-based computer architecture.

See [starch/doc/starch-desc.md](starch/doc/starch-desc.md) for a description of the instruction set.
[starch/inc/starch.h](starch/inc/starch.h) is a C header file which defines all Starch instruction opcodes.

Projects
--------

 * [build.py](build.py) is a custom Python build script used by Starch.
 * [distasm](distasm) is a Starch disassembler.
 * [starch](starch) contains code associated with the Starch instruction set.
 * [stasm](stasm) is a Starch assembler. See [stasm/doc/stasm-desc.md](stasm/doc/stasm-desc.md) for more details.
 * [stem](stem) is a Starch emulator.
 * [stub](stub) is a custom binary file format used by Starch projects.
 * [util](util) is a collection of utilities used by Starch projects.

How to Build
------------

Simply run `./build.py`.

Name
----

Starch probably stands for "Stack Architecture" but might also stand for "Smart Things and Really Cool Hacks".
