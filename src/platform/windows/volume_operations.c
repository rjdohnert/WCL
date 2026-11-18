#include "../../include/wcl_internal.h"
#include "volume_operations.h"

bool ResolveDriveRoot(const char *device, char *root, size_t capacity) {
    if (!device || !isalpha((unsigned char)device[0]) || device[1] != ':' ||
        (device[2] && device[2] != '\\' && device[2] != '/')) return false;
    if (capacity < 4) return false;
    root[0] = (char)toupper((unsigned char)device[0]);
    root[1] = ':';
    root[2] = '\\';
    root[3] = '\0';
    return true;
}

bool IsWindowsSystemVolume(const char *root) {
    char windows_directory[MAX_PATH];
    UINT length = GetWindowsDirectoryA(windows_directory, sizeof(windows_directory));
    if (!length || length >= sizeof(windows_directory)) return true;
    return _strnicmp(windows_directory, root, 3) == 0;
}

int InitializeWindowsVolume(ExecutionContext *ctx, int argc, char **argv,
    QualifierList *quals) {
    if (argc != 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "INITIALIZE requires one Windows drive letter such as D:");
        return 1;
    }
    char root[4];
    if (!ResolveDriveRoot(argv[1], root, sizeof(root))) {
        ctx->PrintStatus(ctx, 'E', "BADDEVICE", "INITIALIZE accepts only a local drive letter");
        return 1;
    }
    UINT drive_type = GetDriveTypeA(root);
    if (drive_type != DRIVE_FIXED && drive_type != DRIVE_REMOVABLE) {
        ctx->PrintStatus(ctx, 'E', "BADDEVICE", "Only fixed or removable local volumes can be initialized");
        return 1;
    }
    if (IsWindowsSystemVolume(root)) {
        ctx->PrintStatus(ctx, 'E', "SYSTEMVOLUME",
            "WCL refuses to format the Windows system volume");
        return 1;
    }

    const char *file_system = GetQualifierValueByPrefix(quals, "FS");
    if (!file_system) file_system = "NTFS";
    if (_stricmp(file_system, "NTFS") && _stricmp(file_system, "FAT32") &&
        _stricmp(file_system, "EXFAT")) {
        ctx->PrintStatus(ctx, 'E', "BADFS", "Supported file systems are NTFS, FAT32, and exFAT");
        return 1;
    }
    const char *label = GetQualifierValueByPrefix(quals, "LABEL");
    if (label && strlen(label) > 11) {
        ctx->PrintStatus(ctx, 'E', "BADLABEL", "Volume labels are limited to 11 characters");
        return 1;
    }

    if (!quals->HasQualifier(quals, "CONFIRM") &&
        !quals->HasQualifier(quals, "DRYRUN")) {
        ctx->PrintStatus(ctx, 'E', "CONFIRMREQ",
            "Formatting is destructive and requires explicit /CONFIRM; use /DRYRUN to preview");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        char target[WCL_MAX_INPUT];
        int length = snprintf(target, sizeof(target), "%s as %s%s%s", root, file_system,
            label && label[0] ? " label " : "", label && label[0] ? label : "");
        ReportDryRun(ctx, "format Windows volume", length >= 0 &&
            (size_t)length < sizeof(target) ? target : root);
        return 0;
    }

    int confirmed = ConfirmMutation(ctx, "FORMAT (ERASE DATA ON)", root);
    if (confirmed <= 0) return confirmed < 0 ? 1 : 0;

    char format_path[MAX_PATH];
    UINT system_length = GetSystemDirectoryA(format_path, sizeof(format_path));
    if (!system_length || system_length >= sizeof(format_path) ||
        !AppendString(format_path, sizeof(format_path), "\\format.com")) {
        ctx->PrintStatus(ctx, 'E', "FORMATFAIL", "Unable to locate the Windows format utility");
        return 1;
    }
    char command_line[WCL_MAX_INPUT];
    command_line[0] = '\0';
    if (!AppendWindowsArgument(command_line, sizeof(command_line), format_path) ||
        !AppendString(command_line, sizeof(command_line), " ") ||
        !AppendWindowsArgument(command_line, sizeof(command_line), root) ||
        !AppendString(command_line, sizeof(command_line), " /FS:") ||
        !AppendString(command_line, sizeof(command_line), file_system) ||
        !AppendString(command_line, sizeof(command_line), " /Q") ||
        (label && label[0] && (!AppendString(command_line, sizeof(command_line), " /V:") ||
            !AppendWindowsArgument(command_line, sizeof(command_line), label)))) {
        ctx->PrintStatus(ctx, 'E', "FORMATLONG", "Windows format command exceeds the supported length");
        return 1;
    }

    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));
    if (!CreateProcessA(format_path, command_line, NULL, NULL, FALSE, 0,
        NULL, NULL, &startup, &process)) {
        ctx->PrintStatus(ctx, 'E', "FORMATFAIL", "Unable to start Windows format utility");
        return 1;
    }
    DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    BOOL have_exit_code = wait_result == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    if (!have_exit_code || exit_code != 0) {
        ctx->PrintStatus(ctx, 'E', "FORMATFAIL",
            "Windows format utility did not complete successfully");
        return 1;
    }
    ctx->PrintStatus(ctx, 'S', "FORMATTED", "Windows volume formatted");
    return 0;
}

