#!/usr/bin/bash

# Run trap on error, even within functions.
# Expansion of undefined variables is an error.
set -Eeu

trap 'echo test.sh failed on line $LINENO' ERR

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
$STASM allops.sta
echo disassembling all opcodes
$DISTASM a.stb -o dis.sta
echo checking for symmetric dis/assembly
cmp allops.sta dis.sta

# Check assembly of pseudo-ops
echo assembling pseudo-ops
$STASM psops.sta
echo disassembling pseudo-ops
$DISTASM a.stb -o dis.sta
echo checking for proper disassembly
cmp psops-dis.sta dis.sta

# Run individual tests
echo testing add, sub
$STASM test-add-sub.sta
$STEM a.stb

echo all tests passed
