Starch (Stack Architecture)
===========================

Introduction
------------

Starch is a theoretical stack-oriented computer architecture. Most instructions consist of a single 8-bit byte and operate on memory on a stack. Memory outside the stack can be accessed in a load & store fashion. Addressing is byte-oriented as is common in modern architectures (that is to say, a byte is the smallest addressable quantity). Addresses are 64 bits wide. Two's complement notation is used to represent integers. The bytes in multi-byte integers are arranged in little-endian order (the byte at the lowest address is the least significant).

Processor State
---------------

The processor state is small, consisting of the following registers:

| Register | Description          |
|:-------- |:-------------------- |
| PC       | Program Counter      |
| SBP      | Stack Bottom Pointer |
| SFP      | Stack Frame Pointer  |
| SP       | Stack Pointer        |
| SLP      | Stack Limit Pointer  |

### Stack Diagram

The stack grows downward with the "top" of the stack being at the lowest address and the "bottom" of the stack being at the highest address. The stack top address is stored in the stack pointer register (SP). The stack bottom address is stored in the stack bottom pointer register (SBP). The uppermost limit of stack data is stored in the stack limit pointer register (SLP).

Though the stack is a contiguous region of memory it is logically segmented into stack frames, each of which contains values used in the execution of a single function invocation. When one function calls another a new stack frame is created for the called function. When the called function returns the stack frame is popped. The stack frame pointer register (SFP) keeps track of the address just above the current stack frame. The return address (RETA) and the previous stack frame pointer (PSFP) are stored at the SFP address.

To call a function that takes arguments, arguments are pushed onto the stack beginning with the last (ARGn) and ending with the first (ARG1). This arrangement allows variadic functions to be called easily. Before a function returns, the return value (RETV) is stored at the bottom (highest address) of its stack frame.

| Address | Data                  |
|:------- |:--------------------- |
|         | Calling function data |
|         | ARGn                  |
|         | ...                   |
| SFP+16  | ARG1                  |
| SFP+8   | PSFP                  |
| SFP     | RETA                  |
|         | RETV                  |
|         | Called function data  |

Note that while function arguments are pushed to the stack from last to first, the last argument pushed being thought of as the "leftmost", individual instructions which perform a non-commutative operation such as subtraction typically consider the last argument pushed to be the "rightmost". This is to be more similar to postfix notation. These instructions also typically have an order-reversed variant.

Functions are prohibited by the memory management unit from accessing (reading or writing) stack frame control data, that is, data at addresses from SFP to SFP + 15, inclusively. This helps prevent certain classes of attacks which seek to overwrite the return pointer of a function and transfer control to insecure code.

Instruction Set
---------------

Here we document all the instructions defined by Starch. Each instruction is a single byte. Most instructions have variants that operate on 64-, 32-, 16-, and 8-bit operands.

The following conventions are used in this section:
 * When a word ends in a number, the number represents the number of bits. When the number is preceded by a "u" it represents an unsigned integer value. When it is preceded by an "i" it represents a signed integer value. If the preceding letter is neither "u" nor "i", the signedness is not important to the operation of the instruction.
 * The word "as" is used to indicate promotion of the previous value to the size of the following value. The signedness of the following value is used. When promoting to a signed value, sign extension is performed.
 * When a value is surrounded by parenthesis, it represents the value at the address specified within the parenthesis. For instance, (PC)8 would be the 8-bit value at the PC address, which is the current instruction.
 * When it is desired to represent separate elements on the stack they are written separated by commas. The leftmost item as written is closest to the bottom of the stack. The letters "a", "b", "c", and "d" are used to represent arbitrary data.
 * All symbolic operators that appear in the following tables function exactly as they do in the C language.
 * The @ symbol is used to indicate the address at which a value resides if that is significant to the operation being performed.

### Push Operations

These operations are used to push immediate data (data in program memory) onto the stack.

