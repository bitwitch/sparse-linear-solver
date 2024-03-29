@echo off
setlocal

REM Usage:
REM	build              -- build solver debug
REM	build release      -- build solver release
REM	build test         -- build tests debug
REM	build release test -- build tests release
REM
REM	you can also define PROFILE to enable instrumented profiling 

REM set argument variables
for %%a in (%*) do set "%%a=1"

if not exist build\ mkdir build
pushd build

if "%release%" == "1" (
	if "%test%" == "1" (
		cl /O2 /DNDEBUG /W4 /wd4200 /nologo /Zi /std:c11 "%~dp0test_linear_algebra.c"
	) else (
		cl /O2 /DNDEBUG /W4 /wd4200 /nologo /Zi /std:c11 /Felinear_solver.exe "%~dp0main.c"
	)

REM Debug Build
) else (
	if "%test%" == "1" (
		cl /W4 /wd4200 /WX /nologo /Zi /fsanitize=address /std:c11 "%~dp0test_linear_algebra.c"
	) else (
		cl /W4 /wd4200 /WX /nologo /Zi /fsanitize=address /std:c11 /Felinear_solver.exe "%~dp0main.c"
	)
)

popd

