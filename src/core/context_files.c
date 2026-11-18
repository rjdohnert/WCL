#include "../include/wcl_internal.h"

bool QualifierList_HasQualifier(QualifierList *self, const char *name) {
    bool present = false;
    for (int i = 0; i < self->count; ++i) {
        const char *qualifier = self->items[i].name;
        if (MatchesPrefix(qualifier, name)) {
            present = true;
        } else if (_strnicmp(qualifier, "NO", 2) == 0 &&
            MatchesPrefix(qualifier + 2, name)) {
            present = false;
        }
    }
    return present;
}

const char* QualifierList_GetQualifierValue(QualifierList *self, const char *name) {
    for (int i = 0; i < self->count; ++i) {
        if (_stricmp(self->items[i].name, name) == 0) {
            return self->items[i].has_value ? self->items[i].value : "";
        }
    }
    return NULL;
}

void QualifierList_Init(QualifierList *self) {
    self->count = 0;
    self->HasQualifier = QualifierList_HasQualifier;
    self->GetQualifierValue = QualifierList_GetQualifierValue;
}

/* ----------------------------------------------------------------------------
 * ExecutionContext Implementation
 * ---------------------------------------------------------------------------- */
void Context_SetPrompt(ExecutionContext *self, const char *prompt) {
    CopyString(self->current_prompt, sizeof(self->current_prompt), prompt);
}

bool Context_SetLogical(ExecutionContext *self, const char *logical, const char *equiv) {
    if (!logical[0] || strlen(logical) >= sizeof(self->logical_table[0].logical) ||
        strlen(equiv) >= sizeof(self->logical_table[0].equivalence)) {
        return false;
    }
    for (int i = 0; i < self->logical_count; ++i) {
        if (_stricmp(self->logical_table[i].logical, logical) == 0) {
            CopyString(self->logical_table[i].equivalence, sizeof(self->logical_table[i].equivalence), equiv);
            return true;
        }
    }
    if (self->logical_count < WCL_MAX_LOGICALS) {
        CopyString(self->logical_table[self->logical_count].logical,
            sizeof(self->logical_table[self->logical_count].logical), logical);
        CopyString(self->logical_table[self->logical_count].equivalence,
            sizeof(self->logical_table[self->logical_count].equivalence), equiv);
        self->logical_count++;
        return true;
    }
    return false;
}

const char* Context_GetLogical(ExecutionContext *self, const char *logical) {
    if (_stricmp(logical, "$STATUS") == 0) {
        snprintf(self->logical_value, sizeof(self->logical_value), "%lu",
            (unsigned long)self->last_status);
        return self->logical_value;
    }
    for (int i = 0; i < self->logical_count; ++i) {
        if (_stricmp(self->logical_table[i].logical, logical) == 0) {
            return self->logical_table[i].equivalence;
        }
    }
    /* Built-in OpenVMS Null Device mappings */
    if (_stricmp(logical, "NL") == 0 || _stricmp(logical, "_NL") == 0 ||
        _stricmp(logical, "NLA0") == 0 || _stricmp(logical, "_NLA0") == 0 ||
        _stricmp(logical, "NL0") == 0 || _stricmp(logical, "_NL0") == 0) {
        return "NUL";
    }
    /* Fallback to Windows Environment Variables */
    SetLastError(ERROR_SUCCESS);
    DWORD length = GetEnvironmentVariableA(logical, self->logical_value, sizeof(self->logical_value));
    if (length == 0) {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) return NULL;
        self->logical_value[0] = '\0';
    } else if (length >= sizeof(self->logical_value)) {
        return NULL;
    }
    return self->logical_value;
}

