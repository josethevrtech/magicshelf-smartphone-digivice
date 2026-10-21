#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
sdk="${MAGICSHELF_SDK_DIR:-$PWD/vendor/magicshelf}"
if [[ ! -f "$sdk/sdk/version.json" ]]; then
  echo 'Initialize the pinned SDK: git submodule update --init' >&2; exit 1
fi
python3 - "$sdk/sdk/version.json" <<'CHECK'
import json,sys
v=json.load(open(sys.argv[1]))
if v['sdk_version']!='1.0.0' or v['local_adapter_version']!=2:
    raise SystemExit('This creation requires MagicShelf SDK 1.0.0, adapter API 2')
CHECK
case "$(uname -m)" in
  aarch64) platform=linuxarm64;; x86_64) platform=linux64;;
  *) echo 'SteamOS aarch64/x86_64 required' >&2; exit 1;;
esac
vr="${OPENVR_LIB_DIR:-/opt/steamvr/bin/$platform}"
mkdir -p build/tests build/runtime/assets/input
flags=(-O2 -std=c++17 -pthread -Isrc -I"$sdk/native/compat" -I"$sdk/native"
  '-DMAGICSHELF_CREATION_PROFILE="creation_profile.h"')
for name in math_test dock_math_test dock_config_test pet_text_input_test capture_recovery_test; do
  c++ "${flags[@]}" "tests/$name.cpp" -o "build/tests/$name" $(pkg-config --cflags --libs egl glesv2 cairo json-c)
  "build/tests/$name"
done
c++ "${flags[@]}" src/main.cpp -o build/runtime/digitalpartner-vr   -L"$vr" -lopenvr_api -Wl,-rpath,/opt/steamvr/bin/"$platform"   $(pkg-config --cflags --libs egl glesv2 cairo json-c libpipewire-0.3 x11 xtst)
cp assets/input/*.json build/runtime/assets/input/
echo 'Build and unit checks passed. This command does not start or replace a running creation.'
