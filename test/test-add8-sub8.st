; test-add8-sub8.st

.include "sys.st"

; Memory map
; 0x1000 - 0x2000: IO
; 0x2000 - 0x3000: program
; 0x3000 - 0x4000: stack

.section $INIT_PC_VAL

setsbp 0x4000
setsfp 0x4000
setsp  0x4000
setslp 0x3000

:test_add8_sub8

; Test that add8 has a zero identity
push64 $URAND_ADDR
loadpop8    ; a8
dup8        ; a8, a8
dup8        ; a8, a8, a8
push8 0     ; a8, a8, a8, 0_8
add8        ; a8, a8, a8 + 0_8
ceq8        ; a8, a8 == a8 + 0_8
push64 $ASSERT_ADDR
storerpop8  ; assert(a8 == a8 + 0_8)

; Test that sub8 has a zero identity
pop8        ; a8
dup8        ; a8, a8
dup8        ; a8, a8, a8
push8 0     ; a8, a8, a8, 0_8
sub8        ; a8, a8, a8 - 0_8
ceq8        ; a8, a8 == a8 - 0_8
push64 $ASSERT_ADDR
storerpop8  ; assert(a8 == a8 - 0_8)
pop8        ; a8

; Test that add8 is inverse to sub8
push64 $URAND_ADDR
loadpop8    ; a8, b8
push64 -1
loadpopsfp8 ; a8, b8, a8
dup8        ; a8, b8, a8, a8
push64 -2
loadpopsfp8 ; a8, b8, a8, a8, b8
add8        ; a8, b8, a8, a8 + b8
push64 -2
loadpopsfp8 ; a8, b8, a8, a8 + b8, b8
sub8        ; a8, b8, a8, a8 + b8 - b8
ceq8        ; a8, b8, a8 == a8 + b8 - b8
push64 $ASSERT_ADDR
storerpop8  ; assert(a8 == a8 + b8 - b8)
pop8        ; a8, b8

; Test that add8 is commutative
push64 -1
loadpopsfp8 ; a8, b8, a8
push64 -2
loadpopsfp8 ; a8, b8, a8, b8
add8        ; a8, b8, a8 + b8
push64 -2
loadpopsfp8 ; a8, b8, a8 + b8, b8
push64 -1
loadpopsfp8 ; a8, b8, a8 + b8, b8, a8
add8        ; a8, b8, a8 + b8, b8 + a8
ceq8        ; a8, b8, a8 + b8 == b8 + a8
push64 $ASSERT_ADDR
storerpop8  ; assert(a8 + b8 == b8 + a8)

halt
