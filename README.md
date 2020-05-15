# Demon Teacher

A simple roguelike game.

# Building

Building is super easy, just run the appropriate script for your platform.

## Windows

Run the `cmd` program.

Navigate to this folder and run `build.bat`.

If it doesn't work, then you might need to update the path to the `vcvars64.bat` within. The path
should be something like `C:\Program Files (x86)\Microsoft Visual Studios\2019\Community\VC\Auxiliary\Build\vcvars64.bat`.
Once you have it, you can update the `build.bat` script and never worry about it again.

Once the build has succeeded you should be able to run `./bin/demon_teacher.exe`

## Mac / Linux

```bash
cd path/to/repository
chmod +x ./build.sh
./build.sh
./bin/demon_teacher
```
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
