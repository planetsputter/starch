#!/usr/bin/bash

set -Eeu

trap 'echo test failed on line $LINENO' ERR

mkdir -p test
cd test

DISTASM=../distasm/bin/distasm
STASM=../stasm/bin/stasm
STEM=../stem/bin/stem

# Run unit test executables
echo testing smap
../starch/test/smaptest
echo testing emulated memory
../stem/test/memtest
echo testing utf8 library
../utf8/test/test

# Check for symmetric dis/assembly of opcodes with zero immediates
check_op0() {
	echo -n testing symmetric dis/assembly of opcodes with no immediate:
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"
		echo "$1" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1" ]
		I=$(($I + 1))
		shift
	done
	echo
}

check_op0 invalid \
	pop8 pop16 pop32 pop64 \
	dup8 dup16 dup32 dup64 \
	set8 set16 set32 set64 \
	prom8u16 prom8u32 prom8u64 prom8i16 prom8i32 prom8i64 \
	prom16u32 prom16u64 prom16i32 prom16i64 \
	prom32u64 prom32i64 dem64to8 dem32to8 \
	add8 add16 add32 add64 \
	sub8 sub16 sub32 sub64 \
	subr8 subr16 subr32 subr64 \
	mulu8 mulu16 mulu32 mulu64 \
	muli8 muli16 muli32 muli64 \
	divu8 divu16 divu32 divu64 \
	divi8 divi16 divi32 divi64 \
	divru8 divru16 divru32 divru64 \
	divri8 divri16 divri32 divri64 \
	modu8 modu16 modu32 modu64 \
	modi8 modi16 modi32 modi64 \
	modru8 modru16 modru32 modru64 \
	modri8 modri16 modri32 modri64 \
	lshift8 lshift16 lshift32 lshift64 \
	rshiftu8 rshiftu16 rshiftu32 rshiftu64 \
	rshifti8 rshifti16 rshifti32 rshifti64 \
	band8 band16 band32 band64 \
	bor8 bor16 bor32 bor64 \
	bxor8 bxor16 bxor32 bxor64 \
	binv8 binv16 binv32 binv64 \
	land8 land16 land32 land64 \
	lor8 lor16 lor32 lor64 \
	linv8 linv16 linv32 linv64 \
	ceq8 ceq16 ceq32 ceq64 \
	cne8 cne16 cne32 cne64 \
	cgtu8 cgtu16 cgtu32 cgtu64 \
	cgti8 cgti16 cgti32 cgti64 \
	cltu8 cltu16 cltu32 cltu64 \
	clti8 clti16 clti32 clti64 \
	cgeu8 cgeu16 cgeu32 cgeu64 \
   	cgei8 cgei16 cgei32 cgei64 \
	cleu8 cleu16 cleu32 cleu64 \
	clei8 clei16 clei32 clei64 \
	calls ret jmps \
	load8 load16 load32 load64 \
	loadpop8 loadpop16 loadpop32 loadpop64 \
	loadsfp8 loadsfp16 loadsfp32 loadsfp64 \
	loadpopsfp8 loadpopsfp16 loadpopsfp32 loadpopsfp64 \
	store8 store16 store32 store64 \
	storepop8 storepop16 storepop32 storepop64 \
	storesfp8 storesfp16 storesfp32 storesfp64 \
	storepopsfp8 storepopsfp16 storepopsfp32 storepopsfp64 \
	storer8 storer16 storer32 storer64 \
	storerpop8 storerpop16 storerpop32 storerpop64 \
	storersfp8 storersfp16 storersfp32 storersfp64 \
	storerpopsfp8 storerpopsfp16 storerpopsfp32 storerpopsfp64 \
	setsbp setsfp setsp halt \
	ext nop

