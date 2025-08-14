@echo off
setlocal enabledelayedexpansion

:: ============================================================
:: Colors
:: ============================================================
echo.
echo [92m===== Setting up Dependencies =====[0m

:: ============================================================
:: Submodule Setup
:: ============================================================

:: Assimp
git submodule status extern/assimp >nul 2>&1
if errorlevel 1 (
    echo Adding Assimp submodule...
    git submodule add https://github.com/assimp/assimp.git extern/assimp
) else (
    echo Assimp submodule already exists.
)

:: Jolt
git submodule status extern/jolt >nul 2>&1
if errorlevel 1 (
	echo Adding Jolt Physics submodule...
    git submodule add https://github.com/jrouwe/JoltPhysics.git extern/jolt
) else (
    echo Jolt Physics submodule already exists.
)

git submodule update --init --recursive

:: ============================================================
:: Create build directories
:: ============================================================
for %%D in (assimp jolt) do (
    if not exist extern\%%D\build-debug mkdir extern\%%D\build-debug
    if not exist extern\%%D\build-release mkdir extern\%%D\build-release
)

:: ============================================================
:: Build Assimp
:: ============================================================
echo.
echo [96m Building Assimp Debug[0m
pushd extern\assimp\build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DASSIMP_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF || pause
cmake --build . --config Debug || pause
popd

echo.
echo [96m Building Assimp Release[0m
pushd extern\assimp\build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DASSIMP_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF || pause
cmake --build . --config Release || pause
popd

:: ============================================================
:: Build Jolt
:: ============================================================
echo.
echo [96m Building Jolt Debug[0m
pushd extern\jolt\build-debug
cmake ..\Build ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDebugDLL" ^
  -DUSE_STATIC_MSVC_RUNTIME_LIBRARY=OFF ^
  -DOVERRIDE_CXX_FLAGS=OFF ^
  -DJPH_BUILD_LIBRARY=ON ^
  -DTARGET_UNIT_TESTS=OFF ^
  -DTARGET_SAMPLES=OFF ^
  -DTARGET_VIEWER=OFF || pause
cmake --build . --config Debug || pause
popd

echo.
echo [96m Building Jolt Release[0m
pushd extern\jolt\build-release
cmake ..\Build ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDLL" ^
  -DUSE_STATIC_MSVC_RUNTIME_LIBRARY=OFF ^
  -DOVERRIDE_CXX_FLAGS=OFF ^
  -DJPH_BUILD_LIBRARY=ON ^
  -DTARGET_UNIT_TESTS=OFF ^
  -DTARGET_SAMPLES=OFF ^
  -DTARGET_VIEWER=OFF || pause
cmake --build . --config Release || pause
popd

:: ============================================================
:: Create vendor lib output directories
:: ============================================================
for %%C in (Debug Release) do (
    if not exist NANOEngine\vendor\lib\assimp\%%C mkdir NANOEngine\vendor\lib\assimp\%%C
    if not exist NANOEngine\vendor\lib\zlib\%%C mkdir NANOEngine\vendor\lib\zlib\%%C
    if not exist NANOEngine\vendor\lib\jolt\%%C mkdir NANOEngine\vendor\lib\jolt\%%C
)

:: ============================================================
:: Copy Assimp
:: ===========================================	=================
copy /Y extern\assimp\build-debug\lib\Debug\assimp-vc143-mtd.lib NANOEngine\vendor\lib\assimp\Debug\
copy /Y extern\assimp\build-release\lib\Release\assimp-vc143-mt.lib NANOEngine\vendor\lib\assimp\Release\

:: ============================================================
:: Copy zlib
:: ============================================================
copy /Y extern\assimp\build-debug\contrib\zlib\Debug\zlibstaticd.lib NANOEngine\vendor\lib\zlib\Debug\
copy /Y extern\assimp\build-release\contrib\zlib\Release\zlibstatic.lib NANOEngine\vendor\lib\zlib\Release\

:: ============================================================
:: Copy Jolt libs
:: ============================================================
copy /Y extern\jolt\build-debug\Debug\Jolt.lib NANOEngine\vendor\lib\jolt\Debug\
copy /Y extern\jolt\build-release\Release\Jolt.lib NANOEngine\vendor\lib\jolt\Release\

:: ============================================================
:: Final cleanup
:: ============================================================
echo.
echo [91m Cleaning up extern folder...[0m
rmdir /S /Q extern

echo.
echo [92mBuild and setup completed successfully.[0m
pause