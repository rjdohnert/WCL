# Windows Command Language (WCL)

**Windows Command Language (WCL)** is a native, lightweight implementation of the DIGITAL Command Language (DCL) environment designed for modern 64-bit Microsoft Windows. 

Originally created for OpenVMS on VAX, Alpha, and Integrity architectures, DCL syntax is brought natively to the Windows NT operating system with direct mapping to Win32 APIs, NTFS version conventions, Registry management, Task Scheduler, and console subsystem features.

---

## Features

- **Native Windows Execution:** Built directly on Win32 C APIs without requiring POSIX translation layers, Cygwin, or virtual machines.
- **Authentic DCL Ergonomics:**
  - Classic prompt (`$ `) with case-insensitive verbs, parameters, and qualifiers (`/QUALIFIER`).
  - Minimum unique abbreviation matching for commands and qualifiers.
  - File versioning simulation on NTFS (`filename.ext;1`, `filename.ext;2`).
  - Process logical names and table equivalences (`DEFINE`, `ASSIGN`, `DEASSIGN`, `SHOW LOGICAL`).
  - DCL Lexical functions (`F$PARSE`, `F$EXTRACT`, `F$GETDVI`, `F$STRING`, `F$INTEGER`, etc.).
  - Command procedure scripting (`.COM` files) supporting labels, `GOTO`, conditional branching, and channel I/O (`OPEN`, `READ`, `WRITE`, `CLOSE`).
  - Paged console viewing with `/PAGE` qualifiers (e.g., `TYPE/PAGE NL:` for instant screen clearing, or `SHOW MEMORY /PAGE`).
- **Interactive Line Editing & History:**
  - Full ANSI and VT terminal support with command history recall.
  - User customization via automatic startup initialization (`%USERPROFILE%\wcl.ini`).
- **Built-in Editor:** Integrated IRIS visual and command-line text editor.

---

## Quick Start

### Building from Source

WCL builds with LLVM `clang`:

**Using PowerShell:**
```powershell
.\build.ps1
```

**Using Command Prompt:**
```cmd
build.bat
```

The compiled binary will be placed at `build\wcl.exe`.

### Installation

To install WCL to `C:\Program Files\wcl\bin`, add it to the system `PATH`, and register a Windows Terminal profile:

- Right-click `installer\install.bat` and run it (or launch `installer\install.ps1` in an elevated PowerShell session).

### Running Tests

Execute the automated test suite in PowerShell:
```powershell
powershell -ExecutionPolicy Bypass -File src\tests\regression.ps1
```

---

## Basic Usage

Launch the shell by running `wcl.exe`:

```dcl
  Windows Command Language Shell
  WCL Version: 1.0.0
  Copyright (C) PC/OpenSystems LLC. All rights reserved.
  Type "HELP" for command descriptions or "LOGOUT" to return to Windows.

$ DIRECTORY
$ SHOW SYSTEM
$ TYPE/PAGE NL:
$ DEFINE WORK E:\Projects
$ SHOW LOGICAL WORK
$ LOGOUT
```

---

## Documentation

- [User Guide](USER_GUIDE.md): Complete end-user tutorial, initialization settings, and script authoring.
- [Command Reference](command-reference.md): Detailed syntax, qualifiers, and examples for all supported verbs.
- [DCL Compatibility Guide](docs/dcl-compatibility.md): Mapping table between OpenVMS DCL constructs and Win32 equivalents.

---

## License

WCL is open-source software licensed under the [BSD 3-Clause License](LICENSE).