| Op Code     | PC After | Stack Before | Stack After     |
|:----------- |:-------- |:------------ |:--------------- |
| push8       | PC+2     |              | (PC+1)8         |
| push8asu16  | PC+2     |              | (PC+1)8 as u16  |
| push8asu32  | PC+2     |              | (PC+1)8 as u32  |
| push8asu64  | PC+2     |              | (PC+1)8 as u64  |
| push8asi16  | PC+2     |              | (PC+1)8 as i16  |
| push8asi32  | PC+2     |              | (PC+1)8 as i32  |
| push8asi64  | PC+2     |              | (PC+1)8 as i64  |
| push16      | PC+3     |              | (PC+1)16        |
| push16asu32 | PC+3     |              | (PC+1)16 as u32 |
| push16asu64 | PC+3     |              | (PC+1)16 as u64 |
| push16asi32 | PC+3     |              | (PC+1)16 as i32 |
| push16asi64 | PC+3     |              | (PC+1)16 as i64 |
| push32      | PC+5     |              | (PC+1)32        |
| push32asu64 | PC+5     |              | (PC+1)32 as u64 |
| push32asi64 | PC+5     |              | (PC+1)32 as i64 |
| push64      | PC+9     |              | (PC+1)64        |

### Pop Operations

These operations are used to pop data from the stack.

| Op Code | PC After | Stack Before | Stack After |
|:------- |:-------- |:------------ |:----------- |
| pop8    | PC+1     | a8           |             |
| pop16   | PC+1     | a16          |             |
| pop32   | PC+1     | a32          |             |
| pop64   | PC+1     | a64          |             |

### Duplication Operations

These operations duplicate the value currently at the top of the stack.

| Op Code | PC After | Stack Before | Stack After |
|:------- |:-------- |:------------ |:----------- |
| dup8    | PC+1     | a8           | a8,  a8     |
| dup16   | PC+1     | a16          | a16, a16    |
| dup32   | PC+1     | a32          | a32, a32    |
| dup64   | PC+1     | a64          | a64, a64    |

### Set Operations

These operations copy the value at the top of the stack down to the position below it, effectively setting the value at that position, and pop the original value from the top of the stack.

| Op Code | PC After | Stack Before | Stack After |
|:------- |:-------- |:------------ |:----------- |
| set8    | PC+1     | a8,  b8      | b8          |
| set16   | PC+1     | a16, b16     | b16         |
| set32   | PC+1     | a32, b32     | b32         |
| set64   | PC+1     | a64, b64     | b64         |

### Promotion and Demotion Operations

These operations promote or demote the integer value at the top of the stack to a larger or smaller size respectively. Promotion can be done in a signed or unsigned manner. Demotion is done by truncation. Some demotions can be accomplished with a pop and are not included here.

| Op Code   | PC After | Stack Before | Stack After |
|:--------- |:-------- |:------------ |:----------- |
| prom8u16  | PC+1     | a8           | a8  as u16  |
| prom8u32  | PC+1     | a8           | a8  as u32  |
| prom8u64  | PC+1     | a8           | a8  as u64  |
| prom8i16  | PC+1     | a8           | a8  as i16  |
| prom8i32  | PC+1     | a8           | a8  as i32  |
| prom8i64  | PC+1     | a8           | a8  as i64  |
| prom16u32 | PC+1     | a16          | a16 as u32  |
| prom16u64 | PC+1     | a16          | a16 as u64  |
| prom16i32 | PC+1     | a16          | a16 as i32  |
| prom16i64 | PC+1     | a16          | a16 as i64  |
| prom32u64 | PC+1     | a32          | a32 as u64  |
| prom32i64 | PC+1     | a32          | a32 as i64  |
| dem64to16 | PC+1     | a64          | a64 as  16  |
| dem64to8  | PC+1     | a64          | a64 as   8  |
| dem32to8  | PC+1     | a32          | a32 as   8  |

### Integer Arithmetic Operations

These operations perform arithmetic operations on integer operands at the top of the stack. The signedness of the operands does not affect integer addition and subtraction when using two's complement notation. Other operations have signed and unsigned variants. Operations that are not commutative have an order-reversed variant indicated by an "r" in the op code name.

