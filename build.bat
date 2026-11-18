@echo off
setlocal
set "ROOT=%~dp0"

if defined WCL_CLANG_PATH (
    set "CLANG=%WCL_CLANG_PATH%"
) else if exist "C:\Program Files\LLVM\bin\clang.exe" (
    set "CLANG=C:\Program Files\LLVM\bin\clang.exe"
) else (
    where clang.exe >nul 2>nul
    if errorlevel 1 (
        echo clang.exe was not found. Install LLVM or set WCL_CLANG_PATH.
        exit /b 1
    )
    set "CLANG=clang.exe"
)

if not exist "%ROOT%build" mkdir "%ROOT%build"

set "BUILD_FLAGS=-fcolor-diagnostics -fansi-escape-codes -std=c17 -O2"
if /i "%~1"=="asan" (
    echo Building with AddressSanitizer and UndefinedBehaviorSanitizer...
    set "BUILD_FLAGS=-fcolor-diagnostics -fansi-escape-codes -std=c17 -g -fsanitize=address,undefined -fno-omit-frame-pointer -O1"
)
if /i "%~1"=="/asan" (
    echo Building with AddressSanitizer and UndefinedBehaviorSanitizer...
    set "BUILD_FLAGS=-fcolor-diagnostics -fansi-escape-codes -std=c17 -g -fsanitize=address,undefined -fno-omit-frame-pointer -O1"
)
if /i "%~1"=="debug" (
    echo Building Debug target...
    set "BUILD_FLAGS=-fcolor-diagnostics -fansi-escape-codes -std=c17 -g -O0"
)
if /i "%~1"=="/debug" (
    echo Building Debug target...
    set "BUILD_FLAGS=-fcolor-diagnostics -fansi-escape-codes -std=c17 -g -O0"
)

"%CLANG%" %BUILD_FLAGS% ^
    "%ROOT%src\core\preamble.c" ^
    "%ROOT%src\core\symbols_lexicals.c" ^
    "%ROOT%src\core\context_files.c" ^
    "%ROOT%src\commands\directory.c" ^
    "%ROOT%src\commands\show.c" ^
    "%ROOT%src\commands\file_operations.c" ^
    "%ROOT%src\commands\process.c" ^
    "%ROOT%src\platform\windows\scheduler.c" ^
    "%ROOT%src\commands\file_channels.c" ^
    "%ROOT%src\core\registry.c" ^
    "%ROOT%src\platform\windows\volume_operations.c" ^
    "%ROOT%src\commands\help.c" ^
    "%ROOT%src\core\parser_conditions.c" ^
    "%ROOT%src\scripting\procedure.c" ^
    "%ROOT%src\core\execution.c" ^
    "%ROOT%src\commands\dcl_extended.c" ^
    "%ROOT%src\editor\iris.c" ^
    "%ROOT%src\core\history.c" ^
    "%ROOT%src\app\main.c" ^
    -o "%ROOT%build\wcl.exe"
if errorlevel 1 exit /b %errorlevel%
echo Built "%ROOT%build\wcl.exe"
exit /b 0
