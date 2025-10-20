@echo off
setlocal enabledelayedexpansion

echo.
echo [92m===== Setting up Dependencies (Submodules) =====[0m

rem --- Config (only used for helpful error text)
set ASSIMP_PATH=extern/assimp
set JOLT_PATH=extern/jolt
set COMPRESSONATOR_PATH=extern/compressonator
set ASSIMP_URL=https://github.com/assimp/assimp.git
set JOLT_URL=https://github.com/jrouwe/JoltPhysics.git
set COMPRESSONATOR_URL=https://github.com/GPUOpen-Tools/compressonator.git
set COMPRESSONATOR_VERSION=V4.5.52

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
  echo [91mERROR: extern/assimp is NOT a tracked submodule on this branch.[0m
  echo        Switch to a branch that tracks it or add it once:
  echo        git submodule add https://github.com/assimp/assimp.git extern/assimp
  exit /b 2
)

rem extern/jolt must be a gitlink
git ls-tree -d HEAD extern/jolt 2>nul | findstr /R "^[ ]*160000" >nul
if errorlevel 1 (
  echo [91mERROR: extern/jolt is NOT a tracked submodule on this branch.[0m
  echo        Switch to a branch that tracks it or add it once:
  echo        git submodule add https://github.com/jrouwe/JoltPhysics.git extern/jolt
  exit /b 2
)

rem extern/jolt must be a gitlink
git ls-tree -d HEAD extern/compressonator 2>nul | findstr /R "^[ ]*160000" >nul
if errorlevel 1 (
  echo [91mERROR: extern/compressonator is NOT a tracked submodule on this branch.[0m
  echo 		Switch to a branch that tracks it or add it once:
  echo 		git submodule add https://github.com/GPUOpen-Tools/compressonator.git extern/compressonator
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
if not exist "extern\compressonator\CMakeLists.txt" (
  echo [91mERROR: Compressonator sources missing at extern\compressonator.[0m
  exit /b 1
)


echo [92mSubmodules ready (pinned to commits recorded in this repo).[0m
echo.

rem ============================================================
rem Build dirs
rem ============================================================
for %%D in (assimp jolt compressonator) do (
  if not exist extern\%%D\build-debug   mkdir extern\%%D\build-debug
  if not exist extern\%%D\build-release mkdir extern\%%D\build-release
)

rem ============================================================
rem Build Assimp
rem ============================================================

rem echo [96m Building Assimp Debug[0m
rem pushd extern\assimp\build-debug
rem cmake .. -DCMAKE_BUILD_TYPE=Debug -DASSIMP_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF || exit /b 1
rem cmake --build . --config Debug || exit /b 1
rem popd
rem 
rem echo [96m Building Assimp Release[0m
rem pushd extern\assimp\build-release
rem cmake .. -DCMAKE_BUILD_TYPE=Release -DASSIMP_BUILD_TESTS=OFF -DBUILD_SHARED_LIBS=OFF || exit /b 1
rem cmake --build . --config Release || exit /b 1
rem popd


rem ============================================================
rem Build Jolt
rem ============================================================

rem echo Building Jolt...
rem echo [96m Building Jolt Debug[0m
rem pushd extern\jolt\build-debug
rem cmake ..\Build ^
rem   -DCMAKE_BUILD_TYPE=Debug ^
rem   -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDebugDLL" ^
rem   -DUSE_STATIC_MSVC_RUNTIME_LIBRARY=OFF ^
rem   -DOVERRIDE_CXX_FLAGS=OFF ^
rem   -DJPH_BUILD_LIBRARY=ON ^
rem   -DTARGET_UNIT_TESTS=OFF ^
rem   -DTARGET_SAMPLES=OFF ^
rem   -DTARGET_VIEWER=OFF || exit /b 1
rem cmake --build . --config Debug || exit /b 1
rem popd
rem 
rem echo [96m Building Jolt Release[0m
rem pushd extern\jolt\build-release
rem cmake ..\Build ^
rem   -DCMAKE_BUILD_TYPE=Release ^
rem   -DCMAKE_MSVC_RUNTIME_LIBRARY="MultiThreadedDLL" ^
rem   -DUSE_STATIC_MSVC_RUNTIME_LIBRARY=OFF ^
rem   -DOVERRIDE_CXX_FLAGS=OFF ^
rem   -DJPH_BUILD_LIBRARY=ON ^
rem   -DTARGET_UNIT_TESTS=OFF ^
rem   -DTARGET_SAMPLES=OFF ^
rem   -DTARGET_VIEWER=OFF || exit /b 1
rem cmake --build . --config Release || exit /b 1
rem popd

rem ============================================================
rem Build Compressonator
rem ============================================================

echo [96m Building Compressonator SDK (tag %COMPRESSONATOR_VERSION%) [0m

rem Ensure vendor dirs
if not exist NANOEngine\vendor\include\compressonator           mkdir NANOEngine\vendor\include\compressonator
if not exist NANOEngine\vendor\lib\compressonator\Release       mkdir NANOEngine\vendor\lib\compressonator\Release
if not exist NANOEngine\vendor\bin\compressonator\Release       mkdir NANOEngine\vendor\bin\compressonator\Release

rem Checkout the specific tag inside the submodule
pushd extern\compressonator
git fetch --tags || ( echo [91mFailed to fetch Compressonator tags.[0m & popd & exit /b 1 )
git checkout --force "%COMPRESSONATOR_VERSION%" || ( echo [91mTag %COMPRESSONATOR_VERSION% not found.[0m & popd & exit /b 1 )
popd

rem Configure SDK-only CMake project (Release, x64)
set "CMP_BUILD_RELEASE=extern\compressonator\build-release"
set "CMP_INSTALL_DIR=%CD%\NANOEngine\vendor\compressonator\install"
if not exist "%CMP_BUILD_RELEASE%" mkdir "%CMP_BUILD_RELEASE%"

echo [96m   - CMake configure (SDK) [0m
cmake -S "extern\compressonator\build\sdk" -B "%CMP_BUILD_RELEASE%" ^
	-G "Visual Studio 17 2022" -A x64 ^
	-DCMAKE_BUILD_TYPE=Release ^
	-DCMAKE_INSTALL_PREFIX="%CMP_INSTALL_DIR%" || exit /b 1

echo [96m   - Build + Install [0m
cmake --build "%CMP_BUILD_RELEASE%" --config Release --target INSTALL || exit /b 1

rem Copy headers
echo [96m   - Copy headers/libs [0m
xcopy /E /I /Y "%CMP_INSTALL_DIR%\include\" "Editor\vendor\include\compressonator\" >nul

rem Copy Release libs (SDK high-level + optional core)
if exist "%CMP_INSTALL_DIR%\lib\Compressonator_MD.lib" (
copy /Y "%CMP_INSTALL_DIR%\lib\Compressonator_MD.lib" "Editor\vendor\lib\compressonator\Release\" >nul
) else (
echo [93mWarning: Compressonator_MD.lib not found under install\lib.[0m
)
if exist "%CMP_INSTALL_DIR%\lib\CMP_Core.lib" (
copy /Y "%CMP_INSTALL_DIR%\lib\CMP_Core.lib" "Editor\vendor\lib\compressonator\Release\" >nul
)

rem If SDK emits any runtime DLLs, copy them for Editor runtime
rem for %%F in ("%CMP_INSTALL_DIR%\bin\*.dll") do (
rem   copy /Y "%%~fF" "Editor\vendor\compressonator\Release\" >nul
rem )

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