bool ExpandLogicalName(ExecutionContext *ctx, const char *file_spec, char *expanded, size_t capacity) {
    const char *colon = strchr(file_spec, ':');
    bool drive_prefix = colon == file_spec + 1 &&
        isalpha((unsigned char)file_spec[0]) &&
        (colon[1] == '\\' || colon[1] == '/');
    if (colon && !drive_prefix) {
        size_t logical_length = (size_t)(colon - file_spec);
        if (logical_length >= 64) return false;
        char logical[64];
        memcpy(logical, file_spec, logical_length);
        logical[logical_length] = '\0';
        const char *equivalence = ctx->GetLogical(ctx, logical);
        if (equivalence) {
            expanded[0] = '\0';
            if (!AppendString(expanded, capacity, equivalence)) return false;
            const char *remainder = colon + 1;
            size_t equivalence_length = strlen(equivalence);
            bool equivalence_has_separator = equivalence_length &&
                (equivalence[equivalence_length - 1] == '\\' || equivalence[equivalence_length - 1] == '/');
            bool remainder_has_separator = remainder[0] == '\\' || remainder[0] == '/';
            DWORD attributes = GetFileAttributesA(equivalence);
            if (*remainder && !equivalence_has_separator && !remainder_has_separator &&
                attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !AppendString(expanded, capacity, "\\")) {
                return false;
            }
            return AppendString(expanded, capacity, remainder);
        }
    } else {
        const char *equivalence = ctx->GetLogical(ctx, file_spec);
        if (equivalence) {
            CopyString(expanded, capacity, equivalence);
            return strlen(equivalence) < capacity;
        }
    }

    CopyString(expanded, capacity, file_spec);
    return strlen(file_spec) < capacity;
}

