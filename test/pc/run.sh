#!/bin/sh
# Build + chay test phan C tren PC. Chay tu thu muc goc repo: sh test/pc/run.sh
set -e
D=$(dirname "$0")
R="$D/../.."
gcc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -I"$D" -I"$R/test/stubs" -I"$R/Core/Inc" \
    "$R/Core/Src/lcd.c" "$R/Core/Src/dht20.c" "$R/Core/Src/display.c" "$R/Core/Src/fsm_tuning.c" \
    "$D/fake_hw.c" "$D/fake_ab.c" "$D/test_c.c" -o "$D/test_c"
"$D/test_c"
