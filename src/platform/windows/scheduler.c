#include "../../include/wcl_internal.h"
#include "scheduler.h"

typedef struct RunCommand {
    struct ICommand base;
} RunCommand;

bool AppendRepeated(char *dest, size_t capacity, char value, size_t count) {
    size_t length = strlen(dest);
    if (length >= capacity || count >= capacity - length) return false;
    memset(dest + length, value, count);
    dest[length + count] = '\0';
    return true;
}

bool AppendWindowsArgument(char *command_line, size_t capacity, const char *argument) {
    bool needs_quotes = !argument[0];
    for (const char *p = argument; *p; ++p) {
        if (isspace((unsigned char)*p) || *p == '"') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) return AppendString(command_line, capacity, argument);

    if (!AppendString(command_line, capacity, "\"")) return false;
    size_t backslashes = 0;
    for (const char *p = argument; *p; ++p) {
        if (*p == '\\') {
            backslashes++;
            continue;
        }
        if (*p == '"') {
            if (!AppendRepeated(command_line, capacity, '\\', backslashes * 2 + 1) ||
                !AppendString(command_line, capacity, "\"")) return false;
        } else {
            if (!AppendRepeated(command_line, capacity, '\\', backslashes) ||
                !AppendRepeated(command_line, capacity, *p, 1)) return false;
        }
        backslashes = 0;
    }
    return AppendRepeated(command_line, capacity, '\\', backslashes * 2) &&
        AppendString(command_line, capacity, "\"");
}

bool GetTaskSchedulerPath(char *path, size_t capacity) {
    UINT length = GetSystemDirectoryA(path, (UINT)capacity);
    if (!length || length >= capacity) return false;
    return AppendString(path, capacity, "\\schtasks.exe");
}

bool RunTaskSchedulerCommand(const char *arguments, char *output, size_t output_capacity,
    DWORD *exit_code) {
    if (!output_capacity) return false;
    output[0] = '\0';

    char scheduler_path[MAX_PATH];
    if (!GetTaskSchedulerPath(scheduler_path, sizeof(scheduler_path))) return false;
    char command_line[WCL_MAX_INPUT];
    command_line[0] = '\0';
    if (!AppendWindowsArgument(command_line, sizeof(command_line), scheduler_path) ||
        !AppendString(command_line, sizeof(command_line), " ") ||
        !AppendString(command_line, sizeof(command_line), arguments)) return false;

    SECURITY_ATTRIBUTES security = { sizeof(security), NULL, TRUE };
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &security, 0) ||
        !SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0)) {
        if (read_pipe) CloseHandle(read_pipe);
        if (write_pipe) CloseHandle(write_pipe);
        return false;
    }
    HANDLE null_input = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (null_input == INVALID_HANDLE_VALUE) {
        CloseHandle(read_pipe);
        CloseHandle(write_pipe);
        return false;
    }

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = null_input;
    startup.hStdOutput = write_pipe;
    startup.hStdError = write_pipe;
    ZeroMemory(&process, sizeof(process));
    BOOL created = CreateProcessA(scheduler_path, command_line, NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, NULL, &startup, &process);
    CloseHandle(null_input);
    CloseHandle(write_pipe);
    if (!created) {
        CloseHandle(read_pipe);
        return false;
    }

    bool success = true;
    bool overflow = false;
    bool read_error = false;
    size_t written = 0;
    char buffer[2048];
    DWORD bytes_read = 0;
    for (;;) {
        if (!ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, NULL)) {
            if (GetLastError() != ERROR_BROKEN_PIPE) read_error = true;
            break;
        }
        if (!bytes_read) break;
        size_t available = output_capacity - written - 1;
        size_t copy_length = bytes_read < available ? bytes_read : available;
        if (copy_length) {
            memcpy(output + written, buffer, copy_length);
            written += copy_length;
            output[written] = '\0';
        }
        if (copy_length < bytes_read) overflow = true;
    }
    if (read_error) success = false;

    DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD child_exit_code = 1;
    BOOL have_exit_code = wait_result == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process.hProcess, &child_exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    CloseHandle(read_pipe);
    if (!success || overflow || !have_exit_code) return false;
    *exit_code = child_exit_code;
    return true;
}

