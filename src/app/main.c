#include "../include/wcl_internal.h"
#include "../core/preamble.h"
#include "../core/registry.h"
#include "../commands/directory.h"
#include "../commands/show.h"
#include "../commands/file_operations.h"
#include "../commands/process.h"
#include "../commands/file_channels.h"
#include "../commands/help.h"
#include "../platform/windows/scheduler.h"
#include "../platform/windows/volume_operations.h"
#include "../commands/dcl_extended.h"
#include "../editor/iris.h"
#include "../core/history.h"

int main(int argc, char **argv) {
    ExecutionContext ctx;
    InitializeExecutionContext(&ctx);

    CommandRegistry registry = {0};
    registry.Register = Registry_Register;
    registry.Resolve = Registry_Resolve;
    ctx.registry = &registry;

    RegisterHelpCommand(&registry);
    RegisterDirectoryCommand(&registry);
    RegisterShowCommand(&registry);
    RegisterFileOperationCommands(&registry);
    RegisterProcessCommands(&registry);
    RegisterSchedulerCommands(&registry);
    RegisterFileChannelCommands(&registry);
    RegisterVolumeCommands(&registry);
    RegisterDclExtendedCommands(&registry);
    RegisterIrisCommand(&registry);

    if (argc > 1) {
        int status = 1;
        if (argv[1][0] == '@' && argv[1][1]) {
            status = ExecuteProcedure(&ctx, &registry, argv[1] + 1, argc - 2, argv + 2);
        } else {
            ctx.PrintStatus(&ctx, 'E', "IVCLI", "Command-line mode accepts @procedure [P1 ... P8]");
        }
        if (!ShutdownExecutionContext(&ctx)) status = 1;
        return status;
    }

    /* Initialize user configuration (wcl.ini) and history (.wcl_history) */
    History_Init(&ctx, &registry);

    /* Welcome Banner */
    printf("\n  Windows Command Language Shell\n");
    printf("  WCL Version: %s\n", WCL_VERSION);
    printf("  Copyright (C) PC/OpenSystems LLC.  All rights reserved.\n");
    printf("  Type \"HELP\" for command descriptions or \"LOGOUT\" to return to Windows.\n\n");

    char input_buffer[WCL_MAX_INPUT];

    /* REPL loop */
    while (ctx.running) {
        if (!History_ReadLine(&ctx, input_buffer, sizeof(input_buffer))) break;

        size_t input_length = strlen(input_buffer);
        size_t len = input_length;
        while (len > 0 && (input_buffer[len - 1] == '\r' || input_buffer[len - 1] == '\n')) {
            input_buffer[--len] = '\0';
        }
        if (len == 0) continue;

        History_Add(input_buffer);
        ExecuteCommandLine(&ctx, &registry, input_buffer, false);
    }

    History_Cleanup();
    if (!ShutdownExecutionContext(&ctx)) return 1;
    return (int)ctx.last_status;
}