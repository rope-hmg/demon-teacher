mkdir -p ./bin/
pushd ./bin/
clang                       \
    -std=c99                \
    -I../lib/include        \
    `sdl2-config --cflags`  \
    -g                      \
    -o ./demon_teacher      \
    ../src/main.c           \
    `sdl2-config --static-libs`
popd