bool IsWclTaskName(const char *name) {
    if (name[0] == '\\') name++;
    if (strlen(name) != 21 || _strnicmp(name, "WCL-", 4) != 0 || name[12] != '-') return false;
    for (size_t i = 4; i < 21; ++i) {
        if (i == 12) continue;
        if (!isxdigit((unsigned char)name[i])) return false;
    }
    return true;
}

bool ParseTaskCsvFirstField(const char *line, char *field, size_t capacity,
    const char **remaining) {
    if (!line[0] || !capacity) return false;
    size_t written = 0;
    const char *cursor = line;
    bool quoted = *cursor == '"';
    if (quoted) cursor++;
    while (*cursor) {
        if (quoted && *cursor == '"' && cursor[1] == '"') {
            if (written + 1 >= capacity) return false;
            field[written++] = '"';
            cursor += 2;
            continue;
        }
        if (quoted && *cursor == '"') {
            cursor++;
            break;
        }
        if (!quoted && *cursor == ',') break;
        if (written + 1 >= capacity) return false;
        field[written++] = *cursor++;
    }
    field[written] = '\0';
    if (quoted && cursor[-1] != '"') return false;
    if (*cursor == ',') cursor++;
    *remaining = cursor;
    return true;
}

int ShowWclTaskQueue(ExecutionContext *ctx) {
    char output[65536];
    DWORD child_exit_code = 1;
    if (!RunTaskSchedulerCommand("/Query /FO CSV /NH", output, sizeof(output), &child_exit_code) ||
        child_exit_code != 0) {
        ctx->PrintStatus(ctx, 'E', "QUEUEFAIL", "Unable to retrieve Task Scheduler jobs");
        return 1;
    }

    printf("\nWCL scheduled tasks (task name and Windows scheduler details):\n");
    int displayed = 0;
    char *line = output;
    while (*line) {
        char *end = strchr(line, '\n');
        if (end) *end = '\0';
        size_t length = strlen(line);
        if (length && line[length - 1] == '\r') line[--length] = '\0';
        if (length) {
            char task_name[MAX_PATH];
            const char *details;
            if (ParseTaskCsvFirstField(line, task_name, sizeof(task_name), &details) &&
                IsWclTaskName(task_name)) {
                const char *display_name = task_name[0] == '\\' ? task_name + 1 : task_name;
                printf("  %-24s %s\n", display_name, details);
                displayed++;
            }
        }
        if (!end) break;
        line = end + 1;
    }
    if (!displayed) printf("  No WCL-created scheduled tasks found.\n");
    printf("\n");
    return 0;
}

int DeleteWclScheduledTask(ExecutionContext *ctx, const char *task_name) {
    const char *canonical_name = task_name[0] == '\\' ? task_name + 1 : task_name;
    char arguments[WCL_MAX_INPUT] = "/Delete /TN ";
    if (!AppendWindowsArgument(arguments, sizeof(arguments), canonical_name) ||
        !AppendString(arguments, sizeof(arguments), " /F")) {
        ctx->PrintStatus(ctx, 'E', "TASKDELFAIL", "Task Scheduler delete command exceeds the supported length");
        return 1;
    }

    char output[4096];
    DWORD child_exit_code = 1;
    if (!RunTaskSchedulerCommand(arguments, output, sizeof(output), &child_exit_code) ||
        child_exit_code != 0) {
        ctx->PrintStatus(ctx, 'E', "TASKDELFAIL", "Unable to delete the specified WCL scheduled task");
        return 1;
    }
    ctx->PrintStatus(ctx, 'S', "TASKDELETED", "Scheduled task deleted");
    return 0;
}

int RunCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Executable file specification required");
        return 1;
    }
    char executable[MAX_PATH];
    if (!ResolveFileSpecification(ctx, argv[1], false, executable, sizeof(executable))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded executable specification exceeds the supported length");
        return 1;
    }

    char command_line[WCL_MAX_INPUT];
    command_line[0] = '\0';
    if (!AppendWindowsArgument(command_line, sizeof(command_line), executable)) {
        ctx->PrintStatus(ctx, 'E', "RUNLONG", "Executable command line exceeds the supported length");
        return 1;
    }
    for (int i = 2; i < argc; ++i) {
        if (!AppendString(command_line, sizeof(command_line), " ") ||
            !AppendWindowsArgument(command_line, sizeof(command_line), argv[i])) {
            ctx->PrintStatus(ctx, 'E', "RUNLONG", "Executable command line exceeds the supported length");
            return 1;
        }
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, "run executable", executable);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Run executable", executable);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));
    if (!CreateProcessA(executable, command_line, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process)) {
        ctx->PrintStatus(ctx, 'E', "RUNFAIL", "Unable to start the requested executable");
        return 1;
    }

    DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    BOOL got_exit_code = wait_result == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    if (!got_exit_code) {
        ctx->PrintStatus(ctx, 'E', "RUNFAIL", "Unable to retrieve executable completion status");
        return 1;
    }
    return (int)exit_code;
}

