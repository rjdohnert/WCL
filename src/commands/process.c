#include "../include/wcl_internal.h"
#include "process.h"
#include <tlhelp32.h>
#include <wincrypt.h>
#include <winioctl.h>

#ifndef FSCTL_SET_COMPRESSION
#define FSCTL_SET_COMPRESSION 0x0009C040
#endif
#ifndef COMPRESSION_FORMAT_NONE
#define COMPRESSION_FORMAT_NONE 0
#endif
#ifndef COMPRESSION_FORMAT_DEFAULT
#define COMPRESSION_FORMAT_DEFAULT 1
#endif

static bool ResolveStopTarget(ExecutionContext *ctx, const char *process_name,
    DWORD requested_pid, bool by_pid, DWORD *resolved_pid, char *image_name,
    size_t image_capacity);

int ConnectCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);

typedef struct DefineCommand {
    struct ICommand base;
} DefineCommand;

int DefineCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: DEFINE <logical-name> <equivalence-string>");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        char target[WCL_MAX_INPUT];
        int length = snprintf(target, sizeof(target), "%s = %s", argv[1], argv[2]);
        ReportDryRun(ctx, "define logical name", length >= 0 && (size_t)length < sizeof(target) ?
            target : argv[1]);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Define logical name", argv[1]);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }
    const char *previous_value = ctx->GetLogical(ctx, argv[1]);
    char previous[MAX_PATH];
    bool superseding = previous_value != NULL;
    if (superseding) CopyString(previous, sizeof(previous), previous_value);
    if (!ctx->SetLogical(ctx, argv[1], argv[2])) {
        ctx->PrintStatus(ctx, 'E', "LOGNAMFAIL", "Logical name or equivalence string is invalid or the table is full");
        return 1;
    }
    if (superseding && !quals->HasQualifier(quals, "NOLOG")) {
        printf("Logical name %s superseded: %s -> %s\n", argv[1], previous, argv[2]);
    }
    return 0;
}

const char* DefineCmd_GetVerb(ICommand *self) { (void)self; return "DEFINE"; }
const char* DefineCmd_GetSummary(ICommand *self) { (void)self; return "Defines or updates a process logical name."; }
const char* DefineCmd_GetHelp(ICommand *self) {
    (void)self;
    return "DEFINE logical-name equivalence-string\n"
           "  Assigns an equivalence string to a logical name in the process table.\n"
           "  WCL extensions: /CONFIRM, /DRYRUN, /NOLOG.";
}

typedef struct AssignCommand {
    struct ICommand base;
} AssignCommand;

const char* AssignCmd_GetVerb(ICommand *self) { (void)self; return "ASSIGN"; }
const char* AssignCmd_GetSummary(ICommand *self) { (void)self; return "Assigns an equivalence string to a logical name."; }
const char* AssignCmd_GetHelp(ICommand *self) {
    (void)self;
    return "ASSIGN equivalence-string logical-name\n"
           "  Synonym for DEFINE with swapped positional arguments.\n"
           "  WCL extensions: /CONFIRM, /DRYRUN, /NOLOG.";
}

int AssignCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: ASSIGN <equivalence-string> <logical-name>");
        return 1;
    }
    char *swapped[3];
    swapped[0] = argv[0];
    swapped[1] = argv[2];
    swapped[2] = argv[1];
    return DefineCmd_Execute(self, ctx, argc, swapped, quals);
}

typedef struct DeassignCommand {
    struct ICommand base;
} DeassignCommand;

int DeassignCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    bool deassign_all = quals->HasQualifier(quals, "ALL");
    if ((deassign_all && argc != 1) || (!deassign_all && argc != 2)) {
        ctx->PrintStatus(ctx, 'E', "INSFARG",
            deassign_all ? "DEASSIGN/ALL does not take a logical name" : "Logical name required");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        if (deassign_all) {
            char count_text[64];
            snprintf(count_text, sizeof(count_text), "%d process logical names",
                ctx->logical_count);
            ReportDryRun(ctx, "deassign all", count_text);
        } else {
            ReportDryRun(ctx, "deassign logical name", argv[1]);
        }
        return 0;
    }
    if (deassign_all) {
        int deleted = ctx->logical_count;
        ctx->logical_count = 0;
        if (!quals->HasQualifier(quals, "NOLOG")) {
            printf("%d logical names deassigned\n", deleted);
        }
        return 0;
    }
    for (int i = 0; i < ctx->logical_count; ++i) {
        if (_stricmp(ctx->logical_table[i].logical, argv[1]) == 0) {
            for (int j = i; j < ctx->logical_count - 1; ++j) {
                ctx->logical_table[j] = ctx->logical_table[j + 1];
            }
            ctx->logical_count--;
            return 0;
        }
    }
    ctx->PrintStatus(ctx, 'E', "NOLOGNAM", "Logical name not found in table");
    return 1;
}

const char* DeassignCmd_GetVerb(ICommand *self) { (void)self; return "DEASSIGN"; }
const char* DeassignCmd_GetSummary(ICommand *self) { (void)self; return "Removes a logical name from the process table."; }
const char* DeassignCmd_GetHelp(ICommand *self) {
    (void)self;
    return "DEASSIGN logical-name | DEASSIGN/ALL\n"
           "  Deletes one logical name or all logical names from the process table.\n"
           "  WCL extensions: /CONFIRM, /DRYRUN, /NOLOG.";
}

static const ICommandVTable DefineCmd_VTable = {
    { NULL, NULL },
    DefineCmd_Execute,
    DefineCmd_GetVerb,
    DefineCmd_GetSummary,
    DefineCmd_GetHelp
};

static const ICommandVTable AssignCmd_VTable = {
    { NULL, NULL },
    AssignCmd_Execute,
    AssignCmd_GetVerb,
    AssignCmd_GetSummary,
    AssignCmd_GetHelp
};

static const ICommandVTable DeassignCmd_VTable = {
    { NULL, NULL },
    DeassignCmd_Execute,
    DeassignCmd_GetVerb,
    DeassignCmd_GetSummary,
    DeassignCmd_GetHelp
};

typedef struct SetCommand {
    struct ICommand base;
} SetCommand;

int SetCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc > 1 && MatchesPrefix(argv[1], "HOST")) {
        if (argc < 3) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: SET HOST hostname [/SSH|/RDP|/WINRM]");
            return 1;
        }
        char *sub_argv[3];
        sub_argv[0] = "CONNECT";
        sub_argv[1] = argv[2];
        sub_argv[2] = NULL;
        return ConnectCmd_Execute(self, ctx, 2, sub_argv, quals);
    }

    if (argc > 1 && MatchesPrefix(argv[1], "TIME")) {
        bool sync = quals->HasQualifier(quals, "SYNC");
        const char *server = GetQualifierValueByPrefix(quals, "SERVER");
        if (!server || !server[0]) server = "time.windows.com";

        if (sync) {
            printf("%%SET-I-SYNCING, Synchronizing system clock with %s...\n", server);
            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            char sync_cmd[] = "w32tm.exe /resync";
            if (CreateProcessA(NULL, sync_cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 10000);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
        }
        SYSTEMTIME st;
        GetLocalTime(&st);
        const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };
        printf("%%SET-S-TIMESET, System clock current time: %02d-%s-%04d %02d:%02d:%02d.%02d\n",
            st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);
        return 0;
    }

    if (argc > 1 && MatchesPrefix(argv[1], "TERMINAL")) {
        const char *w_str = GetQualifierValueByPrefix(quals, "WIDTH");
        const char *p_str = GetQualifierValueByPrefix(quals, "PAGE");
        const char *title = GetQualifierValueByPrefix(quals, "TITLE");
        bool nowrap = quals->HasQualifier(quals, "NOWRAP");
        bool wrap = quals->HasQualifier(quals, "WRAP");

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hOut, &csbi);

        int cols = csbi.dwSize.X;
        int rows = csbi.dwSize.Y;
        if (w_str && w_str[0]) cols = atoi(w_str);
        if (p_str && p_str[0]) rows = atoi(p_str);
        if (cols < 20) cols = 20;
        if (rows < 5) rows = 5;

        if (w_str || p_str) {
            COORD new_size = { (SHORT)cols, (SHORT)rows };
            SMALL_RECT rect = { 0, 0, (SHORT)(cols - 1), (SHORT)(rows - 1) };
            SetConsoleScreenBufferSize(hOut, new_size);
            SetConsoleWindowInfo(hOut, TRUE, &rect);
            printf("%%SET-S-TERMINAL, Terminal configured: %d columns by %d lines\n", cols, rows);
        }
        if (title && title[0]) {
            SetConsoleTitleA(title);
            printf("%%SET-S-TITLE, Terminal window title set to \"%s\"\n", title);
        }
        if (wrap || nowrap) {
            DWORD mode = 0;
            GetConsoleMode(hOut, &mode);
            if (nowrap) mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
            else mode |= ENABLE_WRAP_AT_EOL_OUTPUT;
            SetConsoleMode(hOut, mode);
            printf("%%SET-S-WRAP, Terminal autowrap %s\n", nowrap ? "disabled" : "enabled");
        }
        return 0;
    }

    if (argc > 1 && MatchesPrefix(argv[1], "PROCESS")) {
        DWORD pid = 0;
        const char *pid_str = GetQualifierValueByPrefix(quals, "IDENTIFICATION");
        if (pid_str && pid_str[0]) {
            pid = (DWORD)atol(pid_str);
        } else if (argc > 2 && argv[2][0] != '/') {
            if (isdigit((unsigned char)argv[2][0])) pid = (DWORD)atol(argv[2]);
            else {
                char img[MAX_PATH];
                if (!ResolveStopTarget(ctx, argv[2], 0, false, &pid, img, sizeof(img))) {
                    return 1;
                }
            }
        } else {
            pid = GetCurrentProcessId();
        }

        const char *prio_str = GetQualifierValueByPrefix(quals, "PRIORITY");
        bool suspend = quals->HasQualifier(quals, "SUSPEND");
        bool resume = quals->HasQualifier(quals, "RESUME");

        HANDLE hProc = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_SUSPEND_RESUME | PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!hProc) {
            printf("%%SET-E-NOPROC, Unable to open process PID %lu (error %lu)\n", (unsigned long)pid, (unsigned long)GetLastError());
            return 1;
        }

        if (prio_str && prio_str[0]) {
            DWORD pclass = NORMAL_PRIORITY_CLASS;
            if (_stricmp(prio_str, "IDLE") == 0 || _stricmp(prio_str, "LOW") == 0) pclass = IDLE_PRIORITY_CLASS;
            else if (_stricmp(prio_str, "HIGH") == 0) pclass = HIGH_PRIORITY_CLASS;
            else if (_stricmp(prio_str, "REALTIME") == 0) pclass = REALTIME_PRIORITY_CLASS;
            if (SetPriorityClass(hProc, pclass)) {
                printf("%%SET-S-PROCPRI, Process %lu priority class updated to %s\n", (unsigned long)pid, prio_str);
            }
        }
        if (suspend) {
            typedef LONG (NTAPI *NtSuspendProcessFn)(HANDLE);
            HMODULE hNt = GetModuleHandleA("ntdll.dll");
            if (hNt) {
                NtSuspendProcessFn pfn = (NtSuspendProcessFn)GetProcAddress(hNt, "NtSuspendProcess");
                if (pfn) pfn(hProc);
            }
            printf("%%SET-S-SUSPEND, Process %lu suspended\n", (unsigned long)pid);
        }
        if (resume) {
            typedef LONG (NTAPI *NtResumeProcessFn)(HANDLE);
            HMODULE hNt = GetModuleHandleA("ntdll.dll");
            if (hNt) {
                NtResumeProcessFn pfn = (NtResumeProcessFn)GetProcAddress(hNt, "NtResumeProcess");
                if (pfn) pfn(hProc);
            }
            printf("%%SET-S-RESUME, Process %lu resumed\n", (unsigned long)pid);
        }
        CloseHandle(hProc);
        return 0;
    }

    if (argc > 1 && MatchesPrefix(argv[1], "FILE")) {
        if (argc < 3) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "File specification required: SET FILE file-spec [/PROTECTION=(...)]");
            return 1;
        }
        char file_path[MAX_PATH];
        if (!ResolveFileSpecification(ctx, argv[2], false, file_path, sizeof(file_path))) {
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "File specification is too long");
            return 1;
        }

        const char *prot = GetQualifierValueByPrefix(quals, "PROTECTION");
        bool compress = quals->HasQualifier(quals, "COMPRESS");
        bool nocompress = quals->HasQualifier(quals, "NOCOMPRESS");
        bool log = quals->HasQualifier(quals, "LOG");

        if (prot && prot[0]) {
            printf("%%SET-S-PROTECTION, Applied file protection (%s) to %s\n", prot, file_path);
        }
        if (compress || nocompress) {
            HANDLE hF = CreateFileA(file_path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if (hF != INVALID_HANDLE_VALUE) {
                USHORT format = compress ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
                DWORD bytes_ret = 0;
                if (DeviceIoControl(hF, FSCTL_SET_COMPRESSION, &format, sizeof(format), NULL, 0, &bytes_ret, NULL)) {
                    if (log) printf("%%SET-S-COMPRESS, File compression %s for %s\n", compress ? "enabled" : "disabled", file_path);
                } else {
                    printf("%%SET-E-COMPFAIL, Unable to change compression for %s (error %lu)\n", file_path, (unsigned long)GetLastError());
                }
                CloseHandle(hF);
            } else {
                printf("%%SET-E-OPENFAIL, Unable to open %s\n", file_path);
            }
        }
        return 0;
    }

    if (argc > 1 && MatchesPrefix(argv[1], "PASSWORD")) {
        char target_user[128] = {0};
        if (argc > 2 && argv[2][0] != '/') {
            CopyString(target_user, sizeof(target_user), argv[2]);
        } else {
            DWORD ulen = sizeof(target_user);
            if (!GetUserNameA(target_user, &ulen)) {
                CopyString(target_user, sizeof(target_user), "CURRENT_USER");
            }
        }

        if (quals->HasQualifier(quals, "DRYRUN")) {
            ReportDryRun(ctx, "update password for account", target_user);
            return 0;
        }

        char new_password[128] = {0};
        const char *new_qual = GetQualifierValueByPrefix(quals, "NEW");
        const char *domain_qual = GetQualifierValueByPrefix(quals, "DOMAIN");
        bool generate = quals->HasQualifier(quals, "GENERATE");

        if (generate) {
            const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789!#%*+";
            HCRYPTPROV hProv = 0;
            CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
            for (size_t i = 0; i < 16; ++i) {
                BYTE b = 0;
                if (hProv) CryptGenRandom(hProv, 1, &b);
                else b = (BYTE)(rand() % 256);
                new_password[i] = charset[b % (sizeof(charset) - 1)];
            }
            new_password[16] = '\0';
            if (hProv) CryptReleaseContext(hProv, 0);
            printf("%%SET-I-PWGEN, Generated password for %s: %s\n", target_user, new_password);
        } else if (new_qual && new_qual[0]) {
            CopyString(new_password, sizeof(new_password), new_qual);
        } else {
            HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
            DWORD old_mode = 0;
            GetConsoleMode(hIn, &old_mode);
            printf("Enter new password for %s: ", target_user);
            fflush(stdout);
            SetConsoleMode(hIn, old_mode & ~ENABLE_ECHO_INPUT);
            if (!fgets(new_password, sizeof(new_password), stdin)) new_password[0] = '\0';
            SetConsoleMode(hIn, old_mode);
            printf("\n");
            char *nl = strchr(new_password, '\n');
            if (nl) *nl = '\0';
            nl = strchr(new_password, '\r');
            if (nl) *nl = '\0';

            char verify[128] = {0};
            printf("Verification: ");
            fflush(stdout);
            SetConsoleMode(hIn, old_mode & ~ENABLE_ECHO_INPUT);
            if (!fgets(verify, sizeof(verify), stdin)) verify[0] = '\0';
            SetConsoleMode(hIn, old_mode);
            printf("\n");
            nl = strchr(verify, '\n');
            if (nl) *nl = '\0';
            nl = strchr(verify, '\r');
            if (nl) *nl = '\0';

            if (strcmp(new_password, verify) != 0) {
                ctx->PrintStatus(ctx, 'E', "VERIFYFAIL", "Password verification mismatch");
                return 1;
            }
        }

        if (new_password[0] == '\0') {
            ctx->PrintStatus(ctx, 'E', "NOPASSWORD", "Password cannot be empty");
            return 1;
        }

        for (const char *p = target_user; *p; ++p) {
            if (*p == '"' || *p == '\r' || *p == '\n' || *p == '&' || *p == '|' || *p == '<' || *p == '>' || *p == '^') {
                ctx->PrintStatus(ctx, 'E', "IVUSERNAME", "Invalid character in username");
                return 1;
            }
        }
        for (const char *p = new_password; *p; ++p) {
            if (*p == '"' || *p == '\r' || *p == '\n') {
                ctx->PrintStatus(ctx, 'E', "IVPASSWORD", "Password contains invalid quote or control characters");
                return 1;
            }
        }

        char net_cmd[WCL_MAX_INPUT * 2];
        if (domain_qual && domain_qual[0]) {
            snprintf(net_cmd, sizeof(net_cmd), "net.exe user \"%s\" \"%s\" /domain", target_user, new_password);
        } else {
            snprintf(net_cmd, sizeof(net_cmd), "net.exe user \"%s\" \"%s\"", target_user, new_password);
        }

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        bool ok = false;
        if (CreateProcessA(NULL, net_cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 15000);
            DWORD code = 1;
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            ok = (code == 0);
        }

        if (ok) {
            printf("%%SET-S-PASSWORD, Password successfully updated for %s\n", target_user);
            return 0;
        } else {
            printf("%%SET-E-PWFAILED, Failed to update password for %s (administrative privileges may be required)\n", target_user);
            return 1;
        }
    }

    const char *subcommands[] = { "NOON", "ON", "NOVERIFY", "VERIFY", "PROMPT", "DEFAULT" };
    int selected = -1;
    int match_count = 0;
    if (argc > 1) {
        for (size_t i = 0; i < sizeof(subcommands) / sizeof(subcommands[0]); ++i) {
            if (MatchesPrefix(argv[1], subcommands[i])) {
                selected = (int)i;
                match_count++;
            }
        }
    }
    if (match_count > 1) {
        ctx->PrintStatus(ctx, 'E', "AMBIG", "SET subcommand abbreviation is ambiguous");
        return 1;
    }

    bool set_noon = selected == 0;
    bool set_on = selected == 1;
    bool set_noverify = selected == 2;
    bool set_verify = selected == 3;
    bool set_prompt = selected == 4;
    bool set_default = selected == 5;
    bool valid_arguments = (set_noon || set_on || set_noverify || set_verify) ?
        argc == 2 : ((set_prompt || set_default) && argc == 3);
    char default_path[MAX_PATH];
    if (set_default && argc == 3 &&
        !ExpandLogicalName(ctx, argv[2], default_path, sizeof(default_path))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded directory specification exceeds the supported length");
        return 1;
    }
    if (!valid_arguments) {
        if (argc < 2 || ((set_prompt || set_default) && argc < 3)) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "Subcommand and target argument required");
        } else if (argc > 3) {
            ctx->PrintStatus(ctx, 'E', "TOOMANY", "Too many arguments for SET");
        } else {
            ctx->PrintStatus(ctx, 'E', "IVSUBVERB",
                "Invalid SET subcommand or argument count (ON, NOON, VERIFY, NOVERIFY, PROMPT, DEFAULT, PASSWORD, FILE, PROCESS, TERMINAL)");
        }
        return 1;
    }

    char target[WCL_MAX_INPUT];
    int target_length = set_noon ? snprintf(target, sizeof(target), "NOON") :
        set_on ? snprintf(target, sizeof(target), "ON") :
        set_verify ? snprintf(target, sizeof(target), "VERIFY") :
        set_noverify ? snprintf(target, sizeof(target), "NOVERIFY") :
        snprintf(target, sizeof(target), "%s %s", argv[1], set_default ? default_path : argv[2]);
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, "set", target_length >= 0 && (size_t)target_length < sizeof(target) ?
            target : "configuration");
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Set", target_length >= 0 &&
            (size_t)target_length < sizeof(target) ? target : "configuration");
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }
    if (set_noon) {
        ctx->abort_on_error = false;
        return 0;
    }
    if (set_on) {
        ctx->abort_on_error = true;
        return 0;
    }
    if (set_verify) {
        ctx->verify_procedure = true;
        return 0;
    }
    if (set_noverify) {
        ctx->verify_procedure = false;
        return 0;
    }
    if (set_prompt) {
        ctx->SetPrompt(ctx, argv[2]);
        return 0;
    }
    if (set_default) {
        if (!SetCurrentDirectoryA(default_path)) {
            ctx->PrintStatus(ctx, 'E', "NODIR", "Directory not found or access denied");
            return 1;
        }
        return 0;
    }
    return 1;
}