bool IsNumericVersion(const char *text, unsigned long *version) {
    if (!text[0]) return false;
    unsigned long value = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (!isdigit(*p)) return false;
        unsigned digit = (unsigned)(*p - '0');
        if (value > (ULONG_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (version) *version = value;
    return true;
}

bool FindHighestFileVersion(const char *base, unsigned long *highest) {
    char pattern[MAX_PATH];
    CopyString(pattern, sizeof(pattern), base);
    if (!AppendString(pattern, sizeof(pattern), ";*")) return false;

    WIN32_FIND_DATAA data;
    HANDLE search = FindFirstFileA(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) return false;

    const char *base_name = base;
    for (const char *p = base; *p; ++p) {
        if (*p == '\\' || *p == '/') base_name = p + 1;
    }
    size_t name_length = strlen(base_name);
    bool found = false;
    unsigned long maximum = 0;
    do {
        const char *candidate = data.cFileName;
        if (_strnicmp(candidate, base_name, name_length) != 0 || candidate[name_length] != ';') continue;
        unsigned long value;
        if (IsNumericVersion(candidate + name_length + 1, &value) && (!found || value > maximum)) {
            maximum = value;
            found = true;
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);
    if (found) *highest = maximum;
    return found;
}

bool HasFileWildcard(const char *file_spec) {
    return strpbrk(file_spec, "*?") != NULL;
}

bool BuildMatchedFilePath(const char *pattern, const char *file_name, char *path, size_t capacity) {
    char directory[MAX_PATH];
    CopyString(directory, sizeof(directory), pattern);
    char *separator = strrchr(directory, '\\');
    char *forward_separator = strrchr(directory, '/');
    if (forward_separator && (!separator || forward_separator > separator)) separator = forward_separator;
    if (separator) separator[1] = '\0';
    else directory[0] = '\0';
    path[0] = '\0';
    return AppendString(path, capacity, directory) && AppendString(path, capacity, file_name);
}

bool ResolveFileSpecification(ExecutionContext *ctx, const char *file_spec,
    bool output, char *resolved, size_t capacity) {
    char expanded[MAX_PATH];
    if (!ExpandLogicalName(ctx, file_spec, expanded, sizeof(expanded))) return false;

    char *separator = strrchr(expanded, ';');
    const char *version_text = separator ? separator + 1 : NULL;
    unsigned long requested_version = 0;
    bool has_numeric_version = version_text && IsNumericVersion(version_text, &requested_version);
    bool default_version = !version_text || (has_numeric_version && requested_version == 0);

    if (separator && !has_numeric_version) {
        if (!strpbrk(version_text, "*?")) {
            CopyString(resolved, capacity, expanded);
            return strlen(expanded) < capacity;
        }
        if (output) return false;
        CopyString(resolved, capacity, expanded);
        return strlen(expanded) < capacity;
    }

    if (!separator && HasFileWildcard(expanded)) {
        if (output) return false;
        CopyString(resolved, capacity, expanded);
        return strlen(expanded) < capacity;
    }

    if (!default_version) {
        CopyString(resolved, capacity, expanded);
        return strlen(expanded) < capacity;
    }

    char base[MAX_PATH];
    if (separator) {
        size_t base_length = (size_t)(separator - expanded);
        if (base_length >= sizeof(base)) return false;
        memcpy(base, expanded, base_length);
        base[base_length] = '\0';
    } else {
        CopyString(base, sizeof(base), expanded);
        if (strlen(expanded) >= sizeof(base)) return false;
    }

    if (_stricmp(base, "NUL") == 0 || _stricmp(base, "NUL:") == 0) {
        CopyString(resolved, capacity, "NUL");
        return true;
    }
    if (_stricmp(base, "CON") == 0 || _stricmp(base, "CON:") == 0 ||
        _stricmp(base, "PRN") == 0 || _stricmp(base, "PRN:") == 0 ||
        _stricmp(base, "AUX") == 0 || _stricmp(base, "AUX:") == 0 ||
        _stricmp(base, "SYS$INPUT") == 0 ||
        _stricmp(base, "SYS$OUTPUT") == 0 ||
        _stricmp(base, "SYS$ERROR") == 0) {
        CopyString(resolved, capacity, base);
        return strlen(base) < capacity;
    }

    unsigned long highest = 0;
    bool has_version = FindHighestFileVersion(base, &highest);
    if (output) {
        if (highest == ULONG_MAX) return false;
        int length = snprintf(resolved, capacity, "%s;%lu", base, has_version ? highest + 1 : 1);
        return length >= 0 && (size_t)length < capacity;
    }
    if (has_version) {
        int length = snprintf(resolved, capacity, "%s;%lu", base, highest);
        return length >= 0 && (size_t)length < capacity;
    }
    CopyString(resolved, capacity, base);
    return strlen(base) < capacity;
}

void ResetFileSearchStream(FileSearchStream *stream) {
    if (stream->active) FindClose(stream->handle);
    ZeroMemory(stream, sizeof(*stream));
}

bool GetLexicalSearchResult(ExecutionContext *ctx, const char *file_spec, int stream_id,
    char *result, size_t capacity) {
    if (stream_id < 0 || stream_id >= WCL_MAX_SEARCH_STREAMS || !capacity) return false;
    result[0] = '\0';
    if (!file_spec[0]) return true;

    char pattern[MAX_PATH];
    if (!ResolveFileSpecification(ctx, file_spec, false, pattern, sizeof(pattern))) return false;
    FileSearchStream *stream = &ctx->search_streams[stream_id];
    if (stream->active && _stricmp(stream->pattern, pattern) != 0) {
        ResetFileSearchStream(stream);
    }

    if (!stream->active) {
        WIN32_FIND_DATAA first;
        HANDLE handle = FindFirstFileA(pattern, &first);
        if (handle == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        }
        stream->handle = handle;
        stream->active = true;
        stream->first_pending = true;
        CopyString(stream->pattern, sizeof(stream->pattern), pattern);
        stream->current = first;
    }

    for (;;) {
        if (stream->first_pending) {
            stream->first_pending = false;
        } else if (!FindNextFileA(stream->handle, &stream->current)) {
            DWORD error = GetLastError();
            ResetFileSearchStream(stream);
            return error == ERROR_NO_MORE_FILES;
        }

        if (strcmp(stream->current.cFileName, ".") == 0 ||
            strcmp(stream->current.cFileName, "..") == 0) continue;
        if (!BuildMatchedFilePath(pattern, stream->current.cFileName, result, capacity)) {
            ResetFileSearchStream(stream);
            return false;
        }
        return true;
    }
}

FileChannel* FindFileChannel(ExecutionContext *ctx, const char *name) {
    for (int i = 0; i < ctx->channel_count; ++i) {
        if (_stricmp(ctx->channels[i].name, name) == 0) return &ctx->channels[i];
    }
    return NULL;
}

bool CloseAllFileChannels(ExecutionContext *ctx) {
    bool success = true;
    for (int i = 0; i < ctx->channel_count; ++i) {
        if (ctx->channels[i].is_socket) {
            if (closesocket(ctx->channels[i].sock) != 0) success = false;
        } else if (ctx->channels[i].stream) {
            if (fclose(ctx->channels[i].stream) != 0) success = false;
            ctx->channels[i].stream = NULL;
        }
    }
    ctx->channel_count = 0;
    return success;
}

bool CloseAllFileSearchStreams(ExecutionContext *ctx) {
    bool success = true;
    for (int i = 0; i < WCL_MAX_SEARCH_STREAMS; ++i) {
        FileSearchStream *stream = &ctx->search_streams[i];
        if (stream->active && !FindClose(stream->handle)) success = false;
        ZeroMemory(stream, sizeof(*stream));
    }
    return success;
}

bool CloseProcedureFileChannels(ExecutionContext *ctx, int script_depth) {
    bool success = true;
    int i = 0;
    while (i < ctx->channel_count) {
        FileChannel *channel = &ctx->channels[i];
        if (channel->owner_script_depth < script_depth) {
            ++i;
            continue;
        }
        if (channel->is_socket) {
            if (closesocket(channel->sock) != 0) success = false;
        } else if (channel->stream) {
            if (fclose(channel->stream) != 0) success = false;
        }
        for (int j = i; j < ctx->channel_count - 1; ++j) {
            ctx->channels[j] = ctx->channels[j + 1];
        }
        --ctx->channel_count;
    }
    return success;
}

void Context_PrintStatus(ExecutionContext *self, char severity, const char *ident, const char *text) {
    /* Standard VMS message format: %SYSTEM-S-NORMAL, message text */
    printf("%%WCL-%c-%s, %s\n", toupper(severity), ident, text);
}

int ConfirmMutation(ExecutionContext *ctx, const char *operation, const char *target) {
    DWORD console_mode;
    if (!ctx->hConsoleIn || ctx->hConsoleIn == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(ctx->hConsoleIn, &console_mode)) {
        ctx->PrintStatus(ctx, 'E', "NOCONSOLE",
            "Confirmation requires an interactive console; no change was made");
        return -1;
    }

    printf("%s %s? [Y/N]: ", operation, target);
    fflush(stdout);
    char answer[64];
    DWORD bytes_read = 0;
    if (!ReadConsoleA(ctx->hConsoleIn, answer, (DWORD)sizeof(answer) - 1, &bytes_read, NULL) ||
        bytes_read == 0) {
        ctx->PrintStatus(ctx, 'E', "CONFIRMFAIL",
            "No interactive response was available; no change was made");
        return -1;
    }
    answer[bytes_read] = '\0';
    char *response = answer;
    while (*response && isspace((unsigned char)*response)) response++;
    char *response_end = response + strlen(response);
    while (response_end > response && isspace((unsigned char)response_end[-1])) {
        *--response_end = '\0';
    }
    if (toupper((unsigned char)response[0]) == 'Y' && response[1] == '\0') return 1;
    if (toupper((unsigned char)response[0]) == 'N' && response[1] == '\0') {
        ctx->PrintStatus(ctx, 'I', "ABORTED", "Operation abandoned; no change was made");
        return 0;
    }
    ctx->PrintStatus(ctx, 'E', "BADRESPONSE",
        "Enter Y or N; no change was made");
    return 0;
}

void ReportDryRun(ExecutionContext *ctx, const char *operation, const char *target) {
    char message[WCL_MAX_INPUT];
    int length = snprintf(message, sizeof(message), "DRYRUN: would %s %s; no change was made",
        operation, target);
    if (length < 0 || (size_t)length >= sizeof(message)) {
        CopyString(message, sizeof(message), "DRYRUN: operation previewed; no change was made");
    }
    ctx->PrintStatus(ctx, 'I', "DRYRUN", message);
}

bool DirectoryVersionMatches(const char *search_path, const char *file_name,
    bool all_versions, unsigned long version_limit) {
    const char *search_version_separator = strrchr(search_path, ';');
    unsigned long explicit_version;
    if (search_version_separator &&
        IsNumericVersion(search_version_separator + 1, &explicit_version)) return true;
    char matched_path[MAX_PATH];
    if (!BuildMatchedFilePath(search_path, file_name, matched_path, sizeof(matched_path))) return false;
    char *version_separator = strrchr(matched_path, ';');
    if (!version_separator) return true;
    unsigned long version;
    if (!IsNumericVersion(version_separator + 1, &version)) return true;
    if (all_versions) return true;

    *version_separator = '\0';
    unsigned long highest;
    if (!FindHighestFileVersion(matched_path, &highest)) return true;
    return version <= highest && highest - version < version_limit;
}

