#define _CRT_SECURE_NO_WARNINGS
#include "../include/wcl_internal.h"
#include "file_channels.h"

typedef struct OpenCommand {
    struct ICommand base;
} OpenCommand;

int OpenCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc != 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "OPEN requires a channel name and file specification");
        return 1;
    }
    if (!IsValidSymbolName(argv[1])) {
        ctx->PrintStatus(ctx, 'E', "INVCHANNEL", "Invalid file channel name");
        return 1;
    }
    if (FindFileChannel(ctx, argv[1])) {
        ctx->PrintStatus(ctx, 'E', "CHNINUSE", "File channel is already open");
        return 1;
    }
    if (ctx->channel_count >= WCL_MAX_CHANNELS) {
        ctx->PrintStatus(ctx, 'E', "TOOMANYCHN", "Maximum number of open file channels reached");
        return 1;
    }

    bool requested_read = false;
    bool requested_write = false;
    bool append_mode = false;
    for (int i = 0; i < quals->count; ++i) {
        const char *qualifier = quals->items[i].name;
        if (MatchesPrefix(qualifier, "READ")) {
            requested_read = true;
        } else if (MatchesPrefix(qualifier, "WRITE")) {
            requested_write = true;
        } else if (MatchesPrefix(qualifier, "APPEND")) {
            append_mode = true;
        }
    }
    if (!requested_read && !requested_write && !append_mode) requested_read = true;
    bool readable = requested_read;
    bool writable = requested_write || append_mode;

    bool is_network = quals->HasQualifier(quals, "NETWORK") || quals->HasQualifier(quals, "SOCKET");
    if (is_network) {
        if (writable && quals->HasQualifier(quals, "DRYRUN")) {
            ReportDryRun(ctx, "open network stream", argv[2]);
            return 0;
        }
        if (writable && quals->HasQualifier(quals, "CONFIRM")) {
            int confirmed = ConfirmMutation(ctx, "Open network stream", argv[2]);
            if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        }

        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);

        char host[MAX_PATH] = {0};
        char port_str[32] = "80";
        const char *colon = strrchr(argv[2], ':');
        if (colon) {
            size_t hlen = colon - argv[2];
            if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
            strncpy(host, argv[2], hlen);
            host[hlen] = '\0';
            snprintf(port_str, sizeof(port_str), "%s", colon + 1);
        } else {
            snprintf(host, sizeof(host), "%s", argv[2]);
        }

        struct addrinfo hints = {0}, *res = NULL;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
            ctx->PrintStatus(ctx, 'E', "NETRESOLV", "Unable to resolve network host address");
            return 1;
        }

        SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sock == INVALID_SOCKET) {
            freeaddrinfo(res);
            ctx->PrintStatus(ctx, 'E', "NETSOCK", "Failed to create network socket");
            return 1;
        }

        DWORD timeout = 5000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

        if (connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
            closesocket(sock);
            freeaddrinfo(res);
            ctx->PrintStatus(ctx, 'E', "NETCONN", "Failed to connect to remote network host");
            return 1;
        }
        freeaddrinfo(res);

        FileChannel *channel = &ctx->channels[ctx->channel_count++];
        CopyString(channel->name, sizeof(channel->name), argv[1]);
        CopyString(channel->path, sizeof(channel->path), argv[2]);
        channel->stream = NULL;
        channel->sock = sock;
        channel->is_socket = true;
        channel->readable = true;
        channel->writable = true;
        channel->owner_script_depth = ctx->script_depth;
        return 0;
    }
    const char *mode = append_mode ? "a" :
        requested_read && requested_write ? "r+" :
        requested_write ? "w" : "r";
    char path[MAX_PATH];
    bool output_mode = requested_write && !requested_read && !append_mode;
    if (requested_read || append_mode) {
        if (!ResolveFileSpecification(ctx, argv[2], false, path, sizeof(path))) {
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Resolved file specification exceeds the supported length");
            return 1;
        }
        if (append_mode && GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES &&
            !ResolveFileSpecification(ctx, argv[2], true, path, sizeof(path))) {
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Resolved file specification exceeds the supported length");
            return 1;
        }
    } else if (output_mode && !ResolveFileSpecification(ctx, argv[2], true, path, sizeof(path))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Resolved file specification exceeds the supported length");
        return 1;
    } else if (!output_mode) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }

    if (writable && quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, append_mode ? "open for append" :
            requested_read ? "open for reading and writing" : "open for writing", path);
        return 0;
    }
    if (writable && quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, append_mode ? "Open for append" :
            requested_read ? "Open for reading and writing" : "Open for writing", path);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    FILE *stream = NULL;
    if (fopen_s(&stream, path, mode) != 0) {
        ctx->PrintStatus(ctx, 'E', "OPENFAIL", "Unable to open the requested file");
        return 1;
    }

    FileChannel *channel = &ctx->channels[ctx->channel_count++];
    CopyString(channel->name, sizeof(channel->name), argv[1]);
    CopyString(channel->path, sizeof(channel->path), path);
    channel->stream = stream;
    channel->readable = readable;
    channel->writable = writable;
    channel->owner_script_depth = ctx->script_depth;
    return 0;
}