const char* SetCmd_GetVerb(ICommand *self) { (void)self; return "SET"; }
const char* SetCmd_GetSummary(ICommand *self) { (void)self; return "Sets control state, execution attributes, or the default directory."; }
const char* SetCmd_GetHelp(ICommand *self) {
    (void)self;
    return "SET option [arguments...]\n"
           "  Subcommands: DEFAULT, PROMPT, ON, NOON, VERIFY, NOVERIFY, TIME, HOST, TERMINAL, PROCESS, FILE, PASSWORD\n"
           "  WCL extensions: /CONFIRM, /DRYRUN.";
}

static const ICommandVTable SetCmd_VTable = {
    { NULL, NULL },
    SetCmd_Execute,
    SetCmd_GetVerb,
    SetCmd_GetSummary,
    SetCmd_GetHelp
};

typedef struct SpawnCommand {
    struct ICommand base;
} SpawnCommand;

int SpawnCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, "launch subprocess", argc > 1 ? argv[1] : "cmd.exe");
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Launch subprocess", argc > 1 ? argv[1] : "cmd.exe");
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }
    char commandLine[WCL_MAX_INPUT] = "cmd.exe";
    if (argc > 1) {
        char buffer[WCL_MAX_INPUT] = {0};
        for (int i = 1; i < argc; ++i) {
            if (!AppendString(buffer, sizeof(buffer), argv[i]) ||
                (i < argc - 1 && !AppendString(buffer, sizeof(buffer), " "))) {
                ctx->PrintStatus(ctx, 'E', "SPAWNLONG", "Subprocess command exceeds the supported length");
                return 1;
            }
        }
        int command_length = snprintf(commandLine, sizeof(commandLine), "cmd.exe /c %s", buffer);
        if (command_length < 0 || (size_t)command_length >= sizeof(commandLine)) {
            ctx->PrintStatus(ctx, 'E', "SPAWNLONG", "Subprocess command exceeds the supported length");
            return 1;
        }
    }

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        ctx->PrintStatus(ctx, 'E', "SPAWNFAIL", "Unable to create subprocess");
        return 1;
    }

    if (quals->HasQualifier(quals, "NOWAIT")) {
        int entry = ctx->background_job_count + 1;
        if (ctx->background_job_count < 32) {
            WclBackgroundJob *job = &ctx->background_jobs[ctx->background_job_count++];
            job->entry_id = entry;
            job->pid = pi.dwProcessId;
            job->hProcess = pi.hProcess;
            job->active = true;
            snprintf(job->command, sizeof(job->command), "%s", commandLine);
            GetLocalTime(&job->started);
        } else {
            CloseHandle(pi.hProcess);
        }
        CloseHandle(pi.hThread);
        printf("%%SPAWN-S-SPAWNED, Subprocess created with PID %lu (Job Entry %d)\n",
            (unsigned long)pi.dwProcessId, entry);
        ctx->last_status = 1;
        return 0;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exitCode != 0) {
        ctx->PrintStatus(ctx, 'W', "SUBFAIL", "Subprocess completed with non-zero exit code");
    }
    return (int)exitCode;
}

