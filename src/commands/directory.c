#include "../include/wcl_internal.h"
#include "directory.h"

typedef struct DirectoryCommand {
    struct ICommand base;
} DirectoryCommand;

int DirCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    char search_path[MAX_PATH];
    char target_dir[MAX_PATH];
    const char *versions = NULL;
    for (int i = 0; i < quals->count; ++i) {
        if (MatchesPrefix(quals->items[i].name, "VERSIONS")) {
            versions = quals->items[i].has_value ? quals->items[i].value : NULL;
            break;
        }
    }
    bool all_versions = versions == NULL;
    unsigned long version_limit = 0;
    if (versions && (!IsNumericVersion(versions, &version_limit) || version_limit == 0)) {
        ctx->PrintStatus(ctx, 'E', "BADVERSIONS", "/VERSIONS requires a positive version count");
        return 1;
    }
    if (!GetCurrentDirectoryA(MAX_PATH, target_dir)) {
        ctx->PrintStatus(ctx, 'E', "CURDIRFAIL", "Unable to retrieve current directory");
        return 1;
    }
    if (argc > 1) {
        if (!ExpandLogicalName(ctx, argv[1], search_path, sizeof(search_path))) {
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
            return 1;
        }
        CopyString(target_dir, sizeof(target_dir), search_path);
        DWORD attributes = GetFileAttributesA(search_path);
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
            !strpbrk(search_path, "*?")) {
            size_t path_length = strlen(search_path);
            if (path_length && search_path[path_length - 1] != '\\' && search_path[path_length - 1] != '/') {
                if (!AppendString(search_path, sizeof(search_path), "\\")) {
                    ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Directory path exceeds the supported length");
                    return 1;
                }
            }
            if (!AppendString(search_path, sizeof(search_path), "*.*")) {
                ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Directory path exceeds the supported length");
                return 1;
            }
        }
    } else {
        int length = snprintf(search_path, sizeof(search_path), "%s\\*.*", target_dir);
        if (length < 0 || (size_t)length >= sizeof(search_path)) {
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Current directory path exceeds the supported length");
            return 1;
        }
    }

    unsigned long existing_highest_version;
    if (argc > 1 && !HasFileWildcard(search_path) &&
        !strchr(search_path, ';') &&
        FindHighestFileVersion(search_path, &existing_highest_version)) {
        if (!AppendString(search_path, sizeof(search_path), ";*")) {
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "File specification exceeds the supported length");
            return 1;
        }
    }
    char versioned_search_path[MAX_PATH];
    if (!ResolveFileSpecification(ctx, search_path, false, versioned_search_path, sizeof(versioned_search_path))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Resolved file specification exceeds the supported length");
        return 1;
    }
    CopyString(search_path, sizeof(search_path), versioned_search_path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_path, &fd);

    if (hFind == INVALID_HANDLE_VALUE) {
        ctx->PrintStatus(ctx, 'W', "NOFILES", "No files found matching criteria");
        return 1;
    }

    printf("\nDirectory %s\n\n", target_dir);

    bool full = quals->HasQualifier(quals, "FULL");
    bool show_all = quals->HasQualifier(quals, "ALL");
    ULONGLONG total_bytes = 0;
    ULONGLONG total_blocks = 0;
    int total_files = 0;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (!show_all && (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)) continue;
        if (!DirectoryVersionMatches(search_path, fd.cFileName, all_versions, version_limit)) continue;

        ULARGE_INTEGER fileSize;
        fileSize.LowPart = fd.nFileSizeLow;
        fileSize.HighPart = fd.nFileSizeHigh;
        ULONGLONG blocks = (fileSize.QuadPart + WCL_BLOCK_SIZE - 1) / WCL_BLOCK_SIZE;
        total_bytes += fileSize.QuadPart;
        total_blocks += blocks;
        total_files++;

        char date_str[32];
        FormatVmsTime(&fd.ftLastWriteTime, date_str, sizeof(date_str));
        char size_str[32];
        FormatFileSizeMetric(fileSize.QuadPart, size_str, sizeof(size_str));

        char display_name[MAX_PATH];
        CopyString(display_name, sizeof(display_name), fd.cFileName);
        size_t nlen = strlen(display_name);
        if (nlen >= 2 && display_name[nlen - 2] == ';' && display_name[nlen - 1] == '1') {
            display_name[nlen - 2] = '\0';
        }
        for (char *p = display_name; *p; ++p) *p = (char)toupper(*p);

        if (full) {
            char create_str[32];
            FormatVmsTime(&fd.ftCreationTime, create_str, sizeof(create_str));
            printf("\n%s\n", display_name);
            printf("Size:            %-16s\n", size_str);
            printf("Created:   %s\n", create_str);
            printf("Revised:   %s\n", date_str);
            printf("File attributes:   %s%s%s\n", 
                (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "DIRECTORY " : "",
                (fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? "READONLY " : "RWED ",
                (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? "HIDDEN " : "");
        } else {
            printf("%-36s %10s   %s\n", display_name, size_str, date_str);
        }
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);

    char total_size_str[32];
    FormatFileSizeMetric(total_bytes, total_size_str, sizeof(total_size_str));
    printf("\nTotal of %d file%s, %s.\n\n",
        total_files, total_files == 1 ? "" : "s",
        total_size_str);
    return 0;
}

const char* DirCmd_GetVerb(ICommand *self) { (void)self; return "DIRECTORY"; }
const char* DirCmd_GetSummary(ICommand *self) { (void)self; return "Displays file catalog and directory lists."; }
const char* DirCmd_GetHelp(ICommand *self) {
    (void)self;
    return "DIRECTORY [file-spec] [/FULL] [/VERSIONS=count]\n"
           "  Displays all matching versions by default. /VERSIONS=count limits the display to the most recent count versions.\n"
           "  An explicit ;N file version is honored.\n\n"
           "  Qualifiers:\n"
           "    /ALL        Includes hidden files and directories; hidden entries are otherwise omitted.\n"
           "    /FULL       Provides extensive size, timestamp, and attribute metadata; sizes are exact bytes.\n"
           "    /NOFULL     Uses the default compact listing.\n"
           "    /VERSIONS=count  Limits the display to the most recent count versions.\n"
           "    The compact listing also displays each file's exact byte size and 512-byte block count.\n"
           "  Owner, protection, ACL, RMS record, date-selection, and VMS output-format qualifiers have no direct\n"
           "  Windows mapping and are rejected.";
}

static const ICommandVTable DirCmd_VTable = {
    { NULL, NULL },
    DirCmd_Execute,
    DirCmd_GetVerb,
    DirCmd_GetSummary,
    DirCmd_GetHelp
};

void RegisterDirectoryCommand(CommandRegistry *registry) {
    static DirectoryCommand command = { { &DirCmd_VTable, 1 } };
    registry->Register(registry, (ICommand*)&command);
}

/* ----------------------------------------------------------------------------
 * Built-in Command: SHOW
 * ---------------------------------------------------------------------------- */
