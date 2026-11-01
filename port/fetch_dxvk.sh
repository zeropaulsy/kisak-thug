#!/bin/sh
# Downloads the DXVK native release the Linux build links against into port/third_party/dxvk-native (git-ignored).
set -e
VER=3.1.1
SHA=506fa1ea5bb6ce9a89e11436f7ab5191bc0196e8a577e0cf0b16a45bbc9991b7
DIR="$(dirname "$(readlink -f "$0")")/third_party/dxvk-native"
[ -f "$DIR/.version-$VER" ] && exit 0
mkdir -p "$DIR"
T="$DIR/dxvk.tar.gz"
curl -fsSL -o "$T" "https://github.com/doitsujin/dxvk/releases/download/v$VER/dxvk-native-$VER-steamrt-sniper.tar.gz"
echo "$SHA  $T" | sha256sum -c -
tar xzf "$T" -C "$DIR" && rm "$T"
touch "$DIR/.version-$VER"
