#ifndef WCL_INTERNAL_H
#define WCL_INTERNAL_H

/* ============================================================================
 * WCL: Windows Command Language Shell 
 * Copyright (C) 2026, PC/OpenSystems LLC, All Rights Reserved.
 * Target Subsystem: Win32 Console Application
 * ============================================================================ */

#define _WIN32_WINNT 0x0600
#define PSAPI_VERSION 2
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winspool.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <wininet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <stdbool.h>
#include <io.h>

#pragma comment(lib, "Iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Wininet.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Winspool.lib")

#define WCL_MAX_INPUT       2048
#define WCL_MAX_ARGS        64
#define WCL_MAX_QUALIFIERS  32
#define WCL_MAX_LOGICALS    128
#define WCL_MAX_SYMBOLS     128
#define WCL_MAX_CHANNELS    32
#define WCL_MAX_SEARCH_STREAMS 8
#define WCL_MAX_SCRIPT_LINES 4096
#define WCL_MAX_SCRIPT_DEPTH 8
#define WCL_BLOCK_SIZE      512
#define WCL_VERSION         "9.2-2026"

/* Forward Declarations */
typedef struct IObject IObject;
typedef struct ICommand ICommand;
typedef struct ExecutionContext ExecutionContext;
typedef struct QualifierList QualifierList;
typedef struct CommandRegistry CommandRegistry;

/* ----------------------------------------------------------------------------
 * Object-Oriented Infrastructure: Base Object and VTable
 * ---------------------------------------------------------------------------- */
typedef struct IObjectVTable {
    void (*Dispose)(IObject *self);
    const char* (*ToString)(IObject *self);
} IObjectVTable;

struct IObject {
    const IObjectVTable *vptr;
    int ref_count;
};

/* ----------------------------------------------------------------------------
 * Command Architecture: Polymorphic Interface
 * ---------------------------------------------------------------------------- */
typedef struct Qualifier {
    char name[32];
    char value[MAX_PATH];
    bool has_value;
} Qualifier;

struct QualifierList {
    Qualifier items[WCL_MAX_QUALIFIERS];
    int count;
    bool (*HasQualifier)(QualifierList *self, const char *name);
    const char* (*GetQualifierValue)(QualifierList *self, const char *name);
};

typedef struct ICommandVTable {
    IObjectVTable base;
    int (*Execute)(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
    const char* (*GetVerb)(ICommand *self);
    const char* (*GetSummary)(ICommand *self);
    const char* (*GetHelpTopic)(ICommand *self);
} ICommandVTable;

struct ICommand {
    const ICommandVTable *vptr;
    int ref_count;
};

/* ----------------------------------------------------------------------------
 * Environment & Execution Context (Encapsulation of Shell State)
 * ---------------------------------------------------------------------------- */
typedef struct LogicalTableEntry {
    char logical[64];
    char equivalence[MAX_PATH];
} LogicalTableEntry;

typedef struct SymbolTableEntry {
    char name[64];
    char value[WCL_MAX_INPUT];
} SymbolTableEntry;

typedef struct FileChannel {
    char name[64];
    char path[MAX_PATH];
    FILE *stream;
    bool readable;
    bool writable;
    int owner_script_depth;
    SOCKET sock;
    bool is_socket;
} FileChannel;

typedef struct WclBackgroundJob {
    int entry_id;
    DWORD pid;
    HANDLE hProcess;
    char command[256];
    SYSTEMTIME started;
    bool active;
} WclBackgroundJob;

typedef struct FileSearchStream {
    HANDLE handle;
    char pattern[MAX_PATH];
    bool active;
    bool first_pending;
    WIN32_FIND_DATAA current;
} FileSearchStream;

typedef struct ExecutionContext {
    char current_prompt[32];
    bool running;
    DWORD last_status;
    LogicalTableEntry logical_table[WCL_MAX_LOGICALS];
    int logical_count;
    SymbolTableEntry symbols[WCL_MAX_SYMBOLS];
    int symbol_count;
    FileChannel channels[WCL_MAX_CHANNELS];
    int channel_count;
    WclBackgroundJob background_jobs[32];
    int background_job_count;
    FileSearchStream search_streams[WCL_MAX_SEARCH_STREAMS];
    char procedure_paths[WCL_MAX_SCRIPT_DEPTH][MAX_PATH];
    char logical_value[32768];
    int script_depth;
    bool abort_on_error;
    bool procedure_exit_requested;
    bool verify_procedure;
    bool verify_image;
    HANDLE hConsoleOut;
    HANDLE hConsoleIn;
    WORD default_text_attr;
    UINT orig_output_cp;
    UINT orig_input_cp;
    void *registry;

    /* Member Methods */
    void (*SetPrompt)(ExecutionContext *self, const char *prompt);
    bool (*SetLogical)(ExecutionContext *self, const char *logical, const char *equiv);
    const char* (*GetLogical)(ExecutionContext *self, const char *logical);
    void (*PrintStatus)(ExecutionContext *self, char severity, const char *ident, const char *text);
} ExecutionContext;

struct CommandRegistry {
    ICommand *commands[128];
    int count;

    void (*Register)(struct CommandRegistry *self, ICommand *cmd);
    ICommand* (*Resolve)(struct CommandRegistry *self, const char *verb, bool *ambiguous);
};

/* Module API headers. */
#include "../core/preamble.h"
#include "../core/symbols_lexicals.h"
#include "../core/context_files.h"
#include "../commands/directory.h"
#include "../commands/show.h"
#include "../commands/file_operations.h"
#include "../commands/process.h"
#include "../platform/windows/scheduler.h"
#include "../commands/file_channels.h"
#include "../core/registry.h"
#include "../platform/windows/volume_operations.h"
#include "../commands/help.h"
#include "../core/parser_conditions.h"
#include "../scripting/procedure.h"
#include "../core/execution.h"

#endif /* WCL_INTERNAL_H */