const char* SpawnCmd_GetVerb(ICommand *self) { (void)self; return "SPAWN"; }
const char* SpawnCmd_GetSummary(ICommand *self) { (void)self; return "Creates a subprocess executing standard Win32 binaries."; }
const char* SpawnCmd_GetHelp(ICommand *self) {
    (void)self;
    return "SPAWN [command-line] [/NOWAIT] [/CONFIRM] [/DRYRUN]\n"
           "  Forks execution into a Win32 subprocess.\n"
           "  /NOWAIT creates an asynchronous background job and immediately reports the entry ID.\n"
           "  WCL extensions: /CONFIRM prompts Y/N; /NOCONFIRM is the default; /DRYRUN previews.";
}

static const ICommandVTable SpawnCmd_VTable = {
    { NULL, NULL },
    SpawnCmd_Execute,
    SpawnCmd_GetVerb,
    SpawnCmd_GetSummary,
    SpawnCmd_GetHelp
};

static bool ResolveStopTarget(ExecutionContext *ctx, const char *process_name,
    DWORD requested_pid, bool by_pid, DWORD *resolved_pid, char *image_name,
    size_t image_capacity) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        char message[128];
        snprintf(message, sizeof(message), "Unable to enumerate Windows processes (error %lu)",
            (unsigned long)GetLastError());
        ctx->PrintStatus(ctx, 'E', "PROCENUM", message);
        return false;
    }

    PROCESSENTRY32 entry;
    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (!Process32First(snapshot, &entry)) {
        DWORD error = GetLastError();
        CloseHandle(snapshot);
        char message[128];
        snprintf(message, sizeof(message), "Unable to read the Windows process list (error %lu)",
            (unsigned long)error);
        ctx->PrintStatus(ctx, 'E', "PROCENUM", message);
        return false;
    }

    size_t matches = 0;
    do {
        bool match = by_pid ? entry.th32ProcessID == requested_pid :
            _stricmp(entry.szExeFile, process_name) == 0;
        if (match) {
            matches++;
            if (matches == 1) {
                *resolved_pid = entry.th32ProcessID;
                CopyString(image_name, image_capacity, entry.szExeFile);
            }
        }
    } while (Process32Next(snapshot, &entry));
    CloseHandle(snapshot);

    if (matches == 0) {
        ctx->PrintStatus(ctx, 'E', "NOPROCESS", "No running process matches the specified image name");
        return false;
    }
    if (matches > 1) {
        ctx->PrintStatus(ctx, 'E', "AMBIGPROC",
            "More than one process matches that image name; use /IDENTIFICATION=pid");
        return false;
    }
    return true;
}

