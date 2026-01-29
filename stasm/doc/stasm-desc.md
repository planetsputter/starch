Stasm (Starch Assembler)
========================

Introduction
------------

Stasm is an assembler for the Starch instruction set. It parses a text file in Starch assembly format and produces a binary in stub format. The binary can be emulated by the Starch emulator, stem.

Usage
-----

For usage information, run `stasm --help`.

Assembly Format
---------------

Each line of the text input file is processed by the assembler as a comment, a statement, a label definition, or an assembler command.

### Comments

Comments in Starch assembly begin with a semicolon (';') and continue until the end of the line. A comment may finish a line started by any other kind of statement, for instance a label or assembler command. There is no syntax for multi-line comments.

Example:
```
; This is a comment
:label ; This is a comment following a label definition
```

### Statements

Statements consist of an opcode or pseudo-op followed by an optional argument. Whether or not the argument is present depends on the opcode. The argument may be an integer or string literal or a label usage, again depending on the opcode. The label usage must start with a colon (':'). Typical C-style syntax for integer, character, and string literals is supported.

Example:
```
push8asu64 0
push16asu64 0x100
add64
push64as64 "string literal"
call :some_function
```
For a full list of Starch opcodes, see [../../starch/doc/starch-desc.md](../../starch/doc/starch-desc.md).

#### Pseudo-ops

Psuedo-ops may take the place of an opcode in a Starch assembly statement and are designed to make writing assembly easier. Psuedo-ops may evaluate to a different opcode depending on the value of their argument.

For instance, the following code utilizes the `push64` pseudo-op and evaluates to the same instructions as the example above:
```
push64 0
push64 0x100
add64
push64 "string literal"
call :some_function
```

In each case the pseudo-op resolves to the opcode which requires the smallest immediate value while preserving signedness.

Starch assembly pseudo-ops:
| Psuedo-op | Description |
|:--------- |:----------- |
| push8     | Evaluates to push8as8. |
| push16    | Evaluates to push8asu16, push8asi16, or push16as16. |
| push32    | Evaluates to push8asu32, push8asi32, push16asu32, push16asi32, or push32as32. |
| push64    | Evaluates to push8asu64, push8asi64, push16asu64, push16asi64, push32asu64, push32asi64, or push64as64. |
| brz8      | Evaluates to rbrz8i8, rbrz8i16, or rbrz8i32. |
| brz16     | Evaluates to rbrz16i8, rbrz16i16, or rbrz16i32. |
| brz32     | Evaluates to rbrz32i8, rbrz32i16, or rbrz32i32. |
| brz64     | Evaluates to rbrz64i8, rbrz64i16, or rbrz64i32. |
| rjmp      | Evaluates to rjmpi8, rjmpi16, or rjmpi32. |

### Bracket notation

The push pseudo-ops can take an operand in brackets. In this form the pseudo-op actually generates two instructions, one to push the offset within brackets and another to load data from that offset. If the offset within brackets begins with the keyword "SFP", it is relative to the stack frame pointer. Otherwise it is an absolute address.

Examples:
```
push8 [:some_label]
; Evaluates to
push64 :some_label
loadpop8

push8 [SFP + 10]
; Evaluates to
push64 10
loadpopsfp8

push32 [SFP]
; Evaluates to
push64 0
loadpopsfp32
```

This allows the developer to more consisely write the common operation of bringing a local variable or function parameter to the top of the stack.

### Label Definitions

A label definition begins with a colon (':') and appears at the beginning of a line. This distinguishes it from a label _usage_, which appears as an argument in a statement. A statement may occur on the same line after a label definition. A label definition causes the compiler to associate the current address with the given label. It then substitutes the current address value for all previous and future usages of that label.

Labels are commonly used to assign names to function and data addresses. In the following example, `:some_data` and `:some_function` are label definitions while `:some_other_function` is a label usage:
```
:some_data data64 0

:some_function
call :some_other_function
ret
```

All labels which are used in the program must be defined by the end of a Starch assembler input, or the program is ill-formed.

### Assembler Commands

The Starch assembler supports several assembler commands which aid the developer in various ways.

| Assembler Command | Description |
|:----------------- |:----------- |
| define \<name\> \<value\> | This command allows the developer to define a symbolic constant whose value is substituted where it is used later in the program when preceded by a dollar sign ('$'). |
| section \<address\> | Starts a new section at the given address. |
| include "\<filename\>" | Includes the contents of the file at the quoted path as if inserted into this point in the input. |
| data8 \<value\> | Inserts the given value into the output as an 8-bit integer. |
| data16 \<value\> | Inserts the given value into the output as a 16-bit integer. |
| data32 \<value\> | Inserts the given value into the output as a 32-bit integer. |
| data64 \<value\> | Inserts the given value into the output as a 64-bit integer. |
| strings | Inserts data for all unemitted string literals at the current position. If string literals are used, this command must be appear in the assembly source after all string literal usages. |

## Symbols

As noted above, the `define` assembler command can be used to define a symbolic constant. These constants are arbitrary text words which will be substituted into the input when used later preceded by a dollar sign ('$'). Symbolic constants can be used to define constant integer values as well as opcodes, labels, and string literals.

The following example defines and uses a symbolic constant:
```
define MAX_REPS 0x1000
push64 $MAX_REPS
```

Symbols may not be defined more than once. Symbol redefinition will result in an error.

### Auto-Symbols

The Starch assembler recognizes some automatic symbols which have a default value when used if not defined by the user. Each Starch interrupt name can be used as an automatic symbol which evaluates to the interrupt number (for instance `$STINT_DIV_BY_ZERO`). Each Starch opcode name can be used as an automatic symbol which evaluates to the numeric value of the opcode when capitalized and preceded by "OP_" (for instance `$OP_HALT`). Important IO addresses also have automatic symbols (for instance `$IO_STDOUT_ADDR`).

The following example uses some automatic symbols:
```
push8 $STINT_ASSERT_FAILURE ; Pushes the number of the assertion failure interrupt as an 8-bit integer
push8 $OP_HALT              ; Pushes the value of the "halt" opcode as an 8-bit integer
push64 $IO_STDOUT_ADDR      ; Pushes the stdout IO address as a 64-bit integer
```

Once used, an auto-symbol must not be defined manually. An auto-symbol may be defined manually before its first usage, in which case the defined value will be used instead of the automatic value.

Example Files
-------------

For examples of Starch assembler input files, review the files with extention ".sta" in the test directory of the repository. See also the files in the examples directory.