typedef struct UnsupportedCommand {
    struct ICommand base;
    const char *verb;
    const char *summary;
    const char *help;
} UnsupportedCommand;

int UnsupportedCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    UnsupportedCommand *command = (UnsupportedCommand*)self;
    if (_stricmp(command->verb, "INITIALIZE") == 0) {
        return InitializeWindowsVolume(ctx, argc, argv, quals);
    }
    if (_stricmp(command->verb, "ALLOCATE") == 0) {
        if (argc != 2) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "ALLOCATE requires a Windows drive letter for an access check");
            return 1;
        }
        char root[4];
        if (!ResolveDriveRoot(argv[1], root, sizeof(root)) ||
            GetDriveTypeA(root) == DRIVE_NO_ROOT_DIR) {
            ctx->PrintStatus(ctx, 'E', "BADDEVICE", "Windows drive was not found");
            return 1;
        }
        if (quals->HasQualifier(quals, "DRYRUN")) {
            ReportDryRun(ctx, "check Windows volume access", root);
            return 0;
        }
        printf("Windows mapping for %s: device access check only; no exclusive lease was acquired.\n",
            root);
        printf("Windows does not provide a safe direct equivalent of OpenVMS ALLOCATE here.\n");
        return 0;
    }
    if (_stricmp(command->verb, "MOUNT") == 0) {
        if (argc != 3) {
            ctx->PrintStatus(ctx, 'E', "INSFARG",
                "MOUNT requires a volume GUID path and an existing empty drive/directory mount point");
            return 1;
        }
        if (quals->HasQualifier(quals, "DRYRUN")) {
            char target[WCL_MAX_INPUT];
            int length = snprintf(target, sizeof(target), "%s at %s", argv[1], argv[2]);
            ReportDryRun(ctx, "add Windows volume mount point",
                length >= 0 && (size_t)length < sizeof(target) ? target : "volume");
            return 0;
        }
        if (!quals->HasQualifier(quals, "CONFIRM")) {
            ctx->PrintStatus(ctx, 'E', "CONFIRMREQ",
                "Changing a Windows volume mount point requires explicit /CONFIRM; use /DRYRUN to preview");
            return 1;
        }
        int confirmed = ConfirmMutation(ctx, "Mount Windows volume", argv[1]);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        if (!SetVolumeMountPointA(argv[2], argv[1])) {
            ctx->PrintStatus(ctx, 'E', "MOUNTFAIL",
                "Unable to add Windows mount point; verify the volume GUID, empty mount directory, and permissions");
            return 1;
        }
        ctx->PrintStatus(ctx, 'S', "MOUNTED",
            "Windows volume mount point added (this is not OpenVMS volume mounting)");
        return 0;
    }
    if (_stricmp(command->verb, "DISMOUNT") == 0) {
        if (argc != 2) {
            ctx->PrintStatus(ctx, 'E', "INSFARG",
                "DISMOUNT requires a Windows drive or directory mount point");
            return 1;
        }
        char root[4];
        bool is_drive_root = ResolveDriveRoot(argv[1], root, sizeof(root));
        if (is_drive_root && IsWindowsSystemVolume(root)) {
            ctx->PrintStatus(ctx, 'E', "SYSTEMVOLUME",
                "WCL refuses to remove the Windows system-volume mount point");
            return 1;
        }
        if (quals->HasQualifier(quals, "DRYRUN")) {
            ReportDryRun(ctx, "remove Windows volume mount point", argv[1]);
            return 0;
        }
        if (!quals->HasQualifier(quals, "CONFIRM")) {
            ctx->PrintStatus(ctx, 'E', "CONFIRMREQ",
                "Changing a Windows volume mount point requires explicit /CONFIRM; use /DRYRUN to preview");
            return 1;
        }
        int confirmed = ConfirmMutation(ctx, "Remove Windows mount point", argv[1]);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        if (!DeleteVolumeMountPointA(argv[1])) {
            ctx->PrintStatus(ctx, 'E', "DISMOUNTFAIL",
                "Unable to remove the Windows mount point; this does not forcibly dismount an in-use volume");
            return 1;
        }
        ctx->PrintStatus(ctx, 'S', "DISMOUNTED",
            "Windows volume mount point removed (the volume itself may remain mounted elsewhere)");
        return 0;
    }
    ctx->PrintStatus(ctx, 'E', "NOTSUPPORTED", command->help);
    return 1;
}