const char* OpenCmd_GetVerb(ICommand *self) { (void)self; return "OPEN"; }
const char* OpenCmd_GetSummary(ICommand *self) { (void)self; return "Opens a file and assigns it to a named channel."; }
const char* OpenCmd_GetHelp(ICommand *self) {
    (void)self;
    return "OPEN[/READ][/WRITE|/APPEND] channel file-spec\n"
           "OPEN[/READ][/WRITE] /NETWORK channel host:port\n"
           "  Opens a file or network TCP socket channel. /NETWORK connects to a remote host:port for READ/WRITE.\n"
           "  File mode defaults to /READ. /READ/WRITE updates; /WRITE creates or truncates; /APPEND adds records.\n"
           "  WCL safety extensions: /CONFIRM prompts before opening; /DRYRUN previews channel opening.";
}

static const ICommandVTable OpenCmd_VTable = {
    { NULL, NULL },
    OpenCmd_Execute,
    OpenCmd_GetVerb,
    OpenCmd_GetSummary,
    OpenCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: READ
 * ---------------------------------------------------------------------------- */
typedef struct ReadCommand {
    struct ICommand base;
} ReadCommand;

int ReadCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    (void)quals;
    if (argc != 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "READ requires an open channel and a destination symbol");
        return 1;
    }

    FileChannel *channel = FindFileChannel(ctx, argv[1]);
    if (!channel) {
        ctx->PrintStatus(ctx, 'E', "CHNNOTOPEN", "File channel is not open");
        return 1;
    }
    if (!channel->readable) {
        ctx->PrintStatus(ctx, 'E', "CHNREAD", "File channel is not open for reading");
        return 1;
    }
    if (channel->writable && !channel->is_socket && fseek(channel->stream, 0, SEEK_CUR) != 0) {
        ctx->PrintStatus(ctx, 'E', "READFAIL", "Unable to synchronize the update file channel");
        return 1;
    }
    if (!IsValidSymbolName(argv[2])) {
        ctx->PrintStatus(ctx, 'E', "INVSYMBOL", "Invalid destination symbol name");
        return 1;
    }

    if (channel->is_socket) {
        char record[WCL_MAX_INPUT] = {0};
        int idx = 0;
        while (idx < (int)sizeof(record) - 1) {
            char ch = 0;
            int r = recv(channel->sock, &ch, 1, 0);
            if (r <= 0) {
                if (idx == 0) {
                    ctx->PrintStatus(ctx, 'E', "ENDOFFILE", "End of network stream reached");
                    return 1;
                }
                break;
            }
            if (ch == '\n') break;
            if (ch != '\r') record[idx++] = ch;
        }
        record[idx] = '\0';
        if (!SetSymbol(ctx, argv[2], record)) {
            ctx->PrintStatus(ctx, 'E', "TOOMANYSYM", "Unable to store the input record in a symbol");
            return 1;
        }
        return 0;
    }

    char record[WCL_MAX_INPUT];
    if (!fgets(record, sizeof(record), channel->stream)) {
        ctx->PrintStatus(ctx, 'E', feof(channel->stream) ? "ENDOFFILE" : "READFAIL",
            feof(channel->stream) ? "End of file reached" : "Unable to read from the file channel");
        return 1;
    }
    size_t length = strlen(record);
    if (length && record[length - 1] == '\n') record[--length] = '\0';
    if (length && record[length - 1] == '\r') record[--length] = '\0';
    if (length == sizeof(record) - 1 && record[length - 1] != '\n') {
        int next = fgetc(channel->stream);
        if (next != '\n' && next != EOF) {
            while ((next = fgetc(channel->stream)) != '\n' && next != EOF) {}
            ctx->PrintStatus(ctx, 'E', "RECORDLONG", "Input record exceeds the supported length");
            return 1;
        }
        if (next == EOF && ferror(channel->stream)) {
            ctx->PrintStatus(ctx, 'E', "READFAIL", "Unable to read from the file channel");
            return 1;
        }
    }
    if (!SetSymbol(ctx, argv[2], record)) {
        ctx->PrintStatus(ctx, 'E', "TOOMANYSYM", "Unable to store the input record in a symbol");
        return 1;
    }
    return 0;
}

