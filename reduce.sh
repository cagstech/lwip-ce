#!/bin/sh
/home/john/installs/CEdev/bin/ez80-clang -Wfatal-errors -S -MD -nostdinc -fno-threadsafe-statics -Xclang -fforce-mangle-main-argc-argv -mllvm -profile-guided-section-prefix=false -DNDEBUG  -g0 -Wall -Wextra -Oz buggy.c
grep 'iy + 128' buggy.s