| Op Code   | PC After | Stack Before | Stack After |
|:--------- |:-------- |:------------ |:----------- |
| add8      | PC+1     |   a8,   b8   |   a8 +   b8 |
| add16     | PC+1     |  a16,  b16   |  a16 +  b16 |
| add32     | PC+1     |  a32,  b32   |  a32 +  b32 |
| add64     | PC+1     |  a64,  b64   |  a64 +  b64 |
| sub8      | PC+1     |   a8,   b8   |   a8 -   b8 |
| sub16     | PC+1     |  a16,  b16   |  a16 -  b16 |
| sub32     | PC+1     |  a32,  b32   |  a32 -  b32 |
| sub64     | PC+1     |  a64,  b64   |  a64 -  b64 |
| subr8     | PC+1     |   a8,   b8   |   b8 -   a8 |
| subr16    | PC+1     |  a16,  b16   |  b16 -  a16 |
| subr32    | PC+1     |  a32,  b32   |  b32 -  a32 |
| subr64    | PC+1     |  a64,  b64   |  b64 -  a64 |
| mulu8     | PC+1     |  au8,  bu8   |  au8 *  bu8 |
| mulu16    | PC+1     | au16, bu16   | au16 * bu16 |
| mulu32    | PC+1     | au32, bu32   | au32 * bu32 |
| mulu64    | PC+1     | au64, bu64   | au64 * bu64 |
| muli8     | PC+1     |  ai8,  bi8   |  ai8 *  bi8 |
| muli16    | PC+1     | ai16, bi16   | ai16 * bi16 |
| muli32    | PC+1     | ai32, bi32   | ai32 * bi32 |
| muli64    | PC+1     | ai64, bi64   | ai64 * bi64 |
| divu8     | PC+1     |  au8,  bu8   |  au8 /  bu8 |
| divu16    | PC+1     | au16, bu16   | au16 / bu16 |
| divu32    | PC+1     | au32, bu32   | au32 / bu32 |
| divu64    | PC+1     | au64, bu64   | au64 / bu64 |
| divru8    | PC+1     |  au8,  bu8   |  bu8 /  au8 |
| divru16   | PC+1     | au16, bu16   | bu16 / au16 |
| divru32   | PC+1     | au32, bu32   | bu32 / au32 |
| divru64   | PC+1     | au64, bu64   | bu64 / au64 |
| divi8     | PC+1     |  ai8,  bi8   |  ai8 /  bi8 |
| divi16    | PC+1     | ai16, bi16   | ai16 / bi16 |
| divi32    | PC+1     | ai32, bi32   | ai32 / bi32 |
| divi64    | PC+1     | ai64, bi64   | ai64 / bi64 |
| divri8    | PC+1     |  ai8,  bi8   |  bi8 /  ai8 |
| divri16   | PC+1     | ai16, bi16   | bi16 / ai16 |
| divri32   | PC+1     | ai32, bi32   | bi32 / ai32 |
| divri64   | PC+1     | ai64, bi64   | bi64 / ai64 |
| modu8     | PC+1     |  au8,  bu8   |  au8 %  bu8 |
| modu16    | PC+1     | au16, bu16   | au16 % bu16 |
| modu32    | PC+1     | au32, bu32   | au32 % bu32 |
| modu64    | PC+1     | au64, bu64   | au64 % bu64 |
| modru8    | PC+1     |  au8,  bu8   |  bu8 %  au8 |
| modru16   | PC+1     | au16, bu16   | bu16 % au16 |
| modru32   | PC+1     | au32, bu32   | bu32 % au32 |
| modru64   | PC+1     | au64, bu64   | bu64 % au64 |
| modi8     | PC+1     |  ai8,  bi8   |  ai8 %  bi8 |
| modi16    | PC+1     | ai16, bi16   | ai16 % bi16 |
| modi32    | PC+1     | ai32, bi32   | ai32 % bi32 |
| modi64    | PC+1     | ai64, bi64   | ai64 % bi64 |
| modri8    | PC+1     |  ai8,  bi8   |  bi8 %  ai8 |
| modri16   | PC+1     | ai16, bi16   | bi16 % ai16 |
| modri32   | PC+1     | ai32, bi32   | bi32 % ai32 |
| modri64   | PC+1     | ai64, bi64   | bi64 % ai64 |

### Bitwise Shift Operations

Left-shift operations are equivalent for signed and unsigned operands. Right-shift operations differ for signed and unsigned operands.

