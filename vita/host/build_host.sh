#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${OUT:-$ROOT/build-host}"
mkdir -p "$OUT/obj"
read -r -a SPIRV <<< "${SPIRV_CROSS:-}"
CF=(-g -O1 -fno-omit-frame-pointer -fsanitize=address -fno-strict-aliasing -w
    -D__vita__ -DVITA_HOST_TEST -DREF_HARD_LINKED -DGAME_HARD_LINKED -DC_ONLY -Dstricmp=strcasecmp
    -DVMA_VULKAN_VERSION=1000000 -DVMA_DEDICATED_ALLOCATION=0 -DVMA_BIND_MEMORY2=0 -DVMA_MEMORY_BUDGET=0
    -DVMA_BUFFER_DEVICE_ADDRESS=0 -DVMA_MEMORY_PRIORITY=0 -DVMA_KHR_MAINTENANCE4=0 -DVMA_KHR_MAINTENANCE5=0
    -DVMA_GET_PHYSICAL_DEVICE_PROPERTIES2=0 -DVMA_STATS_STRING_ENABLED=0
    -isystem "$VITA_HEADERS" -I "$VKV/include" -I "$VKV/third_party/Vulkan-Headers/include" -I "$ROOT")
GAME_CF=(-Dmaxclients=game_maxclients -Ddedicated=game_dedicated)
files=$(sed -n '/set(CLIENT_SOURCES/,/^add_executable/p' "$ROOT/vita/CMakeLists.txt" | grep -oE "[a-z_0-9]+/[a-z_0-9]+\.(c|cpp)")
objs=()
for f in $files; do
  o="$OUT/obj/$(echo "$f" | tr / _).o"
  if [[ $f == *.cpp ]]; then g++ -std=gnu++17 "${CF[@]}" -c "$ROOT/$f" -o "$o" &
  elif [[ $f == game/* ]]; then gcc -std=gnu11 -Dfopen=mock_fopen "${GAME_CF[@]}" "${CF[@]}" -c "$ROOT/$f" -o "$o" &
  else gcc -std=gnu11 -Dfopen=mock_fopen "${CF[@]}" -c "$ROOT/$f" -o "$o" &
  fi
  objs+=("$o")
done
gcc -std=gnu11 "${CF[@]}" -c "$ROOT/vita/host/mock_port.c" -o "$OUT/obj/mock_port.o" &
wait
objs+=("$OUT/obj/mock_port.o")
vkv=$(ls "$VKV_OBJS"/vkv_*.c.o "$VKV_OBJS"/mock_sce.c.o)
g++ -fsanitize=address,undefined "${objs[@]}" $vkv "${SPIRV[@]}" -lm -lpthread -o "$OUT/vkquake2"
echo "built $OUT/vkquake2"
