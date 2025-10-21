@echo off
setlocal enabledelayedexpansion

echo.
echo [92m===== Setting up Dependencies (Submodules) =====[0m

rem --- Config (only used for helpful error text)
set ASSIMP_PATH=extern/assimp
set JOLT_PATH=extern/jolt
set ASSIMP_URL=https://github.com/assimp/assimp.git
set JOLT_URL=https://github.com/jrouwe/JoltPhysics.git

rem --- Ensure we are inside a git repo
git rev-parse --is-inside-work-tree >nul 2>&1 || (
  echo [91mERROR: Not a git repository. Run this from your repo root.[0m
  exit /b 1
)

echo.
echo Verifying that this branch tracks the submodules (gitlink mode 160000)...

rem extern/assimp must be a gitlink
git ls-tree -d HEAD extern/assimp 2>nul | findstr /R "^[ ]*160000" >nul
if errorlevel 1 (
  echo [91mERROR: extern/assimp is NOT a tracked submodule on this branch.[0m
  echo        Switch to a branch that tracks it or add it once:
  echo        git submodule add https://github.com/assimp/assimp.git extern/assimp
  exit /b 2
)

rem extern/jolt must be a gitlink
git ls-tree -d HEAD extern/jolt 2>nul | findstr /R "^[ ]*160000" >nul
if errorlevel 1 (
  echo [91mERROR: extern/jolt is NOT a tracked submodule on this branch.[0m
  echo        Switch to a branch that tracks it or add it once:
  echo        git submodule add https://github.com/jrouwe/JoltPhysics.git extern/jolt
  exit /b 2
)

echo Syncing submodule URLs...
git submodule sync --recursive || ( echo [91mERROR: submodule sync failed.[0m & exit /b 1 )

echo Initializing / updating submodules to recorded commits...
git submodule update --init --recursive --checkout --force || ( echo [91mERROR: submodule update failed.[0m & exit /b 1 )

rem Sanity checks before CMake
if not exist "extern\assimp\CMakeLists.txt" (
  echo [91mERROR: Assimp sources missing at extern\assimp.[0m
  exit /b 1
)
if not exist "extern\jolt\Build\CMakeLists.txt" (
  echo [91mERROR: Jolt sources missing at extern\jolt\Build.[0m
  exit /b 1
)



echo [92mSubmodules ready (pinned to commits recorded in this repo).[0m
echo.

rem ============================================================
rem Build dirs
rem ============================================================
for %%D in (assimp jolt) do (
  if not exist extern\%%D\build-debug   mkdir extern\%%D\build-debug
  if not exist extern\%%D\build-release mkdir extern\%%D\build-release
)

rem ============================================================
rem Build Assimp
rem ============================================================
echo [96m Building Assimp Debug[0m
pushd extern\assimp\build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DASSIMP_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF || exit /b 1
cmake --build . --config Debug || exit /b 1
popd

echo [96m Building Assimp Release[0m
pushd extern\assimp\build-release
cmake .. -DCMAKE_BUILD_TYPE=Release -DASSIMP_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF || exit /b 1
cmake --build . --config Release || exit /b 1
popd

rem ============================================================
rem Build Jolt
rem ============================================================
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
  -DTARGET_VIEWER=OFF || exit /b 1
cmake --build . --config Debug || exit /b 1
popd

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
  -DTARGET_VIEWER=OFF || exit /b 1
cmake --build . --config Release || exit /b 1
popd

rem ============================================================
rem Output dirs
rem ============================================================
for %%C in (Debug Release) do (
  if not exist NANOEngine\vendor\lib\assimp\%%C mkdir NANOEngine\vendor\lib\assimp\%%C
  if not exist NANOEngine\vendor\lib\zlib\%%C   mkdir NANOEngine\vendor\lib\zlib\%%C
  if not exist NANOEngine\vendor\lib\jolt\%%C   mkdir NANOEngine\vendor\lib\jolt\%%C
)

rem ============================================================
rem Copy libs
rem ============================================================
copy /Y extern\assimp\build-debug\lib\Debug\assimp-vc143-mtd.lib          NANOEngine\vendor\lib\assimp\Debug\
copy /Y extern\assimp\build-release\lib\Release\assimp-vc143-mt.lib       NANOEngine\vendor\lib\assimp\Release\

copy /Y extern\assimp\build-debug\contrib\zlib\Debug\zlibstaticd.lib      NANOEngine\vendor\lib\zlib\Debug\
copy /Y extern\assimp\build-release\contrib\zlib\Release\zlibstatic.lib   NANOEngine\vendor\lib\zlib\Release\

copy /Y extern\jolt\build-debug\Debug\Jolt.lib                            NANOEngine\vendor\lib\jolt\Debug\
copy /Y extern\jolt\build-release\Release\Jolt.lib                        NANOEngine\vendor\lib\jolt\Release\

echo.
echo [92mBuild and setup completed successfully.[0m
pause
