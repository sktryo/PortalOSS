#!/usr/bin/env bash
# Syntax-only check for PortalOSS source files.
#
# Recreates the lost gcheck_oss.sh using clang++ -fsyntax-only. Verifies one
# or more source files parse without pulling in the whole SDK build.
#
#   gcheck_oss.sh <client|server|shared> <relpath> [<relpath> ...]
#
#   e.g. gcheck_oss.sh client game/client/portal/PortalRender.cpp
#        gcheck_oss.sh client game/client/viewrender.cpp
#
# The <relpath> is relative to the repo root (i.e. the src/ tree is referenced
# as "src/game/..." only when the caller passes it; both src- and root-relative
# paths are accepted).
set -u

cd "$(dirname "$0")" 2>/dev/null || true
cd /home/sktryo/PortalOSS || { echo "repo root not found"; exit 1; }

MODE="$1"; shift
SRCDIR="src"

# Common include dirs (relative to repo root)
COMMON_INC=(
  "tools/syntaxcheck/sse_stubs"
  "$SRCDIR"
  "$SRCDIR/public"
  "$SRCDIR/public/tier0"
  "$SRCDIR/public/tier1"
  "$SRCDIR/public/materialsystem"
  "$SRCDIR/game/shared"
  "$SRCDIR/game/shared/portal"
  "$SRCDIR/game/shared/hl2"
  "$SRCDIR/vgui2/include"
  "$SRCDIR/vgui2/controls"
  "$SRCDIR/common"
  "$SRCDIR/mathlib"
  "$SRCDIR/thirdparty/sixensesdk/include"
  "$SRCDIR/game/client"
  "$SRCDIR/game/client/portal"
  "$SRCDIR/game/client/hl2"
  "$SRCDIR/game/client/hl2/elements"
  "$SRCDIR/game/client/game_controls"
)

COMMON_DEFS=(
  -DPORTAL
  -DSOURCESDK=1
  -DVPC=1
  -DSOURCE_HAS_FREETYPE
  -D_COMPILER_GCC
  -DPOSIX=1
  -DLINUX=1
  -D_LINUX=1
  -DGNUC
  -D_POSIX
  -DPLATFORM_64BITS
  -DPLATFORM_INTEL
  -DPLATFORM_WINDOWS_PC64
  -D_LINUX
  -Dstricmp=strcasecmp
  -D_stricmp=strcasecmp
  -Dstrnicmp=strncasecmp
  -D_ftol=ftol
)

# g++-compatible flags to keep syntax checking happy with GCC-style SDK code
GCC_FLAGS=(
  -std=gnu++14
  -fpermissive
  -w
  -Wno-unknown-pragmas
  -fsyntax-only
)

generate_dirs() {
  for d in "${COMMON_INC[@]}"; do
    printf ' -I%s' "$d"
  done
}

compile_file() {
  local mode="$1"; local f="$2"
  local abs=""
  if [[ -f "$f" ]]; then
    abs="$f"
  elif [[ -f "src/$f" ]]; then
    abs="src/$f"
  else
    echo "!! not found: $f"
    return 2
  fi

  local ext="${f##*.}"
  local maybe_cpp="-x c++"
  [[ "$ext" == "c" ]] && maybe_cpp="-x c"

  local defs=("${COMMON_DEFS[@]}")
  case "$mode" in
    client) defs+=( -DCLIENT_DLL -DHL2_CLIENT_DLL ) ;;
    server) defs+=( -DCLIENT_DLL=0 -DHL2_GAME_DLL -DHL2_DLL ) ;;
    shared) defs+=( -DCLIENT_DLL -DHL2_CLIENT_DLL ) ;;
  esac

  # shellcheck disable=SC2086
  clang++ "${GCC_FLAGS[@]}" ${maybe_cpp:-} "${defs[@]}" $(generate_dirs) \
    "$abs" 2>&1 | grep -Ev "note:|^In file included" | head -80
  local rc=${PIPESTATUS[0]}
  if [[ $rc -eq 0 ]]; then
    echo "OK: $abs"
  else
    echo "!! FAIL ($rc): $abs"
  fi
  return $rc
}

MODE_FIRST="$MODE"
rc_final=0
for p in "$@"; do
  # support per-file mode override like "client:file"
  m="$MODE_FIRST"
  if [[ "$p" == *:* ]]; then
    m="${p%%:*}"; p="${p#*:}"
  fi
  compile_file "$m" "$p" || rc_final=1
done
exit $rc_final
