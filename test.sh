#!/usr/bin/env bash

# Run trap on error, even within functions.
# Expansion of undefined variables is an error.
set -Eeu

trap 'test_fail; test_end ", exiting in test.sh line $LINENO"' ERR

TEST_NAME= # Current test name
HAS_FAIL=0
if [ -t 1 ]; then
	# Use color escape sequences when writing to terminal
	ANSIGRN='\x1b[32m'
	ANSIRED='\x1b[31m'
	ANSIRST='\x1b[39m'
else
	ANSIGRN=
	ANSIRED=
	ANSIRST=
fi

PASS_PREFIX="[${ANSIGRN}✔${ANSIRST}] ${ANSIGRN}pass${ANSIRST}:"
FAIL_PREFIX="[${ANSIRED}✘${ANSIRST}] ${ANSIRED}fail${ANSIRST}:"

test_pass() {
	if [ -n "$TEST_NAME" ]; then
		# Print pass message
		printf "$PASS_PREFIX %s\n" "$TEST_NAME"
		TEST_NAME=
	fi
}
test_fail() {
	if [ -n "$TEST_NAME" ]; then
		# Print fail message
		printf "$FAIL_PREFIX %s\n" "$TEST_NAME"
		TEST_NAME=
	fi
	HAS_FAIL=1
}
test_begin() {
	test_pass # Beginning a new test automatically passes the previous one
	TEST_NAME="$*"
}
test_end() {
	test_begin # Show previous test status, if any
	if [ $HAS_FAIL -eq 0 ]; then
		printf "$PASS_PREFIX all tests passed\n"
	else
		printf "$FAIL_PREFIX some tests failed%s\n" "$*"
	fi
}

# Change to test directory
cd test

# Allow a memory profiler to be specified with the MEMPROF environment variable.
# If not specified, don't use one.
: ${MEMPROF:= }
DISTASM="$MEMPROF ../distasm/bin/distasm"
STASM="$MEMPROF ../stasm/bin/stasm"
STEM="$MEMPROF ../stem/bin/stem"

# Run unit test executables
test_begin "testing smap"
../util/test/smaptest
test_begin "testing emulated memory"
../stem/test/memtest
test_begin "testing utf8 library"
../util/test/utf8test
test_begin "testing literal parsing"
../util/test/littest
test_begin "testing expression parsing"
../stasm/test/exprtest 2>/dev/null

# Assemble a file which contains every opcode
test_begin "assembling all opcodes"
$STASM allops.sta
test_begin "disassembling all opcodes"
$DISTASM a.stb -o dis.sta
test_begin "checking for symmetric dis/assembly"
$STASM dis.sta -o b.stb
cmp a.stb b.stb

# Check assembly of pseudo-ops
test_begin "assembling pseudo-ops"
$STASM psops.sta
test_begin "disassembling pseudo-ops"
$DISTASM a.stb -o dis.sta
test_begin "checking for proper disassembly"
cmp psops-dis.sta dis.sta
test_begin "checking compaction of pseudo-ops"
$STASM compact.sta
$DISTASM a.stb --addr -o dis.sta
cmp compact-dis.sta dis.sta

# Check various errors are detected by the assembler
stasm_accept() {
	$STASM <<EOF
section 0x3000
$*
EOF
}
stasm_reject() {
	if $STASM <<EOF 2>/dev/null
section 0x3000
$*
EOF
	then false; fi
}

test_begin "testing rejection of opcode outside of section"
if $STASM <<EOF 2>/dev/null
push64 0
EOF
then false; fi

test_begin "testing rejection of label outside of section"
if $STASM <<EOF 2>/dev/null
:test_label
EOF
then false; fi

test_begin "testing rejection of strings outside of section"
if $STASM <<EOF 2>/dev/null
strings
EOF
then false; fi

test_begin "testing rejection of invalid maximum number of sections"
if $STASM --maxnsec 0 </dev/null 2>/dev/null; then false; fi
if $STASM --maxnsec -1 </dev/null 2>/dev/null; then false; fi
$STASM --maxnsec 1 </dev/null

test_begin "testing rejection of invalid section address"
if $STASM <<EOF 2>/dev/null
section -1
EOF
then false; fi
if $STASM <<EOF 2>/dev/null
section a
EOF
then false; fi

test_begin "testing rejection of empty symbol name"
stasm_reject 'push64 $'
stasm_accept 'define a 0; push64 $a'

test_begin "testing rejection of integer literal symbol name"
stasm_reject 'define 1 0'

test_begin "testing rejection of string literal symbol name"
stasm_reject 'define "a" 1'
stasm_accept 'define a 1'

test_begin "testing rejection of label symbol name"
stasm_reject 'define :test1 :test2'
stasm_accept 'define test1 :test2'

test_begin "testing rejection of undefined symbol"
stasm_reject 'push64 $b'

test_begin "testing rejection of empty label name"
stasm_reject ':'

test_begin "testing rejection of duplicate labels"
stasm_reject ':test1; :test1'
stasm_accept ':test1; :test2'

test_begin "testing rejection of unquoted include"
stasm_reject 'include psops.sta'

test_begin "testing rejection of invalid opcode"
stasm_reject 'test'

test_begin "testing rejection of incomplete statement"
stasm_reject 'push64'

test_begin "testing rejection of unexpected expression"
stasm_reject 'add8 0'

test_begin "testing rejection of out-of-range arguments"
stasm_reject 'push8as8 -129'
stasm_accept 'push8as8 -128'
stasm_accept 'push8as8 255'
stasm_reject 'push8as8 256'

test_begin "testing rejection of disallowed bracket notation"
stasm_reject 'push8as8 [0]'
stasm_accept 'push8 [0]'

test_begin "testing rejection of missing bracket notation"
stasm_reject 'pop8 0'
stasm_accept 'pop8 [0]'

test_begin "testing rejection of improper SFP notation"
stasm_reject 'push64 [0+SFP]'
stasm_reject 'push64 [+SFP]'
stasm_reject 'push64 [0-SFP]'
stasm_reject 'push64 [-SFP]'

# Run individual tests
test_begin "testing add, sub"
$STASM test-add-sub.sta
$STEM a.stb
test_begin "testing mul, div, mod"
$STASM test-mul-div-mod.sta
$STEM a.stb
test_begin "testing bitwise logical operations"
$STASM test-bit-ops.sta
$STEM a.stb
test_begin "testing interrupts"
$STASM test-int.sta
$STEM a.stb

test_end
