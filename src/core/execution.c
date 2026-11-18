#include "../include/wcl_internal.h"

int LaunchExternalApplication(ExecutionContext *ctx, const char *command) {
    char command_line[WCL_MAX_INPUT];
    CopyString(command_line, sizeof(command_line), command);

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));
    if (!CreateProcessA(NULL, command_line, NULL, NULL, FALSE, 0, NULL, NULL,
        &startup, &process)) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ||
            error == ERROR_INVALID_NAME) {
            ctx->PrintStatus(ctx, 'E', "IVVERB", "Unknown command or application not found in the Windows search path");
        } else {
            char message[128];
            snprintf(message, sizeof(message), "Unable to launch application (Windows error %lu)",
                (unsigned long)error);
            ctx->PrintStatus(ctx, 'E', "APPFAIL", message);
        }
        return 1;
    }

    DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    BOOL have_exit_code = wait_result == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    if (!have_exit_code) {
        ctx->PrintStatus(ctx, 'E', "APPFAIL", "Unable to retrieve application completion status");
        return 1;
    }
    return (int)exit_code;
}

int ExecuteCommandLine(ExecutionContext *ctx, CommandRegistry *registry, const char *line, bool from_script) {
    char source[WCL_MAX_INPUT];
    char expanded[WCL_MAX_INPUT];
    CopyString(source, sizeof(source), line);
    char *command = TrimWhitespace(source);
    if (from_script && command[0] == '$') {
        command++;
        command = TrimWhitespace(command);
        if (command[0] == '!') return 0;
    }
    if (!command[0]) return 0;

    bool assignment = false;
    if (!TryAssignSymbol(ctx, command, &assignment)) {
        ctx->PrintStatus(ctx, 'E', "SYMBOLFAIL", "Invalid symbol assignment or undefined symbol");
        ctx->last_status = 1;
        return 1;
    }
    if (assignment) {
        ctx->last_status = 0;
        return 0;
    }
    if (!ExpandSymbols(ctx, command, expanded, sizeof(expanded))) {
        ctx->PrintStatus(ctx, 'E', "UNDEFSYM", "Undefined symbol or expanded command exceeds the supported length");
        ctx->last_status = 1;
        return 1;
    }

    char *expanded_command = TrimWhitespace(expanded);
    if (expanded_command[0] == '@') {
        int parameter_count = 0;
        char *parameters[8];
        char file_spec[MAX_PATH];
        if (!ParseProcedureInvocation(expanded_command + 1, file_spec, sizeof(file_spec),
            &parameter_count, parameters)) {
            ctx->PrintStatus(ctx, 'E', "ATFAIL", "Invalid command procedure invocation");
            for (int i = 0; i < parameter_count; ++i) free(parameters[i]);
            ctx->last_status = 1;
            return 1;
        }
        int status = ExecuteProcedure(ctx, registry, file_spec, parameter_count, parameters);
        for (int i = 0; i < parameter_count; ++i) free(parameters[i]);
        ctx->last_status = (DWORD)status;
        return status;
    }

    int argc = 0;
    char *argv[WCL_MAX_ARGS];
    QualifierList quals;
    QualifierList_Init(&quals);
    if (!ParseCommandLine(expanded_command, &argc, argv, &quals)) {
        ctx->PrintStatus(ctx, 'E', "SYNTAX", "Invalid or oversized command line");
        for (int i = 0; i < argc; ++i) free(argv[i]);
        ctx->last_status = 1;
        return 1;
    }
    if (argc == 0) return 0;

    bool ambiguous = false;
    ICommand *cmd = registry->Resolve(registry, argv[0], &ambiguous);
    int status;
    if (ambiguous) {
        ctx->PrintStatus(ctx, 'E', "AMBCMD", "Ambiguous command verb - supply more characters");
        status = 1;
    } else if (!cmd) {
        const char *alias = GetSymbol(ctx, argv[0]);
        if (alias && alias[0]) {
            const char *p = expanded_command;
            while (*p && isspace((unsigned char)*p)) p++;
            while (*p && !isspace((unsigned char)*p) && *p != '/') p++;
            while (*p && isspace((unsigned char)*p)) p++;

            char alias_line[WCL_MAX_INPUT];
            if (*p) {
                snprintf(alias_line, sizeof(alias_line), "%s %s", alias, p);
            } else {
                snprintf(alias_line, sizeof(alias_line), "%s", alias);
            }
            for (int i = 0; i < argc; ++i) free(argv[i]);

            static int alias_depth = 0;
            if (alias_depth < 8) {
                alias_depth++;
                int astat = ExecuteCommandLine(ctx, registry, alias_line, from_script);
                alias_depth--;
                return astat;
            }
            ctx->PrintStatus(ctx, 'E', "RECURSION", "Alias recursion limit exceeded");
            ctx->last_status = 1;
            return 1;
        }
        status = LaunchExternalApplication(ctx, expanded_command);
    } else if (!QualifierList_Validate(&quals, cmd, ctx, argc, argv)) {
        status = 1;
    } else {
        status = cmd->vptr->Execute(cmd, ctx, argc, argv, &quals);
    }
    for (int i = 0; i < argc; ++i) free(argv[i]);
    ctx->last_status = (DWORD)status;
    return status;
}

/* ----------------------------------------------------------------------------
 * Entry Point and Main Interactive Loop
 * ---------------------------------------------------------------------------- */
