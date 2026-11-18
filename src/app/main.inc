int main(int argc, char **argv) {
    /* Initialize Host Context */
    ExecutionContext ctx;
    ctx.hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    ctx.hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    ctx.running = true;
    ctx.last_status = 0;
    ctx.logical_count = 0;
    ctx.symbol_count = 0;
    ctx.channel_count = 0;
    ZeroMemory(ctx.search_streams, sizeof(ctx.search_streams));
    ZeroMemory(ctx.procedure_paths, sizeof(ctx.procedure_paths));
    ctx.script_depth = 0;
    ctx.abort_on_error = true;
    ctx.procedure_exit_requested = false;
    ctx.verify_procedure = false;
    ctx.verify_image = false;
    ctx.SetPrompt = Context_SetPrompt;
    ctx.SetLogical = Context_SetLogical;
    ctx.GetLogical = Context_GetLogical;
    ctx.PrintStatus = Context_PrintStatus;
    ctx.SetPrompt(&ctx, "$ ");

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(ctx.hConsoleOut, &csbi)) {
        ctx.default_text_attr = csbi.wAttributes;
    }

    /* Instantiate PolyMorphic Command Objects */
    DirectoryCommand cmdDir   = { { &DirCmd_VTable, 1 } };
    ShowCommand      cmdShow  = { { &ShowCmd_VTable, 1 } };
    TypeCommand      cmdType  = { { &TypeCmd_VTable, 1 } };
    CopyCommand      cmdCopy  = { { &CopyCmd_VTable, 1 } };
    DeleteCommand    cmdDel   = { { &DeleteCmd_VTable, 1 } };
    RenameCommand    cmdRen   = { { &RenameCmd_VTable, 1 } };
    CreateCommand    cmdCre   = { { &CreateCmd_VTable, 1 } };
    SearchCommand    cmdSrch  = { { &SearchCmd_VTable, 1 } };
    DefineCommand    cmdDef   = { { &DefineCmd_VTable, 1 } };
    DefineCommand    cmdAssign = { { &AssignCmd_VTable, 1 } };
    DefineCommand    cmdDeassign = { { &DeassignCmd_VTable, 1 } };
    SetCommand       cmdSet   = { { &SetCmd_VTable, 1 } };
    SpawnCommand     cmdSpw   = { { &SpawnCmd_VTable, 1 } };
    RunCommand       cmdRun    = { { &RunCmd_VTable, 1 } };
    SubmitCommand    cmdSubmit = { { &SubmitCmd_VTable, 1 } };
    OpenCommand      cmdOpen   = { { &OpenCmd_VTable, 1 } };
    ReadCommand      cmdRead   = { { &ReadCmd_VTable, 1 } };
    CloseCommand     cmdClose  = { { &CloseCmd_VTable, 1 } };
    AppendCommand    cmdAppend = { { &AppendCmd_VTable, 1 } };
    PrintCommand     cmdPrint  = { { &PrintCmd_VTable, 1 } };
    WriteCommand     cmdWrt   = { { &WriteCmd_VTable, 1 } };
    ExitCommand      cmdExit  = { { &ExitCmd_VTable, 1 } };
    ExitCommand      cmdExitAlias = { { &ExitAliasCmd_VTable, 1 } };
    UnsupportedCommand cmdAllocate = {
        { &UnsupportedCmd_VTable, 1 }, "ALLOCATE", "OpenVMS device allocation has no direct Windows equivalent.",
        "ALLOCATE device\n"
        "  Windows mapping: checks whether a local drive is present; Windows has no safe equivalent to\n"
        "  an OpenVMS process-exclusive device allocation. It does not reserve the drive.\n"
        "  WCL /DRYRUN extension previews the access check. DCL generic/shared/physical allocation qualifiers are not mapped."
    };
    UnsupportedCommand cmdMount = {
        { &UnsupportedCmd_VTable, 1 }, "MOUNT", "Adds a Windows volume mount point; not OpenVMS volume mounting.",
        "MOUNT volume-GUID mount-point\n"
        "  Uses the Windows volume mount-point API. The target must be an existing empty drive/directory mount point.\n"
        "  Changing a mount point requires explicit /CONFIRM and an interactive Y response. /DRYRUN previews without changing the mount table.\n"
        "  OpenVMS device/volume attributes, cluster, foreign, policy, and override qualifiers are not mapped and are rejected.\n"
        "  Windows mount-point management is not identical to OpenVMS volume mounting."
    };
    UnsupportedCommand cmdDismount = {
        { &UnsupportedCmd_VTable, 1 }, "DISMOUNT", "Removes a Windows volume mount point; not OpenVMS dismount.",
        "DISMOUNT drive-or-mount-point\n"
        "  Removes the specified Windows mount point; it does not forcibly dismount an in-use volume or\n"
        "  remove alternate mount points. The Windows system volume is protected.\n"
        "  Changing a mount point requires explicit /CONFIRM and an interactive Y response. /DRYRUN previews without changing the mount table.\n"
        "  DCL cluster, unload, abort, and override controls have no Windows mount-point equivalent."
    };
    UnsupportedCommand cmdInitialize = {
        { &UnsupportedCmd_VTable, 1 }, "INITIALIZE", "Formats a Windows data volume with explicit confirmation.",
        "INITIALIZE drive [/FS=NTFS|FAT32|EXFAT] [/LABEL=name] /CONFIRM\n"
        "  Maps to Windows format.com; this is destructive and never runs without explicit /CONFIRM and\n"
        "  an interactive Y response. WCL safety extension /DRYRUN previews only. WCL refuses to format the Windows system volume.\n"
        "  /FS and /LABEL are Windows mappings; DCL volume-structure, ownership, protection, cluster, erase, and RMS\n"
        "  initialization qualifiers have no direct mapping and are rejected. Windows formatting is not identical to OpenVMS volume initialization."
    };

    /* Initialize Command Registry */
    CommandRegistry registry;
    registry.count = 0;
    registry.Register = Registry_Register;
    registry.Resolve = Registry_Resolve;

    HelpCommand cmdHelp = { { &HelpCmd_VTable, 1 }, &registry };

    registry.Register(&registry, (ICommand*)&cmdHelp);
    registry.Register(&registry, (ICommand*)&cmdDir);
    registry.Register(&registry, (ICommand*)&cmdShow);
    registry.Register(&registry, (ICommand*)&cmdType);
    registry.Register(&registry, (ICommand*)&cmdCopy);
    registry.Register(&registry, (ICommand*)&cmdDel);
    registry.Register(&registry, (ICommand*)&cmdRen);
    registry.Register(&registry, (ICommand*)&cmdCre);
    registry.Register(&registry, (ICommand*)&cmdSrch);
    registry.Register(&registry, (ICommand*)&cmdDef);
    registry.Register(&registry, (ICommand*)&cmdAssign);
    registry.Register(&registry, (ICommand*)&cmdDeassign);
    registry.Register(&registry, (ICommand*)&cmdSet);
    registry.Register(&registry, (ICommand*)&cmdSpw);
    registry.Register(&registry, (ICommand*)&cmdRun);
    registry.Register(&registry, (ICommand*)&cmdSubmit);
    registry.Register(&registry, (ICommand*)&cmdOpen);
    registry.Register(&registry, (ICommand*)&cmdRead);
    registry.Register(&registry, (ICommand*)&cmdClose);
    registry.Register(&registry, (ICommand*)&cmdAppend);
    registry.Register(&registry, (ICommand*)&cmdPrint);
    registry.Register(&registry, (ICommand*)&cmdWrt);
    registry.Register(&registry, (ICommand*)&cmdExit);
    registry.Register(&registry, (ICommand*)&cmdExitAlias);
    registry.Register(&registry, (ICommand*)&cmdAllocate);
    registry.Register(&registry, (ICommand*)&cmdMount);
    registry.Register(&registry, (ICommand*)&cmdDismount);
    registry.Register(&registry, (ICommand*)&cmdInitialize);

    if (argc > 1) {
        int status = 1;
        if (argv[1][0] == '@' && argv[1][1]) {
            status = ExecuteProcedure(&ctx, &registry, argv[1] + 1, argc - 2, argv + 2);
        } else {
            ctx.PrintStatus(&ctx, 'E', "IVCLI", "Command-line mode accepts @procedure [P1 ... P8]");
        }
        if (!CloseAllFileChannels(&ctx)) {
            ctx.PrintStatus(&ctx, 'E', "CLOSEFAIL", "One or more file channels could not be closed cleanly");
            status = 1;
        }
        if (!CloseAllFileSearchStreams(&ctx)) {
            ctx.PrintStatus(&ctx, 'E', "SEARCHCLOSE", "One or more file search streams could not be closed cleanly");
            status = 1;
        }
        return status;
    }

    /* Welcome Banner */
    printf("\n  Windows Command Language Shell\n");
    printf("  Copyright (C) PC/OpenSystems LLC.  All rights reserved.\n");
    printf("  Type \"HELP\" for command descriptions or \"LOGOUT\" to return to Windows.\n\n");

    char input_buffer[WCL_MAX_INPUT];

    /* REPL loop */
    while (ctx.running) {
        printf("%s", ctx.current_prompt);
        fflush(stdout);

        if (!fgets(input_buffer, sizeof(input_buffer), stdin)) break;

        size_t input_length = strlen(input_buffer);
        if (input_length == sizeof(input_buffer) - 1 && input_buffer[input_length - 1] != '\n') {
            int next = getchar();
            if (next == '\r') next = getchar();
            if (next != '\n' && next != EOF) {
                while (next != '\n' && next != EOF) next = getchar();
                ctx.PrintStatus(&ctx, 'E', "SYNTAX", "Command line exceeds the supported length");
                ctx.last_status = 1;
                continue;
            }
        }

        /* Strip trailing newline */
        size_t len = input_length;
        while (len > 0 && (input_buffer[len - 1] == '\r' || input_buffer[len - 1] == '\n')) {
            input_buffer[--len] = '\0';
        }
        if (len == 0) continue;

        ExecuteCommandLine(&ctx, &registry, input_buffer, false);
    }

    if (!CloseAllFileChannels(&ctx)) {
        ctx.PrintStatus(&ctx, 'E', "CLOSEFAIL", "One or more file channels could not be closed cleanly");
        return 1;
    }
    if (!CloseAllFileSearchStreams(&ctx)) {
        ctx.PrintStatus(&ctx, 'E', "SEARCHCLOSE", "One or more file search streams could not be closed cleanly");
        return 1;
    }
    return (int)ctx.last_status;
}