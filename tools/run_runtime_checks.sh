#!/usr/bin/env bash
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p -- "$repo/build/checks"
g++ -m32 -std=c++17 -O2 -ffreestanding -fno-builtin -fno-pie -fno-stack-protector \
    -fno-exceptions -fno-rtti -nostdlib -static -no-pie -Wl,-e,_start \
    "$repo/tools/tests/runtime_checks.cpp" "$repo/gamecube/debug_menu_core.cpp" \
    "$repo/gamecube/gc_input.cpp" -o "$repo/build/checks/runtime_checks"
"$repo/build/checks/runtime_checks"
for suite in diagnostics settings mission progression spawn scene engine camera ai_export replay devopt; do
    dependencies=()
    if [[ "$suite" == diagnostics || "$suite" == progression || "$suite" == camera || "$suite" == devopt ]]; then
        dependencies+=("$repo/gamecube/debug_menu_core.cpp")
    fi
    g++ -m32 -std=c++17 -O2 -Wall -Wextra -Werror -fno-builtin \
        -fno-exceptions -fno-rtti -fno-stack-protector -nostdlib -static -no-pie \
        -ffunction-sections -fdata-sections -Wl,--gc-sections "-I$repo/gamecube" \
        "$repo/tools/tests/${suite}_checks.cpp" "${dependencies[@]}" \
        -o "$repo/build/checks/${suite}_checks"
    "$repo/build/checks/${suite}_checks"
done
