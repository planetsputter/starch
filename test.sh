#!/usr/bin/bash

# Run trap on error, even within functions.
# Expansion of undefined variables is an error.
set -Eeu

trap 'echo test failed on line $LINENO' ERR

# Make test directory
mkdir -p test
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
cat > test.st <<EOF
.section 0x1000
invalid
push8 0
push8u16 0
push8u32 0
push8u64 0
push8i16 0
push8i32 0
push8i64 0
push16 0
push16u32 0
push16u64 0
push16i32 0
push16i64 0
push32 0
push32u64 0
push32i64 0
push64 0
pop8
pop16
pop32
pop64
dup8
dup16
dup32
dup64
set8
set16
set32
set64
prom8u16
prom8u32
prom8u64
prom8i16
prom8i32
prom8i64
prom16u32
prom16u64
prom16i32
prom16i64
prom32u64
prom32i64
dem64to16
dem64to8
dem32to8
add8
add16
add32
add64
sub8
sub16
sub32
sub64
subr8
subr16
subr32
subr64
mulu8
mulu16
mulu32
mulu64
muli8
muli16
muli32
muli64
divu8
divu16
divu32
divu64
divru8
divru16
divru32
divru64
divi8
divi16
divi32
divi64
divri8
divri16
divri32
divri64
modu8
modu16
modu32
modu64
modru8
modru16
modru32
modru64
modi8
modi16
modi32
modi64
modri8
modri16
modri32
modri64
lshift8
lshift16
lshift32
lshift64
rshiftu8
rshiftu16
rshiftu32
rshiftu64
rshifti8
rshifti16
rshifti32
rshifti64
band8
band16
band32
band64
bor8
bor16
bor32
bor64
bxor8
bxor16
bxor32
bxor64
binv8
binv16
binv32
binv64
land8
land16
land32
land64
lor8
lor16
lor32
lor64
linv8
linv16
linv32
linv64
ceq8
ceq16
ceq32
ceq64
cne8
cne16
cne32
cne64
cgtu8
cgtu16
cgtu32
cgtu64
cgti8
cgti16
cgti32
cgti64
cltu8
cltu16
cltu32
cltu64
clti8
clti16
clti32
clti64
cgeu8
cgeu16
cgeu32
cgeu64
cgei8
cgei16
cgei32
cgei64
cleu8
cleu16
cleu32
cleu64
clei8
clei16
clei32
clei64
call 0
calls
ret
jmp 0
jmps
rjmpi8 0
rjmpi16 0
rjmpi32 0
brnz8 0
brnz16 0
brnz32 0
brnz64 0
rbrnz8i8 0
rbrnz8i16 0
rbrnz8i32 0
rbrnz16i8 0
rbrnz16i16 0
rbrnz16i32 0
rbrnz32i8 0
rbrnz32i16 0
rbrnz32i32 0
rbrnz64i8 0
rbrnz64i16 0
rbrnz64i32 0
load8
load16
load32
load64
loadpop8
loadpop16
loadpop32
loadpop64
loadsfp8
loadsfp16
loadsfp32
loadsfp64
loadpopsfp8
loadpopsfp16
loadpopsfp32
loadpopsfp64
store8
store16
store32
store64
storepop8
storepop16
storepop32
storepop64
storesfp8
storesfp16
storesfp32
storesfp64
storepopsfp8
storepopsfp16
storepopsfp32
storepopsfp64
storer8
storer16
storer32
storer64
storerpop8
storerpop16
storerpop32
storerpop64
storersfp8
storersfp16
storersfp32
storersfp64
storerpopsfp8
storerpopsfp16
storerpopsfp32
storerpopsfp64
setsbp 0
setsfp 0
setsp 0
setslp 0
halt
ext
nop
EOF

echo assembling all opcodes
$STASM test.st
echo disassembling all opcodes
$DISTASM a.stb -o dis.st
echo checking for symmetric dis/assembly
cmp test.st dis.st

echo all tests passed
