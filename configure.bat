@echo off
REM ==============================================================================
REM  configure.bat  -  Generate a Visual Studio 2022 x64 solution with CMake.
REM
REM  Usage:
REM      configure.bat                          -- auto-detect Qt / CMake
REM      configure.bat "D:\Qt\6.8.0\msvc2022_64" -- pass the Qt root explicitly
REM
REM  The script is kept 100 % ASCII (no Chinese text) so that the default
REM  console code page (CP936 / GBK) does not accidentally misinterpret bytes
REM  as executable commands, which was the most common cause of
REM  "'xxx' is not recognized as an internal or external command" errors.
REM ==============================================================================
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "BUILD_DIR=%SCRIPT_DIR%\build"

REM ------------------------------------------------------------------
REM 1) Parse optional Qt path argument
REM ------------------------------------------------------------------
set "USER_QT="
if NOT "%~1"=="" (
    set "USER_QT=%~1"
    echo [INFO] Using Qt from CLI arg: %USER_QT%
    set "CMAKE_PREFIX_PATH=%USER_QT%"
)

REM ------------------------------------------------------------------
REM 2) Locate cmake.exe.
REM    Strategy:
REM      a) PATH via where.exe                        (cmake installer default)
REM      b) VS 2022 bundled cmake - Professional
REM      c) VS 2022 bundled cmake - Community
REM      d) VS 2022 bundled cmake - BuildTools
REM      e) Chocolatey / default install dirs
REM
REM    NOTE: We use CALL subroutines + GOTO instead of if-blocks for error
REM    messages.  Inside an if (...) block, bare ) inside echo strings end the
REM    block early and produce bizarre errors like "was unexpected at this time".
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
echo [INFO] Using CMake: %CMAKE_EXE%

REM ------------------------------------------------------------------
REM 3) Make sure Qt path is available to find_package(Qt6).
REM ------------------------------------------------------------------
set "QT_KNOWN="
if NOT "%CMAKE_PREFIX_PATH%"=="" set "QT_KNOWN=1"
if DEFINED ENV{CMAKE_PREFIX_PATH} set "QT_KNOWN=1"
if DEFINED ENV{QTDIR}             set "QT_KNOWN=1"
if DEFINED ENV{QT_DIR}            set "QT_KNOWN=1"

if NOT "%CMAKE_PREFIX_PATH%"=="" (
    echo [INFO] CMAKE_PREFIX_PATH = %CMAKE_PREFIX_PATH%
) else if DEFINED ENV{CMAKE_PREFIX_PATH} (
    echo [INFO] Using env CMAKE_PREFIX_PATH = %CMAKE_PREFIX_PATH%
) else if DEFINED ENV{QTDIR} (
    echo [INFO] Using env QTDIR = %QTDIR%
) else if DEFINED ENV{QT_DIR} (
    echo [INFO] Using env QT_DIR = %QT_DIR%
)

if DEFINED QT_KNOWN goto :abi_check
echo.
echo [WARN ] No Qt path detected. find_package(Qt6) will likely fail.
echo         Before re-running configure.bat, EITHER:
echo           (a) run   configure.bat "D:\Qt\6.8.0\msvc2022_64"
echo               passing the Qt kit folder as argument, OR
echo           (b) set the user env var:
echo                   CMAKE_PREFIX_PATH = D:\Qt\6.8.0\msvc2022_64
echo               see README - section "How to set env vars on Windows"
echo.
echo         Continue anyway? CMake will fail if Qt is not discoverable.
echo.
goto :qtpath_ok

REM ------------------------------------------------------------------
REM 3b) Dirty build/ cache guard.
REM
REM CMake's find_package(Qt6) caches ABSOLUTE kit paths inside
REM build\CMakeCache.txt + build\CMakeFiles\<hash>\Qt6Config*.cmake.
REM If the user originally ran configure with the MinGW Qt kit and then
REM switches to the MSVC Qt kit (or vice versa), re-running configure
REM only invalidates the cache partially: imported targets like Qt6::Core
REM will still carry MinGW-only linker flags, which causes
REM     LNK1104: cannot open file 'mingw32.lib'
REM at the final link step. This error looks unrelated to Qt and confuses
REM users, so we catch it EARLY and demand a full build/ wipe.
REM ------------------------------------------------------------------
:abi_check
if NOT EXIST "%BUILD_DIR%\CMakeCache.txt" goto :qtpath_ok

set "OLD_QT_DIR="
for /f "usebackq tokens=1,2,3 delims=:=" %%A in ("%BUILD_DIR%\CMakeCache.txt") do (
    if /I "%%A"=="Qt6_DIR" (
        for /f "tokens=* delims= " %%V in ("%%C") do set "OLD_QT_DIR=%%V"
        goto :got_old_qt
    )
)
goto :qtpath_ok

:got_old_qt
if "%OLD_QT_DIR%"=="" goto :qtpath_ok

