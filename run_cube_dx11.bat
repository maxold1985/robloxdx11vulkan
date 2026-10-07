@echo off
setlocal

set EXE=build\Release\roblox_runtime.exe

if not exist "%EXE%" (
	set EXE=build\roblox_runtime.exe
)

if not exist "%EXE%" (
	echo roblox_runtime.exe nao encontrado.
	echo Compile o projeto antes de executar este arquivo.
	exit /b 1
)

"%EXE%" ^
	--render ^
	native\scripts\cube\CubeServer.server.lua ^
	native\scripts\cube\CubeClient.client.lua

endlocal
