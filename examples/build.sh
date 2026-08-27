#!/usr/bin/env bash
#
# This script builds the example Starch programs when run in the "examples" directory
# after building stasm. @todo: Replace this script with an optional target in build.cfg.

STASM=../stasm/bin/stasm

$STASM -I../sta/inc -o hello-world.stb hello-world.sta
$STASM -I../sta/inc -o fibonacci.stb fibonacci.sta

# After the build, each program can be emulated by passing the "stb" file as an argument
# to stem. E.g.,
#   stem/bin/stem hello-world.stb
