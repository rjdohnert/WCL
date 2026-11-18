# Windows Command Language (WCL) — Comprehensive User Guide

> **Version:** 9.2-2026  
> **Copyright:** © 2026 PC/OpenSystems LLC. All Rights Reserved.  
> **Target Subsystem:** Win32 Console Application / Windows 10 & 11, Windows Server  

---

## Table of Contents
1. [Introduction](#1-introduction)
2. [Quick Start & Installation](#2-quick-start--installation)
3. [Shell Basics & Syntax Conventions](#3-shell-basics--syntax-conventions)
4. [Startup Configuration (`wcl.ini`)](#4-startup-configuration-wclini)
5. [Interactive Line Editing & History](#5-interactive-line-editing--history)
6. [Symbols & Logical Names](#6-symbols--logical-names)
7. [DCL Lexical Functions (`F$*`)](#7-dcl-lexical-functions-f)
8. [DCL Scripting Guide (Command Procedures)](#8-dcl-scripting-guide-command-procedures)
9. [Pipelines & I/O Redirection (`PIPE`)](#9-pipelines--io-redirection-pipe)
10. [IRIS Full-Screen Text Editor (EVE/EDT Clone)](#10-iris-full-screen-text-editor-eveedt-clone)
11. [Safety Features: `/CONFIRM` and `/DRYRUN`](#11-safety-features-confirm-and-dryrun)
12. [System Administration & Environment Controls](#12-system-administration--environment-controls)
13. [Troubleshooting & FAQ](#13-troubleshooting--faq)

---

## 1. Introduction

**WCL (Windows Command Language)** brings the power, consistency, and elegance of Digital Equipment Corporation's (DEC) **OpenVMS DCL** to modern Microsoft Windows. 

Unlike shells that merely port POSIX utilities or wrapper scripts, WCL natively implements:
* **True DCL Verb-Qualifier Grammar:** Command verbs followed by slash-delimited qualifiers (e.g., `DIRECTORY/FULL/SIZE`, `COPY/CONFIRM`).
* **Direct Win32 API Mapping:** Commands query Windows performance counters, Event Logs, Service Control Manager (SCM), Volume Shadow Copies, and process internals without intermediate abstraction overhead.
* **OpenVMS File Versioning Emulation:** Transparent management of versioned files (`file.txt;1`, `file.txt;2`) and the `PURGE` utility.
* **Built-in TPU/EVE Screen Editor:** The integrated **IRIS** text editor provides full-screen interactive editing directly in your terminal.
* **UTF-8 Resilience:** High-fidelity Unicode rendering for modern international file paths and usernames.

---

## 2. Quick Start & Installation

### Building from Source
WCL builds cleanly with LLVM/Clang on Windows.

1. Open PowerShell or Windows Command Prompt.
2. Ensure `clang.exe` is installed (via LLVM for Windows or Visual Studio Build Tools).
3. Run the automated build script:
   ```cmd
   build.bat
   ```
   * *AddressSanitizer Build (for debugging/diagnostics):*
     ```cmd
     build.bat asan
     ```
   * *Debug Symbols Build:*
     ```cmd
     build.bat debug
     ```
4. The compiled executable is placed at `build\wcl.exe`.

### Launching the Shell
Run `wcl.exe` from your terminal or pin it to Windows Terminal:
```cmd
.\build\wcl.exe
```
You will be greeted with the WCL banner and prompt:
```
  Windows Command Language Shell
  WCL Version: 9.2-2026
  Copyright (C) PC/OpenSystems LLC.  All rights reserved.
  Type "HELP" for command descriptions or "LOGOUT" to return to Windows.

$ 
```

---

## 3. Shell Basics & Syntax Conventions

### Command Structure
DCL commands follow a predictable structure:
```dcl
$ VERB [parameters...] [/QUALIFIER[=value]...]
```
* **Case-Insensitive:** `directory`, `Directory`, and `DIRECTORY` are identical.
* **Minimum Unique Abbreviation:** You only need to type enough characters to uniquely identify a verb or qualifier:
  * `DIR` expands to `DIRECTORY`
  * `SHO SYS` expands to `SHOW SYSTEM`
  * `SHO DEV` expands to `SHOW DEVICES`
  * `COP` expands to `COPY`
* **Qualifiers (`/`):** Modify how verbs behave. Qualifiers can appear after the verb or after parameters:
  ```dcl
  $ DIRECTORY /SIZE /DATE /FULL
  $ COPY source.txt target.txt /CONFIRM /LOG
  ```
* **Negated Qualifiers:** Many qualifiers can be turned off with a `NO` prefix (e.g., `/NOLOG`, `/NOCONFIRM`, `/NOHEADING`).

---

## 4. Startup Configuration (`wcl.ini`)

Upon first launch, WCL automatically creates your user initialization file at:
```
%USERPROFILE%\wcl.ini
```
This script executes automatically whenever an interactive shell starts.

### Default Aliases & Settings
```dcl
! ============================================================================
! Windows Command Language (WCL) User Initialization File
! Location: %USERPROFILE%\wcl.ini
! ============================================================================
! Command Aliases (Foreign Commands)
LS    :== DIRECTORY
LL    :== DIRECTORY /FULL
CAT   :== TYPE
CP    :== COPY
MV    :== RENAME
RM    :== DELETE
CLEAR :== TYPE/PAGE NL:
CLS   :== TYPE/PAGE NL:
VI    :== IRIS
VIM   :== IRIS
EVE   :== IRIS

! Custom prompt
! SET PROMPT "WCL$ "
```
You can edit this file at any time with IRIS:
```dcl
$ IRIS %USERPROFILE%\wcl.ini
```

---

## 5. Interactive Line Editing & History

WCL provides full terminal keyboard navigation:

| Key Binding | Function |
| :--- | :--- |
| **Up Arrow** | Recall previous command from history |
| **Down Arrow** | Recall newer command from history |
| **Left / Right Arrow** | Move cursor within command line |
| **Home / End** | Jump to beginning / end of line |
| **Backspace** | Delete character before cursor |
| **Delete** | Delete character under cursor |
| **Ctrl + U** | Erase entire line |
| **Ctrl + C / Esc** | Cancel current command line |

### Persistent History File
All executed commands are automatically timestamped and appended to:
```
%USERPROFILE%\.wcl_history
```
Example `.wcl_history` entry:
```
[17-NOV-2026 23:15:02] SHOW SYSTEM
[17-NOV-2026 23:15:10] DIRECTORY /SIZE
[17-NOV-2026 23:15:24] PIPE SHOW USERS | SEARCH SYS$INPUT rjdohnert
```
When WCL launches, previous history is loaded into the in-memory recall buffer.

### Command Recall Built-in (`RECALL`)
WCL supports OpenVMS `RECALL` syntax:
* **Display History:**
  ```dcl
  $ RECALL /ALL
  ```
* **Recall by Number:**
  ```dcl
  $ RECALL 12
  ```
* **Recall by Command Prefix:**
  ```dcl
  $ RECALL SHO
  $ RECALL COMP
  ```
* **Erase History Buffer:**
  ```dcl
  $ RECALL /ERASE
  ```

### Interactive TAB Completion
WCL includes an intelligent, context-aware **TAB completion** engine:
* **Command Verbs:** Pressing `<TAB>` at the start of a command completes registered DCL verbs (e.g., `REG<TAB>` expands to `REGISTRY `).
* **Qualifiers:** Typing a slash `/` and pressing `<TAB>` expands DCL qualifiers (e.g., `DIRECTORY /FU<TAB>` expands to `/FULL `).
* **File Paths:** Pressing `<TAB>` on file or path arguments completes filenames and directories using native Win32 directory searches, automatically appending directory separators (`\`) when navigating folders.

---

## 6. Symbols & Logical Names

### Symbols (Variables and Aliases)
Symbols store string or integer values, or alias commands:

* **String / Numeric Assignment (`=` or `:=`):**
  ```dcl
  $ COUNT = 10
  $ NAME = "Server-01"
  $ TOTAL = COUNT * 5 + 2
  ```
* **Command Alias Assignment (`:==` or `==`):**
  ```dcl
  $ GITSTATUS :== SPAWN git status
  $ EDITHOSTS :== IRIS C:\Windows\System32\drivers\etc\hosts
  ```
* **Symbol Substitution (`'SYMBOL'`):**
  Enclose a symbol name in single quotes to substitute its value inside commands:
  ```dcl
  $ TARGET = "build"
  $ DIRECTORY 'TARGET'
  ```
* **Viewing and Deleting Symbols:**
  ```dcl
  $ SHOW SYMBOL COUNT
  $ SHOW SYMBOL /ALL
  $ DELETE /SYMBOL COUNT
  ```

### Logical Names (Virtual File System & Aliases)
Logical names map virtual devices and paths, similar to OpenVMS logical tables:
```dcl
$ DEFINE WORKSPACE E:\Github\WCL\src
$ DIRECTORY WORKSPACE:
$ DEASSIGN WORKSPACE
```
WCL provides default system logicals including `SYS$SYSROOT`, `SYS$SYSTEM`, `SYS$OUTPUT`, `SYS$INPUT`, and `SYS$ERROR`.

---

## 7. DCL Lexical Functions (`F$*`)

Lexical functions extract system metrics, format dates, and query system information:

```dcl
$ WRITE SYS$OUTPUT F$TIME()
$ WRITE SYS$OUTPUT "Machine: " + F$GETSYI("NODENAME")
$ WRITE SYS$OUTPUT "Free RAM: " + F$GETSYI("FREE_RAM_MB") + " MB"
$ WRITE SYS$OUTPUT "CPU Time: " + F$GETJPI("", "CPU_TIME")
```

### Key Lexical Functions

| Lexical Function | Purpose | Example |
| :--- | :--- | :--- |
| `F$TIME()` | Current date and time in OpenVMS format | `17-NOV-2026 23:20:15.02` |
| `F$GETSYI(item)` | System information (`NODENAME`, `VERSION`, `ARCH_NAME`, `CPU_COUNT`, `UPTIME`, `FREE_RAM_MB`) | `F$GETSYI("UPTIME")` |
| `F$GETJPI(pid, item)` | Process metrics (`CPU_TIME`, `MEM_BYTES`, `HANDLES`, `IMAGNAME`) | `F$GETJPI("", "HANDLES")` |
| `F$PRIVILEGE(priv)` | Check if current user has privilege (`SETPRV`, `SYSPRV`, `OPER`) | `F$PRIVILEGE("SYSPRV")` |
| `F$LENGTH(string)` | Length of string | `F$LENGTH("WCL") -> 3` |
| `F$EXTRACT(start, len, str)` | Substring extraction | `F$EXTRACT(0, 3, "HELLO") -> "HEL"` |
| `F$LOCATE(needle, str)` | Find position of substring | `F$LOCATE("C", "WCL") -> 1` |
| `F$ELEMENT(index, delim, str)` | Tokenize delimited string | `F$ELEMENT(1, ",", "A,B,C") -> "B"` |
| `F$EDIT(str, "UPCASE,TRIM")` | Transform text (`UPCASE`, `COLLAPSE`, `TRIM`, `COMPRESS`) | `F$EDIT(" test ", "UPCASE,TRIM")` |
| `F$SEARCH(filespec)` | Find matching files matching wildcard | `F$SEARCH("*.c")` |
| `F$PARSE(filespec, field)` | Extract path component (`DEVICE`, `DIRECTORY`, `NAME`, `TYPE`) | `F$PARSE("C:\dir\file.txt", "NAME")` |
| `F$REGISTRY(key, value [, default])` | Read Windows Registry value (`HKLM`, `HKCU`, `HKCR`, `HKU`, `HKCC`) | `F$REGISTRY("HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion", "ProductName")` |

---

## 8. DCL Scripting Guide (Command Procedures)

WCL provides a complete OpenVMS DCL script execution engine. Command procedures are stored in `.COM` files and executed via the `@` prefix operator:
```dcl
$ @my_script.com
$ @build.com RELEASE "Win64"
```

---

### 8.1 Structure of a Command Procedure

A DCL procedure is a plain text file containing a sequence of commands, labels, and directives:
* **The `$` Prefix:** In OpenVMS DCL, every command line in a procedure begins with a `$` in column 1. WCL supports both `$`-prefixed lines and clean lines.
* **Comments (`!`):** Any text following an exclamation mark `!` is treated as a comment and ignored.
* **Line Continuation (`-`):** Place a hyphen `-` at the very end of a line to continue the command onto the next line:
  ```dcl
  $ COPY /LOG /CONFIRM -
      SourceDirectory:\*.dat -
      TargetDirectory:\Backup\
  ```

---

### 8.2 Script Parameters (`P1` through `P8`)

When invoking a command procedure, you can pass up to 8 positional parameters:
```dcl
$ @deploy.com PRODUCTION WebServer01 8080
```
Inside the script, parameters are automatically bound to local symbols named `P1`, `P2`, `P3`, `P4`, `P5`, `P6`, `P7`, and `P8`. Unpassed parameters default to empty strings `""`.

#### Parameter Handling Example
```dcl
$ ! deploy.com - Application Deployment Script
$ ENVIRONMENT = P1
$ TARGET_NODE = P2
$ PORT        = P3
$
$ IF ENVIRONMENT .EQS. "" THEN INQUIRE ENVIRONMENT "Enter environment (DEV|STAGING|PRODUCTION)"
$ IF TARGET_NODE .EQS. "" THEN TARGET_NODE = "localhost"
$ IF PORT .EQS. "" THEN PORT = "80"
$
$ WRITE SYS$OUTPUT "Deploying to " + ENVIRONMENT + " on node " + TARGET_NODE + " (Port " + PORT + ")"
```

---

### 8.3 Conditional Logic & Operators

DCL provides rich comparison operators for both strings and numeric integers:

#### Relational Operators
| Numeric Operator | String Operator | Meaning | Example |
| :--- | :--- | :--- | :--- |
| `.EQ.` | `.EQS.` | Equal to | `IF COUNT .EQ. 5` / `IF NAME .EQS. "WCL"` |
| `.NE.` | `.NES.` | Not equal to | `IF STATUS .NE. 0` / `IF P1 .NES. "DEBUG"` |
| `.LT.` | `.LTS.` | Less than | `IF RETRIES .LT. 3` |
| `.LE.` | `.LES.` | Less than or equal | `IF TEMP .LE. 100` |
| `.GT.` | `.GTS.` | Greater than | `IF FILES_FOUND .GT. 0` |
| `.GE.` | `.GES.` | Greater than or equal | `IF FREE_MB .GE. 1024` |

#### Boolean Logical Operators
* `.AND.` — Logical AND (`IF (A .GT. 0) .AND. (B .GT. 0)`)
* `.OR.` — Logical OR (`IF (FLAG .EQ. 1) .OR. (FORCE .EQ. 1)`)
* `.NOT.` — Logical NOT (`IF .NOT. ACTIVE`)

#### `IF-THEN-ELSE-ENDIF` Block Syntax
Single-line syntax:
```dcl
$ IF COUNT .GT. 10 THEN WRITE SYS$OUTPUT "Count exceeded"
$ IF RETRY .EQ. 0 THEN GOTO ABORT_JOB ELSE GOTO RETRY_STEP
```
Multi-line block syntax:
```dcl
$ IF F$PRIVILEGE("SYSPRV") .EQ. 1 THEN
$     WRITE SYS$OUTPUT "Running with Administrator privileges"
$     SERVICE_MODE = "SYSTEM"
$ ELSE
$     WRITE SYS$OUTPUT "Running in standard user mode"
$     SERVICE_MODE = "USER"
$ ENDIF
```

---

### 8.4 Branching & Subroutines

#### Labels and `GOTO`
Labels must end with a colon `:` and be on a line by themselves or at the start of a command line:
```dcl
$ START_LOOP:
$ ! ... procedure body ...
$ IF MORE_WORK THEN GOTO START_LOOP
```

#### Subroutines (`GOSUB` and `RETURN`)
DCL supports call-and-return subroutines using an internal call stack up to 64 levels deep:
```dcl
$ ! Main procedure logic
$ WRITE SYS$OUTPUT "Starting batch process..."
$ GOSUB PRINT_TIMESTAMP
$
$ ! Perform work...
$ GOSUB PRINT_TIMESTAMP
$ WRITE SYS$OUTPUT "Process finished."
$ EXIT 1
$
$ ! Subroutine definition
$ PRINT_TIMESTAMP:
$   CURRENT_TIME = F$TIME()
$   WRITE SYS$OUTPUT ">>> Current Time: " + CURRENT_TIME
$   RETURN
```

#### Script Nesting
Procedures can invoke other procedures up to 8 levels deep:
```dcl
$ @sub_task.com "ParamA" "ParamB"
```
Each nested procedure has its own isolated `P1`-`P8` scope. When the child script finishes via `EXIT`, the parent script resumes immediately with the child's completion code available in `$STATUS`.

---

### 8.5 Loops and Iteration Patterns

#### 1. Counter-Based Loop
```dcl
$ INDEX = 1
$ MAX_ITERATIONS = 5
$
$ LOOP_HEAD:
$ IF INDEX .GT. MAX_ITERATIONS THEN GOTO LOOP_END
$ WRITE SYS$OUTPUT "Processing item " + F$STRING(INDEX) + " of " + F$STRING(MAX_ITERATIONS)
$ INDEX = INDEX + 1
$ GOTO LOOP_HEAD
$
$ LOOP_END:
$ WRITE SYS$OUTPUT "Loop finished."
```

#### 2. Wildcard File Search Loop (`F$SEARCH`)
```dcl
$ ! Loop through all .LOG files in directory
$ SEARCH_SPEC = "*.LOG"
$
$ NEXT_FILE:
$ FILE_NAME = F$SEARCH(SEARCH_SPEC)
$ IF FILE_NAME .EQS. "" THEN GOTO DONE_FILES
$
$ WRITE SYS$OUTPUT "Found log file: " + FILE_NAME
$ CHECKSUM 'FILE_NAME' /ALGORITHM=SHA256
$ WRITE SYS$OUTPUT "  SHA256: " + $CHECKSUM
$ GOTO NEXT_FILE
$
$ DONE_FILES:
$ WRITE SYS$OUTPUT "All files processed."
```

#### 3. Channel File Reading Loop (`OPEN/READ/CLOSE`)
```dcl
$ OPEN/READ IN_CHANNEL servers.txt
$
$ READ_LOOP:
$ SET NOON
$ READ IN_CHANNEL SERVER_ADDR
$ IF $STATUS .NE. 0 THEN GOTO CLOSE_AND_FINISH
$ SET ON
$
$ WRITE SYS$OUTPUT "Pinging server: " + SERVER_ADDR
$ PING 'SERVER_ADDR' /COUNT=2
$ GOTO READ_LOOP
$
$ CLOSE_AND_FINISH:
$ CLOSE IN_CHANNEL
$ WRITE SYS$OUTPUT "Server ping audit complete."
```

---

### 8.6 Error Handling and Flow Control

#### OpenVMS Status Semantics
In DCL, completion status codes stored in `$STATUS` follow OpenVMS condition code conventions:
* **Odd Status Codes (`1`, `3`, `5`...):** Success or Informational (`%FACILITY-S-` or `%FACILITY-I-`).
* **Even Status Codes (`0`, `2`, `4`...):** Warning, Error, or Severe Error (`%FACILITY-W-` or `%FACILITY-E-`).

To test if a previous command succeeded:
```dcl
$ COPY file1.dat file2.dat
$ IF .NOT. $STATUS THEN WRITE SYS$OUTPUT "Copy operation failed!"
```

#### Error Trapping (`ON ERROR`)
By default, WCL aborts procedure execution if an error or severe error occurs. You can intercept failures using `ON ERROR`:
```dcl
$ ! Divert failures to cleanup handler
$ ON ERROR GOTO CLEANUP_AND_EXIT
$
$ ! Risky operations
$ COPY C:\Nonexistent\*.dat D:\Backup\
$ WRITE SYS$OUTPUT "This line will not execute if COPY fails"
$ EXIT 1
$
$ CLEANUP_AND_EXIT:
$ WRITE SYS$OUTPUT "%JOB-E-ABORT, An error occurred during processing: " + $STATUS
$ EXIT $STATUS
```

#### Disabling Abort on Error (`SET NOON`)
To allow a script to continue regardless of individual command failures:
```dcl
$ SET NOON
$ DELETE obsolete.tmp;*
$ STOP /IDENTIFICATION=99999
$ SET ON
```

#### Tracing & Debugging (`SET VERIFY`)
To display each line of a command procedure as it executes:
```dcl
$ SET VERIFY
$ ! All subsequent commands are echoed to the console with a '$ ' prefix
```

---

### 8.7 Complete Production Script Examples

#### Example 1: Automated Nightly Cleanup (`CLEANUP_OLD_LOGS.COM`)
```dcl
$ ! ============================================================================
$ ! CLEANUP_OLD_LOGS.COM - Nightly Maintenance & Log Pruner
$ ! ============================================================================
$ SET NOON
$ START_TIME = F$TIME()
$ WRITE SYS$OUTPUT "=========================================================="
$ WRITE SYS$OUTPUT "Starting log maintenance routine on " + F$GETSYI("NODENAME")
$ WRITE SYS$OUTPUT "Start time: " + START_TIME
$ WRITE SYS$OUTPUT "=========================================================="
$
$ TARGET_DIR = P1
$ IF TARGET_DIR .EQS. "" THEN TARGET_DIR = "C:\Logs"
$
$ ! Purge older versions of log files, keeping only the 3 latest
$ WRITE SYS$OUTPUT "Purging older log generations in " + TARGET_DIR + "..."
$ PURGE 'TARGET_DIR'\*.LOG;* /KEEP=3 /LOG
$
$ ! Verify disk space after purge
$ FREE_MB = F$GETSYI("FREE_RAM_MB")
$ WRITE SYS$OUTPUT "System currently has " + FREE_MB + " MB available RAM."
$
$ WRITE SYS$OUTPUT "Maintenance finished at: " + F$TIME()
$ EXIT 1
```

#### Example 2: Service Monitor & Restarter (`MONITOR_SERVICE.COM`)
```dcl
$ ! ============================================================================
$ ! MONITOR_SERVICE.COM - Checks and auto-restarts a Windows Service
$ ! ============================================================================
$ SET NOON
$ SERVICE_NAME = P1
$ IF SERVICE_NAME .EQS. "" THEN SERVICE_NAME = "Spooler"
$
$ WRITE SYS$OUTPUT "Checking status of Windows service: " + SERVICE_NAME
$
$ ! Verify user has Administrator credentials
$ IF F$PRIVILEGE("SYSPRV") .EQ. 0 THEN
$     WRITE SYS$OUTPUT "%SEC-E-NOPRIV, Administrator privilege (SYSPRV) required"
$     EXIT 2
$ ENDIF
$
$ START 'SERVICE_NAME' /SERVICE
$ IF $STATUS .EQ. 0 THEN
$     WRITE SYS$OUTPUT "%SVC-S-OK, Service " + SERVICE_NAME + " is operational"
$ ELSE
$     WRITE SYS$OUTPUT "%SVC-I-ACTIVE, Service was already running or dispatched"
$ ENDIF
$
$ EXIT 1
```

#### Example 3: Multi-Parameter Deployment Pipeline (`DEPLOY_APP.COM`)
```dcl
$ ! ============================================================================
$ ! DEPLOY_APP.COM - Application Build & Staging Pipeline
$ ! Parameters: P1 = ReleaseType (DEBUG|RELEASE), P2 = TargetDir
$ ! ============================================================================
$ ON ERROR GOTO DEPLOY_FAIL
$
$ BUILD_TYPE = F$EDIT(P1, "UPCASE,TRIM")
$ TARGET_DIR = P2
$
$ IF BUILD_TYPE .EQS. "" THEN BUILD_TYPE = "RELEASE"
$ IF TARGET_DIR .EQS. "" THEN TARGET_DIR = "E:\Deployments\App"
$
$ WRITE SYS$OUTPUT "Initiating " + BUILD_TYPE + " deployment to " + TARGET_DIR
$
$ ! Step 1: Backup current production deployment
$ WRITE SYS$OUTPUT "[1/3] Archiving current deployment..."
$ BACKUP 'TARGET_DIR'\*.* 'TARGET_DIR'\Archive\ /INCREMENTAL /RECORD /LOG
$
$ ! Step 2: Build fresh binaries
$ WRITE SYS$OUTPUT "[2/3] Building binaries..."
$ IF BUILD_TYPE .EQS. "DEBUG" THEN
$     SPAWN build.bat debug
$ ELSE
$     SPAWN build.bat
$ ENDIF
$ IF $STATUS .NE. 0 THEN GOTO BUILD_ERROR
$
$ ! Step 3: Copy binaries to destination
$ WRITE SYS$OUTPUT "[3/3] Copying application to " + TARGET_DIR + "..."
$ COPY build\wcl.exe 'TARGET_DIR'\wcl.exe /CONFIRM
$
$ WRITE SYS$OUTPUT "Deployment completed successfully at " + F$TIME()
$ EXIT 1
$
$ BUILD_ERROR:
$ WRITE SYS$OUTPUT "%DEPLOY-E-BUILDFAIL, Compilation stage failed"
$ EXIT 2
$
$ DEPLOY_FAIL:
$ WRITE SYS$OUTPUT "%DEPLOY-E-ABORTED, Pipeline aborted due to error"
$ EXIT $STATUS
```

---

## 9. Pipelines & I/O Redirection (`PIPE`)

WCL implements native pipeline chaining with the `PIPE` command.

### Pipeline Syntax
```dcl
$ PIPE command-1 | command-2 [ | command-3 ... ]
```
The standard output of stage $N$ is connected directly to standard input of stage $N+1$.

### Examples

#### Search Active System Processes
```dcl
$ PIPE SHOW SYSTEM | SEARCH SYS$INPUT wcl
```

#### Paginate or Tail User Lists
```dcl
$ PIPE SHOW USERS | TYPE SYS$INPUT /TAIL=10
```

#### Conditional Execution
```dcl
! Run command-2 only if command-1 succeeds
$ PIPE COPY file.txt backup.txt && WRITE SYS$OUTPUT "Backup complete"

! Run command-2 if command-1 fails
$ PIPE DELETE obsolete.dat || WRITE SYS$OUTPUT "File was already absent"
```

---

## 10. IRIS Full-Screen Text Editor (EVE/EDT Clone)

WCL includes **IRIS**, an integrated full-screen text editor based on DEC EVE/TPU.

### Starting IRIS
```dcl
$ IRIS myfile.txt
$ IRIS /READONLY config.sys
```

### Display Layout
```
[ IRIS Editor: myfile.txt ]  Line: 1  Col: 1 [Modified] ----------------------
1 | #include <stdio.h>
2 | int main(void) {
3 |     printf("Hello, OpenVMS on Windows!\n");
4 |     return 0;
5 | }
~
~
[Command: Type Ctrl+B for command prompt, Ctrl+G for Help] ------------------
Status: 5 lines read from myfile.txt
```

### Essential Keyboard Shortcuts
* **`Ctrl + B`** — Activate the bottom Command prompt (`Command: `)
* **`Ctrl + S`** — Save buffer to disk
* **`Ctrl + Z`** — Save and exit editor
* **`Ctrl + Q`** — Quit without saving
* **`Ctrl + G`** — Open internal help screen
* **`Arrows / Home / End / PgUp / PgDn`** — Full cursor navigation

### Command Bar Commands (`Ctrl + B`)
Press `Ctrl + B`, type any command, and press Enter:
* `OPEN <file>` — Open a new file into the buffer
* `SAVE [file]` — Save buffer to current or new filename
* `RENAME <newname>` — Change current buffer filename
* `SELECT` — Start marking a text block
* `COPY` — Copy selection (or active line) to pasteboard
* `CUT` — Cut selection (or active line) to pasteboard
* `PASTE` — Insert pasteboard text at cursor
* `FIND <text>` — Search forward for text
* `REPLACE <old> <new>` — Search and replace text occurrences
* `GOTO <line>` — Jump directly to line number
* `EXIT` — Save and return to WCL

---

## 11. Safety Features: `/CONFIRM` and `/DRYRUN`

Accidental command execution on production systems can be disastrous. WCL embeds safety qualifiers across all mutating commands:

### `/DRYRUN`
Simulates command execution without modifying files or system state:
```dcl
$ DELETE *.bak;* /DRYRUN
%WCL-I-DRYRUN, DRYRUN: would delete file E:\Github\WCL\old_backup.bak;1; no change was made

$ SHUTDOWN /REBOOT /TIMEOUT=60 /DRYRUN
%WCL-I-DRYRUN, DRYRUN: would execute shutdown.exe /r /t 60 /c "DCL system shutdown initiated"
```

### `/CONFIRM`
Prompts the operator before modifying each resource:
```dcl
$ DELETE *.log;* /CONFIRM
Delete E:\Github\WCL\build.log;1? [N]: Y
%DELETE-I-DELETED, E:\Github\WCL\build.log;1 deleted

$ STOP /IDENTIFICATION=1420 /CONFIRM
Terminate process powershell.exe (PID 1420)? [N]: Y
%WCL-S-PROCESSSTOP, Windows process terminated
```

---

## 12. System Administration & Environment Controls

WCL integrates OpenVMS system management semantics with modern Windows APIs:

### Compression & Archives (`COMPRESS` & `EXPAND`)
* **Create ZIP Archives:**
  ```dcl
  $ COMPRESS logs\*.log /ZIP=daily_logs.zip /LOG
  ```
* **Extract Archives:**
  ```dcl
  $ EXPAND daily_logs.zip /DESTINATION=C:\Staging /LOG
  ```
* **NTFS File Compression:**
  ```dcl
  $ COMPRESS database.mdf /NTFS
  $ EXPAND database.mdf /NTFS
  ```

### File & Security Management (`SET FILE`)
Set Windows ACL security attributes, DACLs, or NTFS compression attributes:
```dcl
$ SET FILE secure_config.dat /PROTECTION=(S:RWED,O:RWED,G:RE,W:RE) /LOG
$ SET FILE big_table.db /COMPRESS
```

### Process Management (`SET PROCESS`)
Adjust process priorities, suspend/resume execution, or pin processes to CPU cores:
```dcl
$ SET PROCESS /PRIORITY=HIGH /IDENTIFICATION=4210
$ SET PROCESS /SUSPEND /NAME="backup_worker.exe"
$ SET PROCESS /RESUME /NAME="backup_worker.exe"
$ SET PROCESS /AFFINITY=3
```

### Console & Terminal Settings (`SET TERMINAL`)
Adjust terminal geometry, autowrap, and window captions:
```dcl
$ SET TERMINAL /WIDTH=132 /TITLE="Production Batch Monitor" /WRAP
```

### Intrusion Detection & Audit (`SHOW INTRUSION`)
Displays the authentic OpenVMS Intrusion Detection Database mapped directly to Windows Security Event ID 4625 (failed logons) and Local Security Authority lockout policies:
```dcl
$ SHOW INTRUSION
```

### Windows Registry Management (`REGISTRY` & `SHOW REGISTRY`)
Manage the Windows Registry using authentic OpenVMS DCL verbs and safety qualifiers:
* **Query Registry Keys & Values:**
  ```dcl
  $ REGISTRY SHOW "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" /VALUE="ProductName"
  $ REGISTRY SHOW HKCU\Software\AppData /VALUES /PAGE
  $ SHOW REGISTRY "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" /VALUE="ProductName"
  ```
* **Create, Set, and Delete Keys:**
  ```dcl
  $ REGISTRY CREATE HKCU\Software\CustomApp /LOG
  $ REGISTRY SET HKCU\Software\CustomApp /VALUE="LogLevel" /DATA=3 /TYPE=DWORD /LOG
  $ REGISTRY SET HKCU\Software\CustomApp /VALUE="InstallDir" /DATA="C:\App" /TYPE=SZ /LOG
  $ REGISTRY DELETE HKCU\Software\CustomApp /VALUE="LogLevel" /CONFIRM /LOG
  $ REGISTRY DELETE HKCU\Software\CustomApp /TREE /CONFIRM /LOG
  ```
* **Export and Import (.REG files):**
  ```dcl
  $ REGISTRY EXPORT HKCU\Software\CustomApp customapp.reg /LOG
  $ REGISTRY IMPORT customapp.reg /CONFIRM /LOG
  ```
* **Safety Preview with `/DRYRUN`:**
  ```dcl
  $ REGISTRY DELETE HKLM\SOFTWARE\TestingKey /TREE /DRYRUN
  ```

### Background Job Engine (`SPAWN/NOWAIT` & `SYNCHRONIZE`)
Launch detached, asynchronous tasks and monitor execution through WCL's integrated batch queue:
* **Spawn Detached Job:**
  ```dcl
  $ SPAWN/NOWAIT "powershell.exe -File long_task.ps1"
  %SPAWN-S-SPAWNED, Subprocess created with PID 18420 (Job Entry 1)
  ```
* **View Active Batch Queue:**
  ```dcl
  $ SHOW QUEUE /BATCH
  ```
* **Synchronize (Wait for Completion):**
  ```dcl
  $ SYNCHRONIZE 1
  %SYNCHRONIZE-S-JOBCOMP, Job entry completed; Process 18420 completed with status 0x00000000
  ```

### Network TCP Socket Channels (`OPEN/NETWORK`)
DCL file channels support direct TCP network communication:
* **Connect to Network Server:**
  ```dcl
  $ OPEN/WRITE /NETWORK HTTP_CHAN "api.service.internal:80"
  $ WRITE HTTP_CHAN "GET /health HTTP/1.1"
  $ WRITE HTTP_CHAN "Host: api.service.internal"
  $ WRITE HTTP_CHAN ""
  $ READ HTTP_CHAN STATUS_LINE
  $ WRITE SYS$OUTPUT "Remote Status: " + STATUS_LINE
  $ CLOSE HTTP_CHAN
  ```

### License Management Facility (`SHOW LICENSE`)
Displays the OpenVMS License Management Facility (LMF) active license table, showing Windows OS edition, WCL enterprise runtime licenses, Product Authorization Key (PAK) metadata, and processor core quotas:
```dcl
$ SHOW LICENSE
$ SHOW LICENSE /FULL
```

### Encryption & Decryption (`ENCRYPT` & `DECRYPT`)
Securely encrypts and decrypts sensitive files using 256-bit AES or Windows Data Protection API (DPAPI):
```dcl
$ ENCRYPT database_dump.bak /KEY="MySecretPassphrase" /LOG
$ DECRYPT database_dump.bak.enc /KEY="MySecretPassphrase" /LOG
$ ENCRYPT secrets.json /DPAPI
$ DECRYPT secrets.json.enc /DPAPI
```

### Network Downloads (`COPY https://...`)
Directly download remote HTTP, HTTPS, and FTP resources to local files or directories:
```dcl
$ COPY https://example.com/dist/archive.zip [.PACKAGES] /LOG
$ COPY https://raw.githubusercontent.com/curl/curl/master/README.md curl_readme.txt
```

### Password & Account Management (`SET PASSWORD`)
Changes local or Active Directory passwords interactively, with masking, via qualifiers, or with cryptographically secure generation:
```dcl
$ SET PASSWORD /GENERATE
$ SET PASSWORD admin /NEW="SecurePass123!" /LOG
$ SET PASSWORD user01 /DOMAIN=CORP
```

---

## 13. Troubleshooting & FAQ

### Q: Why do my commands say `%WCL-E-IVVERB`?
**A:** The verb was not recognized as a WCL built-in or executable in your Windows `PATH`. Type `HELP` to list all built-ins, or check your `wcl.ini` alias definitions.

### Q: How do file versions work on Windows NTFS?
**A:** NTFS does not natively support file versions, so WCL emulates DEC RMS versioning using the `;version` suffix convention (`report.txt;1`, `report.txt;2`). Commands like `DIRECTORY`, `PURGE`, `COPY`, and `DELETE` manage versions automatically.

### Q: How do I exit WCL?
**A:** Type `LOGOUT` or `EXIT` to return to your standard Windows console.
