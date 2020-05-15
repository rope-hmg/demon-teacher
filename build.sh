mkdir -p ./bin/
clang -std=c99 `sdl2-config --cflags` -g -o ./bin/demon_teacher ./src/main.c `sdl2-config --static-libs`