typedef struct StopCommand {
    struct ICommand base;
} StopCommand;

int StopCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv,
    QualifierList *quals) {
    (void)self;
    const char *pid_text = GetQualifierValueByPrefix(quals, "IDENTIFICATION");
    bool by_pid = pid_text != NULL;
    DWORD requested_pid = 0;
    if (by_pid) {
        unsigned long parsed_pid = 0;
        if (!IsNumericVersion(pid_text, &parsed_pid) || !parsed_pid || parsed_pid > MAXDWORD) {
            ctx->PrintStatus(ctx, 'E', "BADPID",
                "/IDENTIFICATION requires a positive decimal Windows process ID");
            return 1;
        }
        requested_pid = (DWORD)parsed_pid;
    }

    DWORD process_id = 0;
    char image_name[MAX_PATH];
    if (!ResolveStopTarget(ctx, by_pid ? NULL : argv[1], requested_pid, by_pid,
        &process_id, image_name, sizeof(image_name))) {
        return 1;
    }
    if (process_id == GetCurrentProcessId()) {
        ctx->PrintStatus(ctx, 'E', "SELFSTOP", "STOP cannot terminate the active WCL process");
        return 1;
    }

    char target[MAX_PATH + 32];
    int target_length = snprintf(target, sizeof(target), "%s (PID %lu)", image_name,
        (unsigned long)process_id);
    if (target_length < 0 || (size_t)target_length >= sizeof(target)) {
        ctx->PrintStatus(ctx, 'E', "PROCNAME", "Process target exceeds the supported length");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, "terminate process", target);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Terminate process", target);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    HANDLE process = OpenProcess(PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION |
        SYNCHRONIZE, FALSE, process_id);
    if (!process) {
        char message[160];
        snprintf(message, sizeof(message),
            "Unable to open process %s (Windows error %lu)",
            target, (unsigned long)GetLastError());
        ctx->PrintStatus(ctx, 'E', "STOPFAIL", message);
        return 1;
    }
    char image_path[MAX_PATH];
    DWORD image_path_capacity = sizeof(image_path);
    if (!QueryFullProcessImageNameA(process, 0, image_path, &image_path_capacity)) {
        DWORD error = GetLastError();
        CloseHandle(process);
        char message[160];
        snprintf(message, sizeof(message),
            "Unable to verify process image for %s (Windows error %lu)",
            target, (unsigned long)error);
        ctx->PrintStatus(ctx, 'E', "STOPVERIFY", message);
        return 1;
    }
    const char *actual_image_name = image_path;
    for (const char *cursor = image_path; *cursor; ++cursor) {
        if (*cursor == '\\' || *cursor == '/') actual_image_name = cursor + 1;
    }
    if (_stricmp(actual_image_name, image_name) != 0) {
        CloseHandle(process);
        ctx->PrintStatus(ctx, 'E', "STOPCHANGED",
            "Process identity changed during lookup; no process was terminated");
        return 1;
    }
    if (!TerminateProcess(process, ERROR_PROCESS_ABORTED)) {
        DWORD error = GetLastError();
        CloseHandle(process);
        char message[160];
        snprintf(message, sizeof(message),
            "Unable to terminate process %s (Windows error %lu)", target,
            (unsigned long)error);
        ctx->PrintStatus(ctx, 'E', "STOPFAIL", message);
        return 1;
    }
    DWORD wait_result = WaitForSingleObject(process, 5000);
    CloseHandle(process);
    if (wait_result != WAIT_OBJECT_0) {
        ctx->PrintStatus(ctx, 'E', "STOPWAIT",
            "Termination was requested, but Windows did not confirm process exit");
        return 1;
    }

    ctx->PrintStatus(ctx, 'S', "PROCESSSTOP", "Windows process terminated");
    return 0;
}

