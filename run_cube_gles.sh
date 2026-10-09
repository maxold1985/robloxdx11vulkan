#!/data/data/com.termux/files/usr/bin/bash
set -e

BUILD_DIR="${BUILD_DIR:-build_gles32}"

if [ -z "${DISPLAY}" ]; then
	export DISPLAY=:0
fi

exec "./${BUILD_DIR}/roblox_runtime" 	--render-gles 	native/scripts/cube/CubeServer.server.lua 	native/scripts/cube/CubeClient.client.lua