| Op Code   | PC After | Stack Before | Stack After |
|:--------- |:-------- |:------------ |:----------- |
| lshift8   | PC+1     |   a8, b8     |   a8 << b8  |
| lshift16  | PC+1     |  a16, b8     |  a16 << b8  |
| lshift32  | PC+1     |  a32, b8     |  a32 << b8  |
| lshift64  | PC+1     |  a64, b8     |  a64 << b8  |
| rshiftu8  | PC+1     |  au8, b8     |  au8 >> b8  |
| rshiftu16 | PC+1     | au16, b8     | au16 >> b8  |
| rshiftu32 | PC+1     | au32, b8     | au32 >> b8  |
| rshiftu64 | PC+1     | au64, b8     | au64 >> b8  |
| rshifti8  | PC+1     |  ai8, b8     |  ai8 >> b8  |
| rshifti16 | PC+1     | ai16, b8     | ai16 >> b8  |
| rshifti32 | PC+1     | ai32, b8     | ai32 >> b8  |
| rshifti64 | PC+1     | ai64, b8     | ai64 >> b8  |

### Bitwise Logical Operations

All the bitwise logical operations supported by the C language have an associated set of instructions. Signedness does not affect these operations.

| Op Code   | PC After | Stack Before | Stack After |
|:--------- |:-------- |:------------ |:----------- |
| band8     | PC+1     |  a8, b8      |   a8  & b8  |
| band16    | PC+1     | a16, b8      |  a16  & b16 |
| band32    | PC+1     | a32, b8      |  a32  & b32 |
| band64    | PC+1     | a64, b8      |  a64  & b64 |
| bor8      | PC+1     |  a8, b8      |   a8 \| b8  |
| bor16     | PC+1     | a16, b8      |  a16 \| b16 |
| bor32     | PC+1     | a32, b8      |  a32 \| b32 |
| bor64     | PC+1     | a64, b8      |  a64 \| b64 |
| bxor8     | PC+1     |  a8, b8      |   a8  ^ b8  |
| bxor16    | PC+1     | a16, b8      |  a16  ^ b16 |
| bxor32    | PC+1     | a32, b8      |  a32  ^ b32 |
| bxor64    | PC+1     | a64, b8      |  a64  ^ b64 |
| binv8     | PC+1     |  a8          |  ~a8        |
| binv16    | PC+1     | a16          | ~a16        |
| binv32    | PC+1     | a32          | ~a32        |
| binv64    | PC+1     | a64          | ~a64        |

### Boolean Logical Operations

Boolean logical operations result in a Boolean value, either a zero or a one, replacing the operand(s).

| Op Code   | PC After | Stack Before | Stack After   |
|:--------- |:-------- |:------------ |:------------- |
| land8     | PC+1     |  a8,  b8     |   a8   &&  b8 |
| land16    | PC+1     | a16, b16     |  a16   && b16 |
| land32    | PC+1     | a32, b32     |  a32   && b32 |
| land64    | PC+1     | a64, b64     |  a64   && b64 |
| lor8      | PC+1     |  a8,  b8     |   a8 \|\|  b8 |
| lor16     | PC+1     | a16, b16     |  a16 \|\| b16 |
| lor32     | PC+1     | a32, b32     |  a32 \|\| b32 |
| lor64     | PC+1     | a64, b64     |  a64 \|\| b64 |
| linv8     | PC+1     |  a8          |  !a8          |
| linv16    | PC+1     | a16          | !a16          |
| linv32    | PC+1     | a32          | !a32          |
| linv64    | PC+1     | a64          | !a64          |

### Comparison Operations

These instructions result in a Boolean value, either a zero or a one, replacing the two operands. They can be used to calculate the condition for branching instructions. In some cases signedness of the operands is significant.

