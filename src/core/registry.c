#include "../include/wcl_internal.h"
#include "registry.h"
#include "../commands/show.h"


void Registry_Register(CommandRegistry *self, ICommand *cmd) {
    if (self->count < 128) {
        self->commands[self->count++] = cmd;
    }
}

ICommand* Registry_Resolve(CommandRegistry *self, const char *verb, bool *ambiguous) {
    *ambiguous = false;
    ICommand *match = NULL;
    int matches = 0;

    for (int i = 0; i < self->count; ++i) {
        const char *v = self->commands[i]->vptr->GetVerb(self->commands[i]);
        if (_stricmp(v, verb) == 0) {
            return self->commands[i]; /* Exact match takes highest precedence */
        }
        if (MatchesPrefix(verb, v)) {
            match = self->commands[i];
            matches++;
        }
    }

    if (matches > 1) {
        *ambiguous = true;
        return NULL;
    }
    return match;
}

bool QualifierList_Validate(QualifierList *quals, ICommand *cmd, ExecutionContext *ctx,
    int argc, char **argv) {
    const char *verb = cmd->vptr->GetVerb(cmd);
    bool is_show = _stricmp(verb, "SHOW") == 0;
    if (is_show) {
        if (argc < 2) {
            if (quals->count) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL", "A SHOW option is required before qualifiers");
                return false;
            }
            return true;
        }
        bool ambiguous = false;
        const ShowOptionDefinition *option = FindShowOption(argv[1], &ambiguous);
        if (!option) {
            ctx->PrintStatus(ctx, 'E', ambiguous ? "AMBIGSHOW" : "IVSUBVERB",
                ambiguous ? "SHOW option abbreviation is ambiguous" : "Invalid or unknown SHOW option");
            return false;
        }
        bool is_memory = MatchesPrefix(argv[1], "MEMORY");
        bool invalid_combination = false;
        const ShowOptionDefinition *invocation =
            FindShowInvocationOption(option, argc, argv, quals, &invalid_combination);
        if (!invocation) {
            ctx->PrintStatus(ctx, 'E', invalid_combination ? "AMBIGQUAL" : "IVSHOWARG",
                invalid_combination ? "SHOW queue modes cannot be combined" :
                    "Invalid SHOW option parameter or option combination");
            return false;
        }
        for (int i = 0; i < quals->count; ++i) {
            const Qualifier *qualifier = &quals->items[i];
            if (MatchesPrefix(qualifier->name, "DRYRUN") ||
                MatchesPrefix(qualifier->name, "CONFIRM") ||
                MatchesPrefix(qualifier->name, "NOCONFIRM")) {
                if (qualifier->has_value ||
                    (MatchesPrefix(qualifier->name, "NOCONFIRM") &&
                     MatchesPrefix(qualifier->name, "CONFIRM"))) {
                    ctx->PrintStatus(ctx, 'E', "IVQUAL", "Invalid WCL SHOW safety qualifier");
                    return false;
                }
                continue;
            }
            if (!ShowQualifierIsValid(invocation, qualifier->name) ||
                (ShowQualifierBaseName(invocation, qualifier->name) != qualifier->name &&
                 qualifier->has_value)) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL", "Invalid qualifier for this SHOW option");
                return false;
            }
            if (is_memory &&
                qualifier->has_value &&
                !MatchesPrefix(qualifier->name, "OUTPUT") &&
                !MatchesPrefix(qualifier->name, "PAGE") &&
                !MatchesPrefix(qualifier->name, "SEARCH") &&
                !MatchesPrefix(qualifier->name, "HIGHLIGHT")) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "Only /PAGE, /SEARCH, /HIGHLIGHT, and /OUTPUT accept values for SHOW MEMORY");
                return false;
            }
            for (int j = 0; j < i; ++j) {
                const char *current_base = ShowQualifierBaseName(invocation, qualifier->name);
                const char *prior_base = ShowQualifierBaseName(invocation, quals->items[j].name);
                if (_stricmp(current_base, prior_base) == 0 &&
                    ((current_base != qualifier->name) !=
                     (prior_base != quals->items[j].name))) {
                    ctx->PrintStatus(ctx, 'E', "IVQUAL", "Conflicting positive and negative SHOW qualifiers");
                    return false;
                }
            }
        }
        if (is_memory) {
            bool has_page = quals->HasQualifier(quals, "PAGE");
            bool has_output = quals->HasQualifier(quals, "OUTPUT");
            bool has_nooutput = quals->HasQualifier(quals, "NOOUTPUT");
            bool has_search = quals->HasQualifier(quals, "SEARCH");
            bool has_highlight = quals->HasQualifier(quals, "HIGHLIGHT");
            bool page_save = false;
            const char *page_mode = GetQualifierValueByPrefix(quals, "PAGE");
            if (has_page && page_mode && page_mode[0]) {
                page_save = _stricmp(page_mode, "SAVE") == 0 ||
                    _strnicmp(page_mode, "SAVE=", 5) == 0;
                if (!page_save && _stricmp(page_mode, "SCROLL") != 0 &&
                    _stricmp(page_mode, "CLEAR_SCREEN") != 0) {
                    ctx->PrintStatus(ctx, 'E', "IVQUAL",
                        "/PAGE accepts SAVE, SCROLL, or CLEAR_SCREEN");
                    return false;
                }
                if (_strnicmp(page_mode, "SAVE=", 5) == 0) {
                    unsigned long page_count;
                    if (!IsNumericVersion(page_mode + 5, &page_count) ||
                        page_count == 0 || page_count > 5) {
                        ctx->PrintStatus(ctx, 'E', "IVQUAL",
                            "/PAGE=SAVE accepts a page count from 1 through 5");
                        return false;
                    }
                }
            }
            if (has_page && has_output) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/PAGE and /OUTPUT cannot be combined for SHOW MEMORY");
                return false;
            }
            if (has_output && has_nooutput) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/OUTPUT and /NOOUTPUT cannot be combined for SHOW MEMORY");
                return false;
            }
            const char *search_value = GetQualifierValueByPrefix(quals, "SEARCH");
            if (has_search && (!search_value || !search_value[0] || !page_save)) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/SEARCH requires a value and /PAGE=SAVE");
                return false;
            }
            if (has_highlight && !page_save) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/HIGHLIGHT requires /PAGE=SAVE");
                return false;
            }
            if (has_highlight) {
                const char *style = GetQualifierValueByPrefix(quals, "HIGHLIGHT");
                if (style && style[0] && _stricmp(style, "BOLD") != 0 &&
                    _stricmp(style, "BLINK") != 0 && _stricmp(style, "REVERSE") != 0 &&
                    _stricmp(style, "UNDERLINE") != 0) {
                    ctx->PrintStatus(ctx, 'E', "IVQUAL",
                        "/HIGHLIGHT accepts BOLD, BLINK, REVERSE, or UNDERLINE");
                    return false;
                }
            }
            if (quals->HasQualifier(quals, "EXACT") && !page_save) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/EXACT requires /PAGE=SAVE");
                return false;
            }
            if ((quals->HasQualifier(quals, "WRAP") ||
                quals->HasQualifier(quals, "NOWRAP")) && !page_save) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/WRAP and /NOWRAP apply only to /PAGE=SAVE");
                return false;
            }
            if (quals->HasQualifier(quals, "WRAP") &&
                quals->HasQualifier(quals, "NOWRAP")) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/WRAP and /NOWRAP cannot be combined");
                return false;
            }
        }
        bool output_requested = false;
        for (int i = 0; i < quals->count; ++i) {
            if (MatchesPrefix(quals->items[i].name, "OUTPUT") &&
                quals->items[i].has_value && quals->items[i].value[0]) {
                output_requested = true;
            }
        }
        if (!output_requested) {
            for (int i = 0; i < quals->count; ++i) {
                if (MatchesPrefix(quals->items[i].name, "CONFIRM") ||
                    MatchesPrefix(quals->items[i].name, "NOCONFIRM") ||
                    MatchesPrefix(quals->items[i].name, "DRYRUN")) {
                    ctx->PrintStatus(ctx, 'E', "IVQUAL",
                        "SHOW safety qualifiers require /OUTPUT=filespec");
                    return false;
                }
            }
        }
        return true;
    }

    const char *allowed[64];
    size_t allowed_count = 0;
    bool is_open = _stricmp(verb, "OPEN") == 0;
    bool is_submit = _stricmp(verb, "SUBMIT") == 0;
    bool is_stop = _stricmp(verb, "STOP") == 0;
    bool is_initialize = _stricmp(verb, "INITIALIZE") == 0;
    bool is_type = _stricmp(verb, "TYPE") == 0;
    bool is_search = _stricmp(verb, "SEARCH") == 0;
    bool is_deassign = _stricmp(verb, "DEASSIGN") == 0;
    bool is_logout = _stricmp(verb, "LOGOUT") == 0;
    bool is_close = _stricmp(verb, "CLOSE") == 0;
    bool supports_dryrun = false;
    bool supports_confirm = false;
    bool supports_log = false;
    size_t open_mode_count = 0;
    size_t open_read_count = 0;
    size_t open_write_count = 0;
    size_t open_append_count = 0;
    bool open_read_mode = false;
    bool open_write_mode = false;
    bool open_append_mode = false;
    size_t submit_time_count = 0;
    size_t stop_id_count = 0;
    bool is_directory = _stricmp(verb, "DIRECTORY") == 0;
    if (is_directory) {
        allowed[allowed_count++] = "FULL";
        allowed[allowed_count++] = "ALL";
        allowed[allowed_count++] = "VERSIONS";
    } else if (is_type) {
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "TAIL";
        allowed[allowed_count++] = "PAGE";
        supports_confirm = true;
        allowed[allowed_count++] = "DRYRUN";
        supports_dryrun = true;
    } else if (is_search) {
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "EXACT";
        allowed[allowed_count++] = "NUMBERS";
        allowed[allowed_count++] = "HEADING";
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "SKIP";
        allowed[allowed_count++] = "LIMIT";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
    } else if (is_deassign) {
        allowed[allowed_count++] = "ALL";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        allowed[allowed_count++] = "LOG";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    } else if (is_logout) {
        allowed[allowed_count++] = "BRIEF";
        allowed[allowed_count++] = "FULL";
    } else if (is_stop) {
        allowed[allowed_count++] = "IDENTIFICATION";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
    } else if (is_close) {
        allowed[allowed_count++] = "DISPOSITION";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        allowed[allowed_count++] = "LOG";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    } else if (_stricmp(verb, "COPY") == 0 || _stricmp(verb, "APPEND") == 0 ||
        _stricmp(verb, "RENAME") == 0 || _stricmp(verb, "CREATE") == 0 ||
        _stricmp(verb, "PRINT") == 0 || _stricmp(verb, "DEFINE") == 0 ||
        _stricmp(verb, "ASSIGN") == 0 ||
        _stricmp(verb, "SET") == 0 || _stricmp(verb, "SPAWN") == 0 ||
        _stricmp(verb, "RUN") == 0) {
        allowed[allowed_count++] = _stricmp(verb, "CREATE") == 0 ? "DIRECTORY" : "CONFIRM";
        if (_stricmp(verb, "CREATE") != 0) supports_confirm = true;
        allowed[allowed_count++] = "DRYRUN";
        supports_dryrun = true;
        if (_stricmp(verb, "SPAWN") == 0) {
            allowed[allowed_count++] = "WAIT";
            allowed[allowed_count++] = "NOTIFY";
        }
        if (_stricmp(verb, "SET") == 0) {
            allowed[allowed_count++] = "PROTECTION";
            allowed[allowed_count++] = "SECURITY";
            allowed[allowed_count++] = "ACL";
            allowed[allowed_count++] = "PROCESS";
            allowed[allowed_count++] = "PRIORITY";
            allowed[allowed_count++] = "SUSPEND";
            allowed[allowed_count++] = "RESUME";
            allowed[allowed_count++] = "AFFINITY";
            allowed[allowed_count++] = "IDENTIFICATION";
            allowed[allowed_count++] = "ID";
            allowed[allowed_count++] = "SSH";
            allowed[allowed_count++] = "RDP";
            allowed[allowed_count++] = "WINRM";
            allowed[allowed_count++] = "PORT";
            allowed[allowed_count++] = "USER";
            allowed[allowed_count++] = "KEY";
            allowed[allowed_count++] = "SYNC";
            allowed[allowed_count++] = "SERVER";
            allowed[allowed_count++] = "TITLE";
            allowed[allowed_count++] = "WIDTH";
            allowed[allowed_count++] = "PAGE";
            allowed[allowed_count++] = "WRAP";
            allowed[allowed_count++] = "COMPRESS";
            allowed[allowed_count++] = "NAME";
            allowed[allowed_count++] = "PASSWORD";
            allowed[allowed_count++] = "OLD";
            allowed[allowed_count++] = "NEW";
            allowed[allowed_count++] = "GENERATE";
            allowed[allowed_count++] = "DOMAIN";
            allowed[allowed_count++] = "LOG";
            supports_log = true;
        }
        if (_stricmp(verb, "COPY") == 0 || _stricmp(verb, "APPEND") == 0 ||
            _stricmp(verb, "RENAME") == 0 || _stricmp(verb, "CREATE") == 0 ||
            _stricmp(verb, "DEFINE") == 0 || _stricmp(verb, "ASSIGN") == 0) {
            allowed[allowed_count++] = "LOG";
            supports_log = true;
        }
        if (_stricmp(verb, "CREATE") == 0) {
            allowed[allowed_count++] = "CONFIRM";
            supports_confirm = true;
        }
    } else if (_stricmp(verb, "DELETE") == 0) {
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "SYMBOL";
        allowed[allowed_count++] = "ENTRY";
        allowed[allowed_count++] = "DRYRUN";
        allowed[allowed_count++] = "LOG";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    } else if (_stricmp(verb, "CREATE") == 0) {
        allowed[allowed_count++] = "DIRECTORY";
    } else if (is_open) {
        allowed[allowed_count++] = "READ";
        allowed[allowed_count++] = "WRITE";
        allowed[allowed_count++] = "APPEND";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        allowed[allowed_count++] = "NETWORK";
        allowed[allowed_count++] = "SOCKET";
        supports_confirm = true;
        supports_dryrun = true;
    } else if (_stricmp(verb, "WRITE") == 0 || _stricmp(verb, "SUBMIT") == 0) {
        allowed[allowed_count++] = _stricmp(verb, "SUBMIT") == 0 ? "AT" : "CONFIRM";
        if (_stricmp(verb, "SUBMIT") == 0) {
            allowed[allowed_count++] = "AFTER";
            allowed[allowed_count++] = "CONFIRM";
            allowed[allowed_count++] = "DRYRUN";
            supports_confirm = true;
            supports_dryrun = true;
        } else {
            allowed[allowed_count++] = "DRYRUN";
            supports_confirm = true;
            supports_dryrun = true;
        }
    } else if (_stricmp(verb, "ALLOCATE") == 0) {
        allowed[allowed_count++] = "DRYRUN";
        supports_dryrun = true;
        if (_stricmp(verb, "SPAWN") == 0) {
            allowed[allowed_count++] = "WAIT";
            allowed[allowed_count++] = "NOTIFY";
        }
    } else if (_stricmp(verb, "MOUNT") == 0 || _stricmp(verb, "DISMOUNT") == 0) {
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
    } else if (is_initialize) {
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        allowed[allowed_count++] = "FS";
        allowed[allowed_count++] = "LABEL";
        supports_dryrun = true;
        supports_confirm = true;
    } else if (_stricmp(verb, "PURGE") == 0) {
        allowed[allowed_count++] = "KEEP";
        allowed[allowed_count++] = "LOG";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    } else if (_stricmp(verb, "DIFFERENCE") == 0) {
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "IGNORE";
        allowed[allowed_count++] = "MAXIMUM_DIFFERENCES";
    } else if (_stricmp(verb, "DUMP") == 0) {
        allowed[allowed_count++] = "HEX";
        allowed[allowed_count++] = "ASCII";
        allowed[allowed_count++] = "RECORDS";
        allowed[allowed_count++] = "BLOCKS";
        allowed[allowed_count++] = "BYTE";
        allowed[allowed_count++] = "LONGWORD";
        allowed[allowed_count++] = "OUTPUT";
    } else if (_stricmp(verb, "BACKUP") == 0) {
        allowed[allowed_count++] = "LOG";
        allowed[allowed_count++] = "VERIFY";
        allowed[allowed_count++] = "COMPARE";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        allowed[allowed_count++] = "IMAGE";
        allowed[allowed_count++] = "RECORD";
        allowed[allowed_count++] = "SINCE";
        allowed[allowed_count++] = "SAVE_SET";
        allowed[allowed_count++] = "SAVESET";
        allowed[allowed_count++] = "INCREMENTAL";
        allowed[allowed_count++] = "FAST";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    } else if (_stricmp(verb, "SORT") == 0) {
        allowed[allowed_count++] = "KEY";
        allowed[allowed_count++] = "DUPLICATES";
        allowed[allowed_count++] = "NODUPLICATES";
        allowed[allowed_count++] = "LOG";
        supports_log = true;
    } else if (_stricmp(verb, "INQUIRE") == 0) {
        allowed[allowed_count++] = "NOPUNCTUATION";
        allowed[allowed_count++] = "GLOBAL";
    } else if (_stricmp(verb, "START") == 0) {
        allowed[allowed_count++] = "SERVICE";
        allowed[allowed_count++] = "QUEUE";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        allowed[allowed_count++] = "FORCE";
        supports_confirm = true;
        supports_dryrun = true;
    } else if (_stricmp(verb, "SHUTDOWN") == 0) {
        allowed[allowed_count++] = "REBOOT";
        allowed[allowed_count++] = "POWEROFF";
        allowed[allowed_count++] = "DELAY";
        allowed[allowed_count++] = "TIMEOUT";
        allowed[allowed_count++] = "REASON";
        allowed[allowed_count++] = "FORCE";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
    } else if (_stricmp(verb, "REPLY") == 0) {
        allowed[allowed_count++] = "ALL";
        allowed[allowed_count++] = "USER";
        allowed[allowed_count++] = "BELL";
    } else if (_stricmp(verb, "AUTHORIZE") == 0) {
        allowed[allowed_count++] = "USER";
        allowed[allowed_count++] = "GROUP";
        allowed[allowed_count++] = "FULL";
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
    } else if (_stricmp(verb, "CHECKSUM") == 0) {
        allowed[allowed_count++] = "ALGORITHM";
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "LOG";
        supports_log = true;
    } else if (_stricmp(verb, "CONVERT") == 0) {
        allowed[allowed_count++] = "FORMAT";
        allowed[allowed_count++] = "ENCODING";
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "LOG";
        supports_log = true;
    } else if (_stricmp(verb, "ANALYZE") == 0) {
        allowed[allowed_count++] = "IMAGE";
        allowed[allowed_count++] = "DISK";
        allowed[allowed_count++] = "CRASH_DUMP";
    } else if (_stricmp(verb, "UNLOCK") == 0) {
        allowed[allowed_count++] = "RELEASE";
        allowed[allowed_count++] = "KILL";
        allowed[allowed_count++] = "LOG";
        supports_log = true;
    } else if (_stricmp(verb, "SYNCHRONIZE") == 0) {
        allowed[allowed_count++] = "ID";
        allowed[allowed_count++] = "IDENTIFICATION";
        allowed[allowed_count++] = "TIMEOUT";
    } else if (_stricmp(verb, "CONNECT") == 0) {
        allowed[allowed_count++] = "SSH";
        allowed[allowed_count++] = "RDP";
        allowed[allowed_count++] = "WINRM";
        allowed[allowed_count++] = "PORT";
        allowed[allowed_count++] = "USER";
        allowed[allowed_count++] = "KEY";
        allowed[allowed_count++] = "DRYRUN";
        supports_dryrun = true;
        if (_stricmp(verb, "SPAWN") == 0) {
            allowed[allowed_count++] = "WAIT";
            allowed[allowed_count++] = "NOTIFY";
        }
    } else if (_stricmp(verb, "DISCONNECT") == 0) {
        allowed[allowed_count++] = "SESSION";
    } else if (_stricmp(verb, "PING") == 0) {
        allowed[allowed_count++] = "PACKETS";
        allowed[allowed_count++] = "TIMEOUT";
        allowed[allowed_count++] = "BUFFER_SIZE";
    } else if (_stricmp(verb, "PRODUCT") == 0) {
        allowed[allowed_count++] = "SHOW";
        allowed[allowed_count++] = "PATCHES";
        allowed[allowed_count++] = "INSTALL";
        allowed[allowed_count++] = "DELETE";
    } else if (_stricmp(verb, "MONITOR") == 0) {
        allowed[allowed_count++] = "SYSTEM";
        allowed[allowed_count++] = "CPU";
        allowed[allowed_count++] = "DISK";
        allowed[allowed_count++] = "PROCESSES";
        allowed[allowed_count++] = "INTERVAL";
        allowed[allowed_count++] = "COUNT";
    } else if (_stricmp(verb, "ERRORLOG") == 0) {
        allowed[allowed_count++] = "SYSTEM";
        allowed[allowed_count++] = "APPLICATION";
        allowed[allowed_count++] = "SECURITY";
        allowed[allowed_count++] = "SEVERITY";
        allowed[allowed_count++] = "LIMIT";
    } else if (_stricmp(verb, "IRIS") == 0 || _stricmp(verb, "EDIT") == 0) {
        allowed[allowed_count++] = "SCRIPT";
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "READONLY";
        allowed[allowed_count++] = "RECOVER";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        allowed[allowed_count++] = "IRIS";
        allowed[allowed_count++] = "TPU";
        allowed[allowed_count++] = "EVE";
        supports_confirm = true;
        supports_dryrun = true;
    } else if (_stricmp(verb, "PIPE") == 0) {
        allowed[allowed_count++] = "DRYRUN";
        supports_dryrun = true;
        if (_stricmp(verb, "SPAWN") == 0) {
            allowed[allowed_count++] = "WAIT";
            allowed[allowed_count++] = "NOTIFY";
        }
    } else if (_stricmp(verb, "RECALL") == 0) {
        allowed[allowed_count++] = "ALL";
        allowed[allowed_count++] = "PAGE";
        allowed[allowed_count++] = "ERASE";
    } else if (_stricmp(verb, "COMPRESS") == 0) {
        allowed[allowed_count++] = "ZIP";
        allowed[allowed_count++] = "NTFS";
        allowed[allowed_count++] = "LOG";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    } else if (_stricmp(verb, "EXPAND") == 0) {
        allowed[allowed_count++] = "DESTINATION";
        allowed[allowed_count++] = "NTFS";
        allowed[allowed_count++] = "LOG";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    } else if (_stricmp(verb, "ENCRYPT") == 0 || _stricmp(verb, "DECRYPT") == 0) {
        allowed[allowed_count++] = "KEY";
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "ALGORITHM";
        allowed[allowed_count++] = "DPAPI";
        allowed[allowed_count++] = "LOG";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    } else if (_stricmp(verb, "REGISTRY") == 0) {
        allowed[allowed_count++] = "VALUES";
        allowed[allowed_count++] = "DATA";
        allowed[allowed_count++] = "TYPE";
        allowed[allowed_count++] = "TREE";
        allowed[allowed_count++] = "OUTPUT";
        allowed[allowed_count++] = "PAGE";
        allowed[allowed_count++] = "LOG";
        allowed[allowed_count++] = "CONFIRM";
        allowed[allowed_count++] = "DRYRUN";
        supports_confirm = true;
        supports_dryrun = true;
        supports_log = true;
    }

    for (int i = 0; i < quals->count; ++i) {
        const char *name = quals->items[i].name;
        bool negated = _strnicmp(name, "NO", 2) == 0;
        if (negated) name += 2;
        bool recognized = false;
        size_t matches = 0;
        for (size_t j = 0; j < allowed_count; ++j) {
            if (MatchesPrefix(name, allowed[j])) {
                recognized = true;
                ++matches;
                if (is_open && !negated &&
                    MatchesPrefix(name, "READ")) {
                    ++open_mode_count;
                    ++open_read_count;
                    open_read_mode = true;
                } else if (is_open && !negated && MatchesPrefix(name, "WRITE")) {
                    ++open_mode_count;
                    ++open_write_count;
                    open_write_mode = true;
                } else if (is_open && !negated && MatchesPrefix(name, "APPEND")) {
                    ++open_mode_count;
                    ++open_append_count;
                    open_append_mode = true;
                }
                if (is_submit && !negated && quals->items[i].has_value &&
                    (MatchesPrefix(name, "AT") || MatchesPrefix(name, "AFTER"))) {
                    ++submit_time_count;
                }
                if (is_stop && !negated && MatchesPrefix(name, "IDENTIFICATION")) {
                    ++stop_id_count;
                }
            }
        }
        bool valid_negated = MatchesPrefix(name, "CONFIRM") ||
            MatchesPrefix(name, "FULL") || MatchesPrefix(name, "ALL") ||
            MatchesPrefix(name, "DIRECTORY") ||
            MatchesPrefix(name, "WAIT") ||
            MatchesPrefix(name, "WRAP") || MatchesPrefix(name, "COMPRESS") ||
            MatchesPrefix(name, "PAGE");
        if (supports_log && MatchesPrefix(name, "LOG")) valid_negated = true;
        if (MatchesPrefix(name, "DUPLICATES") || MatchesPrefix(name, "PUNCTUATION")) valid_negated = true;
        if ((is_type || is_search) &&
            (MatchesPrefix(name, "OUTPUT") || MatchesPrefix(name, "EXACT") ||
             MatchesPrefix(name, "NUMBERS") || MatchesPrefix(name, "HEADING"))) {
            valid_negated = true;
        }
        bool value_allowed = (is_submit &&
            (MatchesPrefix(name, "AT") || MatchesPrefix(name, "AFTER"))) ||
            (is_stop && MatchesPrefix(name, "IDENTIFICATION")) ||
            (is_close && MatchesPrefix(name, "DISPOSITION")) ||
            (is_directory && MatchesPrefix(name, "VERSIONS")) ||
            ((is_type || is_search) &&
             (MatchesPrefix(name, "OUTPUT") ||
              (is_type && MatchesPrefix(name, "TAIL")) ||
              (is_search && (MatchesPrefix(name, "SKIP") ||
                             MatchesPrefix(name, "LIMIT")))));
        if (is_initialize &&
            (MatchesPrefix(name, "FS") || MatchesPrefix(name, "LABEL"))) {
            value_allowed = true;
        }
        if (MatchesPrefix(name, "ALGORITHM") || MatchesPrefix(name, "OUTPUT") ||
            MatchesPrefix(name, "FORMAT") || MatchesPrefix(name, "ENCODING") ||
            MatchesPrefix(name, "PRIORITY") || MatchesPrefix(name, "AFFINITY") ||
            MatchesPrefix(name, "ID") || MatchesPrefix(name, "IDENTIFICATION") ||
            MatchesPrefix(name, "TIMEOUT") || MatchesPrefix(name, "PORT") ||
            MatchesPrefix(name, "SESSION") || MatchesPrefix(name, "PACKETS") ||
            MatchesPrefix(name, "BUFFER_SIZE") || MatchesPrefix(name, "INTERVAL") ||
            MatchesPrefix(name, "COUNT") || MatchesPrefix(name, "SEVERITY") ||
            MatchesPrefix(name, "LIMIT") || MatchesPrefix(name, "KEEP") ||
            MatchesPrefix(name, "USER") || MatchesPrefix(name, "GROUP") ||
            MatchesPrefix(name, "KEY") || MatchesPrefix(name, "IGNORE") ||
            MatchesPrefix(name, "SCRIPT") ||
            MatchesPrefix(name, "MAXIMUM_DIFFERENCES") || MatchesPrefix(name, "SINCE") ||
            MatchesPrefix(name, "DELAY") || MatchesPrefix(name, "REASON") ||
            MatchesPrefix(name, "SERVER") ||
            MatchesPrefix(name, "TITLE") || MatchesPrefix(name, "WIDTH") ||
            MatchesPrefix(name, "PAGE") || MatchesPrefix(name, "NAME") ||
            MatchesPrefix(name, "ZIP") || MatchesPrefix(name, "DESTINATION") ||
            MatchesPrefix(name, "PROTECTION") || MatchesPrefix(name, "ACL") ||
            MatchesPrefix(name, "OLD") || MatchesPrefix(name, "NEW") || MatchesPrefix(name, "DOMAIN") ||
            MatchesPrefix(name, "VALUES") || MatchesPrefix(name, "DATA") || MatchesPrefix(name, "TYPE") ||
            MatchesPrefix(name, "SECURITY")) {
            value_allowed = true;
        }
        if (!recognized || matches != 1 ||
            (quals->items[i].has_value && !value_allowed) ||
            (is_initialize && (MatchesPrefix(name, "FS") || MatchesPrefix(name, "LABEL")) &&
             !quals->items[i].has_value) ||
            (is_directory && MatchesPrefix(name, "VERSIONS") &&
             !quals->items[i].has_value) ||
            (is_search && (MatchesPrefix(name, "SKIP") || MatchesPrefix(name, "LIMIT")) &&
             !quals->items[i].has_value) ||
            (is_submit && (MatchesPrefix(name, "AT") || MatchesPrefix(name, "AFTER")) &&
             !quals->items[i].has_value) ||
            (is_stop && MatchesPrefix(name, "IDENTIFICATION") &&
             !quals->items[i].has_value) ||
            (is_close && MatchesPrefix(name, "DISPOSITION") &&
             !quals->items[i].has_value) ||
            (is_open && negated && MatchesPrefix(name, "READ")) ||
            (is_submit && negated) ||
            (negated && (quals->items[i].has_value || !valid_negated ||
                (is_deassign && MatchesPrefix(name, "ALL")))) ||
            (is_initialize && negated && MatchesPrefix(name, "CONFIRM")) ||
            (MatchesPrefix(name, "DRYRUN") && negated) ||
            (MatchesPrefix(name, "CONFIRM") && !supports_confirm) ||
            (MatchesPrefix(name, "LOG") && !supports_log) ||
            (MatchesPrefix(name, "DRYRUN") && !supports_dryrun)) {
            char message[160];
            snprintf(message, sizeof(message),
               "Qualifier is unsupported or has no direct Windows mapping for %s; see HELP %s",
               verb, verb);
            ctx->PrintStatus(ctx, 'E', "IVQUAL", message);
            return false;
        }
        bool numeric_qualifier = (is_type && MatchesPrefix(name, "TAIL") &&
            quals->items[i].has_value) ||
            (is_search && (MatchesPrefix(name, "SKIP") || MatchesPrefix(name, "LIMIT")));
        if (is_stop && MatchesPrefix(name, "IDENTIFICATION")) {
            unsigned long process_id;
            if (!IsNumericVersion(quals->items[i].value, &process_id) || !process_id ||
                process_id > MAXDWORD) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/IDENTIFICATION requires a positive decimal Windows process ID");
                return false;
            }
        }
        if (numeric_qualifier) {
            unsigned long number;
            if (!IsNumericVersion(quals->items[i].value, &number) ||
                (is_search && MatchesPrefix(name, "LIMIT") && number == 0) ||
                (is_type && MatchesPrefix(name, "TAIL") && (!number || number > 10000))) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL", "Qualifier requires a valid positive integer value");
                return false;
            }
        }
        if (is_type && MatchesPrefix(name, "TAIL") && !quals->items[i].has_value) continue;
        if (is_type || is_search || is_deassign || is_logout) {
            const char *current_base = negated ? name : quals->items[i].name;
            for (int j = 0; j < i; ++j) {
                const char *prior_name = quals->items[j].name;
                bool prior_negated = _strnicmp(prior_name, "NO", 2) == 0;
                if (prior_negated) prior_name += 2;
                if (negated != prior_negated &&
                    MatchesPrefix(current_base, prior_name) &&
                    MatchesPrefix(prior_name, current_base)) {
                    ctx->PrintStatus(ctx, 'E', "IVQUAL", "Conflicting positive and negative qualifiers");
                    return false;
                }
            }
        }
    }
    if (is_open && (open_read_count > 1 || open_write_count > 1 || open_append_count > 1 ||
        (open_append_mode && (open_read_mode || open_write_mode)) || open_mode_count > 2)) {
        ctx->PrintStatus(ctx, 'E', "IVQUAL",
            "OPEN permits /READ, /WRITE, /READ/WRITE, or /APPEND");
        return false;
    }
    if (is_logout && quals->HasQualifier(quals, "BRIEF") &&
        quals->HasQualifier(quals, "FULL")) {
        ctx->PrintStatus(ctx, 'E', "IVQUAL", "LOGOUT/BRIEF and /FULL are mutually exclusive");
        return false;
    }
    if (is_open && !open_mode_count) open_read_mode = true;
    if (is_open && open_read_mode && !open_write_mode && !open_append_mode) {
        for (int i = 0; i < quals->count; ++i) {
            const char *name = quals->items[i].name;
            if (!_strnicmp(name, "NO", 2)) name += 2;
            if (MatchesPrefix(name, "CONFIRM") || MatchesPrefix(name, "DRYRUN")) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/CONFIRM and /DRYRUN apply only to OPEN/WRITE or OPEN/APPEND");
                return false;
            }
        }
    }
    if (is_submit && submit_time_count != 1) {
        ctx->PrintStatus(ctx, 'E', "IVQUAL", "SUBMIT requires exactly one /AT=time qualifier");
        return false;
    }
    if (is_stop && (stop_id_count > 1 ||
        (stop_id_count && argc != 1) || (!stop_id_count && argc != 2))) {
        ctx->PrintStatus(ctx, 'E', "IVQUAL",
            "STOP requires exactly one process image name or /IDENTIFICATION=pid");
        return false;
    }
    if (is_stop && quals->HasQualifier(quals, "CONFIRM") &&
        quals->HasQualifier(quals, "NOCONFIRM")) {
        ctx->PrintStatus(ctx, 'E', "IVQUAL", "/CONFIRM and /NOCONFIRM cannot be combined");
        return false;
    }
    if (is_type) {
        bool has_page = quals->HasQualifier(quals, "PAGE");
        bool has_output = quals->HasQualifier(quals, "OUTPUT");
        bool has_nooutput = quals->HasQualifier(quals, "NOOUTPUT");
        const char *page_mode = GetQualifierValueByPrefix(quals, "PAGE");
        if (has_page && page_mode && page_mode[0]) {
            bool page_save = _stricmp(page_mode, "SAVE") == 0 ||
                _strnicmp(page_mode, "SAVE=", 5) == 0;
            if (!page_save && _stricmp(page_mode, "SCROLL") != 0 &&
                _stricmp(page_mode, "CLEAR_SCREEN") != 0) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/PAGE accepts SAVE, SCROLL, or CLEAR_SCREEN");
                return false;
            }
            if (_strnicmp(page_mode, "SAVE=", 5) == 0) {
                unsigned long page_count;
                if (!IsNumericVersion(page_mode + 5, &page_count) ||
                    page_count == 0 || page_count > 5) {
                    ctx->PrintStatus(ctx, 'E', "IVQUAL",
                        "/PAGE=SAVE accepts a page count from 1 through 5");
                    return false;
                }
            }
        }
        if (has_page && has_output && !has_nooutput) {
            const char *out_val = GetQualifierValueByPrefix(quals, "OUTPUT");
            if (out_val && out_val[0]) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "/PAGE and /OUTPUT cannot be combined for TYPE");
                return false;
            }
        }
    }
    return true;
}

const char *GetQualifierValueByPrefix(QualifierList *quals, const char *name) {
    for (int i = 0; i < quals->count; ++i) {
        if (MatchesPrefix(quals->items[i].name, name)) {
            return quals->items[i].has_value ? quals->items[i].value : "";
        }
    }
    return NULL;
}

