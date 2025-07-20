#!/usr/bin/bash

# Run trap on error, even within functions.
# Expansion of undefined variables is an error.
set -Eeu

trap 'echo test failed on line $LINENO' ERR

# Change to test directory
cd test

DISTASM=../distasm/bin/distasm
STASM=../stasm/bin/stasm
STEM=../stem/bin/stem

# Run unit test executables
echo testing smap
../util/test/smaptest
echo testing emulated memory
../stem/test/memtest
echo testing utf8 library
../util/test/utf8test

# Assemble a file which contains every opcode
echo assembling all opcodes
$STASM allops.st
echo disassembling all opcodes
$DISTASM a.stb -o dis.st
echo checking for symmetric dis/assembly
cmp allops.st dis.st

# Check assembly of pseudo-ops
echo assembling pseudo-ops
$STASM psops.st
echo disassembling pseudo-ops
$DISTASM a.stb -o dis.st
echo checking for proper disassembly
cmp psops-dis.st dis.st

echo all tests passed