const char* RunCmd_GetVerb(ICommand *self) { (void)self; return "RUN"; }
const char* RunCmd_GetSummary(ICommand *self) { (void)self; return "Runs a Windows executable and waits for completion."; }
const char* RunCmd_GetHelp(ICommand *self) {
    (void)self;
    return "RUN executable [arguments...]\n"
           "  Starts a Windows executable directly, waits, and returns its exit status.\n"
           "  WCL extensions: /CONFIRM prompts Y/N; /NOCONFIRM is the default; /DRYRUN previews.\n"
           "  DCL process quotas, UIC, privilege, debug, and process-name qualifiers have no direct Windows mapping and are rejected.";
}

static const ICommandVTable RunCmd_VTable = {
    { NULL, NULL },
    RunCmd_Execute,
    RunCmd_GetVerb,
    RunCmd_GetSummary,
    RunCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: SUBMIT (one-time, current-user Task Scheduler job)
 * ---------------------------------------------------------------------------- */
typedef struct SubmitCommand {
    struct ICommand base;
} SubmitCommand;

bool AppendXmlText(char *xml, size_t capacity, const char *text) {
    for (const char *p = text; *p; ++p) {
        const char *escaped = NULL;
        switch (*p) {
        case '&': escaped = "&amp;"; break;
        case '<': escaped = "&lt;"; break;
        case '>': escaped = "&gt;"; break;
        case '"': escaped = "&quot;"; break;
        case '\'': escaped = "&apos;"; break;
        default:
            if (!AppendRepeated(xml, capacity, *p, 1)) return false;
            continue;
        }
        if (!AppendString(xml, capacity, escaped)) return false;
    }
    return true;
}

bool WriteTaskXml(const char *path, const char *xml) {
    int wide_count = MultiByteToWideChar(CP_ACP, 0, xml, -1, NULL, 0);
    if (!wide_count) return false;
    wchar_t *wide_xml = (wchar_t*)calloc((size_t)wide_count, sizeof(wchar_t));
    if (!wide_xml) return false;
    if (!MultiByteToWideChar(CP_ACP, 0, xml, -1, wide_xml, wide_count)) {
        free(wide_xml);
        return false;
    }

    const unsigned char utf16le_bom[] = { 0xFF, 0xFE };
    FILE *file = NULL;
    bool success = fopen_s(&file, path, "wb") == 0;
    if (success) {
        size_t text_size = ((size_t)wide_count - 1) * sizeof(wchar_t);
        success = fwrite(utf16le_bom, 1, sizeof(utf16le_bom), file) == sizeof(utf16le_bom) &&
            fwrite(wide_xml, 1, text_size, file) == text_size;
        if (fclose(file) != 0) success = false;
    }
    free(wide_xml);
    return success;
}

bool ParseSubmitTime(const char *text, char *boundary, size_t boundary_capacity) {
    if (strlen(text) != 16 || text[4] != '-' || text[7] != '-' ||
        text[10] != 'T' || text[13] != ':') return false;
    for (size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13) continue;
        if (!isdigit((unsigned char)text[i])) return false;
    }

    SYSTEMTIME local_time = {0};
    local_time.wYear = (WORD)((text[0] - '0') * 1000 + (text[1] - '0') * 100 +
        (text[2] - '0') * 10 + (text[3] - '0'));
    local_time.wMonth = (WORD)((text[5] - '0') * 10 + (text[6] - '0'));
    local_time.wDay = (WORD)((text[8] - '0') * 10 + (text[9] - '0'));
    local_time.wHour = (WORD)((text[11] - '0') * 10 + (text[12] - '0'));
    local_time.wMinute = (WORD)((text[14] - '0') * 10 + (text[15] - '0'));
    local_time.wSecond = 0;

    FILETIME validation_time;
    SYSTEMTIME utc_time;
    if (!SystemTimeToFileTime(&local_time, &validation_time) ||
        !TzSpecificLocalTimeToSystemTime(NULL, &local_time, &utc_time) ||
        !SystemTimeToFileTime(&utc_time, &validation_time)) return false;

    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER requested_value;
    ULARGE_INTEGER now_value;
    requested_value.LowPart = validation_time.dwLowDateTime;
    requested_value.HighPart = validation_time.dwHighDateTime;
    now_value.LowPart = now.dwLowDateTime;
    now_value.HighPart = now.dwHighDateTime;
    if (requested_value.QuadPart <= now_value.QuadPart) return false;

    int length = snprintf(boundary, boundary_capacity, "%.16s:00", text);
    return length >= 0 && (size_t)length < boundary_capacity;
}

