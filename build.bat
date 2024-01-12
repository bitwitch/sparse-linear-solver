@echo off
if not exist build\ mkdir build
pushd build

if "%1" == "test" (
	cl /DPROFILE /W4 /wd4200 /WX /nologo /Zi /fsanitize=address /std:c11 "%~dp0test_linear_algebra.c"
) else (
	REM cl /W4 /wd4200 /WX /nologo /Zi /fsanitize=address /std:c11 /Felinear_solver.exe "%~dp0main.c"
	cl /DPROFILE /W4 /wd4200 /WX /nologo /Zi /fsanitize=address /std:c11 /Felinear_solver.exe "%~dp0main.c"
)
popd
