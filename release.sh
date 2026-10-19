#!/usr/bin/env sh
# Builds the release images in Docker from the committed tree (HEAD) and collects them into
# dist/. Needs Docker and network access (npm and the library sources).
#
#   ./release.sh               # BOARD defaults to A9_B_V1_3
#
# Images: camera, ram_loader and bootloader_installer (it carries the bootloader build inside).

set -eu

cd "$(dirname "$0")"

BOARD=${BOARD:-A9_B_V1_3}
# The console port the images are linked with (platform_stdio_uart2 in every application for now).
CONSOLE=uart2
IMAGE=bk7252-cam-build
VERSION=$(git describe --tags --always)
WORK=build-release
OUT=$PWD/dist/$VERSION

echo "Releasing $VERSION for $BOARD"
if ! git diff --quiet HEAD; then
    echo "Note: uncommitted changes are not part of the release"
fi

docker build -t "$IMAGE" docker

rm -rf "$WORK"
mkdir -p "$WORK/src"
git archive HEAD | tar -x -C "$WORK/src"

docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp -v "$PWD/$WORK:/work" "$IMAGE" sh -ec "
    npm --prefix src/src/applications/camera/frontend ci
    cmake -S src -B out -DBOARD=$BOARD -DBK_VERSION=$VERSION
    cmake --build out --target camera
    cmake --build out --target ram_loader
    cmake --build out --target bootloader_installer--iram
"

# Copies the built image $1 to the release directory as <name>-<version>-<console>_<kind>.bin, e.g.
# camera_crc.bin to A9_B_V1_3-camera-v0.1.0-uart2_crc.bin. $2 is the name, $3 is crc or iram.
# Only the camera depends on the board; the loaders have the same image for every board.
collect() {
    built=$(find "$WORK/out" -name "$1" | head -n 1)
    [ -n "$built" ] || { echo "Not built: $1"; exit 1; }
    cp "$built" "$OUT/$2-${VERSION}-${CONSOLE}_$3.bin"
}

mkdir -p "$OUT"
collect camera_crc.bin "${BOARD}-camera" crc
collect ram_loader_crc.bin ram_loader crc
collect bootloader_installer--iram.bin bootloader_installer iram

echo
echo "Done: ${OUT#"$PWD"/}"
ls -l "$OUT"
