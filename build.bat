@echo off

rem Update this path if it doesn't work.
call "Program Files (x86)\Microsoft Visual Studios\2019\Community\VC\Auxiliary\Build\vcvars64.bat"

mkdir bin
pushd bin
    cl                                       ^
        /nologo                              ^
        /TC                                  ^
        /I..\lib\windows\SDL2-2.0.12\include ^
        /I..\lib\include                     ^
        /D WINDOWS                           ^
        /O2                                  ^
        ..\src\main.c

    link                                            ^
        /nologo                                     ^
        /libpath:..\lib\windows\SDL2-2.0.12\lib\x64 ^
        main.obj                                    ^
        SDL2.lib
popd

copy lib\windows\SDL2-2.0.12\x64\SDL2.dll bin\SDL2.dll
