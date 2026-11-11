#!/usr/bin/env sh
# Builds the release images in Docker from the committed tree (HEAD) and collects them into
# dist/. Needs Docker and network access (npm and the library sources).
#
#   ./release.sh                        # every board in src/boards
#   BOARDS="A9_B_V1_3 HB6_V1_0D211115" ./release.sh
#
# Images:
#   camera                  for the boards that have a camera (CAMERA_I2C in board.h).
#   ram_loader              for the boards without a camera: it is the same image for every board
#   bootloader_installer    with that console port (it carries the bootloader build inside).
#
# File names: <image>-<target>-<version>_<kind>.bin. The target is the board. The kind is crc
# (flash it) or iram (load it).

set -eu

cd "$(dirname "$0")"

BOARDS=${BOARDS:-$(git ls-tree -d --name-only HEAD:src/boards/)}
IMAGE=bk7252-cam-build
VERSION=$(git describe --tags --always)
WORK=build-release
OUT=$PWD/dist/$VERSION
# Kept between releases: the library sources (dependencies.cmake) and the npm download cache.
CACHE=$PWD/.cache/release

# Runs the command in the build container. The tree is mounted at /work; the library sources
# and the npm cache come from CACHE, so they are not downloaded again for every release.
in_docker() {
    docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp \
        -v "$PWD/$WORK:/work" \
        -v "$CACHE/libs:/work/src/libs" \
        -v "$CACHE/npm:/tmp/.npm" \
        "$IMAGE" "$@"
}

# Copies the built image $1 of board $2 to the release directory as <name>-<target>-<version>_<kind>.bin.
# $3 is the image name, $4 the target and $5 the kind.
collect() {
    built=$(find "$WORK/out/$2" -name "$1" | head -n 1)
    [ -n "$built" ] || { echo "Not built: $1 for $2"; exit 1; }
    cp "$built" "$OUT/$3-$4-${VERSION}_$5.bin"
}

for board in $BOARDS; do
    git cat-file -e "HEAD:src/boards/$board/board.h" 2>/dev/null \
        || { echo "Unknown board '$board'"; exit 1; }
done

echo "Releasing $VERSION for: $BOARDS"
if ! git diff --quiet HEAD; then
    echo "Note: uncommitted changes are not part of the release"
fi

docker build -t "$IMAGE" docker

rm -rf "$WORK"
# The mount points are made here, by the current user, so that Docker does not create them as root.
mkdir -p "$WORK/src/libs" "$CACHE/libs" "$CACHE/npm" "$OUT"
git archive HEAD | tar -x -C "$WORK/src"

in_docker npm --prefix src/src/applications/camera/frontend ci

for board in $BOARDS; do
    # A board with a camera gets the camera firmware, a board without one gets the loaders.
    if git show "HEAD:src/boards/$board/board.h" | grep -q '^#define CAMERA_I2C'; then
        camera=1
        loaders=0
    else
        camera=0
        loaders=1
    fi

    echo
    echo "== $board =="
    in_docker sh -ec "
        cmake -S src -B out/$board -DBOARD=$board -DBK_VERSION=$VERSION
        if [ $camera = 1 ]; then
            cmake --build out/$board --target camera
        fi
        if [ $loaders = 1 ]; then
            cmake --build out/$board --target ram_loader
            cmake --build out/$board --target bootloader_installer--iram
        fi
    "

    if [ "$camera" = 1 ]; then
        collect camera_crc.bin "$board" camera "$board" crc
    fi
    if [ "$loaders" = 1 ]; then
        collect ram_loader_crc.bin "$board" ram_loader "$board" crc
        collect bootloader_installer--iram.bin "$board" bootloader_installer "$board" iram
    fi
done

echo
echo "Done: ${OUT#"$PWD"/}"
ls -l "$OUT"