int CreateScheduledTask(ExecutionContext *ctx, const char *procedure, int parameter_count,
    char **parameters, const char *scheduled_time) {
    if (parameter_count > 8) {
        ctx->PrintStatus(ctx, 'E', "TOOMANYARG", "SUBMIT supports at most eight procedure parameters");
        return 1;
    }
    if (HasFileWildcard(procedure)) {
        ctx->PrintStatus(ctx, 'E', "BADSPEC", "SUBMIT requires one exact procedure file");
        return 1;
    }

    char procedure_path[MAX_PATH];
    if (!ResolveFileSpecification(ctx, procedure, false, procedure_path, sizeof(procedure_path))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Procedure file specification exceeds the supported length");
        return 1;
    }
    char absolute_procedure[MAX_PATH];
    DWORD procedure_length = GetFullPathNameA(procedure_path, sizeof(absolute_procedure), absolute_procedure, NULL);
    if (!procedure_length || procedure_length >= sizeof(absolute_procedure)) {
        ctx->PrintStatus(ctx, 'E', "BADSPEC", "Unable to resolve the procedure file path");
        return 1;
    }
    DWORD procedure_attributes = GetFileAttributesA(absolute_procedure);
    if (procedure_attributes == INVALID_FILE_ATTRIBUTES ||
        (procedure_attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        ctx->PrintStatus(ctx, 'E', "PROCOPEN", "Command procedure file was not found");
        return 1;
    }

    char boundary[32];
    if (!ParseSubmitTime(scheduled_time, boundary, sizeof(boundary))) {
        ctx->PrintStatus(ctx, 'E', "BADTIME", "Use a valid future local time in YYYY-MM-DDTHH:MM format");
        return 1;
    }

    char executable[MAX_PATH];
    DWORD executable_length = GetModuleFileNameA(NULL, executable, sizeof(executable));
    if (!executable_length || executable_length >= sizeof(executable)) {
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Unable to resolve the running WCL executable path");
        return 1;
    }
    char working_directory[MAX_PATH];
    DWORD working_length = GetCurrentDirectoryA(sizeof(working_directory), working_directory);
    if (!working_length || working_length >= sizeof(working_directory)) {
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Unable to resolve the current working directory");
        return 1;
    }

    char task_arguments[WCL_MAX_INPUT];
    task_arguments[0] = '\0';
    char procedure_argument[MAX_PATH + 2];
    int procedure_arg_length = snprintf(procedure_argument, sizeof(procedure_argument), "@%s", absolute_procedure);
    if (procedure_arg_length < 0 || (size_t)procedure_arg_length >= sizeof(procedure_argument) ||
        !AppendWindowsArgument(task_arguments, sizeof(task_arguments), procedure_argument)) {
        ctx->PrintStatus(ctx, 'E', "SUBMITLONG", "Scheduled procedure arguments exceed the supported length");
        return 1;
    }
    for (int i = 0; i < parameter_count; ++i) {
        if (!AppendString(task_arguments, sizeof(task_arguments), " ") ||
            !AppendWindowsArgument(task_arguments, sizeof(task_arguments), parameters[i])) {
            ctx->PrintStatus(ctx, 'E', "SUBMITLONG", "Scheduled procedure arguments exceed the supported length");
            return 1;
        }
    }

    char username[256];
    char domain[256];
    DWORD username_length = GetEnvironmentVariableA("USERNAME", username, sizeof(username));
    DWORD domain_length = GetEnvironmentVariableA("USERDOMAIN", domain, sizeof(domain));
    if (!username_length || username_length >= sizeof(username)) {
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Unable to identify the current Windows user");
        return 1;
    }
    char user_id[600];
    int user_id_length = domain_length && domain_length < sizeof(domain) ?
        snprintf(user_id, sizeof(user_id), "%s\\%s", domain, username) :
        snprintf(user_id, sizeof(user_id), "%s", username);
    if (user_id_length < 0 || (size_t)user_id_length >= sizeof(user_id)) {
        ctx->PrintStatus(ctx, 'E', "SUBMITLONG", "Current Windows user name exceeds the supported length");
        return 1;
    }

    char task_name[64];
    int task_name_length = snprintf(task_name, sizeof(task_name), "WCL-%08lX-%08lX",
        (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount());
    if (task_name_length < 0 || (size_t)task_name_length >= sizeof(task_name)) {
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Unable to generate a scheduled task name");
        return 1;
    }

    char xml[WCL_MAX_INPUT * 8];
    xml[0] = '\0';
    bool xml_ok = AppendString(xml, sizeof(xml),
        "<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
        "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
        "<RegistrationInfo><Description>WCL command procedure submitted by the current user.</Description></RegistrationInfo>"
        "<Triggers><TimeTrigger><StartBoundary>") &&
        AppendString(xml, sizeof(xml), boundary) &&
        AppendString(xml, sizeof(xml),
        "</StartBoundary><Enabled>true</Enabled></TimeTrigger></Triggers>"
        "<Principals><Principal id=\"Author\"><UserId>") &&
        AppendXmlText(xml, sizeof(xml), user_id) &&
        AppendString(xml, sizeof(xml),
        "</UserId><LogonType>InteractiveToken</LogonType><RunLevel>LeastPrivilege</RunLevel></Principal></Principals>"
        "<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
        "<StartWhenAvailable>true</StartWhenAvailable><Enabled>true</Enabled><Hidden>false</Hidden>"
        "<ExecutionTimeLimit>PT72H</ExecutionTimeLimit><Priority>7</Priority></Settings>"
        "<Actions Context=\"Author\"><Exec><Command>") &&
        AppendXmlText(xml, sizeof(xml), executable) &&
        AppendString(xml, sizeof(xml), "</Command><Arguments>") &&
        AppendXmlText(xml, sizeof(xml), task_arguments) &&
        AppendString(xml, sizeof(xml), "</Arguments><WorkingDirectory>") &&
        AppendXmlText(xml, sizeof(xml), working_directory) &&
        AppendString(xml, sizeof(xml), "</WorkingDirectory></Exec></Actions></Task>");
    if (!xml_ok) {
        ctx->PrintStatus(ctx, 'E', "SUBMITLONG", "Task definition exceeds the supported size");
        return 1;
    }

    char temp_directory[MAX_PATH];
    DWORD temp_directory_length = GetTempPathA(sizeof(temp_directory), temp_directory);
    char xml_path[MAX_PATH];
    if (!temp_directory_length || temp_directory_length >= sizeof(temp_directory) ||
        !GetTempFileNameA(temp_directory, "wcl", 0, xml_path)) {
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Unable to create a temporary Task Scheduler definition");
        return 1;
    }
    if (!WriteTaskXml(xml_path, xml)) {
        DeleteFileA(xml_path);
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Unable to write the temporary Task Scheduler definition");
        return 1;
    }

    char system_directory[MAX_PATH];
    UINT system_length = GetSystemDirectoryA(system_directory, sizeof(system_directory));
    if (!system_length || system_length >= sizeof(system_directory) ||
        !AppendString(system_directory, sizeof(system_directory), "\\schtasks.exe")) {
        DeleteFileA(xml_path);
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Unable to locate the Windows Task Scheduler utility");
        return 1;
    }

    char task_command[WCL_MAX_INPUT];
    task_command[0] = '\0';
    bool command_ok = AppendWindowsArgument(task_command, sizeof(task_command), system_directory) &&
        AppendString(task_command, sizeof(task_command), " /Create /TN ") &&
        AppendWindowsArgument(task_command, sizeof(task_command), task_name) &&
        AppendString(task_command, sizeof(task_command), " /XML ") &&
        AppendWindowsArgument(task_command, sizeof(task_command), xml_path);
    if (!command_ok) {
        DeleteFileA(xml_path);
        ctx->PrintStatus(ctx, 'E', "SUBMITLONG", "Task Scheduler command exceeds the supported length");
        return 1;
    }

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));
    BOOL created = CreateProcessA(system_directory, task_command, NULL, NULL, FALSE, 0,
        NULL, NULL, &startup, &process);
    if (!created) {
        DeleteFileA(xml_path);
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Unable to start the Windows Task Scheduler utility");
        return 1;
    }

    DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    BOOL got_exit_code = wait_result == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    bool xml_removed = DeleteFileA(xml_path) != 0;
    if (!got_exit_code || exit_code != 0) {
        ctx->PrintStatus(ctx, 'E', "SUBMITFAIL", "Windows Task Scheduler rejected the task definition");
        if (!xml_removed) ctx->PrintStatus(ctx, 'W', "TEMPLEFT", "Temporary task definition could not be removed");
        return 1;
    }
    if (!xml_removed) {
        ctx->PrintStatus(ctx, 'W', "TEMPLEFT", "Temporary task definition could not be removed");
    }
    printf("Task %s scheduled for %s (current user; run only while logged on).\n", task_name, scheduled_time);
    return 0;
}

int SubmitCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "SUBMIT requires a procedure file specification");
        return 1;
    }
    const char *scheduled_time = NULL;
    for (int i = 0; i < quals->count; ++i) {
        if (MatchesPrefix(quals->items[i].name, "AT") ||
            MatchesPrefix(quals->items[i].name, "AFTER")) {
            scheduled_time = quals->items[i].value;
            break;
        }
    }
    if (!scheduled_time) {
        ctx->PrintStatus(ctx, 'E', "IVQUAL", "Specify exactly one /AFTER=YYYY-MM-DDTHH:MM time");
        return 1;
    }
    char boundary[32];
    if (!ParseSubmitTime(scheduled_time, boundary, sizeof(boundary))) {
        ctx->PrintStatus(ctx, 'E', "BADTIME", "Use a valid future local time in YYYY-MM-DDTHH:MM format");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, "schedule procedure", argv[1]);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Schedule procedure", argv[1]);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }
    return CreateScheduledTask(ctx, argv[1], argc - 2, argv + 2, scheduled_time);
}

const char* SubmitCmd_GetVerb(ICommand *self) { (void)self; return "SUBMIT"; }
const char* SubmitCmd_GetSummary(ICommand *self) { (void)self; return "Schedules a one-time command procedure with Windows Task Scheduler."; }
const char* SubmitCmd_GetHelp(ICommand *self) {
    (void)self;
    return "SUBMIT procedure [P1 ... P8] /AFTER=YYYY-MM-DDTHH:MM\n"
           "  Creates a one-time local Windows Task Scheduler task for the specified future local time.\n"
           "  It runs the procedure as the current user at the current working directory, with least privilege,\n"
           "  only while that user is logged on. No credentials or DCL batch log are stored; write procedure output\n"
           "  to files when it must be retained. Use SHOW QUEUE to list and DELETE/ENTRY to cancel WCL jobs.\n"
           "  /CONFIRM prompts Y/N before creating the task; WCL /DRYRUN previews without scheduling.\n"
           "  /AFTER is the DCL delayed-start qualifier; WCL also accepts /AT as a synonymous extension.\n"
           "  Only YYYY-MM-DDTHH:MM future local times map to this Windows schedule. Recurring and elevated jobs,\n"
           "  and the other OpenVMS batch queue controls, are unsupported.";
}

static const ICommandVTable SubmitCmd_VTable = {
    { NULL, NULL },
    SubmitCmd_Execute,
    SubmitCmd_GetVerb,
    SubmitCmd_GetSummary,
    SubmitCmd_GetHelp
};

void RegisterSchedulerCommands(CommandRegistry *registry) {
    static RunCommand run = { { &RunCmd_VTable, 1 } };
    static SubmitCommand submit = { { &SubmitCmd_VTable, 1 } };
    registry->Register(registry, (ICommand*)&run);
    registry->Register(registry, (ICommand*)&submit);
}