const char* ReadCmd_GetVerb(ICommand *self) { (void)self; return "READ"; }
const char* ReadCmd_GetSummary(ICommand *self) { (void)self; return "Reads one record from an open file channel into a symbol."; }
const char* ReadCmd_GetHelp(ICommand *self) {
    (void)self;
    return "READ channel symbol\n"
           "  Reads one line from an OPEN/READ channel into a DCL symbol. End of file is reported as an error status.\n"
           "  DCL error-handler, record-format, and end-of-file control qualifiers have no direct Windows mapping.";
}

static const ICommandVTable ReadCmd_VTable = {
    { NULL, NULL },
    ReadCmd_Execute,
    ReadCmd_GetVerb,
    ReadCmd_GetSummary,
    ReadCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: CLOSE
 * ---------------------------------------------------------------------------- */
typedef struct CloseCommand {
    struct ICommand base;
} CloseCommand;

int CloseCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    (void)quals;
    if (argc != 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "CLOSE requires one open channel name");
        return 1;
    }

    int channel_index = -1;
    for (int i = 0; i < ctx->channel_count; ++i) {
        if (_stricmp(ctx->channels[i].name, argv[1]) == 0) {
            channel_index = i;
            break;
        }
    }
    if (channel_index < 0) {
        if (quals->HasQualifier(quals, "NOLOG")) return 0;
        ctx->PrintStatus(ctx, 'E', "CHNNOTOPEN", "File channel is not open");
        return 1;
    }

    const char *disposition = NULL;
    for (int i = 0; i < quals->count; ++i) {
        if (MatchesPrefix(quals->items[i].name, "DISPOSITION")) {
            disposition = quals->items[i].value;
            break;
        }
    }
    bool delete_on_close = disposition && _stricmp(disposition, "DELETE") == 0;
    if (disposition && _stricmp(disposition, "DELETE") != 0 &&
        _stricmp(disposition, "KEEP") != 0) {
        ctx->PrintStatus(ctx, 'E', "BADDISPOSITION",
            "Windows mapping supports CLOSE/DISPOSITION=KEEP or DELETE; PRINT and SUBMIT are not mapped");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, delete_on_close ? "close and delete file channel" :
            "close file channel", ctx->channels[channel_index].name);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, delete_on_close ? "Close and delete" :
            "Close file channel", ctx->channels[channel_index].name);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    bool is_sock = ctx->channels[channel_index].is_socket;
    SOCKET sock = ctx->channels[channel_index].sock;
    FILE *stream = ctx->channels[channel_index].stream;
    char path[MAX_PATH];
    CopyString(path, sizeof(path), ctx->channels[channel_index].path);
    for (int i = channel_index; i < ctx->channel_count - 1; ++i) {
        ctx->channels[i] = ctx->channels[i + 1];
    }
    --ctx->channel_count;
    if (is_sock) {
        closesocket(sock);
        return 0;
    }
    if (fclose(stream) != 0) {
        ctx->PrintStatus(ctx, 'E', "CLOSEFAIL", "Unable to close the file channel cleanly");
        return 1;
    }
    if (delete_on_close && !DeleteFileA(path)) {
        ctx->PrintStatus(ctx, 'E', "CLOSEDELFAIL", "Channel closed, but its file could not be deleted");
        return 1;
    }
    return 0;
}