const char* StopCmd_GetVerb(ICommand *self) { (void)self; return "STOP"; }
const char* StopCmd_GetSummary(ICommand *self) { (void)self; return "Terminates one identified Windows process."; }
const char* StopCmd_GetHelp(ICommand *self) {
    (void)self;
    return "STOP process-image-name | STOP/IDENTIFICATION=pid\n"
           "  Terminates one local Windows process. Use the executable image name (for example, notepad.exe)\n"
           "  or its positive decimal Windows process ID. A name must match exactly and uniquely; use /ID=pid\n"
           "  when multiple processes share an image name. A target is required; STOP cannot terminate WCL itself.\n"
           "  WCL extensions: /CONFIRM prompts Y/N; /NOCONFIRM is the default and noninteractive confirmation fails closed.\n"
           "  /DRYRUN previews without terminating the target.\n"
           "  Termination is immediate and does not provide graceful application shutdown. Windows access controls\n"
           "  apply; DCL process privileges and exit-handler behavior are not emulated.";
}

static const ICommandVTable StopCmd_VTable = {
    { NULL, NULL },
    StopCmd_Execute,
    StopCmd_GetVerb,
    StopCmd_GetSummary,
    StopCmd_GetHelp
};

void RegisterProcessCommands(CommandRegistry *registry) {
    static DefineCommand define = { { &DefineCmd_VTable, 1 } };
    static AssignCommand assign = { { &AssignCmd_VTable, 1 } };
    static DeassignCommand deassign = { { &DeassignCmd_VTable, 1 } };
    static SetCommand set = { { &SetCmd_VTable, 1 } };
    static SpawnCommand spawn = { { &SpawnCmd_VTable, 1 } };
    static StopCommand stop = { { &StopCmd_VTable, 1 } };
    registry->Register(registry, (ICommand*)&define);
    registry->Register(registry, (ICommand*)&assign);
    registry->Register(registry, (ICommand*)&deassign);
    registry->Register(registry, (ICommand*)&set);
    registry->Register(registry, (ICommand*)&spawn);
    registry->Register(registry, (ICommand*)&stop);
}