REM Resolve the NEW kit folder that CMake will use this run.
set "NEW_QT_ROOT="
if NOT "%CMAKE_PREFIX_PATH%"=="" (
    set "NEW_QT_ROOT=%CMAKE_PREFIX_PATH%"
) else if DEFINED ENV{CMAKE_PREFIX_PATH} (
    set "NEW_QT_ROOT=%CMAKE_PREFIX_PATH%"
) else if DEFINED ENV{QTDIR} (
    set "NEW_QT_ROOT=%QTDIR%"
) else if DEFINED ENV{QT_DIR} (
    set "NEW_QT_ROOT=%QT_DIR%"
)
if "%NEW_QT_ROOT%"=="" goto :qtpath_ok

set "OLD_ABI_MINGW=0"
set "NEW_ABI_MINGW=0"
echo "%OLD_QT_DIR%" | findstr /I /C:"mingw" 1>nul 2>nul && set "OLD_ABI_MINGW=1"
echo "%NEW_QT_ROOT%" | findstr /I /C:"mingw" 1>nul 2>nul && set "NEW_ABI_MINGW=1"

set "OLD_ABI_MSVC=0"
set "NEW_ABI_MSVC=0"
echo "%OLD_QT_DIR%" | findstr /I /C:"msvc" 1>nul 2>nul && set "OLD_ABI_MSVC=1"
echo "%NEW_QT_ROOT%" | findstr /I /C:"msvc" 1>nul 2>nul && set "NEW_ABI_MSVC=1"

if "%OLD_ABI_MINGW%"=="%NEW_ABI_MINGW%" if "%OLD_ABI_MSVC%"=="%NEW_ABI_MSVC%" goto :qtpath_ok

echo.
echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
echo  ABI SWITCH DETECTED  !
echo.
echo   PREVIOUS  Qt kit (in build\CMakeCache.txt):
echo     %OLD_QT_DIR%
echo   CURRENT   Qt kit (we are about to configure with):
echo     %NEW_QT_ROOT%
echo.
echo   The build\ directory still has CMake import-cache from the OLD Qt
echo   kit. Continuing will almost certainly fail with linker errors such
echo   as:
echo       LNK1104: cannot open file 'mingw32.lib'
echo   or mysterious "Qt6Core.dll not found" at runtime (ABI mismatch).
echo.
echo   MANDATORY FIX:
echo       1) Close Visual Studio if build\PPDT_Routing.sln is open.
echo       2) DELETE  the whole  build\  folder.
echo       3) Re-run configure.bat (same args / env vars).
echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
echo.
setlocal DisableDelayedExpansion
set /p "GOON=   Continue anyway? [y/N] "
endlocal
if /I NOT "%GOON%"=="Y" (
    echo Aborted by user. Please delete build\ and re-run configure.bat.
    exit /b 3
)
echo.
echo Continuing with dirty build-cache. Don't file a bug if LNK1104 happens.
echo.

:qtpath_ok

REM ------------------------------------------------------------------
REM 4) Run CMake configure. CMake itself will create the build/ folder.
REM ------------------------------------------------------------------
echo [INFO] Build dir: %BUILD_DIR%
echo [INFO] Generator: Visual Studio 17 2022  x64
echo.

"%CMAKE_EXE%" -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
set "RC=%ERRORLEVEL%"

if "%RC%"=="0" goto :configure_ok
goto :configure_error_exit

:configure_ok
echo.
echo [OK  ] CMake configure succeeded.
echo        Open %BUILD_DIR%\PPDT_Routing.sln in Visual Studio, OR
echo        run build.bat to build from command line.
endlocal
exit /b 0


REM ==============================================================================
REM  Error exit routines (kept OUTSIDE any if-block so echo strings can contain
REM  punctuation freely).
REM ==============================================================================

:cmake_error_exit
echo.
echo [ERROR] cmake.exe was not found.
echo.
echo Possible reasons:
echo   1. You installed CMake but did NOT tick Add CMake to the system PATH.
echo   2. You never installed CMake at all.
echo   3. You have CMake only inside VS 2022 but the VS install dir is non-standard.
echo.
echo How to fix:
echo   * Re-run the CMake installer and TICK the box
echo     "Add CMake to the system PATH for all users".
echo     Download from: https://cmake.org/download/   windows-x86_64.msi
echo   * OR install CMake via the Visual Studio Installer
echo     (workload Desktop development with C++ already includes it, but the
echo      PATH entry may still be missing; install the standalone msi is safest).
echo   * OR set CMAKE_EXE env var to the full path of cmake.exe before running.
exit /b 1

:configure_error_exit
echo.
echo [ERROR] CMake configure failed with exit code %RC%.
echo         99 percent of the time this means Qt6 could not be found.
echo         Fix the Qt path, see the [WARN ] block earlier in this output,
echo         then re-run configure.bat.
exit /b 2


REM ==============================================================================
REM Subroutine : try_cmake  RETURN_VAR_NAME  FULL_PATH
REM   If FULL_PATH exists, assign it to RETURN_VAR_NAME outer scope.
REM ==============================================================================
:try_cmake
set "RT_VAR=%~1"
set "TRY=%~2"
if EXIST "%TRY%" (
    for /f "delims=" %%A in ("%TRY%") do set "%RT_VAR%=%%~fA"
)
exit /b 0