const char* CloseCmd_GetVerb(ICommand *self) { (void)self; return "CLOSE"; }
const char* CloseCmd_GetSummary(ICommand *self) { (void)self; return "Closes an open file channel."; }
const char* CloseCmd_GetHelp(ICommand *self) {
    (void)self;
    return "CLOSE channel [/DISPOSITION=KEEP|DELETE]\n"
           "  Closes a channel previously opened with OPEN. /DISPOSITION=DELETE removes its file after close;\n"
           "  /DISPOSITION=KEEP preserves it (default). PRINT and SUBMIT dispositions have no Windows mapping.\n"
    "  DCL /LOG (default) reports an unopened channel; /NOLOG suppresses that error. WCL safety extensions\n"
    "  /CONFIRM and /DRYRUN prompt or preview channel closure. DCL error-handler and other RMS channel qualifiers are rejected.";
}

static const ICommandVTable CloseCmd_VTable = {
    { NULL, NULL },
    CloseCmd_Execute,
    CloseCmd_GetVerb,
    CloseCmd_GetSummary,
    CloseCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: WRITE
 * ---------------------------------------------------------------------------- */
typedef struct WriteCommand {
    struct ICommand base;
} WriteCommand;

int WriteCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    FILE *output = stdout;
    FileChannel *write_channel = NULL;
    int first_record_argument = 1;
    if (argc > 1) {
        if (_stricmp(argv[1], "SYS$OUTPUT") == 0) {
            first_record_argument = 2;
        } else if (_stricmp(argv[1], "SYS$ERROR") == 0) {
            output = stderr;
            first_record_argument = 2;
        } else {
            FileChannel *channel = FindFileChannel(ctx, argv[1]);
            if (channel) {
                if (!channel->writable) {
                    ctx->PrintStatus(ctx, 'E', "CHNWRITE", "File channel is not open for writing");
                    return 1;
                }
                output = channel->stream;
                write_channel = channel;
                first_record_argument = 2;
            }
        }
    }
    char record[WCL_MAX_INPUT];
    record[0] = '\0';
    for (int i = first_record_argument; i < argc; ++i) {
        const char *val = GetSymbol(ctx, argv[i]);
        if (!val) val = ctx->GetLogical(ctx, argv[i]);
        if (!AppendString(record, sizeof(record), val ? val : argv[i]) ||
            (i < argc - 1 && !AppendString(record, sizeof(record), " "))) {
            ctx->PrintStatus(ctx, 'E', "WRITEFAIL", "Output record exceeds the supported length");
            return 1;
        }
        if (write_channel && !write_channel->is_socket && write_channel->readable &&
            fseek(write_channel->stream, 0, SEEK_CUR) != 0) {
            ctx->PrintStatus(ctx, 'E', "WRITEFAIL", "Unable to synchronize the update file channel");
            return 1;
        }
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        char target[96];
        CopyString(target, sizeof(target), output == stdout ? "SYS$OUTPUT" :
            output == stderr ? "SYS$ERROR" : argv[1]);
        ReportDryRun(ctx, "write record to", target);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        const char *output_name = first_record_argument == 1 ? "SYS$OUTPUT" : argv[1];
        int confirmed = ConfirmMutation(ctx, "Write record to", output_name);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }
    if (write_channel && write_channel->is_socket) {
        if (send(write_channel->sock, record, (int)strlen(record), 0) < 0 ||
            send(write_channel->sock, "\r\n", 2, 0) < 0) {
            ctx->PrintStatus(ctx, 'E', "WRITEFAIL", "Unable to write record to network stream");
            return 1;
        }
        return 0;
    }
    if (fprintf(output, "%s\n", record) < 0 || fflush(output) != 0) {
        ctx->PrintStatus(ctx, 'E', "WRITEFAIL", "Unable to write the output record");
        return 1;
    }
    return 0;
}

const char* WriteCmd_GetVerb(ICommand *self) { (void)self; return "WRITE"; }
const char* WriteCmd_GetSummary(ICommand *self) { (void)self; return "Writes a record to an open file channel or standard output."; }
const char* WriteCmd_GetHelp(ICommand *self) {
    (void)self;
    return "WRITE [channel] string-expression ...\n"
           "  Writes a record to an open output channel, SYS$OUTPUT, or SYS$ERROR. Without a channel, writes to the host console.\n"
           "  WCL extensions: /CONFIRM prompts Y/N before writing; /DRYRUN previews without writing.\n"
           "  DCL error-handler and record-control qualifiers have no direct Windows mapping and are rejected.";
}

