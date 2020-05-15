@echo off

for /r "C:\Program Files (x86)" %%a in (*) do if "%%~nxa"=="vcvarsall.bat" set vcvarsall=%%~dpnxa

if defined vcvarsall (
    call vcvarsall x64

    mkdir .\bin
    pushd .\bin
    cl /O2 /Fe:demon_teacher ..\src\main.c
    popd
) else (
    echo You need to install Visual Studios
)
