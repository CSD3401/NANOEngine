@echo off
setlocal enabledelayedexpansion

:: Step 1: Add the submodule if not already added
if not exist extern\assimp (
    echo Adding Assimp submodule...
    git submodule add https://github.com/assimp/assimp.git extern/assimp
)
git submodule update --init --recursive

:: Step 2: Create build directories
mkdir extern\assimp\build-debug
mkdir extern\assimp\build-release

:: Step 3: Build Debug
cd extern\assimp\build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DASSIMP_BUILD_TESTS=OFF
cmake --build . --config Debug
cd ..\..

:: Step 4: Build Release
cd extern\assimp\build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DASSIMP_BUILD_TESTS=OFF
cmake --build . --config Release
cd ..\..

:: Step 5: Create target output directories
mkdir NANOEngine\NANOEngine\vendor\assimp\lib\Debug
mkdir NANOEngine\NANOEngine\vendor\assimp\lib\Release

:: Step 6: Copy built libs
copy /Y extern\assimp\build-debug\bin\Debug\assimp.lib NANOEngine\NANOEngine\vendor\assimp\lib\Debug\
copy /Y extern\assimp\build-release\bin\Release\assimp.lib NANOEngine\NANOEngine\vendor\assimp\lib\Release\

echo Build and copy completed successfully.
pause