| Op Code   | PC After | Stack Before | Stack After  |
|:--------- |:-------- |:------------ |:------------ |
|    ceq8   | PC + 1   |   a8,   b8   |   a8 ==   b8 |
|   ceq16   | PC + 1   |  a16,  b16   |  a16 ==  b16 |
|   ceq32   | PC + 1   |  a32,  b32   |  a32 ==  b32 |
|   ceq64   | PC + 1   |  a64,  b64   |  a64 ==  b64 |
|    cne8   | PC + 1   |   a8,   b8   |   a8 !=   b8 |
|   cne16   | PC + 1   |  a16,  b16   |  a16 !=  b16 |
|   cne32   | PC + 1   |  a32,  b32   |  a32 !=  b32 |
|   cne64   | PC + 1   |  a64,  b64   |  a64 !=  b64 |
|   cgtu8   | PC + 1   |  au8,  bu8   |  au8  >  bu8 |
|  cgtu16   | PC + 1   | au16, bu16   | au16  > bu16 |
|  cgtu32   | PC + 1   | au32, bu32   | au32  > bu32 |
|  cgtu64   | PC + 1   | au64, bu64   | au64  > bu64 |
|   cgti8   | PC + 1   |  ai8,  bi8   |  ai8  >  bi8 |
|  cgti16   | PC + 1   | ai16, bi16   | ai16  > bi16 |
|  cgti32   | PC + 1   | ai32, bi32   | ai32  > bi32 |
|  cgti64   | PC + 1   | ai64, bi64   | ai64  > bi64 |
|   cltu8   | PC + 1   |  au8,  bu8   |  au8  <  bu8 |
|  cltu16   | PC + 1   | au16, bu16   | au16  < bu16 |
|  cltu32   | PC + 1   | au32, bu32   | au32  < bu32 |
|  cltu64   | PC + 1   | au64, bu64   | au64  < bu64 |
|   clti8   | PC + 1   |  ai8,  bi8   |  ai8  <  bi8 |
|  clti16   | PC + 1   | ai16, bi16   | ai16  < bi16 |
|  clti32   | PC + 1   | ai32, bi32   | ai32  < bi32 |
|  clti64   | PC + 1   | ai64, bi64   | ai64  < bi64 |
|  cgteu8   | PC + 1   |  au8,  bu8   |  au8 >=  bu8 |
| cgteu16   | PC + 1   | au16, bu16   | au16 >= bu16 |
| cgteu32   | PC + 1   | au32, bu32   | au32 >= bu32 |
| cgteu64   | PC + 1   | au64, bu64   | au64 >= bu64 |
|  cgtei8   | PC + 1   |  ai8,  bi8   |  ai8 >=  bi8 |
| cgtei16   | PC + 1   | ai16, bi16   | ai16 >= bi16 |
| cgtei32   | PC + 1   | ai32, bi32   | ai32 >= bi32 |
| cgtei64   | PC + 1   | ai64, bi64   | ai64 >= bi64 |
|  clteu8   | PC + 1   |  au8,  bu8   |  au8 <=  bu8 |
| clteu16   | PC + 1   | au16, bu16   | au16 <= bu16 |
| clteu32   | PC + 1   | au32, bu32   | au32 <= bu32 |
| clteu64   | PC + 1   | au64, bu64   | au64 <= bu64 |
|  cltei8   | PC + 1   |  ai8,  bi8   |  ai8 <=  bi8 |
| cltei16   | PC + 1   | ai16, bi16   | ai16 <= bi16 |
| cltei32   | PC + 1   | ai32, bi32   | ai32 <= bi32 |
| cltei64   | PC + 1   | ai64, bi64   | ai64 <= bi64 |

### Function Call and Return Instructions

These instructions call functions and return from them. Variants with an "s" use an address on the stack.

| Op Code   | PC After   | Stack Before                  | Stack After     | SFP After   |
|:--------- |:---------- |:----------------------------- |:--------------- |:----------- |
| call      | (PC + 1)64 |                               | SFP64, PC64 + 9 | SP - 16     |
| calls     | a64        | a64                           | SFP64, PC64 + 1 | SP - 8      |
| ret       | (SFP)64    | PSFP64, RETA64 @ SFP64, [...] |                 | (SFP + 8)64 |

### Jump Instructions

These instructions can transfer control flow to a non-sequential instruction.

| Op Code   | PC After         | Stack Before | Stack After |
|:--------- |:---------------- |:------------ |:----------- |
| jmp       | (PC + 1)64       |              |             |
| jmps      | a64              | a64          |             |
|  rjmpi8   | PC +  (PC + 1)i8 |              |             |
| rjmpi16   | PC + (PC + 1)i16 |              |             |
| rjmpi32   | PC + (PC + 1)i32 |              |             |