#
# Check for symmetric dis/assembly of opcodes with a8 immediate
#
check_opa8() {
	echo -n testing symmetric dis/assembly of opcodes with a8 immediate:
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with imm -128
		echo "$1 -128" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 128" ]

		# Check with imm 255
		echo "$1 0xff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 255" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opa8 push8

#
# Check for symmetric dis/assembly of opcodes with u8 immediate
#
check_opu8() {
	echo -n testing symmetric dis/assembly of opcodes with u8 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with imm 255
		echo "$1 0xff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 255" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opu8 push8u16 push8u32 push8u64

#
# Check for symmetric dis/assembly of opcodes with i8 immediate
#
check_opi8() {
	echo -n testing symmetric dis/assembly of opcodes with i8 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with imm 127
		echo "$1 127" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 127" ]

		# Check with imm -128
		echo "$1 -128" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 -128" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opi8 push8i16 push8i32 push8i64 rjmpi8 rbrnz8i8 rbrnz16i8 rbrnz32i8 rbrnz64i8

#
# Check for symmetric dis/assembly of opcodes with a16 immediate
#
check_opa16() {
	echo -n testing symmetric dis/assembly of opcodes with a16 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with imm 0xffff
		echo "$1 0xffff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 65535" ]

		# Check with imm -32768
		echo "$1 -32768" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 32768" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opa16 push16

#
# Check for symmetric dis/assembly of opcodes with u16 immediate
#
check_opu16() {
	echo -n testing symmetric dis/assembly of opcodes with u16 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with imm 0xffff
		echo "$1 0xffff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 65535" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opu16 push16u32 push16u64

#
# Check for symmetric dis/assembly of opcodes with i16 immediate
#
check_opi16() {
	echo -n testing symmetric dis/assembly of opcodes with i16 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with imm -32768
		echo "$1 -0x8000" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 -32768" ]

		# Check with imm 0x7fff
		echo "$1 0x7fff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 32767" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opi16 push16i32 push16i64 rjmpi16 rbrnz8i16 rbrnz16i16 rbrnz32i16 rbrnz64i16

#
# Check for symmetric dis/assembly of opcodes with a32 immediate
#
check_opa32() {
	echo -n testing symmetric dis/assembly of opcodes with a32 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with most negative immediate
		echo "$1 -0x80000000" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 2147483648" ]

		# Check with most positive immediate
		echo "$1 0xffffffff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 4294967295" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opa32 push32

#
# Check for symmetric dis/assembly of opcodes with u32 immediate
#
check_opu32() {
	echo -n testing symmetric dis/assembly of opcodes with u32 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with most positive immediate
		echo "$1 0xffffffff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 4294967295" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opu32 push32u64

#
# Check for symmetric dis/assembly of opcodes with i32 immediate
#
check_opi32() {
	echo -n testing symmetric dis/assembly of opcodes with i32 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# Check with most negative immediate
		echo "$1 -0x80000000" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 -2147483648" ]

		# Check with most positive immediate
		echo "$1 0x7fffffff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 2147483647" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opi32 push32i64 rjmpi32 rbrnz8i32 rbrnz16i32 rbrnz32i32 rbrnz64i32

#
# Check for symmetric dis/assembly of opcodes with u64 immediate
#
check_opu64() {
	echo -n testing symmetric dis/assembly of opcodes with u64 immediate
	local I=0
	while [ "$#" -ne 0 ]; do
		if [ $(($I % 8)) -eq 0 ]; then echo; fi
		echo -n " $1"

		# Check with imm 0
		echo "$1 0" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 0" ]

		# @todo: test for rejection of negative immediate

		# Check with most positive immediate
		echo "$1 0xffffffffffffffff" | $STASM
		local RESULT=$($DISTASM a.stb)
		[ "$RESULT" = "$1 18446744073709551615" ]

		I=$((I + 1))
		shift
	done
	echo
}

check_opu64 call jmp brnz8 brnz16 brnz32 brnz64

# @todo: test opcodes which expect i64 immediates

#
# Test halt
#
echo halt | $STASM
$STEM a.stb

#
# Test invalid
#
echo invalid | $STASM
if $STEM a.stb 2>/dev/null; then false; else test $? -eq 1; fi

#
# Test push8
#
echo "test push8"
$STASM <<EOF
push8 0xff
push8 0x01
push8 0x00
push8 -1
push8 -128
halt
EOF
$STEM -d x.hex a.stb
grep "000000003ffffff0: 00 00 00 00 00 00 00 00 00 00 00 80 ff 00 01 ff" x.hex > /dev/null

#
# Test push8u16
#
echo "test push8u16"
$STASM <<EOF
push8u16 0xff
push8u16 0x00
push8u16 0x01
halt
EOF
$STEM -d x.hex a.stb
grep "000000003ffffff0: 00 00 00 00 00 00 00 00 00 00 01 00 00 00 ff 00" x.hex > /dev/null

#
# Test push8u32
#
echo "test push8u32"
$STASM <<EOF
push8u32 0xff
push8u32 0x00
push8u32 0x01
halt
EOF
$STEM -d x.hex a.stb
grep "000000003ffffff0: 00 00 00 00 01 00 00 00 00 00 00 00 ff 00 00 00" x.hex > /dev/null

#
# Test push8u64
#
echo "test push8u64"
$STASM <<EOF
push8u64 0xff
push8u64 0x00
push8u64 0x01
halt
EOF
$STEM -d x.hex a.stb
grep "000000003fffffe0: 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00" x.hex > /dev/null
grep "000000003ffffff0: 00 00 00 00 00 00 00 00 ff 00 00 00 00 00 00 00" x.hex > /dev/null

echo all tests passed
