#!/usr/bin/env bash
# Freestanding PPC EABI build, usable on Linux and through WSL.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build="$repo/build/gamecube"
input=""
output=""
backup=""
expected=3dd0ad5ede2ef9df27a1aba1be9bf9cd5dfd7a14caa4bb8da396f2574c89565c
dry=()
while (($#)); do
    case "$1" in
        --input) input=$2; shift 2 ;;
        --build-dir) build=$2; shift 2 ;;
        --output) output=$2; shift 2 ;;
        --backup) backup=$2; shift 2 ;;
        --expected-sha256) expected=$2; shift 2 ;;
        --dry-run) dry=(--dry-run); shift ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done
if [[ -z "$input" ]]; then
    input="$repo/main.original.dol"
    [[ -f "$input" ]] || input="$repo/main.dol"
fi
output=${output:-"$build/main_patched.dol"}
backup=${backup:-"$build/main_original.dol"}

# Prefer devkitPPC. The GNU cross compiler also supports the EABI explicitly;
# no Linux startup files, headers, shared libraries or runtime are linked.
local_tools="$repo/build/toolchain/root/usr"
if [[ -n "${DEVKITPPC:-}" ]]; then
    prefix="$DEVKITPPC/bin/powerpc-eabi-"
elif [[ -x /opt/devkitpro/devkitPPC/bin/powerpc-eabi-g++ ]]; then
    prefix=/opt/devkitpro/devkitPPC/bin/powerpc-eabi-
elif command -v powerpc-eabi-g++ >/dev/null; then
    prefix=powerpc-eabi-
elif [[ -x "$local_tools/bin/powerpc-linux-gnu-g++-15" ]]; then
    prefix="$local_tools/bin/powerpc-linux-gnu-"
    export LD_LIBRARY_PATH="$local_tools/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
elif command -v powerpc-linux-gnu-g++ >/dev/null; then
    prefix=powerpc-linux-gnu-
else
    echo 'Install devkitPPC or g++-powerpc-linux-gnu to build the payload.' >&2
    exit 2
fi
cxx="${prefix}g++"
cc="${prefix}gcc"
if [[ "$prefix" == *powerpc-linux-gnu- ]]; then
    if [[ -x "${cxx}-15" ]]; then cxx="${cxx}-15"; cc="${cc}-15"; fi
fi
"$cxx" --version | head -1
common=(-mcpu=750 -mcall-eabi -meabi -mbig-endian -mhard-float -msdata=none
    -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin
    -ffunction-sections -fdata-sections -fno-common -Os -Wall -Wextra
    -DUSM_GAMECUBE=1 "-I$repo/gamecube")
cxxflags=(-std=gnu++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics
    -fno-use-cxa-atexit -fno-unwind-tables -fno-asynchronous-unwind-tables)
mkdir -p -- "$build/obj"
objects=()
sources=("$repo/main.cpp")
while IFS= read -r -d '' file; do sources+=("$file"); done < <(
    find "$repo/gamecube" -type f \( -name '*.cpp' -o -name '*.c' -o -name '*.S' \) -print0 | sort -z)
for file in "${sources[@]}"; do
    relative=${file#"$repo/"}
    object="$build/obj/${relative%.*}.o"
    mkdir -p -- "$(dirname -- "$object")"
    echo "CXX/CC $relative"
    case "$file" in
        *.cpp) "$cxx" "${common[@]}" "${cxxflags[@]}" -c "$file" -o "$object" ;;
        *.c) "$cc" "${common[@]}" -std=gnu11 -c "$file" -o "$object" ;;
        *.S) "$cc" "${common[@]}" -c "$file" -o "$object" ;;
    esac
    objects+=("$object")
done
elf="$build/usm_debug_menu.elf"
"$cxx" -mcpu=750 -mcall-eabi -meabi -mbig-endian -mhard-float -msdata=none \
    -nostdlib -static -no-pie "-Wl,-T,$repo/gamecube/linker.ld" \
    "-Wl,-Map,$build/usm_debug_menu.map" -Wl,--gc-sections -Wl,--build-id=none \
    -Wl,--no-undefined -Wl,--undefined=usm_gc_arena_lo_wrapper \
    -Wl,--undefined=usm_debug_menu_hook -Wl,--undefined=usm_debug_menu_update_hook \
    -Wl,--undefined=usm_debug_menu_pad_read_hook \
    "${objects[@]}" -lgcc -o "$elf"
"${prefix}objcopy" -O binary "$elf" "$build/usm_debug_menu.bin"
"${prefix}nm" -n --defined-only "$elf" > "$build/usm_debug_menu.symbols.txt"
python3 "$repo/tools/inject_gamecube_payload.py" --dol "$input" --payload "$elf" \
    --symbols "$build/usm_debug_menu.symbols.txt" --output "$output" \
    --backup "$backup" --expected-sha256 "$expected" "${dry[@]}"