### Branching Instructions

These instructions will transfer control flow to a non-sequential instruction if their argument is zero.

| Op Code   | PC After                                  | Stack Before | Stack After |
|:--------- |:----------------------------------------- |:------------ |:----------- |
|  brz8     | if  !a8 then (PC + 1)64 else PC + 9       |  a8          |             |
| brz16     | if !a16 then (PC + 1)64 else PC + 9       | a16          |             |
| brz32     | if !a32 then (PC + 1)64 else PC + 9       | a32          |             |
| brz64     | if !a64 then (PC + 1)64 else PC + 9       | a64          |             |
|  rbrz8i8  | if  !a8 then PC + (PC + 1)i8  else PC + 2 |  a8          |             |
|  rbrz8i16 | if  !a8 then PC + (PC + 1)i16 else PC + 3 |  a8          |             |
|  rbrz8i32 | if  !a8 then PC + (PC + 1)i32 else PC + 5 |  a8          |             |
| rbrz16i8  | if !a16 then PC + (PC + 1)i8  else PC + 2 | a16          |             |
| rbrz16i16 | if !a16 then PC + (PC + 1)i16 else PC + 3 | a16          |             |
| rbrz16i32 | if !a16 then PC + (PC + 1)i32 else PC + 5 | a16          |             |
| rbrz32i8  | if !a32 then PC + (PC + 1)i8  else PC + 2 | a32          |             |
| rbrz32i16 | if !a32 then PC + (PC + 1)i16 else PC + 3 | a32          |             |
| rbrz32i32 | if !a32 then PC + (PC + 1)i32 else PC + 5 | a32          |             |
| rbrz64i8  | if !a64 then PC + (PC + 1)i8  else PC + 2 | a64          |             |
| rbrz64i16 | if !a64 then PC + (PC + 1)i16 else PC + 3 | a64          |             |
| rbrz64i32 | if !a64 then PC + (PC + 1)i32 else PC + 5 | a64          |             |

### Memory Operations

These instructions load data from memory to the stack or store data from the stack to memory. Instructions with "sfp" use offsets from the stack frame pointer address, SFP. This allows to bring arguments to the current function and local variables within the current function to the top of the stack, because they reside at known offsets from SFP. The offset operand is always 64 bits wide. Opcode names with an additional "r" indicate an order-reversed variant.