static const ICommandVTable WriteCmd_VTable = {
    { NULL, NULL },
    WriteCmd_Execute,
    WriteCmd_GetVerb,
    WriteCmd_GetSummary,
    WriteCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: LOGOUT / EXIT
 * ---------------------------------------------------------------------------- */
typedef struct ExitCommand {
    struct ICommand base;
} ExitCommand;

int ExitCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)quals;
    const char *verb = self->vptr->GetVerb(self);
    bool is_exit = _stricmp(verb, "EXIT") == 0;
    if ((!is_exit && argc != 1) || (is_exit && argc > 2)) {
        ctx->PrintStatus(ctx, 'E', "TOOMANY", is_exit ?
            "EXIT accepts at most one numeric status value" : "LOGOUT does not accept parameters");
        return 1;
    }

    DWORD exit_status = ctx->last_status;
    if (is_exit && argc == 2) {
        char *end = NULL;
        unsigned long value = strtoul(argv[1], &end, 10);
        if (!argv[1][0] || !end || *end || value > MAXDWORD) {
            ctx->PrintStatus(ctx, 'E', "BADSTATUS", "EXIT status must be an unsigned integer");
            return 1;
        }
        exit_status = (DWORD)value;
    }

    if (is_exit && ctx->script_depth > 0) {
        ctx->procedure_exit_requested = true;
        ctx->PrintStatus(ctx, 'I', "PROCEDURE", "Command procedure terminated");
    } else {
        ctx->running = false;
        if (!is_exit && quals->HasQualifier(quals, "FULL")) {
            char user[256] = "unknown";
            char host[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
            DWORD user_length = sizeof(user);
            DWORD host_length = sizeof(host);
            GetUserNameA(user, &user_length);
            GetComputerNameA(host, &host_length);
            printf("WCL session terminated for %s on Windows host %s.\n", user, host);
        } else {
            ctx->PrintStatus(ctx, 'I', is_exit ? "EXIT" : "LOGGEDOUT",
                is_exit ? "WCL Process Terminated" : "WCL Process Terminated");
        }
    }
    return (int)exit_status;
}

const char* ExitCmd_GetVerb(ICommand *self) { (void)self; return "LOGOUT"; }
const char* ExitCmd_GetSummary(ICommand *self) { (void)self; return "Ends the active interactive WCL session."; }
const char* ExitCmd_GetHelp(ICommand *self) {
    (void)self;
    return "LOGOUT / EXIT\n"
           "  LOGOUT ends the WCL session; /BRIEF gives a short notice and /FULL adds the Windows account and host.\n"
           "  The DCL /HANGUP and /NOHANGUP terminal controls have no Windows console equivalent and are rejected.\n"
           "  EXIT [status] ends the current command procedure (or session) with the requested numeric status.";
}

static const ICommandVTable ExitCmd_VTable = {
    { NULL, NULL },
    ExitCmd_Execute,
    ExitCmd_GetVerb,
    ExitCmd_GetSummary,
    ExitCmd_GetHelp
};

const char* ExitAliasCmd_GetVerb(ICommand *self) { (void)self; return "EXIT"; }

static const ICommandVTable ExitAliasCmd_VTable = {
    { NULL, NULL },
    ExitCmd_Execute,
    ExitAliasCmd_GetVerb,
    ExitCmd_GetSummary,
    ExitCmd_GetHelp
};

void RegisterFileChannelCommands(CommandRegistry *registry) {
    static OpenCommand open = { { &OpenCmd_VTable, 1 } };
    static ReadCommand read = { { &ReadCmd_VTable, 1 } };
    static CloseCommand close = { { &CloseCmd_VTable, 1 } };
    static WriteCommand write = { { &WriteCmd_VTable, 1 } };
    static ExitCommand logout = { { &ExitCmd_VTable, 1 } };
    static ExitCommand exit_alias = { { &ExitAliasCmd_VTable, 1 } };
    registry->Register(registry, (ICommand*)&open);
    registry->Register(registry, (ICommand*)&read);
    registry->Register(registry, (ICommand*)&close);
    registry->Register(registry, (ICommand*)&write);
    registry->Register(registry, (ICommand*)&logout);
    registry->Register(registry, (ICommand*)&exit_alias);
}

/* ----------------------------------------------------------------------------
 * Command Registry / Dispatcher Architecture
 * ---------------------------------------------------------------------------- */
