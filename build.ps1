param(
    [string]$ClangPath,
    [switch]$ASan,
    [switch]$Debug
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$sourceRoot = Join-Path $root "src"
$sources = @(
    "core\preamble.c",
    "core\symbols_lexicals.c",
    "core\context_files.c",
    "commands\directory.c",
    "commands\show.c",
    "commands\file_operations.c",
    "commands\process.c",
    "platform\windows\scheduler.c",
    "commands\file_channels.c",
    "core\registry.c",
    "platform\windows\volume_operations.c",
    "commands\help.c",
    "core\parser_conditions.c",
    "scripting\procedure.c",
    "core\execution.c",
    "commands\dcl_extended.c",
    "editor\iris.c",
    "core\history.c",
    "app\main.c"
) | ForEach-Object { Join-Path $sourceRoot $_ }
$outputDirectory = Join-Path $root "build"
$output = Join-Path $outputDirectory "wcl.exe"

if (-not $ClangPath) {
    if ($env:WCL_CLANG_PATH) {
        $ClangPath = $env:WCL_CLANG_PATH
    } elseif (Test-Path -LiteralPath "C:\Program Files\LLVM\bin\clang.exe") {
        $ClangPath = "C:\Program Files\LLVM\bin\clang.exe"
    } else {
        $clang = Get-Command clang.exe -ErrorAction SilentlyContinue
        if ($clang) { $ClangPath = $clang.Source }
    }
}

if (-not $ClangPath -or -not (Test-Path -LiteralPath $ClangPath)) {
    Write-Error "clang.exe was not found. Install LLVM or set WCL_CLANG_PATH / -ClangPath."
    exit 1
}

$buildFlags = @("-fcolor-diagnostics", "-fansi-escape-codes", "-std=c17", "-O2")
if ($ASan) {
    Write-Host "Building with AddressSanitizer and UndefinedBehaviorSanitizer..."
    $buildFlags = @("-fcolor-diagnostics", "-fansi-escape-codes", "-std=c17", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-O1")
} elseif ($Debug) {
    Write-Host "Building Debug target..."
    $buildFlags = @("-fcolor-diagnostics", "-fansi-escape-codes", "-std=c17", "-g", "-O0")
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
& $ClangPath @buildFlags @sources -o $output
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Built $output"
