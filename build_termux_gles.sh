#!/data/data/com.termux/files/usr/bin/bash
set -e

BUILD_DIR="${BUILD_DIR:-build_gles32}"

cmake 	-S . 	-B "${BUILD_DIR}" 	-G Ninja 	-DCMAKE_BUILD_TYPE=Release 	-DROBLOX_BUILD_GLES32=ON

cmake --build "${BUILD_DIR}"

echo
echo "Build concluido:"
echo "  ${BUILD_DIR}/roblox_runtime"
echo
echo "Para Termux:X11:"
echo "  export DISPLAY=${DISPLAY:-:0}"
echo "  ./${BUILD_DIR}/roblox_runtime --render-gles"
