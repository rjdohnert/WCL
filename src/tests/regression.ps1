param(
    [switch]$SkipBuild,
    [string]$ClangPath = "C:\Program Files\LLVM\bin\clang.exe"
)

$ErrorActionPreference = "Stop"
$sourceRoot = Split-Path -Parent $PSScriptRoot
$sourceFiles = @(
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
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("wcl-regression-" + [guid]::NewGuid().ToString("N"))
$procedureRoot = Join-Path $tempRoot "procedures"
$fixtureRoot = Join-Path $tempRoot "fixtures"
$executable = Join-Path $tempRoot "wcl-regression.exe"
$script:passed = 0
$script:failed = 0
$script:failures = New-Object System.Collections.Generic.List[string]

function Assert-True {
    param([string]$Name, [bool]$Condition, [string]$Details)
    if ($Condition) {
        $script:passed++
        Write-Host "PASS $Name"
    } else {
        $script:failed++
        $script:failures.Add("$Name -- $Details")
        Write-Host "FAIL $Name -- $Details"
    }
}

function Invoke-Procedure {
    param([string]$Name, [string[]]$Commands, [string[]]$Parameters = @())
    $procedure = Join-Path $procedureRoot ($Name + ".com")
    [System.IO.File]::WriteAllLines($procedure, $Commands, [System.Text.Encoding]::ASCII)
    $processArguments = @("@$procedure") + $Parameters
    $outputLines = & $executable @processArguments 2>&1
    $exitCode = $LASTEXITCODE
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = ($outputLines | Out-String)
    }
}

function Assert-Procedure {
    param(
        [string]$Name,
        [string[]]$Commands,
        [int]$ExpectedExitCode = 0,
        [string]$ExpectedOutput = $null,
        [string]$ForbiddenOutput = $null,
        [string[]]$Parameters = @()
    )
    $result = Invoke-Procedure $Name $Commands $Parameters
    $ok = $result.ExitCode -eq $ExpectedExitCode
    if ($ExpectedOutput) {
        $ok = $ok -and $result.Output -match $ExpectedOutput
    }
    if ($ForbiddenOutput) {
        $ok = $ok -and $result.Output -notmatch $ForbiddenOutput
    }
    $details = "exit=$($result.ExitCode), output=$($result.Output.Trim())"
    Assert-True $Name $ok $details
    return $result
}

