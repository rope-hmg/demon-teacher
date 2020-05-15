# Demon Teacher

A simple roguelike game.

# Controls

## Right Handed

```
w          - Move forward
a          - Move left
s          - Move backwards
d          - Move right
q          - Inventory
e          - Action / Attack
left shift - Maintain facing direction
```

## Left Handed

```
i           - Move forward
j           - Move left
k           - Move backwards
l           - Move right
o           - Inventory
u           - Action / Attack
right shift - Maintain facing direction
```

## General

```
escape - Exit game
```


# Building from Source

Building is super easy, just run the appropriate script for your platform.

## Windows

Follow this [link](https://www.libsdl.org/release/SDL2-devel-2.0.12-VC.zip) to download the SDL development library.
Unzip the folder into `.\lib\windows\` so that you have `.\lib\windows\SDL2-2.0.12\...`.

Run the `cmd` program.

Navigate to this folder and run `build.bat`.

If it doesn't work, then you might need to update the path to the `vcvars64.bat` within. The path
should be something like `C:\Program Files (x86)\Microsoft Visual Studios\2019\Community\VC\Auxiliary\Build\vcvars64.bat`.
Once you have it, you can update the `build.bat` script and never worry about it again.

Once the build has succeeded you should be able to run `./bin/demon_teacher.exe`

## Mac

Follow this [link](https://www.libsdl.org/release/SDL2-2.0.12.dmg) to download the SDL development library. Or you can get it via homebrew.

Open a terminal.

Navigate to this folder and run `build.sh`. You may need to do `chmod +x ./build.sh` first to give
it permission to run.

Once the build has succeeded you should be able to run `./bin/demon_teacher`

## Linux

First install SDL2-2.0.12 via your distribution's package manager.

Open a terminal.

Navigate to this folder and run `build.sh`. You may need to do `chmod +x ./build.sh` first to give
it permission to run.

Once the build has succeeded you should be able to run `./bin/demon_teacher`
