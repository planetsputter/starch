#!/usr/bin/env bash

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
echo testing literal parsing
../util/test/littest

# Assemble a file which contains every opcode
echo assembling all opcodes
$STASM allops.sta
echo disassembling all opcodes
$DISTASM a.stb -o dis.sta
echo checking for symmetric dis/assembly
$STASM dis.sta -o b.stb
cmp a.stb b.stb

# Check assembly of pseudo-ops
echo assembling pseudo-ops
$STASM psops.sta
echo disassembling pseudo-ops
$DISTASM a.stb -o dis.sta
echo checking for proper disassembly
cmp psops-dis.sta dis.sta
echo checking compaction of pseudo-ops
$STASM compact.sta
$DISTASM a.stb --addr -o dis.sta
cmp compact-dis.sta dis.sta

# Check various errors are detected by the assembler
echo testing rejection of opcode outside of section
if $STASM <<EOF 2>/dev/null
push64 0
EOF
then false; fi

echo testing rejection of label outside of section
if $STASM <<EOF 2>/dev/null
:test_label
EOF
then false; fi

echo testing rejection of strings outside of section
if $STASM <<EOF 2>/dev/null
strings
EOF
then false; fi

echo testing rejection of invalid maximum number of sections
if $STASM --maxnsec 0 </dev/null 2>/dev/null; then false; fi
$STASM --maxnsec 1 </dev/null

echo testing rejection of invalid section address
if $STASM <<EOF 2>/dev/null
section -1
EOF
then false; fi
if $STASM <<EOF 2>/dev/null
section a
EOF
then false; fi

echo testing rejection of empty symbol name
if $STASM <<EOF 2>/dev/null
section 0x3000
push64 \$
EOF
then false; fi

echo testing rejection of integer literal symbol name
if $STASM <<EOF 2>/dev/null
define 1 0
EOF
then false; fi

echo testing rejection of string literal symbol name
if $STASM <<EOF 2>/dev/null
define "a" 1
EOF
then false; fi
$STASM <<EOF
define a "1"
EOF

echo testing rejection of label symbol name
if $STASM <<EOF 2>/dev/null
define :test1 :test2
EOF
then false; fi
$STASM <<EOF
define test1 :test1
EOF

echo testing rejection of undefined symbol
if $STASM <<EOF 2>/dev/null
define a 1
section 0x3000
push64 \$b
EOF
then false; fi

echo testing rejection of empty label name
if $STASM <<EOF 2>/dev/null
section 0x3000
:
EOF
then false; fi

echo testing rejection of duplicate labels
if $STASM <<EOF 2>/dev/null
section 0x3000
:test
:test
EOF
then false; fi

echo testing rejection of unquoted include
if $STASM <<EOF 2>/dev/null
include psops.sta
EOF
then false; fi

echo testing rejection of invalid opcode
if $STASM <<EOF 2>/dev/null
section 0x3000
test
EOF
then false; fi

echo testing rejection of incomplete statement
if $STASM <<EOF 2>/dev/null
section 0x3000
push64
EOF
then false; fi

# Run individual tests
echo testing add, sub
$STASM test-add-sub.sta
$STEM a.stb
echo testing mul, div, mod
$STASM test-mul-div-mod.sta
$STEM a.stb
echo testing bitwise logical operations
$STASM test-bit-ops.sta
$STEM a.stb
echo testing interrupts
$STASM test-int.sta
$STEM a.stb

echo all tests passed