try {
    New-Item -ItemType Directory -Path $procedureRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $fixtureRoot -Force | Out-Null

    if (-not $SkipBuild) {
        if (-not (Test-Path -LiteralPath $ClangPath)) {
            $clangCommand = Get-Command clang.exe -ErrorAction SilentlyContinue
            if (-not $clangCommand) {
                throw "clang.exe was not found. Pass -ClangPath or install LLVM."
            }
            $ClangPath = $clangCommand.Source
        }
        $buildOutput = & $ClangPath -fcolor-diagnostics -fansi-escape-codes -std=c17 -g @sourceFiles -o $executable 2>&1
        $buildExitCode = $LASTEXITCODE
        $buildText = $buildOutput | Out-String
        Assert-True "clang build" ($buildExitCode -eq 0) "exit=$buildExitCode $buildText"
        if ($buildExitCode -ne 0) {
            throw "Build failed; regression suite cannot run."
        }
    } else {
        $candidate = Join-Path $sourceRoot "wcl.exe"
        if (-not (Test-Path -LiteralPath $candidate)) {
            throw "No existing wcl.exe found for -SkipBuild."
        }
        Copy-Item -LiteralPath $candidate -Destination $executable
    }

    $builtIns = @(
        "HELP", "DIRECTORY", "SHOW", "TYPE", "COPY", "DELETE", "RENAME", "CREATE",
        "SEARCH", "DEFINE", "ASSIGN", "DEASSIGN", "SET", "SPAWN", "RUN", "STOP", "SUBMIT",
        "OPEN", "READ", "CLOSE", "APPEND", "PRINT", "WRITE", "EXIT", "LOGOUT",
        "ALLOCATE", "MOUNT", "DISMOUNT", "INITIALIZE"
    )
    $helpResult = Assert-Procedure "help-catalog" @("HELP") 0 "Available Commands"
    Assert-True "help reports WCL version" ($helpResult.Output -match "WCL Version: 9\.2-2026") `
        "HELP did not report WCL version 9.2-2026"
    foreach ($verb in $builtIns) {
        Assert-True "help lists $verb" ($helpResult.Output -match "(?m)^\s+$([regex]::Escape($verb))\s+-") `
            "HELP output did not list $verb"
    }
    foreach ($verb in $builtIns | Where-Object { $_ -ne "HELP" }) {
        Assert-Procedure "help-$($verb.ToLowerInvariant())" @("HELP $verb") 0
    }

    $sizeRoot = Join-Path $fixtureRoot "sizes"
    $hiddenRoot = Join-Path $fixtureRoot "hidden"
    $ioRoot = Join-Path $fixtureRoot "io"
    New-Item -ItemType Directory -Path $sizeRoot, $hiddenRoot, $ioRoot -Force | Out-Null
    [System.IO.File]::WriteAllBytes((Join-Path $sizeRoot "zero.bin"), [byte[]]@())
    [System.IO.File]::WriteAllBytes((Join-Path $sizeRoot "one.bin"), [byte[]]@(1))
    [System.IO.File]::WriteAllBytes((Join-Path $sizeRoot "512.bin"), [byte[]]::new(512))
    [System.IO.File]::WriteAllBytes((Join-Path $sizeRoot "513.bin"), [byte[]]::new(513))
    [System.IO.File]::WriteAllText((Join-Path $hiddenRoot "visible.txt"), "visible")
    $hiddenFile = Join-Path $hiddenRoot "hidden.txt"
    [System.IO.File]::WriteAllText($hiddenFile, "hidden")
    (Get-Item -LiteralPath $hiddenFile).Attributes = [System.IO.FileAttributes]::Hidden
    $hiddenDirectory = Join-Path $hiddenRoot "hidden-dir"
    New-Item -ItemType Directory -Path $hiddenDirectory | Out-Null
    (Get-Item -LiteralPath $hiddenDirectory).Attributes = [System.IO.FileAttributes]::Hidden -bor [System.IO.FileAttributes]::Directory
    $sourceFilePath = Join-Path $ioRoot "source.txt"
    $targetFilePath = Join-Path $ioRoot "target.txt"
    [System.IO.File]::WriteAllText($sourceFilePath, "alpha`r`nbeta`r`ngamma`r`n")
    [System.IO.File]::WriteAllText($targetFilePath, "existing`r`n")

    $sizeListing = Assert-Procedure "directory-sizes" @("DIRECTORY `"$sizeRoot`"") 0 "1.00 KB"
    foreach ($size in @("zero.bin", "one.bin", "512.bin", "513.bin")) {
        Assert-True "directory lists $size" ($sizeListing.Output -match [regex]::Escape($size)) "file missing from DIRECTORY"
    }
    foreach ($expected in @("zero.bin\s+0 KB", "one.bin\s+0.01 KB", "512.bin\s+0.50 KB", "513.bin\s+0.50 KB")) {
        Assert-True "directory exact size row $expected" ($sizeListing.Output -match $expected) "exact size row missing"
    }
    $fullListing = Assert-Procedure "directory-full-sizes" @("DIRECTORY `"$sizeRoot`" /FULL") 0 "0.50 KB"
    $defaultHidden = Assert-Procedure "directory-hidden-default" @("DIRECTORY `"$hiddenRoot`"") 0 "visible.txt" "hidden.txt|hidden-dir"
    $allHidden = Assert-Procedure "directory-all-hidden" @("DIRECTORY `"$hiddenRoot`" /ALL") 0 "hidden.txt"
    Assert-True "directory /ALL includes hidden directory" ($allHidden.Output -match "hidden-dir") "hidden directory was not listed"
    $versionRoot = Join-Path $fixtureRoot "versions"
    New-Item -ItemType Directory -Path $versionRoot -Force | Out-Null
    foreach ($version in 1..3) {
        [System.IO.File]::WriteAllText((Join-Path $versionRoot "sample.txt;$version"), "version $version")
    }
    $limitedVersions = Assert-Procedure "directory-version-limit" @(
        "DIRECTORY `"$versionRoot\sample.txt`" /VERSIONS=2"
    ) 0 "sample.txt;3" "sample.txt;1"
    Assert-True "directory /VERSIONS includes second latest" ($limitedVersions.Output -match "sample.txt;2") `
        "second latest version was not listed"
    Assert-Procedure "directory-invalid-version-limit" @(
        "DIRECTORY `"$versionRoot\sample.txt`" /VERSIONS=0"
    ) 1 "BADVERSIONS"

    Assert-Procedure "show-time" @("SHOW TIME") 0 "\d{2}-[A-Z]{3}-\d{4}"
    $memoryReport = Assert-Procedure "show-memory-default-all" @("SHOW MEMORY") 0 "System Memory Resources"
    Assert-True "show memory default includes all standard sections" (
        $memoryReport.Output -match "Physical Memory Usage" -and
        $memoryReport.Output -match "Cache Usage" -and
        $memoryReport.Output -match "Slot Usage" -and
        $memoryReport.Output -match "Dynamic Memory Usage" -and
        $memoryReport.Output -match "Paging File Usage" -and
        $memoryReport.Output -notmatch "Windows mapping|maps to Windows"
    ) "SHOW MEMORY did not include its default DCL report sections"
    Assert-Procedure "show-process" @("SHOW PROCESS") 0 "Process Name"
    Assert-Procedure "show-system" @("SHOW SYSTEM") 0 "Grand total:"
    $systemReport = Assert-Procedure "show-system-release-version" @("SHOW SYSTEM") 0 "Windows release:"
    Assert-True "show system includes OS version" ($systemReport.Output -match "OS version: \d+\.\d+\.\d+") `
        "SHOW SYSTEM omitted the Windows OS version"
    $cacheReport = Assert-Procedure "show-memory-cache" @("SHOW MEMORY /CACHE") 0 "Cache Usage"
    Assert-True "show memory cache selects cache section" (
        $cacheReport.Output -notmatch "Physical Memory Usage|Slot Usage|Dynamic Memory Usage|Paging File Usage"
    ) "SHOW MEMORY/CACHE included unrelated report sections"
    $poolReport = Assert-Procedure "show-memory-pool" @("SHOW MEMORY /POOL") 0 "Dynamic Memory Usage"
    Assert-True "show memory pool selects pool section" (
        $poolReport.Output -notmatch "Physical Memory Usage|Cache Usage|Slot Usage|Paging File Usage"
    ) "SHOW MEMORY/POOL included unrelated report sections"
    Assert-Procedure "show-memory-files" @("SHOW MEMORY /FILES") 0 "Paging File Usage"
    Assert-Procedure "show-memory-slots" @("SHOW MEMORY /SLOTS") 0 "Slot Usage"
    Assert-Procedure "show-memory-physical-pages" @("SHOW MEMORY /PHYSICAL_PAGES") 0 "Physical Memory Usage"
    Assert-Procedure "show-memory-reserved" @("SHOW MEMORY /RESERVED") 0 "Reserved Memory"
    Assert-Procedure "show-memory-bitmap" @("SHOW MEMORY /BITMAP") 0 "Memory Bitmap"
    Assert-Procedure "show-memory-buffer-objects" @("SHOW MEMORY /BUFFER_OBJECTS") 0 "Buffer Objects"
    Assert-Procedure "show-memory-gh-regions" @("SHOW MEMORY /GH_REGIONS") 0 "Granularity Hint Regions"
    Assert-Procedure "show-memory-full-pool" @("SHOW MEMORY /POOL /FULL") 0 "Total kernel memory:"
    Assert-Procedure "show-memory-full-files" @("SHOW MEMORY /FILES /FULL") 0 "Peak usage:"
    Assert-Procedure "show-memory-full-cache" @("SHOW MEMORY /CACHE /FULL") 0 "Configured cache limits|Configured minimum"
    $memoryFullDefault = Assert-Procedure "show-memory-full-default" @("SHOW MEMORY /FULL") 0 "Paging File Usage"
    Assert-True "show memory full alone adds no section detail" (
        $memoryFullDefault.Output -notmatch "Total kernel memory:|Peak usage:|Configured minimum"
    ) "SHOW MEMORY/FULL without a section qualifier should not expand sections"
    Assert-Procedure "show-memory-units" @("SHOW MEMORY /CACHE /UNITS") 0 "bytes"
    Assert-Procedure "show-memory-page" @("SHOW MEMORY /PAGE") 0 "System Memory Resources"
    Assert-Procedure "show-memory-page-save-search" @(
        "SHOW MEMORY /PAGE=SAVE=5 /SEARCH=cache /EXACT /HIGHLIGHT=REVERSE"
    ) 0 "Cache Usage"
    Assert-Procedure "show-memory-page-save-quoted-search" @(
        'SHOW MEMORY /PAGE=SAVE /SEARCH="Cache Usage"'
    ) 0 "Cache Usage"
    Assert-Procedure "show-memory-page-save-abbreviations" @(
        'SHOW MEMORY /PA=SAVE=5 /SEA="Cache Usage"'
    ) 0 "Cache Usage"
    Assert-Procedure "show-memory-page-scroll" @("SHOW MEMORY /PAGE=SCROLL") 0 "System Memory Resources"
    Assert-Procedure "show-memory-page-clear" @("SHOW MEMORY /PAGE=CLEAR_SCREEN") 0 "System Memory Resources"
    Assert-Procedure "show-memory-wrap-requires-save" @("SHOW MEMORY /WRAP") 1 "IVQUAL"
    Assert-Procedure "show-memory-wrap" @("SHOW MEMORY /PAGE=SAVE /WRAP") 0 "System Memory Resources"
    Assert-Procedure "show-memory-nowrap" @("SHOW MEMORY /PAGE=SAVE /NOWRAP") 0 "System Memory Resources"
    Assert-Procedure "show-memory-nooutput" @("SHOW MEMORY /NOOUTPUT") 0 $null `
        "System Memory Resources|Physical Memory Usage|Cache Usage"
    Assert-Procedure "show-memory-page-redirect-conflict" @("SHOW MEMORY /PAGE /OUTPUT=memory.txt") 1 "IVQUAL"
    Assert-Procedure "show-memory-search-requires-page-save" @("SHOW MEMORY /SEARCH=cache") 1 "IVQUAL"
    Assert-Procedure "show-memory-invalid-page-mode" @("SHOW MEMORY /PAGE=UNKNOWN") 1 "IVQUAL"
    Assert-Procedure "show-memory-invalid-save-count" @("SHOW MEMORY /PAGE=SAVE=0") 1 "IVQUAL"
    Assert-Procedure "show-memory-all" @("SHOW MEMORY /ALL") 0 "Paging File Usage"
    Assert-Procedure "show-network" @("SHOW NETWORK") 0 "Internet \(Windows-reported\)"
    Assert-Procedure "show-devices" @("SHOW DEVICES") 0 "Device"
    Assert-Procedure "show-time-difference" @("SHOW TIME /DIFF") 0 "Local-to-UTC difference:"
    $showReportSpec = Join-Path $ioRoot "show-time.txt"
    $showReportPath = Join-Path $ioRoot "show-time.txt;1"
    Assert-Procedure "show-output-file" @("SHOW TIME /OUTPUT=$showReportSpec") 0
    Assert-True "show output contains report" ([System.IO.File]::ReadAllText($showReportPath) -match "\d{2}-[A-Z]{3}-\d{4}") `
        "SHOW/OUTPUT did not write the report"
    $memoryOutputSpec = Join-Path $ioRoot "memory-report.txt"
    $memoryOutputPath = Join-Path $ioRoot "memory-report.txt;1"
    Assert-Procedure "show-memory-output-pool" @(
        "SHOW MEMORY /POOL /OUTPUT=$memoryOutputSpec"
    ) 0
    $memoryOutput = [System.IO.File]::ReadAllText($memoryOutputPath)
    Assert-True "show memory output preserves selected section" (
        $memoryOutput -match "Dynamic Memory Usage" -and
        $memoryOutput -notmatch "Cache Usage|Paging File Usage"
    ) "SHOW MEMORY/OUTPUT did not preserve the /POOL selection"
    Assert-Procedure "show-logical" @(
        "DEFINE WCL_REGRESSION_LOGICAL C:\wcl-regression",
        "SHOW LOGICAL WCL_REGRESSION_LOGICAL"
    ) 0 "WCL_REGRESSION_LOGICAL"
    Assert-Procedure "show-symbol" @("SHOW SYMBOL") 0

    Assert-Procedure "type-tail" @("TYPE `"$sourceFilePath`" /TAIL=2") 0 "beta" "alpha"
    $typeOutputSpec = Join-Path $ioRoot "type-output.txt"
    $typeOutputPath = Join-Path $ioRoot "type-output.txt;1"
    Assert-Procedure "type-output-file" @("TYPE `"$sourceFilePath`" /OUTPUT=$typeOutputSpec") 0
    Assert-True "type output contents" ([System.IO.File]::ReadAllText($typeOutputPath) -eq
        [System.IO.File]::ReadAllText($sourceFilePath)) "TYPE/OUTPUT did not preserve file contents"
    Assert-Procedure "type-nooutput" @("TYPE `"$sourceFilePath`" /NOOUTPUT") 0 $null "alpha|beta|gamma"
    $typePageNl = Assert-Procedure "type-page-nl" @("TYPE/PAGE NL:") 0
    Assert-True "type-page-nl clears screen" ($typePageNl.Output.Contains([char]27 + "[2J")) "TYPE/PAGE NL: did not emit clear screen sequence"
    Assert-Procedure "type-nl" @("TYPE NL:") 0
    Assert-Procedure "type-page-nla0" @("TYPE/PAGE NLA0:") 0
    Assert-Procedure "type-page-file" @("TYPE/PAGE `"$sourceFilePath`"") 0 "alpha"
    Assert-Procedure "type-page-clear-screen" @("TYPE/PAGE=CLEAR_SCREEN `"$sourceFilePath`"") 0 "alpha"
    Assert-Procedure "type-page-scroll" @("TYPE/PAGE=SCROLL `"$sourceFilePath`"") 0 "alpha"
    Assert-Procedure "type-nopage" @("TYPE/NOPAGE `"$sourceFilePath`"") 0 "alpha"
    Assert-Procedure "type-page-output-conflict" @("TYPE/PAGE/OUTPUT=$typeOutputSpec `"$sourceFilePath`"") 1 "IVQUAL"
    Assert-Procedure "type-page-invalid-mode" @("TYPE/PAGE=INVALID `"$sourceFilePath`"") 1 "IVQUAL"
    $copyPreview = Assert-Procedure "copy-dryrun" @("COPY `"$sourceFilePath`" `"$targetFilePath`" /DRYRUN") 0 "DRYRUN"
    Assert-True "copy dry-run leaves target unchanged" ([System.IO.File]::ReadAllText($targetFilePath) -eq "existing`r`n") "target changed during dry-run"
    Assert-Procedure "append-dryrun" @("APPEND `"$sourceFilePath`" `"$targetFilePath`" /DRYRUN") 0 "DRYRUN"
    Assert-True "append dry-run leaves target unchanged" ([System.IO.File]::ReadAllText($targetFilePath) -eq "existing`r`n") "target changed during dry-run"
    $renameTarget = Join-Path $ioRoot "renamed.txt"
    Assert-Procedure "rename-dryrun" @("RENAME `"$sourceFilePath`" `"$renameTarget`" /DRYRUN") 0 "DRYRUN"
    Assert-True "rename dry-run preserves source" (Test-Path -LiteralPath $sourceFilePath) "source missing after dry-run"
    Assert-True "rename dry-run does not create target" (-not (Test-Path -LiteralPath $renameTarget)) "target created during dry-run"
    Assert-Procedure "delete-dryrun" @("DELETE `"$sourceFilePath`" /DRYRUN") 0 "DRYRUN"
    Assert-True "delete dry-run preserves source" (Test-Path -LiteralPath $sourceFilePath) "source deleted during dry-run"
    $newFile = Join-Path $ioRoot "created.txt"
    Assert-Procedure "create-dryrun" @("CREATE `"$newFile`" /DRYRUN") 0 "DRYRUN"
    Assert-True "create dry-run creates no file" (-not (Test-Path -LiteralPath $newFile)) "file created during dry-run"
    Assert-Procedure "search-dryrun" @("SEARCH `"$sourceFilePath`" beta /DRYRUN") 0 "DRYRUN" "alpha|beta|gamma"
    Assert-Procedure "search-limits" @("SEARCH `"$sourceFilePath`" a /SKIP=1 /LIMIT=1 /NUMBERS") 0 "beta"
    Assert-Procedure "search-exact-numbered-heading" @(
        "SEARCH `"$sourceFilePath`" beta /EXACT /NUMBERS /HEADING"
    ) 0 "(?s)source\.txt.*2\s+beta"
    Assert-Procedure "search-nooutput" @(
        "SEARCH `"$sourceFilePath`" beta /NOOUTPUT"
    ) 0 "Found 1 matching record" "beta"
    $searchOutputSpec = Join-Path $ioRoot "search-output.txt"
    $searchOutputPath = Join-Path $ioRoot "search-output.txt;1"
    Assert-Procedure "search-output-file" @(
        "SEARCH `"$sourceFilePath`" beta /OUTPUT=$searchOutputSpec"
    ) 0
    Assert-True "search output contents" ([System.IO.File]::ReadAllText($searchOutputPath) -match "beta") `
        "SEARCH/OUTPUT did not write matching records"
    Assert-Procedure "search-invalid-skip" @(
        "SEARCH `"$sourceFilePath`" beta /SKIP=bad"
    ) 1 "IVQUAL"

    $copyLoggedPath = Join-Path $ioRoot "copy-logged.txt;1"
    Assert-Procedure "copy-log" @(
        "COPY `"$sourceFilePath`" `"$copyLoggedPath`" /LOG"
    ) 0 "copied to"
    Assert-True "copy log creates destination" (Test-Path -LiteralPath $copyLoggedPath) `
        "COPY/LOG did not create its destination"
    $appendTargetPath = Join-Path $ioRoot "append-logged.txt"
    [System.IO.File]::WriteAllText($appendTargetPath, "before`r`n")
    Assert-Procedure "append-log" @(
        "APPEND `"$sourceFilePath`" `"$appendTargetPath`" /LOG"
    ) 0 "appended to"
    Assert-True "append adds source data" ([System.IO.File]::ReadAllText($appendTargetPath) -match "(?s)before.*alpha") `
        "APPEND did not append source data"
    $renameSourcePath = Join-Path $ioRoot "rename-source.txt"
    $renameLoggedPath = Join-Path $ioRoot "rename-logged.txt;1"
    [System.IO.File]::WriteAllText($renameSourcePath, "rename me")
    Assert-Procedure "rename-log" @(
        "RENAME `"$renameSourcePath`" `"$renameLoggedPath`" /LOG"
    ) 0 "renamed to"
    Assert-True "rename moves source" (-not (Test-Path -LiteralPath $renameSourcePath) -and
        (Test-Path -LiteralPath $renameLoggedPath)) "RENAME did not move the file"
    $deleteLoggedPath = Join-Path $ioRoot "delete-logged.txt"
    [System.IO.File]::WriteAllText($deleteLoggedPath, "delete me")
    Assert-Procedure "delete-log" @(
        "DELETE `"$deleteLoggedPath`" /LOG"
    ) 0 "deleted"
    Assert-True "delete removes target" (-not (Test-Path -LiteralPath $deleteLoggedPath)) `
        "DELETE did not remove the file"
    $createDirectory = Join-Path $ioRoot "created-directory"
    Assert-Procedure "create-directory-log" @(
        "CREATE `"$createDirectory`" /DIRECTORY /LOG"
    ) 0 "created"
    Assert-True "create directory qualifier" (Test-Path -LiteralPath $createDirectory -PathType Container) `
        "CREATE/DIRECTORY did not create a directory"
    $printPreview = Assert-Procedure "print-dryrun" @(
        "PRINT `"$sourceFilePath`" /DRYRUN"
    ) 0 "DRYRUN"
    Assert-True "print dry-run does not invoke print" ($printPreview.Output -match "no change was made") `
        "PRINT/DRYRUN did not report a non-mutating preview"

    $definedName = "WCL_REGRESSION_LOGICAL"
    $defineResult = Assert-Procedure "define-dryrun" @(
        "DEFINE $definedName C:\wcl-regression /DRYRUN",
        "SHOW LOGICAL $definedName"
    ) 1 "DRYRUN"
    $deassignResult = Assert-Procedure "deassign-dryrun" @(
        "DEFINE $definedName C:\wcl-regression",
        "DEASSIGN $definedName /DRYRUN",
        "SHOW LOGICAL $definedName"
    ) 0 $definedName
    Assert-True "define dry-run does not define logical" ($defineResult.Output -match "No logical names matched") "logical was created during dry-run"
    Assert-True "deassign dry-run preserves logical" ($deassignResult.Output -match $definedName) "logical missing after dry-run"

    $confirmResult = Assert-Procedure "delete-confirm-noninteractive" @("DELETE `"$sourceFilePath`" /CONFIRM") 1 "NOCONSOLE"
    Assert-True "confirmation fails closed" (Test-Path -LiteralPath $sourceFilePath) "target changed without interactive confirmation"
    Assert-Procedure "initialize-system-volume-dryrun" @(
        "INITIALIZE $($env:SystemDrive)\ /DRYRUN"
    ) 1 "SYSTEMVOLUME|BADDEVICE"

    $readTest = Assert-Procedure "open-read-write-close" @(
        "OPEN/READ INPUT `"$sourceFilePath`"",
        "READ INPUT RECORD",
        "CLOSE INPUT",
        "OPEN/WRITE OUTPUT `"$((Join-Path $ioRoot "written.txt"))`"",
        "WRITE OUTPUT `"regression-output`"",
        "CLOSE OUTPUT",
        "TYPE `"$((Join-Path $ioRoot "written.txt"))`""
    ) 0 "regression-output"
    $appendChannelPath = Join-Path $ioRoot "channel-append.txt"
    [System.IO.File]::WriteAllText($appendChannelPath, "existing`r`n")
    Assert-Procedure "open-append-channel" @(
        "OPEN/APPEND APPEND_CHANNEL `"$appendChannelPath`"",
        'WRITE APPEND_CHANNEL "appended-record"',
        "CLOSE APPEND_CHANNEL"
    ) 0
    Assert-True "OPEN/APPEND appends records" ([System.IO.File]::ReadAllText($appendChannelPath) -match
        "(?s)existing.*appended-record") "OPEN/APPEND did not append"
    $dispositionDeletePath = Join-Path $ioRoot "close-disposition-delete.txt;1"
    Assert-Procedure "close-disposition-delete" @(
        "OPEN/WRITE DELETE_CHANNEL `"$dispositionDeletePath`"",
        'WRITE DELETE_CHANNEL "temporary-record"',
        "CLOSE DELETE_CHANNEL /DISPOSITION=DELETE"
    ) 0
    Assert-True "CLOSE disposition deletes output" (-not (Test-Path -LiteralPath $dispositionDeletePath)) `
        "CLOSE/DISPOSITION=DELETE did not remove the file"

    Write-Host ""
    Write-Host "Procedure control-flow regressions:"
    Assert-Procedure "procedure-if-else-and-string-condition" @(
        'IF 1 .EQ. 1 THEN',
        'WRITE SYS$OUTPUT IF_TRUE',
        'ELSE',
        'WRITE SYS$OUTPUT IF_FALSE',
        'ENDIF',
        'IF "left" .EQS. "left" THEN',
        'WRITE SYS$OUTPUT STRING_TRUE',
        'ENDIF'
    ) 0 "IF_TRUE" "IF_FALSE"
    Assert-Procedure "procedure-dcl-truth-values" @(
        'IF 2 THEN',
        'WRITE SYS$OUTPUT WRONG_EVEN_TRUE',
        'ELSE',
        'WRITE SYS$OUTPUT EVEN_FALSE',
        'ENDIF',
        'IF 3 THEN',
        'WRITE SYS$OUTPUT ODD_TRUE',
        'ENDIF',
        'IF "no" THEN',
        'WRITE SYS$OUTPUT WRONG_NO_TRUE',
        'ELSE',
        'WRITE SYS$OUTPUT NO_FALSE',
        'ENDIF',
        'IF "yes" THEN',
        'WRITE SYS$OUTPUT YES_TRUE',
        'ENDIF',
        'IF "FALSE" THEN',
        'WRITE SYS$OUTPUT WRONG_FALSE_TRUE',
        'ELSE',
        'WRITE SYS$OUTPUT FALSE_FALSE',
        'ENDIF'
    ) 0 'EVEN_FALSE\s+ODD_TRUE\s+NO_FALSE\s+YES_TRUE\s+FALSE_FALSE' 'WRONG_'
    Assert-Procedure "procedure-parenthesized-conditions" @(
        'IF (1 .EQ. 1) .AND. (2 .EQ. 2) THEN',
        'WRITE SYS$OUTPUT GROUPED_AND',
        'ELSE',
        'WRITE SYS$OUTPUT WRONG_AND',
        'ENDIF',
        'IF ((1 .EQ. 0) .OR. (3 .EQ. 3)) THEN',
        'WRITE SYS$OUTPUT GROUPED_OR',
        'ENDIF'
    ) 0 'GROUPED_AND' 'WRONG_AND'
    Assert-Procedure "procedure-condition-overflow" @(
        'IF 999999999999999999999999 .EQ. 1 THEN',
        'WRITE SYS$OUTPUT WRONG_OVERFLOW'
    ) 1 'PROCCOND'
    $quotedApostropheCommand = 'WRITE SYS$OUTPUT "Hello ' + "''" + "VALUE'" + '"'
    Assert-Procedure "procedure-apostrophe-substitution" @(
        'VALUE = "World"',
        "WRITE SYS$OUTPUT 'VALUE'",
        $quotedApostropheCommand,
        'WRITE SYS$OUTPUT "&&VALUE"'
    ) 0 'World\s+Hello World\s+&VALUE'
    Assert-Procedure "procedure-line-continuation" @(
        'WRITE SYS$OUTPUT "continued" -',
        '$ "command"',
        'WRITE SYS$OUTPUT "second" -',
        '    "continuation"'
    ) 0 'continued command\s+second continuation'
    Assert-Procedure "procedure-unfinished-continuation" @(
        'WRITE SYS$OUTPUT unfinished -'
    ) 1 'PROCREAD'
    $loopResult = Assert-Procedure "procedure-goto-loop" @(
        'count = 0',
        'loop:',
        'IF count .EQ. 3 THEN GOTO done',
        'count = &count + 1',
        'GOTO loop',
        'done:',
        'WRITE SYS$OUTPUT count'
    ) 0 '3'
    Assert-Procedure "procedure-gosub-return" @(
        'GOSUB subroutine',
        'WRITE SYS$OUTPUT AFTER_SUBROUTINE',
        'EXIT 0',
        'subroutine:',
        'WRITE SYS$OUTPUT IN_SUBROUTINE',
        'RETURN'
    ) 0 'IN_SUBROUTINE' 'UNREACHABLE'
    Assert-Procedure "procedure-gosub-restores-if" @(
        'IF 1 .EQ. 1 THEN',
        'GOSUB nested',
        'WRITE SYS$OUTPUT IF_RESTORED',
        'ELSE',
        'WRITE SYS$OUTPUT WRONG_BRANCH',
        'ENDIF',
        'EXIT 0',
        'nested:',
        'WRITE SYS$OUTPUT IN_NESTED',
        'RETURN'
    ) 0 'IF_RESTORED' 'WRONG_BRANCH'
    Assert-Procedure "procedure-error-goto" @(
        'ON ERROR GOTO recovered',
        'WCL_COMMAND_THAT_DOES_NOT_EXIST',
        'WRITE SYS$OUTPUT WRONG_CONTINUATION',
        'recovered:',
        'WRITE SYS$OUTPUT ERROR_RECOVERED'
    ) 0 'ERROR_RECOVERED' 'WRONG_CONTINUATION'
    Assert-Procedure "procedure-error-then" @(
        'ON ERROR THEN WRITE SYS$OUTPUT ERROR_HANDLER_RAN',
        'WCL_COMMAND_THAT_DOES_NOT_EXIST',
        'WRITE SYS$OUTPUT PROCEDURE_CONTINUED'
    ) 0 'ERROR_HANDLER_RAN' 'WCL_COMMAND_THAT_DOES_NOT_EXIST'
    Assert-Procedure "procedure-set-noon" @(
        'SET NOON',
        'WCL_COMMAND_THAT_DOES_NOT_EXIST',
        'WRITE SYS$OUTPUT CONTINUED_AFTER_ERROR'
    ) 0 'CONTINUED_AFTER_ERROR'
    Assert-Procedure "procedure-set-verify" @(
        'WRITE SYS$OUTPUT BEFORE_VERIFY',
        'SET VERIFY',
        'WRITE SYS$OUTPUT VERIFY_ON',
        'SET NOVERIFY',
        'WRITE SYS$OUTPUT VERIFY_OFF'
    ) 0 '\$\s+WRITE SYS\$OUTPUT VERIFY_ON\s+VERIFY_ON\s+\$\s+SET NOVERIFY\s+VERIFY_OFF' '\$\s+WRITE SYS\$OUTPUT VERIFY_OFF'
    Assert-Procedure "procedure-verify-lexical" @(
        'WRITE SYS$OUTPUT F$VERIFY()',
        'WRITE SYS$OUTPUT F$VERIFY(1)',
        'WRITE SYS$OUTPUT F$ENVIRONMENT("VERIFY_PROCEDURE")',
        'OLD = F$VERIFY(0)',
        'WRITE SYS$OUTPUT OLD',
        'WRITE SYS$OUTPUT F$VERIFY(,1)',
        'WRITE SYS$OUTPUT F$ENVIRONMENT("VERIFY_IMAGE")',
        'IMAGE_OLD = F$VERIFY(,0)',
        'WRITE SYS$OUTPUT F$ENVIRONMENT("VERIFY_IMAGE")'
    ) 0 '0\s+0\s+\$\s+WRITE SYS\$OUTPUT F\$ENVIRONMENT\("VERIFY_PROCEDURE"\)\s+1\s+\$\s+OLD = F\$VERIFY\(0\)\s+1\s+0\s+1\s+0'
    Assert-Procedure "procedure-set-ambiguous-abbreviation" @(
        'SET N'
    ) 1 'AMBIG'
    Assert-Procedure "procedure-return-status" @(
        'RETURN 17'
    ) 17

    $childProcedure = Join-Path $procedureRoot "nested-child.com"
    [System.IO.File]::WriteAllLines($childProcedure, @('WRITE SYS$OUTPUT P1'), [System.Text.Encoding]::ASCII)
    $nestedChildCall = '@"' + $childProcedure + '" "child-value"'
    Assert-Procedure "procedure-nested-parameter-scope" @(
        'WRITE SYS$OUTPUT P1',
        $nestedChildCall,
        'WRITE SYS$OUTPUT P1'
    ) 0 'parent-value\s+child-value\s+parent-value' $null -Parameters @('parent-value')
    Assert-Procedure "procedure-parameter-limit" @(
        'WRITE SYS$OUTPUT P1'
    ) 1 'PROCLIMIT' $null -Parameters @('1','2','3','4','5','6','7','8','9')

    $cleanupFile = Join-Path $ioRoot "nested-channel-cleanup.txt"
    $cleanupChild = Join-Path $procedureRoot "nested-channel-child.com"
    [System.IO.File]::WriteAllLines($cleanupChild, @(
        "OPEN/WRITE CHILD `"$cleanupFile`"",
        'WRITE CHILD "nested-cleanup-record"'
    ), [System.Text.Encoding]::ASCII)
    $cleanupCall = '@"' + $cleanupChild + '"'
    Assert-Procedure "procedure-nested-channel-cleanup" @(
        $cleanupCall,
        "OPEN/READ CHECK `"$cleanupFile`"",
        'READ CHECK VALUE',
        'WRITE SYS$OUTPUT VALUE',
        'CLOSE CHECK'
    ) 0 'nested-cleanup-record'

    for ($index = 0; $index -lt 9; $index++) {
        $chainPath = Join-Path $procedureRoot ("depth-{0}.com" -f $index)
        if ($index -lt 8) {
            $nextPath = Join-Path $procedureRoot ("depth-{0}.com" -f ($index + 1))
            [System.IO.File]::WriteAllLines($chainPath, @('@"' + $nextPath + '"'), [System.Text.Encoding]::ASCII)
        } else {
            [System.IO.File]::WriteAllLines($chainPath, @('WRITE SYS$OUTPUT DEPTH_LIMIT_REACHED'), [System.Text.Encoding]::ASCII)
        }
    }
    Assert-Procedure "procedure-nesting-limit" @(
        '@"' + (Join-Path $procedureRoot "depth-0.com") + '"'
    ) 1 'PROCLIMIT'

    Assert-Procedure "external-path-launch" @("where.exe cmd.exe") 0 "cmd.exe"
    Assert-Procedure "invalid-qualifier" @("DIRECTORY `"$sizeRoot`" /BOGUS") 1 "IVQUAL"
    Assert-Procedure "invalid-type-qualifier" @("TYPE `"$sourceFilePath`" /BOGUS") 1 "IVQUAL"
    Assert-Procedure "invalid-search-qualifier" @("SEARCH `"$sourceFilePath`" beta /BOGUS") 1 "IVQUAL"
    Assert-Procedure "invalid-copy-qualifier" @("COPY `"$sourceFilePath`" `"$targetFilePath`" /BOGUS") 1 "IVQUAL"
    Assert-Procedure "invalid-open-qualifier" @("OPEN/BOGUS INPUT `"$sourceFilePath`"") 1 "IVQUAL"
    Assert-Procedure "invalid-show-qualifier" @("SHOW PROCESS /BOGUS") 1 "IVQUAL"
    Assert-Procedure "invalid-set-subcommand" @("SET BOGUS VALUE") 1 "IVSUBVERB"
    Assert-Procedure "directory-command-abbreviation" @("DIR `"$sizeRoot`"") 0 "zero.bin"
    Assert-Procedure "invalid-argument" @("COPY") 1 "INSFARG"
    Assert-Procedure "stop-requires-target" @("STOP") 1 "IVQUAL"
    Assert-Procedure "stop-invalid-process-id" @("STOP/ID=0") 1 "IVQUAL"
    Assert-Procedure "stop-missing-process-name" @("STOP WCL_NO_SUCH_PROCESS.exe") 1 "NOPROCESS"
    $pwshBin = if (Test-Path (Join-Path $PSHOME "pwsh.exe")) { (Join-Path $PSHOME "pwsh.exe") } else { (Join-Path $PSHOME "powershell.exe") }
    $stopTarget = Start-Process -FilePath $pwshBin `
        -ArgumentList @("-NoProfile", "-Command", "Start-Sleep -Seconds 30") `
        -PassThru -WindowStyle Hidden
    try {
        Assert-Procedure "stop-ambiguous-image-name" @("STOP $($stopTarget.ProcessName).exe /DRYRUN") 1 "AMBIGPROC"
        Assert-Procedure "stop-dryrun" @("STOP/ID=$($stopTarget.Id) /DRYRUN") 0 "DRYRUN"
        $stopTarget.Refresh()
        Assert-True "stop dry-run preserves process" (-not $stopTarget.HasExited) `
            "STOP/DRYRUN terminated the target process"
        Assert-Procedure "stop-confirm-noninteractive" @(
            "STOP/ID=$($stopTarget.Id) /CONFIRM"
        ) 1 "NOCONSOLE"
        $stopTarget.Refresh()
        Assert-True "stop confirmation fails closed" (-not $stopTarget.HasExited) `
            "STOP/CONFIRM terminated a process without interactive confirmation"
        Assert-Procedure "stop-by-id" @("STOP/ID=$($stopTarget.Id)") 0 "PROCESSSTOP"
        Assert-True "stop-by-id terminates target" ($stopTarget.WaitForExit(5000)) `
            "STOP/ID did not terminate the target process"
    } finally {
        $stopTarget.Refresh()
        if (-not $stopTarget.HasExited) {
            Stop-Process -Id $stopTarget.Id -Force
        }
    }
    $stopName = "wcl-stop-target-$([guid]::NewGuid().ToString('N')).exe"
    $stopNamePath = Join-Path $tempRoot $stopName
    Copy-Item -LiteralPath $executable -Destination $stopNamePath
    $stopNameStart = New-Object System.Diagnostics.ProcessStartInfo
    $stopNameStart.FileName = $stopNamePath
    $stopNameStart.UseShellExecute = $false
    $stopNameStart.CreateNoWindow = $true
    $stopNameStart.RedirectStandardInput = $true
    $stopNameStart.RedirectStandardOutput = $true
    $stopNameStart.RedirectStandardError = $true
    $stopNameTarget = [System.Diagnostics.Process]::Start($stopNameStart)
    try {
        Assert-Procedure "stop-by-image-name" @("STOP $stopName") 0 "PROCESSSTOP"
        Assert-True "stop-by-name terminates target" ($stopNameTarget.WaitForExit(5000)) `
            "STOP by image name did not terminate the target process"
    } finally {
        $stopNameTarget.Refresh()
        if (-not $stopNameTarget.HasExited) {
            $stopNameTarget.Kill()
            $stopNameTarget.WaitForExit(5000) | Out-Null
        }
    }
    Assert-Procedure "run-dryrun" @("RUN `"$env:ComSpec`" /DRYRUN") 0 "DRYRUN"
    Assert-Procedure "spawn-dryrun" @("SPAWN /DRYRUN") 0 "DRYRUN"
    Assert-Procedure "submit-dryrun" @(
        "SUBMIT `"$childProcedure`" /AFTER=2099-12-31T23:59 /DRYRUN"
    ) 0 "DRYRUN"

    Write-Host ""
    Write-Host "Regression results: $script:passed passed; $script:failed failed."
    if ($script:failed) {
        Write-Host "Failures:"
        foreach ($failure in $script:failures) { Write-Host "  $failure" }
        exit 1
    }
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