| Op Code        | PC After | Stack Before | Stack After          | Side Effect          |
|:-------------- |:-------- |:------------ |:-------------------- |:-------------------- |
|  load8         | PC + 1   | a64          | a64, (a64)8          |                      |
| load16         | PC + 1   | a64          | a64, (a64)16         |                      |
| load32         | PC + 1   | a64          | a64, (a64)32         |                      |
| load64         | PC + 1   | a64          | a64, (a64)64         |                      |
|  loadpop8      | PC + 1   | a64          | (a64)8               |                      |
| loadpop16      | PC + 1   | a64          | (a64)16              |                      |
| loadpop32      | PC + 1   | a64          | (a64)32              |                      |
| loadpop64      | PC + 1   | a64          | (a64)64              |                      |
|  loadsfp8      | PC + 1   | ai64         | ai64, (SFP + ai64)8  |                      |
| loadsfp16      | PC + 1   | ai64         | ai64, (SFP + ai64)16 |                      |
| loadsfp32      | PC + 1   | ai64         | ai64, (SFP + ai64)32 |                      |
| loadsfp64      | PC + 1   | ai64         | ai64, (SFP + ai64)64 |                      |
|  loadpopsfp8   | PC + 1   | ai64         | (SFP + ai64)8        |                      |
| loadpopsfp16   | PC + 1   | ai64         | (SFP + ai64)16       |                      |
| loadpopsfp32   | PC + 1   | ai64         | (SFP + ai64)32       |                      |
| loadpopsfp64   | PC + 1   | ai64         | (SFP + ai64)64       |                      |
|  store8        | PC + 1   | a64,  b8     | a64,  b8             |  (a64)8 =  b8        |
| store16        | PC + 1   | a64, b16     | a64, b16             | (a64)16 = b16        |
| store32        | PC + 1   | a64, b32     | a64, b32             | (a64)32 = b32        |
| store64        | PC + 1   | a64, b64     | a64, b64             | (a64)64 = b64        |
|  storepop8     | PC + 1   | a64,  b8     | a64                  |  (a64)8 =  b8        |
| storepop16     | PC + 1   | a64, b16     | a64                  | (a64)16 = b16        |
| storepop32     | PC + 1   | a64, b32     | a64                  | (a64)32 = b32        |
| storepop64     | PC + 1   | a64, b64     | a64                  | (a64)64 = b64        |
|  storesfp8     | PC + 1   | ai64,  b8    | ai64,  b8            |  (SFP + ai64)8 =  b8 |
| storesfp16     | PC + 1   | ai64, b16    | ai64, b16            | (SFP + ai64)16 = b16 |
| storesfp32     | PC + 1   | ai64, b32    | ai64, b32            | (SFP + ai64)32 = b32 |
| storesfp64     | PC + 1   | ai64, b64    | ai64, b64            | (SFP + ai64)64 = b64 |
|  storepopsfp8  | PC + 1   | ai64,  b8    | ai64                 |  (SFP + ai64)8 =  b8 |
| storepopsfp16  | PC + 1   | ai64, b16    | ai64                 | (SFP + ai64)16 = b16 |
| storepopsfp32  | PC + 1   | ai64, b32    | ai64                 | (SFP + ai64)32 = b32 |
| storepopsfp64  | PC + 1   | ai64, b64    | ai64                 | (SFP + ai64)64 = b64 |
|  storer8       | PC + 1   |  a8, b64     |  a8, b64             |  (b64)8 =  a8        |
| storer16       | PC + 1   | a16, b64     | a16, b64             | (b64)16 = a16        |
| storer32       | PC + 1   | a32, b64     | a32, b64             | (b64)32 = a32        |
| storer64       | PC + 1   | a64, b64     | a64, b64             | (b64)64 = a64        |
|  storerpop8    | PC + 1   |  a8, b64     |  a8                  |  (b64)8 =  a8        |
| storerpop16    | PC + 1   | a16, b64     | a16                  | (b64)16 = a16        |
| storerpop32    | PC + 1   | a32, b64     | a32                  | (b64)32 = a32        |
| storerpop64    | PC + 1   | a64, b64     | a64                  | (b64)64 = a64        |
|  storersfp8    | PC + 1   |  a8, bi64    |  a8, bi64            |  (SFP + bi64)8 =  a8 |
| storersfp16    | PC + 1   | a16, bi64    | a16, bi64            | (SFP + bi64)16 = a16 |
| storersfp32    | PC + 1   | a32, bi64    | a32, bi64            | (SFP + bi64)32 = a32 |
| storersfp64    | PC + 1   | a64, bi64    | a64, bi64            | (SFP + bi64)64 = a64 |
|  storerpopsfp8 | PC + 1   |  a8, bi64    |  a8                  |  (SFP + bi64)8 =  a8 |
| storerpopsfp16 | PC + 1   | a16, bi64    | a16                  | (SFP + bi64)16 = a16 |
| storerpopsfp32 | PC + 1   | a32, bi64    | a32                  | (SFP + bi64)32 = a32 |
| storerpopsfp64 | PC + 1   | a64, bi64    | a64                  | (SFP + bi64)64 = a64 |

### Miscellaneous Operations

| Op Code | PC After | Note                                                       |
|:------- |:-------- |:---------------------------------------------------------- |
| setsbp  | PC + 9   | Sets SBP to the 64-bit immediate value                     |
| setsfp  | PC + 9   | Sets SFP to the 64-bit immediate value                     |
| setsp   | PC + 9   | Sets SP to the 64-bit immediate value                      |
| setslp  | PC + 9   | Sets SLP to the 64-bit immediate value                     |
| nop     | PC + 1   | Performs no operation                                      |
| ext     | PC + 1   | Introduces an extended opcode                              |
| halt    | PC       | Halts the processor                                        |
| invalid |          | Intentionally invalid instruction, may be used for testing |
