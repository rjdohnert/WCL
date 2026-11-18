# WCL Built-in Command Reference Manual

> **Windows Command Language (WCL)**  
> **Reference Version:** 9.2-2026  
> **Format:** Alphabetical Command Reference  

---

## Command Quick Index

| Command | Category | Summary |
| :--- | :--- | :--- |
| [ALLOCATE](#allocate) | Storage | Allocates or reserves a logical drive or volume. |
| [ANALYZE](#analyze) | System Diagnostics | Analyzes crash dumps, disk structures, or event logs. |
| [APPEND](#append) | File Operations | Appends source file contents to an existing destination file. |
| [ASSIGN](#assign) | Environment | Creates or assigns a process logical name equivalence. |
| [AUTHORIZE](#authorize) | Security | Displays or inspects user security attributes and UAF credentials. |
| [BACKUP](#backup) | Storage & Archive | Copies, archives, and verifies files and directory structures. |
| [CHECKSUM](#checksum) | Security & Integrity | Computes cryptographic SHA-256 or MD5 hashes of files. |
| [CLOSE](#close) | File Channels | Closes an active I/O channel opened via `OPEN`. |
| [CONNECT](#connect) | Remote Management | Connects terminal to a remote node via SSH, RDP, or WinRM. |
| [CONTINUE](#continue) | Script Control | Resumes execution of a paused procedure or loop. |
| [CONVERT](#convert) | File Operations | Converts file record formats, line endings, or character encodings. |
| [COPY](#copy) | File Operations | Copies one or more files to a destination specification. |
| [CREATE](#create) | File Operations | Creates a new text file interactively or creates a directory. |
| [DEASSIGN](#deassign) | Environment | Removes a previously assigned logical name. |
| [DEFINE](#define) | Environment | Associates an equivalence string with a logical name. |
| [DELETE](#delete) | File Operations | Removes files, directory structures, or process symbols. |
| [DIFFERENCE](#difference) | File Operations | Compares two text files and displays line-by-line differences. |
| [DIRECTORY](#directory) | File Operations | Lists files, directories, sizes, versions, and attributes. |
| [DISCONNECT](#disconnect) | Remote Management | Disconnects an active remote console or terminal session. |
| [DISMOUNT](#dismount) | Storage | Dismounts or unbinds a disk volume or mapped drive. |
| [DUMP](#dump) | File Operations | Displays binary file contents in formatted hexadecimal and ASCII. |
| [ERRORLOG](#errorlog) | System Diagnostics | Displays recent Windows System, Application, or Security event logs. |
| [EXIT](#exit) | Script Control | Terminates the current command procedure with an exit status code. |
| [HELP](#help) | System Information | Displays interactive help topics and qualifier syntax descriptions. |
| [INITIALIZE](#initialize) | Storage | Formats a volume or initializes disk partition metadata. |
| [INQUIRE](#inquire) | Script Control | Interactively prompts the operator and sets a symbol. |
| [IRIS](#iris) | Text Editor | Launches the integrated full-screen text editor (DEC EVE clone). |
| [LOGOUT](#logout) | Shell Control | Terminates the interactive WCL session and returns to Windows. |
| [MONITOR](#monitor) | System Diagnostics | Displays real-time CPU, memory, and I/O utilization counters. |
| [MOUNT](#mount) | Storage | Mounts a volume or assigns a drive letter to a block device. |
| [OPEN](#open) | File Channels | Opens a file stream and assigns it to a logical channel name. |
| [PING](#ping) | Networking | Sends ICMP echo requests to verify remote node connectivity. |
| [PIPE](#pipe) | Shell Execution | Connects commands with standard I/O pipes and conditionals. |
| [PRINT](#print) | Spooling | Submits a document to the Windows Print Spooler queue. |
| [PRODUCT](#product) | Package Management | Queries installed software, MSI packages, or Windows updates. |
| [PURGE](#purge) | File Operations | Deletes superseded versions of files, preserving the latest. |
| [READ](#read) | File Channels | Reads a record from an open file channel into a DCL symbol. |
| [RECALL](#recall) | History & Editing | Recalls and re-executes previous command lines from history. |
| [REGISTRY](#registry) | System Administration | Views, edits, creates, deletes, and exports Windows Registry keys and values. |
| [RENAME](#rename) | File Operations | Renames or moves a file or directory. |
| [REPLY](#reply) | Operations | Broadcasts an administrative message or rings terminal bells. |
| [RUN](#run) | Process Control | Launches an external binary executable. |
| [SEARCH](#search) | File Operations | Searches files or standard input for text strings. |
| [SET](#set) | Environment | Sets shell options, prompt, verification, host, or clock time. |
| [SHOW](#show) | System Information | Displays reports on system, process, device, and network status. |
| [SHUTDOWN](#shutdown) | Operations | Initiates controlled system shutdown, restart, or power-off. |
| [SORT](#sort) | File Operations | Sorts records from an input file into an output file. |
| [SPAWN](#spawn) | Process Control | Forks execution into a Win32 subprocess and waits for completion. |
| [START](#start) | Subsystem Control | Starts a Windows service or print/batch queue. |
| [STOP](#stop) | Process Control | Terminates an identified process by image name or PID. |
| [SUBMIT](#submit) | Job Scheduling | Enqueues a `.COM` script into the Windows Task Scheduler. |
| [SYNCHRONIZE](#synchronize) | Process Control | Waits for completion of a scheduled batch job. |
| [TYPE](#type) | File Operations | Displays the text contents of one or more files. |
| [UNLOCK](#unlock) | File Operations | Clears stale file locks or closes abandoned file handles. |
| [WAIT](#wait) | Script Control | Delays procedure execution for a specified time interval. |
| [WRITE](#write) | File Channels | Writes text or symbol values to standard output or a channel. |

---

## Detailed Command Specifications

---

### ALLOCATE
**Syntax:** `ALLOCATE device-name [logical-name]`  
**Summary:** Reserves a logical device or volume.  
* **Qualifiers:**  
  * `/GENERIC` — Allocates any available device of the requested class.  
* **Windows Mapping:** Validates local drive letter availability and mounts.  
* **Example:**  
  ```dcl
  $ ALLOCATE E:
  ```

---

### ANALYZE
**Syntax:** `ANALYZE [/CRASH] [/SYSTEM] [/DISK=drive]`  
**Summary:** Inspects system crash dump files, disk integrity, or memory structures.  
* **Qualifiers:**  
  * `/CRASH` — Scans `%SystemRoot%\MEMORY.DMP` and Minidumps.  
  * `/DISK=drive` — Inspects volume geometry and NTFS cluster health.  
  * `/SYSTEM` — Analyzes kernel architecture and OS builds.  
* **Example:**  
  ```dcl
  $ ANALYZE /CRASH
  $ ANALYZE /DISK=C:
  ```

---

### APPEND
**Syntax:** `APPEND source-spec destination-spec`  
**Summary:** Appends the contents of one or more source files to the end of a destination file.  
* **Qualifiers:**  
  * `/LOG` — Reports each file appended.  
  * `/CONFIRM` — Prompts for confirmation before appending.  
* **Example:**  
  ```dcl
  $ APPEND log1.txt combined.txt /LOG
  ```

---

### ASSIGN / DEFINE
**Syntax:** `ASSIGN equivalence-string logical-name`  
**Syntax:** `DEFINE logical-name equivalence-string`  
**Summary:** Maps a logical name to a path or string definition.  
* **Qualifiers:**  
  * `/SYSTEM` — Sets system-level equivalence.  
  * `/PROCESS` — Sets process-level equivalence (default).  
* **Example:**  
  ```dcl
  $ DEFINE TOOLS C:\Tools\Bin
  $ DIRECTORY TOOLS:
  ```

---

### AUTHORIZE
**Syntax:** `AUTHORIZE [username] [/FULL] [/USER=username]`  
**Summary:** Displays OpenVMS User Authorization File (UAF) security records mapped to Windows accounts.  
* **Qualifiers:**  
  * `/FULL` — Displays extended password policy, security privileges, and UIC.  
  * `/USER=name` — Selects target Windows user account.  
* **Example:**  
  ```dcl
  $ AUTHORIZE /FULL
  $ AUTHORIZE Administrator
  ```

---

### BACKUP
**Syntax:** `BACKUP input-spec output-spec`  
**Summary:** Comprehensive backup, archiving, and verification utility.  
* **Qualifiers:**  
  * `/INCREMENTAL` — Copies only files with the archive attribute set.  
  * `/RECORD` — Clears archive bits after successful backup.  
  * `/COMPARE` or `/VERIFY` — Verifies destination files against sources.  
  * `/SAVE_SET` — Packages output directory as a backup archive.  
  * `/LOG` — Displays progress for every file backed up.  
  * `/CONFIRM` — Prompts before backing up each file.  
  * `/DRYRUN` — Previews backup operations without writing files.  
* **Example:**  
  ```dcl
  $ BACKUP C:\Source\*.* D:\Backup\ /INCREMENTAL /RECORD /LOG
  $ BACKUP C:\Source\*.* D:\Backup\ /COMPARE
  ```

---

### CHECKSUM
**Syntax:** `CHECKSUM file-spec [/ALGORITHM=SHA256|MD5] [/LOG]`  
**Summary:** Computes cryptographic hashes and stores the result in the `$CHECKSUM` symbol.  
* **Qualifiers:**  
  * `/ALGORITHM=type` — Selects cryptographic hash algorithm (default: `SHA256`).  
  * `/LOG` — Prints the hash output to console.  
* **Example:**  
  ```dcl
  $ CHECKSUM wcl.exe /ALGORITHM=SHA256 /LOG
  $ WRITE SYS$OUTPUT "Hash: " + $CHECKSUM
  ```

---

### CLOSE
**Syntax:** `CLOSE channel-name [/DISPOSITION=KEEP|DELETE]`  
**Summary:** Closes an open I/O channel.  
* **Qualifiers:**  
  * `/DISPOSITION=DELETE` — Deletes the underlying file upon closure.  
  * `/DISPOSITION=KEEP` — Preserves the file (default).  
* **Example:**  
  ```dcl
  $ CLOSE DATA_FILE
  $ CLOSE TEMP_STREAM /DISPOSITION=DELETE
  ```

---

### COMPRESS
**Syntax:** `COMPRESS file-spec [/ZIP=archive.zip] [/NTFS] [/LOG] [/CONFIRM] [/DRYRUN]`  
**Summary:** Compresses files into zip archives or applies transparent NTFS file-system compression.  
* **Qualifiers:**  
  * `/ZIP=archive.zip` — Creates or adds files to a specified ZIP archive.  
  * `/NTFS` — Applies native NTFS file compression on the target file.  
  * `/LOG` — Displays progress messages as each file is compressed.  
  * `/CONFIRM` — Prompts before compressing each target.  
  * `/DRYRUN` — Previews compression without altering files on disk.  
* **Example:**  
  ```dcl
  $ COMPRESS logs\*.log /ZIP=logs_archive.zip /LOG
  $ COMPRESS database.mdf /NTFS
  ```

---

### CONNECT
**Syntax:** `CONNECT host [/SSH] [/RDP] [/WINRM] [/PORT=port] [/USER=user] [/KEY=keyfile]`  
**Summary:** Initiates remote terminal or desktop connections.  
* **Qualifiers:**  
  * `/SSH` — Launches OpenSSH client connection (default).  
  * `/RDP` — Dispatches Remote Desktop Client (`mstsc.exe`).  
  * `/WINRM` — Connects via Windows Remote Shell (`winrs.exe`).  
  * `/PORT=p` — Custom TCP port number.  
  * `/USER=u` — Username for authentication.  
  * `/KEY=k` — Private key file path.  
  * `/DRYRUN` — Previews connection string without launching.  
* **Example:**  
  ```dcl
  $ CONNECT server01.corp /SSH /USER=admin /PORT=2222
  $ CONNECT workstation05 /RDP
  ```

---

### COPY
**Syntax:** `COPY source-spec destination-spec`  
**Summary:** Copies local files or downloads remote HTTP/HTTPS/FTP network resources directly to target directories.  
* **Qualifiers:**  
  * `/CONFIRM` — Prompts for approval before each file copy or download.  
  * `/LOG` — Displays progress messages and byte counts.  
  * `/DRYRUN` — Previews the copy or download action without modifying disks.  
* **Example:**  
  ```dcl
  $ COPY *.c backup\ /LOG
  $ COPY data.txt archive.txt /CONFIRM
  $ COPY https://raw.githubusercontent.com/curl/curl/master/README.md curl_readme.txt /LOG
  $ COPY https://example.com/dist/package.zip [.DOWNLOADS]
  ```

---

### CREATE
**Syntax:** `CREATE [file-spec | dir-spec]`  
**Summary:** Interactively creates a new text file from console input or creates a directory.  
* **Qualifiers:**  
  * `/DIRECTORY` — Creates a directory instead of a file.  
  * `/LOG` — Confirms creation.  
  * `/CONFIRM` — Prompts before directory creation.  
* **Example:**  
  ```dcl
  $ CREATE /DIRECTORY [.BUILD.OBJ]
  $ CREATE notes.txt
  (Type lines, end with Ctrl+Z)
  ```

---

### DELETE
**Syntax:** `DELETE file-spec`  
**Summary:** Deletes one or more files or process symbols.  
* **Qualifiers:**  
  * `/SYMBOL` — Deletes a symbol name instead of a file (`DELETE/SYMBOL name`).  
  * `/CONFIRM` — Prompts before deleting each file.  
  * `/LOG` — Reports each deleted file.  
  * `/DRYRUN` — Previews deletion without touching disk.  
* **Example:**  
  ```dcl
  $ DELETE *.tmp;* /LOG /CONFIRM
  $ DELETE /SYMBOL MY_VAR
  ```

---

### DECRYPT
**Syntax:** `DECRYPT input-file [output-file] [/KEY=key] [/DPAPI] [/LOG] [/CONFIRM] [/DRYRUN]`  
**Summary:** Decrypts files protected by WCL `ENCRYPT` using 256-bit AES or Windows DPAPI.  
* **Qualifiers:**  
  * `/KEY=passphrase` — Secret decryption passphrase used to derive the 256-bit AES key.  
  * `/OUTPUT=file` — Explicit output file destination (defaults to stripping `.enc`).  
  * `/DPAPI` — Uses Windows Data Protection API (machine/user identity).  
  * `/LOG` — Reports decrypted file metrics and byte sizes.  
  * `/CONFIRM` — Prompts before decrypting.  
  * `/DRYRUN` — Previews action without creating decrypted files.  
* **Example:**  
  ```dcl
  $ DECRYPT secret_backup.enc /KEY="CorpPass2026" /LOG
  $ DECRYPT tokens.enc /DPAPI
  ```

---

### DIFFERENCE
**Syntax:** `DIFFERENCE file1 file2 [/OUTPUT=out]`  
**Summary:** Compares two text files line by line and highlights differences.  
* **Qualifiers:**  
  * `/OUTPUT=file` — Redirects difference listing to a file.  
* **Example:**  
  ```dcl
  $ DIFFERENCE config.old config.new
  ```

---

### DIRECTORY
**Syntax:** `DIRECTORY [file-spec]`  
**Summary:** Displays directory listings with OpenVMS formatting and Windows metrics.  
* **Qualifiers:**  
  * `/FULL` — Comprehensive listing including version, sizes, dates, and attributes.  
  * `/SIZE` — Displays allocated file sizes in KB/MB.  
  * `/DATE` — Shows modification and creation timestamps.  
  * `/SECURITY` — Shows file ownership and Windows ACLs.  
  * `/OUTPUT=file` — Redirects output to a file.  
  * `/PAGE` — Paginates output screen by screen.  
* **Example:**  
  ```dcl
  $ DIRECTORY /SIZE /DATE
  $ DIRECTORY /FULL src\*.c
  ```

---

### DUMP
**Syntax:** `DUMP file-spec [/HEX] [/ASCII] [/RECORDS=count]`  
**Summary:** Displays file contents in formatted hex and ASCII representations.  
* **Qualifiers:**  
  * `/RECORDS=n` — Dumps only the first $n$ 512-byte blocks.  
* **Example:**  
  ```dcl
  $ DUMP kernel32.dll /RECORDS=2
  ```

---

### ERRORLOG
**Syntax:** `ERRORLOG [/SYSTEM] [/APPLICATION] [/SECURITY] [/COUNT=n]`  
**Summary:** Displays recent Windows Event Log error records.  
* **Qualifiers:**  
  * `/SYSTEM` — Reads Windows System log (default).  
  * `/APPLICATION` — Reads Application event log.  
  * `/COUNT=n` — Number of recent records to display (default: 10).  
* **Example:**  
  ```dcl
  $ ERRORLOG /SYSTEM /COUNT=5
  ```

---

### ENCRYPT
**Syntax:** `ENCRYPT input-file [output-file] [/KEY=key] [/DPAPI] [/LOG] [/CONFIRM] [/DRYRUN]`  
**Summary:** Encrypts sensitive files using 256-bit AES authenticated stream or Windows DPAPI.  
* **Qualifiers:**  
  * `/KEY=passphrase` — Secret encryption passphrase used to derive 256-bit AES key.  
  * `/OUTPUT=file` — Explicit output file destination (defaults to `input-file.enc`).  
  * `/DPAPI` — Uses Windows Data Protection API tied to Windows user credentials.  
  * `/LOG` — Reports encrypted file metrics and sizes.  
  * `/CONFIRM` — Prompts before encrypting.  
  * `/DRYRUN` — Previews action without creating encrypted files.  
* **Example:**  
  ```dcl
  $ ENCRYPT credentials.txt /KEY="SuperSecretKey!" /LOG
  $ ENCRYPT api_token.json /DPAPI
  ```

---

### EXPAND
**Syntax:** `EXPAND archive.zip [/DESTINATION=dir] [/NTFS] [/LOG] [/CONFIRM] [/DRYRUN]`  
**Summary:** Extracts ZIP archives into a target directory or decompresses NTFS files.  
* **Qualifiers:**  
  * `/DESTINATION=path` — Specifies output extraction folder (defaults to current directory).  
  * `/NTFS` — Removes NTFS compression from target file.  
  * `/LOG` — Reports each extracted file.  
  * `/CONFIRM` — Prompts before extracting files.  
  * `/DRYRUN` — Previews archive contents without writing files to disk.  
* **Example:**  
  ```dcl
  $ EXPAND release.zip /DESTINATION=C:\Deploy /LOG
  $ EXPAND database.mdf /NTFS
  ```

---

### HELP
**Syntax:** `HELP [topic]`  
**Summary:** Interactive DEC-style help subsystem.  
* **Example:**  
  ```dcl
  $ HELP
  $ HELP BACKUP
  $ HELP SHOW SYSTEM
  ```

---

### INQUIRE
**Syntax:** `INQUIRE symbol [prompt]`  
**Summary:** Prompts the user interactively and sets a process symbol to the response.  
* **Qualifiers:**  
  * `/NOPUNCTUATION` — Suppresses the default trailing colon `:`.  
* **Example:**  
  ```dcl
  $ INQUIRE TARGET_IP "Enter destination node"
  $ WRITE SYS$OUTPUT "Target selected: " + TARGET_IP
  ```

---

### IRIS
**Syntax:** `IRIS [file-spec] [/READONLY] [/OUTPUT=out]`  
**Summary:** Full-screen visual text editor clone of DEC EVE / TPU.  
* **Qualifiers:**  
  * `/READONLY` — Opens file in read-only view mode.  
  * `/OUTPUT=out` — Saves buffer under another filename.  
* **Key Bindings:**  
  * `Ctrl+B` — Command Prompt (`Command: `)  
  * `Ctrl+S` — Save  
  * `Ctrl+Z` — Save & Exit  
  * `Ctrl+Q` — Quit without saving  
  * `Ctrl+G` — Help  
* **Example:**  
  ```dcl
  $ IRIS main.c
  ```

---

### MONITOR
**Syntax:** `MONITOR [SYSTEM|CPU|MEMORY|DISK] [/INTERVAL=secs] [/COUNT=n]`  
**Summary:** Displays dynamic performance counters.  
* **Qualifiers:**  
  * `/INTERVAL=s` — Refresh interval in seconds (default: 2).  
  * `/COUNT=n` — Iteration count (default: 5).  
* **Example:**  
  ```dcl
  $ MONITOR CPU /INTERVAL=1 /COUNT=10
  ```

---

### MOUNT / DISMOUNT
**Syntax:** `MOUNT drive-letter /DEVICE=path`  
**Syntax:** `DISMOUNT drive-letter`  
**Summary:** Mounts or unmounts Windows drives and volume links.  
* **Example:**  
  ```dcl
  $ MOUNT X: /DEVICE=\\server\share
  $ DISMOUNT X:
  ```

---

### OPEN / READ / WRITE
**Syntax:** 
* `OPEN[/READ][/WRITE|/APPEND] channel file-spec`  
* `OPEN[/READ][/WRITE] /NETWORK channel host:port`  
* `READ channel destination-symbol`  
* `WRITE channel-or-SYS$OUTPUT data`  
* `CLOSE channel [/DISPOSITION=KEEP|DELETE]`  

**Summary:** Record and network stream I/O operations on file channels and TCP sockets.  
* **Qualifiers for OPEN:**  
  * `/READ` — Opens file or socket for reading (default).  
  * `/WRITE` — Creates or truncates file, or connects socket for writing.  
  * `/APPEND` — Opens file for appending records.  
  * `/NETWORK` or `/SOCKET` — Connects to a remote TCP socket stream (`host:port`).  
* **Example:**  
  ```dcl
  $ OPEN/READ IN_FILE data.txt
  $ READ IN_FILE LINE
  $ WRITE SYS$OUTPUT "Read: " + LINE
  $ CLOSE IN_FILE

  $ ! TCP Socket Stream Communication
  $ OPEN/WRITE /NETWORK HTTP_SOCK "api.example.com:80"
  $ WRITE HTTP_SOCK "GET / HTTP/1.1"
  $ WRITE HTTP_SOCK "Host: api.example.com"
  $ WRITE HTTP_SOCK ""
  $ READ HTTP_SOCK RESPONSE
  $ WRITE SYS$OUTPUT "Server Response: " + RESPONSE
  $ CLOSE HTTP_SOCK
  ```

---

### PIPE
**Syntax:** `PIPE command-1 | command-2 [ | command-3 ... ]`  
**Summary:** Executes pipelined DCL commands with standard I/O redirection.  
* **Operators:**  
  * `|` — Connects standard output of stage $N$ to standard input of stage $N+1$.  
  * `;` — Sequential command execution.  
  * `&&` — Conditional execution on success.  
  * `||` — Conditional execution on failure.  
* **Example:**  
  ```dcl
  $ PIPE SHOW SYSTEM | SEARCH SYS$INPUT wcl
  $ PIPE SHOW USERS | TYPE SYS$INPUT /TAIL=5
  ```

---

### PURGE
**Syntax:** `PURGE [file-spec] [/KEEP=n] [/LOG] [/CONFIRM] [/DRYRUN]`  
**Summary:** Deletes older versions of files, retaining only the $n$ highest versions.  
* **Qualifiers:**  
  * `/KEEP=n` — Number of latest versions to keep (default: 1).  
  * `/LOG` — Lists purged files.  
  * `/CONFIRM` — Prompts before deleting each file.  
  * `/DRYRUN` — Previews purged files without deletion.  
* **Example:**  
  ```dcl
  $ PURGE *.txt /KEEP=2 /LOG
  ```

---

### RECALL
**Syntax:** `RECALL [command-number | prefix-string | /ALL | /PAGE | /ERASE]`  
**Summary:** Recalls and displays or executes previous command lines from the persistent interactive history buffer.  
* **Qualifiers & Arguments:**  
  * `command-number` — Recalls and re-executes the command at the given index (e.g. `RECALL 1`).  
  * `prefix-string` — Recalls and executes the most recent command beginning with that string (e.g. `RECALL DIR`).  
  * `/ALL` — Displays all remembered command lines with their numerical indices.  
  * `/PAGE` — Displays remembered commands with terminal pagination.  
  * `/ERASE` — Clears the active history memory buffer.  
* **Example:**  
  ```dcl
  $ RECALL /ALL
  $ RECALL 4
  $ RECALL SHOW
  ```

---

### REGISTRY
**Syntax:** 
* `REGISTRY SHOW [key-path] [/VALUE=name] [/VALUES] [/PAGE]`
* `REGISTRY SET <key-path> /VALUE=name /DATA=data [/TYPE=SZ|DWORD|EXPAND_SZ] [/LOG] [/CONFIRM] [/DRYRUN]`
* `REGISTRY CREATE <key-path> [/LOG] [/DRYRUN]`
* `REGISTRY DELETE <key-path> [/VALUE=name] [/TREE] [/CONFIRM] [/DRYRUN] [/LOG]`
* `REGISTRY EXPORT <key-path> <file.reg> [/LOG] [/DRYRUN]`
* `REGISTRY IMPORT <file.reg> [/LOG] [/CONFIRM] [/DRYRUN]`

**Summary:** Views, creates, edits, deletes, exports, and imports Windows Registry keys and values with authentic OpenVMS DCL grammar and safety controls.

* **Root Hive Aliases:**
  * `HKLM` — `HKEY_LOCAL_MACHINE`
  * `HKCU` — `HKEY_CURRENT_USER`
  * `HKCR` — `HKEY_CLASSES_ROOT`
  * `HKU` — `HKEY_USERS`
  * `HKCC` — `HKEY_CURRENT_CONFIG`

* **Qualifiers:**
  * `/VALUE=name` — Specifies the value entry name to query, set, or delete.
  * `/VALUES` — Lists all value entries under the specified key with types and data.
  * `/DATA=string` — Value data to write (supports strings, decimal integers, or hex with `0x`).
  * `/TYPE=SZ|DWORD|EXPAND_SZ` — Registry value type (default: `SZ`).
  * `/TREE` — Recursively deletes a registry subkey hierarchy.
  * `/PAGE` — Paginates output when viewing keys or values.
  * `/LOG` — Displays status confirmation on modifications.
  * `/CONFIRM` — Prompts the operator before creating, setting, deleting, or importing.
  * `/DRYRUN` — Previews registry modifications without changing system state.

* **Example:**
  ```dcl
  $ REGISTRY SHOW "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion" /VALUE="ProductName"
  $ REGISTRY SHOW HKCU\Software\AppData /VALUES
  $ REGISTRY CREATE HKCU\Software\MyCompany\MyApp /LOG
  $ REGISTRY SET HKCU\Software\MyCompany\MyApp /VALUE="LogLevel" /DATA=2 /TYPE=DWORD /LOG
  $ REGISTRY DELETE HKCU\Software\MyCompany\MyApp /TREE /CONFIRM /LOG
  $ REGISTRY EXPORT HKCU\Software\MyCompany myapp.reg /LOG
  $ REGISTRY IMPORT myapp.reg /CONFIRM /LOG
  ```

---

### RENAME
**Syntax:** `RENAME source-spec destination-spec`  
**Summary:** Renames or moves files and directories.  
* **Qualifiers:**  
  * `/LOG` — Confirms rename operation.  
  * `/CONFIRM` — Prompts before rename.  
* **Example:**  
  ```dcl
  $ RENAME oldname.txt newname.txt /LOG
  ```

---

### SEARCH
**Syntax:** `SEARCH file-spec string`  
**Summary:** Searches files or `SYS$INPUT` for lines containing the specified text string.  
* **Qualifiers:**  
  * `/EXACT` — Case-sensitive matching.  
  * `/NUMBERS` — Displays line numbers.  
  * `/HEADING` — Displays file header banner.  
  * `/OUTPUT=file` — Redirects match results to a file.  
* **Example:**  
  ```dcl
  $ SEARCH *.c "CreateProcess" /NUMBERS
  $ PIPE DIRECTORY | SEARCH SYS$INPUT .EXE
  ```

---

### SET
**Syntax:** `SET option [arguments...]`  
**Summary:** Modifies session attributes and control parameters.  
* **Subcommands:**  
  * `SET DEFAULT path` — Changes working directory.  
  * `SET PROMPT "text"` — Changes command line prompt.  
  * `SET NOON` / `SET ON` — Controls abort-on-error behavior in scripts.  
  * `SET VERIFY` / `SET NOVERIFY` — Toggles script line echoing.  
  * `SET TIME /SYNC` — Synchronizes system clock via Windows Time service.  
  * `SET HOST hostname` — Alias for `CONNECT hostname`.  
  * `SET FILE file-spec [/PROTECTION=(S:...,O:...,G:...,W:...)] [/COMPRESS|/NOCOMPRESS] [/LOG]` — Sets file permissions, NTFS compression, or access control.  
  * `SET PROCESS [/PRIORITY=NORMAL|HIGH|IDLE|REALTIME] [/AFFINITY=mask] [/SUSPEND|/RESUME] [/IDENTIFICATION=pid | name]` — Controls process priority, core affinity, and scheduling state.  
  * `SET TERMINAL [/WIDTH=col] [/PAGE=lines] [/TITLE="text"] [/WRAP|/NOWRAP]` — Configures console buffer width, window title, page size, and autowrap.  
  * `SET PASSWORD [username] [/NEW="pwd"] [/GENERATE] [/DOMAIN=dom] [/LOG]` — Sets account password, prompts masked interactively, or generates cryptographically secure passwords.  
* **Example:**  
  ```dcl
  $ SET DEFAULT E:\Github\WCL
  $ SET PROMPT "VMS$ "
  $ SET TERMINAL /TITLE="Production Shell" /WRAP
  $ SET FILE data.txt /PROTECTION=(S:RWED,O:RWED,G:RE,W:RE) /LOG
  $ SET PROCESS /PRIORITY=HIGH /IDENTIFICATION=1420
  $ SET PASSWORD /GENERATE
  $ SET PASSWORD admin /NEW="P@ssw0rd2026!" /LOG
  $ SET TIME /SYNC
  ```

---

### SHOW
**Syntax:** `SHOW item [sub-options...]`  
**Summary:** Primary query command for system status and environment state.  

| Subcommand | Information Displayed |
| :--- | :--- |
| `SHOW AUDIT` | Windows Security event log availability and audit policies. |
| `SHOW CLUSTER` | Windows Domain and Active Directory membership. |
| `SHOW CPU` | Processor topology, cores, sockets, affinity, and load. |
| `SHOW DEFAULT` | Current working drive and directory path. |
| `SHOW DEVICES` | Mounted disk drives, file system types, and capacity. |
| `SHOW DUMP_OPTIONS` | Windows crash dump configuration and registry settings. |
| `SHOW ERRORLOG` | Recent critical event log entries. |
| `SHOW INTRUSION` | Authentic Intrusion Detection Database table mapped to Windows failed logons (Event ID 4625). |
| `SHOW LICENSE` | Authentic License Management Facility (LMF) table with Windows OS edition, PAK types, and allocated cores. |
| `SHOW LOGICAL` | Process and system logical name tables. |
| `SHOW MEMORY` | Physical RAM, paging file usage, commit charges, and cache. |
| `SHOW NETWORK` | Network adapter addresses, IPv4/IPv6 status, gateways, DNS. |
| `SHOW PRINTER` | Print Spooler queues, driver models, and port mappings. |
| `SHOW PROCESS` | Current shell process ID, handles, threads, and memory. |
| `SHOW PROTECTION` | Default ACL inheritance and current directory rights. |
| `SHOW QUEUE` | Windows services and print spooler batch queues. |
| `SHOW QUOTA` | User disk quotas and memory commit limits. |
| `SHOW REGISTRY` | Windows Registry key values, data types, and subkeys (alias for `REGISTRY SHOW`). |
| `SHOW ROOT` | Windows directory, System32, and WCL image locations. |
| `SHOW STATUS` | Completion status of the most recently executed command. |
| `SHOW SYMBOL` | Active process and global symbols. |
| `SHOW SYSTEM` | Full running process table with PIDs, image names, and memory. |
| `SHOW TERMINAL` | Console window buffer dimensions, colors, and VT mode. |
| `SHOW TIME` | Current system date and wall-clock time. |
| `SHOW USERS` | Logged-in interactive and remote terminal sessions. |
| `SHOW VERSION` | WCL and Windows kernel version strings. |
| `SHOW WORKING_SET`| WCL process working set size and page limits. |

* **Example:**  
  ```dcl
  $ SHOW SYSTEM
  $ SHOW MEMORY /FULL
  $ SHOW NETWORK /IP
  $ SHOW USERS
  ```

---

### SHUTDOWN
**Syntax:** `SHUTDOWN [/REBOOT] [/POWEROFF] [/TIMEOUT=seconds] [/REASON="text"] [/FORCE] [/CONFIRM] [/DRYRUN]`  
**Summary:** Initiates a controlled Windows operating system shutdown or reboot.  
* **Qualifiers:**  
  * `/REBOOT` — Restarts the computer after shutdown.  
  * `/POWEROFF` — Powers off system hardware.  
  * `/TIMEOUT=s` — Delay timer in seconds (default: 30).  
  * `/REASON="text"` — Message broadcast to users.  
  * `/FORCE` — Closes running applications without waiting.  
  * `/CONFIRM` — Prompts operator before initiating.  
  * `/DRYRUN` — Previews shutdown parameters safely.  
* **Example:**  
  ```dcl
  $ SHUTDOWN /REBOOT /TIMEOUT=60 /REASON="Routine Maintenance"
  ```

---

### SORT
**Syntax:** `SORT input-file output-file [/NODUPLICATES] [/LOG]`  
**Summary:** Sorts text records from an input file into an output file.  
* **Qualifiers:**  
  * `/NODUPLICATES` — Discards duplicate lines.  
  * `/LOG` — Reports record count sorted.  
* **Example:**  
  ```dcl
  $ SORT names.txt sorted_names.txt /NODUPLICATES /LOG
  ```

---

### SPAWN / RUN
**Syntax:** `SPAWN [command-line] [/NOWAIT] [/CONFIRM] [/DRYRUN]`  
**Syntax:** `RUN executable-path`  
**Summary:** Forks and launches a Win32 executable subprocess or asynchronous background job.  
* **Qualifiers for SPAWN:**  
  * `/NOWAIT` — Asynchronous execution; creates a background job entry, immediately returns the Entry ID, and proceeds without blocking.  
  * `/CONFIRM` — Prompts before launching.  
  * `/DRYRUN` — Previews subprocess command line.  
* **Example:**  
  ```dcl
  $ SPAWN notepad.exe notes.txt
  $ SPAWN/NOWAIT "powershell.exe -File long_task.ps1"
  $ SHOW QUEUE /BATCH
  $ SYNCHRONIZE 1
  $ RUN C:\Windows\System32\calc.exe
  ```

---

### START / STOP
**Syntax:** `START [service-name] [/SERVICE] [/QUEUE]`  
**Syntax:** `STOP process-name | /IDENTIFICATION=pid`  
**Summary:** Starts Windows background services or terminates running processes.  
* **Example:**  
  ```dcl
  $ START Spooler /SERVICE
  $ STOP /IDENTIFICATION=4210 /CONFIRM
  $ STOP notepad.exe
  ```

---

### SUBMIT
**Syntax:** `SUBMIT procedure-spec [/AFTER=time]`  
**Summary:** Schedules a command procedure to run in the background via Windows Task Scheduler.  
* **Qualifiers:**  
  * `/AFTER="time"` — Execution time (e.g., `"23:30"` or `"+01:00"`).  
  * `/LOG` — Logs job submission details.  
* **Example:**  
  ```dcl
  $ SUBMIT DAILY_BACKUP.COM /AFTER="02:00"
  ```

---

### TYPE
**Syntax:** `TYPE file-spec [/OUTPUT[=filespec] | /NOOUTPUT] [/TAIL[=count]] [/CONFIRM] [/PAGE[=mode]] [/NOPAGE]`  
**Summary:** Displays the contents of text files to standard output.  
* **Qualifiers:**  
  * `/PAGE[=CLEAR_SCREEN|SCROLL|SAVE[=n]]` — Displays output one screenful at a time. Defaults to `CLEAR_SCREEN`. `SAVE` retains up to $n$ pages (1–5) with backward navigation. `TYPE/PAGE NL:` clears the screen.  
  * `/NOPAGE` — Disables paged display.  
  * `/TAIL=n` — Displays only the trailing $n$ lines (1 to 10,000).  
  * `/OUTPUT=file` — Redirects text to output file (cannot be combined with `/PAGE`).  
  * `/NOOUTPUT` — Suppresses output display.  
  * `/CONFIRM` — Prompts before typing each file.  
* **Example:**  
  ```dcl
  $ TYPE server.log /TAIL=20
  $ TYPE/PAGE server.log
  $ TYPE/PAGE NL:                ! Clears the terminal screen (similar to CLS)
  $ TYPE SYS$INPUT
  ```

---

### WAIT
**Syntax:** `WAIT [hh:mm:ss | seconds]`  
**Summary:** Pauses execution for a specified duration.  
* **Example:**  
  ```dcl
  $ WAIT 00:00:05
  $ WAIT 10
  ```
