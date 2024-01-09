@echo off
if not exist build\ mkdir build
pushd build
cl /W4 /wd4200 /WX /nologo /Zi /fsanitize=address /std:c11 /Felinear_solver.exe "%~dp0main.c"
popd
