@echo off
REM ==============================================================================
REM  build.bat  -  Command line build via CMake.
REM
REM  Default (double-click) = Release, so that the routing algo runs faster.
REM  Pass "Debug" explicitly if you need a debug build with symbols.
REM
REM  Usage:
REM      build.bat                          -- Release all-build (DEFAULT)
REM      build.bat Debug                    -- Debug all-build
REM      build.bat Debug PTreeRouting       -- Debug only PTreeRouting target
REM
REM  The script is kept 100 % ASCII on purpose: see configure.bat header.
REM ==============================================================================
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "BUILD_DIR=%SCRIPT_DIR%\build"
set "BUILD_TYPE=Release"
set "BUILD_TARGET=ALL_BUILD"

REM ------------------------------------------------------------------
REM 1) Parse arguments
REM      build.bat                       -> Release
REM      build.bat Debug                 -> Debug
REM      build.bat RelWithDebInfo        -> RelWithDebInfo
REM      build.bat Release PTreeRouting  -> only PTreeRouting target
REM ------------------------------------------------------------------
if NOT "%~1"=="" set "BUILD_TYPE=%~1"

if /I "%BUILD_TYPE%"=="Debug"          goto :type_ok
if /I "%BUILD_TYPE%"=="Release"        goto :type_ok
if /I "%BUILD_TYPE%"=="RelWithDebInfo" goto :type_ok
if /I "%BUILD_TYPE%"=="MinSizeRel"     goto :type_ok
echo [WARN ] Unknown build type "%~1" - falling back to Release.
set "BUILD_TYPE=Release"
:type_ok

if NOT "%~2"=="" set "BUILD_TARGET=%~2"

REM ------------------------------------------------------------------
REM 2) Make sure configure has been run.
REM    If not, call configure.bat first.
REM ------------------------------------------------------------------
if EXIST "%BUILD_DIR%\CMakeCache.txt" goto :configure_done
echo [INFO] No CMakeCache.txt under build/. Running configure.bat first.
call "%SCRIPT_DIR%\configure.bat"
if errorlevel 1 goto :configure_fail
goto :configure_done

:configure_fail
echo [ERROR] configure.bat failed. Aborting build.
exit /b 1
:configure_done

REM ------------------------------------------------------------------
REM 2.5) Parse Qt6_DIR from CMakeCache.txt, then prepend Qt\bin to PATH.
REM
REM      This is the MANDATORY step that eliminates the build-time error:
REM          "Qt6Gui.dll was not found" / "Qt6Core.dll was not found"
REM
REM      Qt6_DIR in CMakeCache.txt reads like:
REM          Qt6_DIR:PATH=C:/Qt/6.8.0/msvc2022_64/lib/cmake/Qt6
REM      go up 3 levels to reach the kit root, its bin\ subdir contains the
REM      Qt runtime DLLs needed by moc.exe / uic.exe / rcc.exe at runtime.
REM ------------------------------------------------------------------
setlocal EnableDelayedExpansion
set "QT6_CMAKE_DIR="
if EXIST "%BUILD_DIR%\CMakeCache.txt" (
    for /f "usebackq tokens=1,* delims=:=" %%A in ("%BUILD_DIR%\CMakeCache.txt") do (
        if /I "%%~A"=="Qt6_DIR" (
            set "QT6_CMAKE_DIR=%%~B"
            for /f "delims=" %%C in ("!QT6_CMAKE_DIR!") do set "QT6_CMAKE_DIR=%%~C"
            goto :qt6dir_found
        )
    )
)
:qt6dir_found

set "QT_BIN_DIR="
if NOT "%QT6_CMAKE_DIR%"=="" (
    for %%I in ("%QT6_CMAKE_DIR%")       do set "_D1=%%~dpI"
    for %%I in ("!_D1!..")               do set "_D2=%%~fI"
    for %%I in ("!_D2!..")               do set "_D3=%%~fI"
    for %%I in ("!_D3!..")               do set "_KIT=%%~fI"
    if EXIST "!_KIT!\bin\Qt6Core.dll" set "QT_BIN_DIR=!_KIT!\bin"
)

if NOT "%QT_BIN_DIR%"=="" (
    echo [INFO] Qt bin dir from CMakeCache: %QT_BIN_DIR%
    echo [INFO] Prepending to PATH so moc/uic/rcc find Qt DLLs.
    endlocal & set "PATH=%QT_BIN_DIR%;%PATH%"
) else (
    endlocal
    if NOT "%QT6_CMAKE_DIR%"=="" (
        echo [WARN ] Qt6_DIR was found but the Qt\bin folder could not be derived.
        echo         If the build fails with Qt6Gui.dll missing, see README troubleshooting.
    )
)

REM ------------------------------------------------------------------
REM 3) Locate cmake.exe (same logic as configure.bat)
REM ------------------------------------------------------------------
set "CMAKE_EXE="

for /f "delims=" %%I in ('where.exe cmake.exe 2^>nul') do (
    set "CMAKE_EXE=%%~fI"
    goto :cmake_found
)

call :try_cmake "CMAKE_EXE" "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if defined CMAKE_EXE goto :cmake_found

call :try_cmake "CMAKE_EXE" "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if defined CMAKE_EXE goto :cmake_found

call :try_cmake "CMAKE_EXE" "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if defined CMAKE_EXE goto :cmake_found

call :try_cmake "CMAKE_EXE" "C:\Program Files\CMake\bin\cmake.exe"
if defined CMAKE_EXE goto :cmake_found

call :try_cmake "CMAKE_EXE" "C:\ProgramData\chocolatey\bin\cmake.exe"
if defined CMAKE_EXE goto :cmake_found

goto :cmake_error_exit

:cmake_found

REM ------------------------------------------------------------------
REM 4) Run the build
REM ------------------------------------------------------------------
echo [INFO] CMake build: TYPE=%BUILD_TYPE%   TARGET=%BUILD_TARGET%
echo.

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config "%BUILD_TYPE%" --target "%BUILD_TARGET%" -- -m
set "RC=%ERRORLEVEL%"

if "%RC%"=="0" goto :build_ok
goto :build_error_exit

:build_ok
echo.
echo [OK  ] Build finished.
echo        Executable: %BUILD_DIR%\bin\%BUILD_TYPE%\PTreeRouting.exe
echo        Qt DLLs, data\ and config\ are already copied next to the .exe.
endlocal
exit /b 0


REM ==============================================================================
REM  Error exit routines outside if-blocks - see configure.bat note.
REM ==============================================================================

:cmake_error_exit
echo.
echo [ERROR] cmake.exe was not found.
echo Run configure.bat first to see step-by-step installation instructions, OR
echo install the standalone CMake MSI from https://cmake.org/download/
echo and TICK "Add CMake to the system PATH for all users".
exit /b 1

:build_error_exit
echo.
echo [ERROR] Build failed with exit code %RC%.
exit /b 2


REM ==============================================================================
REM Subroutine : try_cmake  RETURN_VAR_NAME  FULL_PATH
REM ==============================================================================
:try_cmake
set "RT_VAR=%~1"
set "TRY=%~2"
if EXIST "%TRY%" (
    for /f "delims=" %%A in ("%TRY%") do set "%RT_VAR%=%%~fA"
)
exit /b 0