const char* UnsupportedCmd_GetVerb(ICommand *self) {
    return ((UnsupportedCommand*)self)->verb;
}

const char* UnsupportedCmd_GetSummary(ICommand *self) {
    return ((UnsupportedCommand*)self)->summary;
}

const char* UnsupportedCmd_GetHelp(ICommand *self) {
    return ((UnsupportedCommand*)self)->help;
}

static const ICommandVTable UnsupportedCmd_VTable = {
    { NULL, NULL },
    UnsupportedCmd_Execute,
    UnsupportedCmd_GetVerb,
    UnsupportedCmd_GetSummary,
    UnsupportedCmd_GetHelp
};

void RegisterVolumeCommands(CommandRegistry *registry) {
    static UnsupportedCommand allocate = {
        { &UnsupportedCmd_VTable, 1 }, "ALLOCATE",
        "OpenVMS device allocation has no direct Windows equivalent.",
        "ALLOCATE device\n"
        "  Windows mapping: checks whether a local drive is present; Windows has no safe equivalent to\n"
        "  an OpenVMS process-exclusive device allocation. It does not reserve the drive.\n"
        "  WCL /DRYRUN extension previews the access check. DCL generic/shared/physical allocation qualifiers are not mapped."
    };
    static UnsupportedCommand mount = {
        { &UnsupportedCmd_VTable, 1 }, "MOUNT",
        "Adds a Windows volume mount point; not OpenVMS volume mounting.",
        "MOUNT volume-GUID mount-point\n"
        "  Uses the Windows volume mount-point API. The target must be an existing empty drive/directory mount point.\n"
        "  Changing a mount point requires explicit /CONFIRM and an interactive Y response. /DRYRUN previews without changing the mount table.\n"
        "  OpenVMS device/volume attributes, cluster, foreign, policy, and override qualifiers are not mapped and are rejected.\n"
        "  Windows mount-point management is not identical to OpenVMS volume mounting."
    };
    static UnsupportedCommand dismount = {
        { &UnsupportedCmd_VTable, 1 }, "DISMOUNT",
        "Removes a Windows volume mount point; not OpenVMS dismount.",
        "DISMOUNT drive-or-mount-point\n"
        "  Removes the specified Windows mount point; it does not forcibly dismount an in-use volume or\n"
        "  remove alternate mount points. The Windows system volume is protected.\n"
        "  Changing a mount point requires explicit /CONFIRM and an interactive Y response. /DRYRUN previews without changing the mount table.\n"
        "  DCL cluster, unload, abort, and override controls have no Windows mount-point equivalent."
    };
    static UnsupportedCommand initialize = {
        { &UnsupportedCmd_VTable, 1 }, "INITIALIZE",
        "Formats a Windows data volume with explicit confirmation.",
        "INITIALIZE drive [/FS=NTFS|FAT32|EXFAT] [/LABEL=name] /CONFIRM\n"
        "  Maps to Windows format.com; this is destructive and never runs without explicit /CONFIRM and\n"
        "  an interactive Y response. WCL safety extension /DRYRUN previews only. WCL refuses to format the Windows system volume.\n"
        "  /FS and /LABEL are Windows mappings; DCL volume-structure, ownership, protection, cluster, erase, and RMS\n"
        "  initialization qualifiers have no direct mapping and are rejected. Windows formatting is not identical to OpenVMS volume initialization."
    };
    registry->Register(registry, (ICommand*)&allocate);
    registry->Register(registry, (ICommand*)&mount);
    registry->Register(registry, (ICommand*)&dismount);
    registry->Register(registry, (ICommand*)&initialize);
}

/* ----------------------------------------------------------------------------
 * Built-in Command: HELP (Provides comprehensive documentation library)
 * ---------------------------------------------------------------------------- */

