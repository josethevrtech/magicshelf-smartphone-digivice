#!/usr/bin/env bash
set -euo pipefail
package=$(cd -- "$(dirname -- "$0")/.." && pwd)
python3 "$package/backend/private_source.py"
steam="$HOME/.local/share/Steam"
export SteamAppId=2000991199 SteamGameId=2000991199
export STEAM_COMPAT_DATA_PATH="$MAGICSHELF_DATA_DIR/proton-data"
export STEAM_COMPAT_INSTALL_PATH="$MAGICSHELF_GAME_DIR"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$steam"
export WINEDEBUG=-all DXVK_LOG_LEVEL=warn
unset PROTON_LOG PROTON_LOG_DIR
mkdir -p "$STEAM_COMPAT_DATA_PATH"
cd -- "$MAGICSHELF_GAME_DIR"
runtime="${MAGICSHELF_STEAM_APP_4185400_DIR:-$steam/steamapps/common/SteamLinuxRuntime_4-arm64}"
proton="${MAGICSHELF_STEAM_APP_4628740_DIR:-$steam/steamapps/common/Proton 11.0 (ARM64)}"
exec "$runtime/_v2-entry-point" --verb=waitforexitandrun -- "$proton/proton" waitforexitandrun "$MAGICSHELF_GAME_DIR/DigitalPartner.exe"
