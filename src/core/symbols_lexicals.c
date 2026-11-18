#include "../include/wcl_internal.h"

void CopyString(char *dest, size_t capacity, const char *src) {
    if (!dest || capacity == 0) return;
    if (!src) { dest[0] = '\0'; return; }
    size_t length = strlen(src);
    if (length >= capacity) length = capacity - 1;
    memcpy(dest, src, length);
    dest[length] = '\0';
}

bool AppendString(char *dest, size_t capacity, const char *src) {
    if (!dest || !src || capacity == 0) return false;
    size_t dest_length = strlen(dest);
    size_t src_length = strlen(src);
    if (dest_length >= capacity || src_length >= capacity - dest_length) return false;
    memcpy(dest + dest_length, src, src_length + 1);
    return true;
}

bool IsSymbolNameChar(unsigned char c) {
    return isalnum(c) || c == '_' || c == '$';
}

bool IsValidSymbolName(const char *name) {
    if (!name || !name[0] || !(isalpha((unsigned char)name[0]) || name[0] == '_' || name[0] == '$')) return false;
    for (const unsigned char *p = (const unsigned char *)name + 1; *p; ++p) {
        if (!IsSymbolNameChar(*p)) return false;
    }
    return true;
}

const char* GetSymbol(ExecutionContext *ctx, const char *name) {
    if (!ctx || !name) return NULL;
    if (_stricmp(name, "$STATUS") == 0) return ctx->GetLogical(ctx, "$STATUS");
    for (int i = 0; i < ctx->symbol_count; ++i) {
        if (_stricmp(ctx->symbols[i].name, name) == 0) return ctx->symbols[i].value;
    }
    return NULL;
}

bool SetSymbol(ExecutionContext *ctx, const char *name, const char *value) {
    if (!ctx || !name || !value || !IsValidSymbolName(name) || _stricmp(name, "$STATUS") == 0 ||
        strlen(name) >= sizeof(ctx->symbols[0].name) ||
        strlen(value) >= sizeof(ctx->symbols[0].value)) {
        return false;
    }
    for (int i = 0; i < ctx->symbol_count; ++i) {
        if (_stricmp(ctx->symbols[i].name, name) == 0) {
            CopyString(ctx->symbols[i].value, sizeof(ctx->symbols[i].value), value);
            return true;
        }
    }
    if (ctx->symbol_count >= WCL_MAX_SYMBOLS) return false;
    CopyString(ctx->symbols[ctx->symbol_count].name, sizeof(ctx->symbols[0].name), name);
    CopyString(ctx->symbols[ctx->symbol_count].value, sizeof(ctx->symbols[0].value), value);
    ctx->symbol_count++;
    return true;
}

bool DeleteSymbol(ExecutionContext *ctx, const char *name) {
    if (!ctx || !name) return false;
    for (int i = 0; i < ctx->symbol_count; ++i) {
        if (_stricmp(ctx->symbols[i].name, name) == 0) {
            for (int j = i; j < ctx->symbol_count - 1; ++j) {
                ctx->symbols[j] = ctx->symbols[j + 1];
            }
            ctx->symbol_count--;
            return true;
        }
    }
    return false;
}

typedef enum LexicalFunction {
    LEXICAL_LENGTH,
    LEXICAL_EXTRACT,
    LEXICAL_LOCATE,
    LEXICAL_ELEMENT,
    LEXICAL_EDIT,
    LEXICAL_SEARCH,
    LEXICAL_ENVIRONMENT,
    LEXICAL_INTEGER,
    LEXICAL_STRING,
    LEXICAL_VERIFY,
    LEXICAL_TIME,
    LEXICAL_GETSYI,
    LEXICAL_GETJPI,
    LEXICAL_GETDVI,
    LEXICAL_FILE_ATTRIBUTES,
    LEXICAL_PARSE,
    LEXICAL_TRNLNM,
    LEXICAL_PRIVILEGE,
    LEXICAL_REGISTRY
} LexicalFunction;

bool ExpandSymbolsInternal(ExecutionContext *ctx, const char *source, char *dest,
    size_t capacity, int depth);
static bool EvaluateNumericExpression(const char *text, long long *value);

bool ParseLexicalInteger(const char *text, long long *value) {
    while (isspace((unsigned char)*text)) text++;
    if (!*text) return false;
    errno = 0;
    char *end = NULL;
    long long parsed = strtoll(text, &end, 10);
    if (errno == ERANGE || end == text) return false;
    while (isspace((unsigned char)*end)) end++;
    if (*end) return false;
    *value = parsed;
    return true;
}

bool IsQuotedLexicalArgument(const char *source) {
    while (isspace((unsigned char)*source)) source++;
    size_t length = strlen(source);
    while (length && isspace((unsigned char)source[length - 1])) length--;
    return length >= 2 && source[0] == '"' && source[length - 1] == '"';
}

static bool BeginsNumericExpression(const char *text) {
    while (isspace((unsigned char)*text)) text++;
    return isdigit((unsigned char)*text) || *text == '+' || *text == '-' || *text == '(';
}

bool ParseLexicalArguments(const char *open_paren, char arguments[3][WCL_MAX_INPUT],
    int *argument_count, const char **after_function) {
    const char *start = open_paren + 1;
    const char *cursor = start;
    int nesting = 1;
    bool quoted = false;
    *argument_count = 0;

    while (*cursor) {
        if (*cursor == '"') {
            if (quoted && cursor[1] == '"') {
                cursor += 2;
                continue;
            }
            quoted = !quoted;
            cursor++;
            continue;
        }
        if (!quoted && *cursor == '(') {
            nesting++;
            cursor++;
            continue;
        }
        if (!quoted && *cursor == ')') {
            if (--nesting == 0) {
                if (*argument_count >= 3) return false;
                size_t length = (size_t)(cursor - start);
                if (length >= WCL_MAX_INPUT) return false;
                memcpy(arguments[*argument_count], start, length);
                arguments[*argument_count][length] = '\0';
                if (length || *argument_count > 0) (*argument_count)++;
                *after_function = cursor + 1;
                return true;
            }
            cursor++;
            continue;
        }
        if (!quoted && *cursor == ',' && nesting == 1) {
            if (*argument_count >= 2) return false;
            size_t length = (size_t)(cursor - start);
            if (length >= WCL_MAX_INPUT) return false;
            memcpy(arguments[*argument_count], start, length);
            arguments[*argument_count][length] = '\0';
            (*argument_count)++;
            start = cursor + 1;
        }
        cursor++;
    }
    return false;
}

bool ExpandLexicalArgument(ExecutionContext *ctx, const char *source, char *value,
    size_t capacity, int depth) {
    while (isspace((unsigned char)*source)) source++;
    size_t length = strlen(source);
    while (length && isspace((unsigned char)source[length - 1])) length--;
    bool quoted = length >= 2 && source[0] == '"' && source[length - 1] == '"';
    if (quoted) {
        source++;
        length -= 2;
    }

    char unquoted[WCL_MAX_INPUT];
    size_t written = 0;
    for (size_t i = 0; i < length; ++i) {
        if (quoted && source[i] == '"' && i + 1 < length && source[i + 1] == '"') i++;
        if (written + 1 >= sizeof(unquoted)) return false;
        unquoted[written++] = source[i];
    }
    unquoted[written] = '\0';
    return ExpandSymbolsInternal(ctx, unquoted, value, capacity, depth + 1);
}

bool ApplyLexicalEdit(char *value, const char *edit_list) {
    char edits[WCL_MAX_INPUT];
    CopyString(edits, sizeof(edits), edit_list);
    char *cursor = edits;
    while (*cursor) {
        while (*cursor && (isspace((unsigned char)*cursor) || *cursor == ',')) cursor++;
        if (!*cursor) break;
        char *end = cursor;
        while (*end && !isspace((unsigned char)*end) && *end != ',') end++;
        char saved = *end;
        *end = '\0';

        if (_stricmp(cursor, "UPCASE") == 0 || _stricmp(cursor, "UPPERCASE") == 0) {
            for (char *p = value; *p; ++p) *p = (char)toupper((unsigned char)*p);
        } else if (_stricmp(cursor, "LOWERCASE") == 0) {
            for (char *p = value; *p; ++p) *p = (char)tolower((unsigned char)*p);
        } else if (_stricmp(cursor, "TRIM") == 0) {
            char *first = value;
            while (*first && isspace((unsigned char)*first)) first++;
            if (first != value) memmove(value, first, strlen(first) + 1);
            size_t length = strlen(value);
            while (length && isspace((unsigned char)value[length - 1])) value[--length] = '\0';
        } else if (_stricmp(cursor, "COLLAPSE") == 0) {
            char *read = value;
            char *write = value;
            while (*read) {
                if (!isspace((unsigned char)*read)) *write++ = *read;
                read++;
            }
            *write = '\0';
        } else if (_stricmp(cursor, "COMPRESS") == 0) {
            char *read = value;
            char *write = value;
            bool previous_space = false;
            while (*read) {
                bool space = isspace((unsigned char)*read) != 0;
                if (!space || !previous_space) *write++ = space ? ' ' : *read;
                previous_space = space;
                read++;
            }
            *write = '\0';
        } else if (_stricmp(cursor, "UNCOMMENT") == 0) {
            char *comment = strchr(value, '!');
            if (comment) *comment = '\0';
        } else {
            return false;
        }

        *end = saved;
        cursor = end;
    }
    return true;
}

bool EvaluateLexicalEnvironment(ExecutionContext *ctx, const char *item,
    char *result, size_t capacity) {
    if (_stricmp(item, "DEFAULT") == 0) {
        DWORD length = GetCurrentDirectoryA((DWORD)capacity, result);
        return length > 0 && length < capacity;
    }
    if (_stricmp(item, "PROMPT") == 0) {
        CopyString(result, capacity, ctx->current_prompt);
        return strlen(ctx->current_prompt) < capacity;
    }
    if (_stricmp(item, "PROCEDURE") == 0) {
        const char *procedure = ctx->script_depth > 0 ?
            ctx->procedure_paths[ctx->script_depth - 1] : "";
        CopyString(result, capacity, procedure);
        return strlen(procedure) < capacity;
    }
    if (_stricmp(item, "VERIFY_PROCEDURE") == 0 ||
        _stricmp(item, "VERIFY_IMAGE") == 0) {
        int length = snprintf(result, capacity, "%d",
            _stricmp(item, "VERIFY_PROCEDURE") == 0 ?
                (ctx->verify_procedure ? 1 : 0) : (ctx->verify_image ? 1 : 0));
        return length >= 0 && (size_t)length < capacity;
    }
    if (_stricmp(item, "INTERACTIVE") == 0 || _stricmp(item, "JOBTYPE") == 0) {
        DWORD console_mode;
        bool interactive = ctx->hConsoleIn != INVALID_HANDLE_VALUE &&
            GetConsoleMode(ctx->hConsoleIn, &console_mode) != 0;
        CopyString(result, capacity, _stricmp(item, "JOBTYPE") == 0 ?
            (interactive ? "INTERACTIVE" : "BATCH") : (interactive ? "TRUE" : "FALSE"));
        return strlen(result) < capacity;
    }
    return false;
}

static bool EvaluateLexicalFunction(ExecutionContext *ctx, LexicalFunction function,
    char arguments[3][WCL_MAX_INPUT], int argument_count, char *result, size_t capacity, int depth) {
    char values[3][WCL_MAX_INPUT];
    for (int i = 0; i < argument_count; ++i) {
        if (!ExpandLexicalArgument(ctx, arguments[i], values[i], sizeof(values[i]), depth)) return false;
    }

    long long first;
    if (function == LEXICAL_VERIFY && argument_count <= 2) {
        bool previous = ctx->verify_procedure;
        for (int i = 0; i < argument_count; ++i) {
            if (!values[i][0]) continue;
            long long setting;
            if (!ParseLexicalInteger(values[i], &setting) || (setting != 0 && setting != 1)) return false;
            if (i == 0) ctx->verify_procedure = setting != 0;
            else ctx->verify_image = setting != 0;
        }
        int length = snprintf(result, capacity, "%d", previous ? 1 : 0);
        return length >= 0 && (size_t)length < capacity;
    }
    if (function == LEXICAL_LENGTH && argument_count == 1) {
        int length = snprintf(result, capacity, "%zu", strlen(values[0]));
        return length >= 0 && (size_t)length < capacity;
    }
    if (function == LEXICAL_EXTRACT && argument_count == 3) {
        long long start;
        long long count;
        if (!ParseLexicalInteger(values[0], &start) || !ParseLexicalInteger(values[1], &count) ||
            start < 0 || count < 0) return false;
        size_t value_length = strlen(values[2]);
        size_t offset = (unsigned long long)start > value_length ? value_length : (size_t)start;
        size_t available = value_length - offset;
        size_t result_length = (unsigned long long)count > available ? available : (size_t)count;
        if (result_length >= capacity) return false;
        memcpy(result, values[2] + offset, result_length);
        result[result_length] = '\0';
        return true;
    }
    if (function == LEXICAL_LOCATE && argument_count == 2) {
        const char *found = strstr(values[1], values[0]);
        size_t offset = found ? (size_t)(found - values[1]) : strlen(values[1]);
        int length = snprintf(result, capacity, "%zu", offset);
        return length >= 0 && (size_t)length < capacity;
    }
    if (function == LEXICAL_ELEMENT && argument_count == 3) {
        if (!ParseLexicalInteger(values[0], &first) || first < 0 || !values[1][0]) return false;
        const char *element = values[2];
        long long index = 0;
        while (index < first) {
            const char *delimiter = strstr(element, values[1]);
            if (!delimiter) {
                CopyString(result, capacity, values[1]);
                return strlen(values[1]) < capacity;
            }
            element = delimiter + strlen(values[1]);
            index++;
        }
        const char *delimiter = strstr(element, values[1]);
        size_t length = delimiter ? (size_t)(delimiter - element) : strlen(element);
        if (length >= capacity) return false;
        memcpy(result, element, length);
        result[length] = '\0';
        return true;
    }
    if (function == LEXICAL_EDIT && argument_count == 2) {
        if (strlen(values[0]) >= capacity) return false;
        CopyString(result, capacity, values[0]);
        return ApplyLexicalEdit(result, values[1]);
    }
    if (function == LEXICAL_SEARCH && (argument_count == 1 || argument_count == 2)) {
        long long stream_id = 0;
        if (argument_count == 2 &&
            (!ParseLexicalInteger(values[1], &stream_id) ||
             stream_id < 0 || stream_id >= WCL_MAX_SEARCH_STREAMS)) return false;
        return GetLexicalSearchResult(ctx, values[0], (int)stream_id, result, capacity);
    }
    if (function == LEXICAL_ENVIRONMENT && argument_count == 1) {
        return EvaluateLexicalEnvironment(ctx, values[0], result, capacity);
    }
    if (function == LEXICAL_INTEGER && argument_count == 1) {
        long long value;
        if (!EvaluateNumericExpression(values[0], &value) || value < INT_MIN || value > INT_MAX) return false;
        int length = snprintf(result, capacity, "%lld", value);
        return length >= 0 && (size_t)length < capacity;
    }
    if (function == LEXICAL_STRING && argument_count == 1) {
        long long value;
        if (IsQuotedLexicalArgument(arguments[0])) {
            if (strlen(values[0]) >= capacity) return false;
            CopyString(result, capacity, values[0]);
            return true;
        }
        if (EvaluateNumericExpression(values[0], &value)) {
            if (value < INT_MIN || value > INT_MAX) return false;
            int length = snprintf(result, capacity, "%lld", value);
            return length >= 0 && (size_t)length < capacity;
        }
        if (BeginsNumericExpression(values[0])) return false;
        if (strlen(values[0]) >= capacity) return false;
        CopyString(result, capacity, values[0]);
        return true;
    }
    if (function == LEXICAL_TIME) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };
        snprintf(result, capacity, "%02d-%s-%04d %02d:%02d:%02d.%02d",
            st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);
        return true;
    }
    if (function == LEXICAL_GETSYI && argument_count >= 1) {
        const char *item = values[0];
        if (_stricmp(item, "NODENAME") == 0 || _stricmp(item, "NODE") == 0) {
            char node[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
            DWORD len = sizeof(node);
            GetComputerNameA(node, &len);
            CopyString(result, capacity, node);
            return true;
        }
        if (_stricmp(item, "VERSION") == 0 || _stricmp(item, "OS") == 0) {
            CopyString(result, capacity, "Windows");
            return true;
        }
        if (_stricmp(item, "CPU_COUNT") == 0 || _stricmp(item, "PROCCNT") == 0) {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            snprintf(result, capacity, "%lu", (unsigned long)si.dwNumberOfProcessors);
            return true;
        }
        if (_stricmp(item, "TOTAL_PHYSICAL_MEM") == 0 || _stricmp(item, "MEM_MB") == 0) {
            MEMORYSTATUSEX mem;
            mem.dwLength = sizeof(mem);
            GlobalMemoryStatusEx(&mem);
            snprintf(result, capacity, "%llu", mem.ullTotalPhys / (1024 * 1024));
            return true;
        }
        if (_stricmp(item, "FREE_RAM_MB") == 0 || _stricmp(item, "AVAIL_MEM") == 0) {
            MEMORYSTATUSEX mem;
            mem.dwLength = sizeof(mem);
            GlobalMemoryStatusEx(&mem);
            snprintf(result, capacity, "%llu", mem.ullAvailPhys / (1024 * 1024));
            return true;
        }
        if (_stricmp(item, "UPTIME") == 0) {
            ULONGLONG up_sec = GetTickCount64() / 1000;
            unsigned long days = (unsigned long)(up_sec / 86400);
            unsigned long hours = (unsigned long)((up_sec % 86400) / 3600);
            unsigned long mins = (unsigned long)((up_sec % 3600) / 60);
            unsigned long secs = (unsigned long)(up_sec % 60);
            snprintf(result, capacity, "%lu-%02lu:%02lu:%02lu", days, hours, mins, secs);
            return true;
        }
        if (_stricmp(item, "ARCH") == 0 || _stricmp(item, "HW_MODEL") == 0) {
            #if defined(_M_AMD64) || defined(__x86_64__)
            CopyString(result, capacity, "x86_64");
            #elif defined(_M_ARM64) || defined(__aarch64__)
            CopyString(result, capacity, "ARM64");
            #else
            CopyString(result, capacity, "x86");
            #endif
            return true;
        }
        CopyString(result, capacity, "");
        return true;
    }
    if (function == LEXICAL_GETJPI && argument_count >= 1) {
        DWORD pid = GetCurrentProcessId();
        const char *item = values[0];
        if (argument_count >= 2) {
            item = values[1];
            if (values[0][0]) {
                long long parsed_pid = 0;
                if (ParseLexicalInteger(values[0], &parsed_pid) && parsed_pid > 0) {
                    pid = (DWORD)parsed_pid;
                }
            }
        }
        if (_stricmp(item, "PID") == 0) {
            snprintf(result, capacity, "%08lX", (unsigned long)pid);
            return true;
        }
        if (_stricmp(item, "USERNAME") == 0) {
            char user[64] = "unknown";
            DWORD ulen = sizeof(user);
            GetUserNameA(user, &ulen);
            CopyString(result, capacity, user);
            return true;
        }
        if (_stricmp(item, "IMAGENAME") == 0 || _stricmp(item, "IMAGE_NAME") == 0) {
            char img[MAX_PATH] = "wcl.exe";
            GetModuleFileNameA(NULL, img, sizeof(img));
            char *p = strrchr(img, '\\');
            CopyString(result, capacity, p ? p + 1 : img);
            return true;
        }
        if (_stricmp(item, "WSSIZE") == 0 || _stricmp(item, "WSPEAK") == 0) {
            PROCESS_MEMORY_COUNTERS pmc;
            pmc.cb = sizeof(pmc);
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProc) {
                if (K32GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                    unsigned long long bytes = (_stricmp(item, "WSPEAK") == 0) ?
                        pmc.PeakWorkingSetSize : pmc.WorkingSetSize;
                    snprintf(result, capacity, "%llu", bytes / 1024);
                }
                CloseHandle(hProc);
                return true;
            }
            CopyString(result, capacity, "0");
            return true;
        }
        if (_stricmp(item, "CPU_TIME") == 0) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                FILETIME create, exit, kernel, user;
                if (GetProcessTimes(hProc, &create, &exit, &kernel, &user)) {
                    ULARGE_INTEGER k, u;
                    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
                    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
                    snprintf(result, capacity, "%llu", (k.QuadPart + u.QuadPart) / 10000);
                    CloseHandle(hProc);
                    return true;
                }
                CloseHandle(hProc);
            }
            CopyString(result, capacity, "0");
            return true;
        }
        if (_stricmp(item, "MEM_BYTES") == 0) {
            PROCESS_MEMORY_COUNTERS pmc;
            pmc.cb = sizeof(pmc);
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProc) {
                if (K32GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                    snprintf(result, capacity, "%llu", (unsigned long long)pmc.WorkingSetSize);
                }
                CloseHandle(hProc);
                return true;
            }
            CopyString(result, capacity, "0");
            return true;
        }
        if (_stricmp(item, "HANDLES") == 0) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (hProc) {
                DWORD hCount = 0;
                if (GetProcessHandleCount(hProc, &hCount)) {
                    snprintf(result, capacity, "%lu", (unsigned long)hCount);
                    CloseHandle(hProc);
                    return true;
                }
                CloseHandle(hProc);
            }
            CopyString(result, capacity, "0");
            return true;
        }
        CopyString(result, capacity, "");
        return true;
    }
    if (function == LEXICAL_GETDVI && argument_count >= 2) {
        const char *dev = values[0];
        const char *item = values[1];
        char root[8] = "C:\\";
        if (dev[0] && isalpha((unsigned char)dev[0])) {
            root[0] = dev[0];
            root[1] = ':';
            root[2] = '\\';
            root[3] = '\0';
        }
        if (_stricmp(item, "FREE_MB") == 0 || _stricmp(item, "FREEBLOCKS") == 0) {
            ULARGE_INTEGER free_b, total_b, avail_b;
            if (GetDiskFreeSpaceExA(root, &avail_b, &total_b, &free_b)) {
                if (_stricmp(item, "FREEBLOCKS") == 0)
                    snprintf(result, capacity, "%llu", free_b.QuadPart / 512);
                else
                    snprintf(result, capacity, "%llu", free_b.QuadPart / (1024 * 1024));
                return true;
            }
            CopyString(result, capacity, "0");
            return true;
        }
        if (_stricmp(item, "TOTAL_MB") == 0 || _stricmp(item, "MAXBLOCK") == 0) {
            ULARGE_INTEGER free_b, total_b, avail_b;
            if (GetDiskFreeSpaceExA(root, &avail_b, &total_b, &free_b)) {
                if (_stricmp(item, "MAXBLOCK") == 0)
                    snprintf(result, capacity, "%llu", total_b.QuadPart / 512);
                else
                    snprintf(result, capacity, "%llu", total_b.QuadPart / (1024 * 1024));
                return true;
            }
            CopyString(result, capacity, "0");
            return true;
        }
        if (_stricmp(item, "FS_TYPE") == 0) {
            char fs[32] = "NTFS";
            GetVolumeInformationA(root, NULL, 0, NULL, NULL, NULL, fs, sizeof(fs));
            CopyString(result, capacity, fs);
            return true;
        }
        if (_stricmp(item, "MOUNTED") == 0) {
            UINT dt = GetDriveTypeA(root);
            CopyString(result, capacity, (dt != DRIVE_NO_ROOT_DIR && dt != DRIVE_UNKNOWN) ? "TRUE" : "FALSE");
            return true;
        }
        CopyString(result, capacity, "");
        return true;
    }
    if (function == LEXICAL_FILE_ATTRIBUTES && argument_count >= 2) {
        const char *filepath = values[0];
        const char *item = values[1];
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExA(filepath, GetFileExInfoStandard, &fad)) {
            if (_stricmp(item, "EOF") == 0 || _stricmp(item, "SIZE") == 0) {
                ULARGE_INTEGER sz;
                sz.LowPart = fad.nFileSizeLow;
                sz.HighPart = fad.nFileSizeHigh;
                snprintf(result, capacity, "%llu", sz.QuadPart);
                return true;
            }
            if (_stricmp(item, "SIZE_KB") == 0) {
                ULARGE_INTEGER sz;
                sz.LowPart = fad.nFileSizeLow;
                sz.HighPart = fad.nFileSizeHigh;
                snprintf(result, capacity, "%llu", sz.QuadPart / 1024);
                return true;
            }
            if (_stricmp(item, "DIRECTORY") == 0) {
                CopyString(result, capacity, (fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "TRUE" : "FALSE");
                return true;
            }
            if (_stricmp(item, "READONLY") == 0) {
                CopyString(result, capacity, (fad.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? "TRUE" : "FALSE");
                return true;
            }
        }
        CopyString(result, capacity, "");
        return true;
    }
    if (function == LEXICAL_PARSE && argument_count >= 1) {
        const char *filepath = values[0];
        const char *field = (argument_count >= 2 && values[1][0]) ? values[1] : "";
        char full[MAX_PATH];
        if (!GetFullPathNameA(filepath, sizeof(full), full, NULL)) CopyString(full, sizeof(full), filepath);
        if (!field[0]) {
            CopyString(result, capacity, full);
            return true;
        }
        if (_stricmp(field, "DEVICE") == 0) {
            if (full[0] && full[1] == ':') {
                snprintf(result, capacity, "%c:", full[0]);
            } else {
                CopyString(result, capacity, "");
            }
            return true;
        }
        if (_stricmp(field, "DIRECTORY") == 0) {
            char *last_slash = strrchr(full, '\\');
            if (last_slash) {
                size_t len = (size_t)(last_slash - full + 1);
                if (len >= capacity) len = capacity - 1;
                memcpy(result, full, len);
                result[len] = '\0';
            } else {
                CopyString(result, capacity, "");
            }
            return true;
        }
        if (_stricmp(field, "NAME") == 0) {
            char *last_slash = strrchr(full, '\\');
            const char *name_part = last_slash ? last_slash + 1 : full;
            char *dot = strrchr(name_part, '.');
            size_t len = dot ? (size_t)(dot - name_part) : strlen(name_part);
            if (len >= capacity) len = capacity - 1;
            memcpy(result, name_part, len);
            result[len] = '\0';
            return true;
        }
        if (_stricmp(field, "TYPE") == 0) {
            char *dot = strrchr(full, '.');
            CopyString(result, capacity, dot ? dot : "");
            return true;
        }
        CopyString(result, capacity, full);
        return true;
    }
    if (function == LEXICAL_TRNLNM && argument_count >= 1) {
        const char *logname = values[0];
        for (int i = 0; i < ctx->logical_count; ++i) {
            if (_stricmp(logname, ctx->logical_table[i].logical) == 0) {
                CopyString(result, capacity, ctx->logical_table[i].equivalence);
                return true;
            }
        }
        char env[WCL_MAX_INPUT];
        DWORD elen = GetEnvironmentVariableA(logname, env, sizeof(env));
        if (elen > 0 && elen < sizeof(env)) {
            CopyString(result, capacity, env);
            return true;
        }
        CopyString(result, capacity, "");
        return true;
    }
    if (function == LEXICAL_PRIVILEGE && argument_count >= 1) {
        const char *priv = values[0];
        if (_stricmp(priv, "ELEVATED") == 0 || _stricmp(priv, "ADMIN") == 0 ||
            _stricmp(priv, "SETPRV") == 0 || _stricmp(priv, "SYSPRV") == 0) {
            BOOL is_admin = FALSE;
            PSID admin_group = NULL;
            SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;
            if (AllocateAndInitializeSid(&nt_auth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_group)) {
                CheckTokenMembership(NULL, admin_group, &is_admin);
                FreeSid(admin_group);
            }
            CopyString(result, capacity, is_admin ? "TRUE" : "FALSE");
            return true;
        }
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            LUID luid;
            char priv_name[128];
            snprintf(priv_name, sizeof(priv_name), "%s", priv);
            if (_stricmp(priv, "DEBUG") == 0) CopyString(priv_name, sizeof(priv_name), SE_DEBUG_NAME);
            else if (_stricmp(priv, "SHUTDOWN") == 0) CopyString(priv_name, sizeof(priv_name), SE_SHUTDOWN_NAME);
            else if (_stricmp(priv, "BACKUP") == 0) CopyString(priv_name, sizeof(priv_name), SE_BACKUP_NAME);
            else if (_stricmp(priv, "RESTORE") == 0) CopyString(priv_name, sizeof(priv_name), SE_RESTORE_NAME);
            else if (_stricmp(priv, "SECURITY") == 0) CopyString(priv_name, sizeof(priv_name), SE_SECURITY_NAME);

            if (LookupPrivilegeValueA(NULL, priv_name, &luid)) {
                PRIVILEGE_SET privs = {0};
                privs.PrivilegeCount = 1;
                privs.Control = PRIVILEGE_SET_ALL_NECESSARY;
                privs.Privilege[0].Luid = luid;
                BOOL result_priv = FALSE;
                PrivilegeCheck(hToken, &privs, &result_priv);
                CloseHandle(hToken);
                CopyString(result, capacity, result_priv ? "TRUE" : "FALSE");
                return true;
            }
            CloseHandle(hToken);
        }
        CopyString(result, capacity, "FALSE");
        return true;
    }
    if (function == LEXICAL_REGISTRY && argument_count >= 1) {
        const char *full_key = values[0];
        const char *val_name = (argument_count >= 2) ? values[1] : "";
        const char *def_val = (argument_count >= 3) ? values[2] : "";

        HKEY hRoot = HKEY_CURRENT_USER;
        const char *subkey = full_key;
        if (_strnicmp(full_key, "HKLM\\", 5) == 0) { hRoot = HKEY_LOCAL_MACHINE; subkey = full_key + 5; }
        else if (_strnicmp(full_key, "HKEY_LOCAL_MACHINE\\", 19) == 0) { hRoot = HKEY_LOCAL_MACHINE; subkey = full_key + 19; }
        else if (_strnicmp(full_key, "HKCU\\", 5) == 0) { hRoot = HKEY_CURRENT_USER; subkey = full_key + 5; }
        else if (_strnicmp(full_key, "HKEY_CURRENT_USER\\", 18) == 0) { hRoot = HKEY_CURRENT_USER; subkey = full_key + 18; }
        else if (_strnicmp(full_key, "HKCR\\", 5) == 0) { hRoot = HKEY_CLASSES_ROOT; subkey = full_key + 5; }
        else if (_strnicmp(full_key, "HKEY_CLASSES_ROOT\\", 18) == 0) { hRoot = HKEY_CLASSES_ROOT; subkey = full_key + 18; }
        else if (_strnicmp(full_key, "HKU\\", 4) == 0) { hRoot = HKEY_USERS; subkey = full_key + 4; }
        else if (_strnicmp(full_key, "HKEY_USERS\\", 11) == 0) { hRoot = HKEY_USERS; subkey = full_key + 11; }
        else if (_strnicmp(full_key, "HKCC\\", 5) == 0) { hRoot = HKEY_CURRENT_CONFIG; subkey = full_key + 5; }
        else if (_strnicmp(full_key, "HKEY_CURRENT_CONFIG\\", 20) == 0) { hRoot = HKEY_CURRENT_CONFIG; subkey = full_key + 20; }

        HKEY hKey = NULL;
        if (RegOpenKeyExA(hRoot, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD type = 0;
            BYTE data[4096];
            DWORD size = sizeof(data) - 1;
            LONG qres = RegQueryValueExA(hKey, val_name, NULL, &type, data, &size);
            RegCloseKey(hKey);
            if (qres == ERROR_SUCCESS) {
                if (type == REG_SZ || type == REG_EXPAND_SZ) {
                    data[size] = '\0';
                    CopyString(result, capacity, (const char*)data);
                    return true;
                } else if (type == REG_DWORD && size >= sizeof(DWORD)) {
                    DWORD dw = *(DWORD*)data;
                    snprintf(result, capacity, "%lu", (unsigned long)dw);
                    return true;
                } else if (type == REG_QWORD && size >= sizeof(unsigned long long)) {
                    unsigned long long qw = *(unsigned long long*)data;
                    snprintf(result, capacity, "%llu", qw);
                    return true;
                } else if (type == REG_MULTI_SZ) {
                    data[size] = '\0';
                    CopyString(result, capacity, (const char*)data);
                    return true;
                }
            }
        }
        CopyString(result, capacity, def_val);
        return true;
    }
    return false;
}

bool ExpandSymbolsInternal(ExecutionContext *ctx, const char *source, char *dest,
    size_t capacity, int depth) {
    if (depth > 16 || capacity == 0) return false;
    size_t written = 0;
    bool in_double_quotes = false;
    for (size_t i = 0; source[i];) {
        if ((source[i] == 'F' || source[i] == 'f') && source[i + 1] == '$') {
            size_t name_start = i + 2;
            size_t name_end = name_start;
            while (isalpha((unsigned char)source[name_end])) name_end++;
            char name[32];
            size_t name_length = name_end - name_start;
            if (name_length >= sizeof(name)) return false;
            memcpy(name, source + name_start, name_length);
            name[name_length] = '\0';

            LexicalFunction function;
            bool recognized = true;
            if (_stricmp(name, "LENGTH") == 0) function = LEXICAL_LENGTH;
            else if (_stricmp(name, "EXTRACT") == 0) function = LEXICAL_EXTRACT;
            else if (_stricmp(name, "LOCATE") == 0) function = LEXICAL_LOCATE;
            else if (_stricmp(name, "ELEMENT") == 0) function = LEXICAL_ELEMENT;
            else if (_stricmp(name, "EDIT") == 0) function = LEXICAL_EDIT;
            else if (_stricmp(name, "SEARCH") == 0) function = LEXICAL_SEARCH;
            else if (_stricmp(name, "ENVIRONMENT") == 0) function = LEXICAL_ENVIRONMENT;
            else if (_stricmp(name, "INTEGER") == 0) function = LEXICAL_INTEGER;
            else if (_stricmp(name, "STRING") == 0) function = LEXICAL_STRING;
            else if (_stricmp(name, "VERIFY") == 0) function = LEXICAL_VERIFY;
            else if (_stricmp(name, "TIME") == 0) function = LEXICAL_TIME;
            else if (_stricmp(name, "GETSYI") == 0) function = LEXICAL_GETSYI;
            else if (_stricmp(name, "GETJPI") == 0) function = LEXICAL_GETJPI;
            else if (_stricmp(name, "GETDVI") == 0) function = LEXICAL_GETDVI;
            else if (_stricmp(name, "FILE_ATTRIBUTES") == 0) function = LEXICAL_FILE_ATTRIBUTES;
            else if (_stricmp(name, "PARSE") == 0) function = LEXICAL_PARSE;
            else if (_stricmp(name, "TRNLNM") == 0) function = LEXICAL_TRNLNM;
            else if (_stricmp(name, "PRIVILEGE") == 0) function = LEXICAL_PRIVILEGE;
            else if (_stricmp(name, "REGISTRY") == 0) function = LEXICAL_REGISTRY;
            else recognized = false;

            if (recognized) {
                const char *open = source + name_end;
                while (isspace((unsigned char)*open)) open++;
                if (*open != '(') return false;
                char arguments[3][WCL_MAX_INPUT];
                int argument_count;
                const char *after_function;
                char value[WCL_MAX_INPUT];
                if (!ParseLexicalArguments(open, arguments, &argument_count, &after_function) ||
                    !EvaluateLexicalFunction(ctx, function, arguments, argument_count, value, sizeof(value), depth)) {
                    return false;
                }
                size_t value_length = strlen(value);
                if (value_length >= capacity - written) return false;
                memcpy(dest + written, value, value_length);
                written += value_length;
                i = (size_t)(after_function - source);
                continue;
            }
        }

        if (source[i] == '\'') {
            bool recursive_substitution = !in_double_quotes;
            size_t name_start = i + 1;
            if (in_double_quotes) {
                if (source[i + 1] != '\'') {
                    if (written + 1 >= capacity) return false;
                    dest[written++] = source[i++];
                    continue;
                }
                name_start++;
            }
            const char *closing_quote = strchr(source + name_start, '\'');
            if (!closing_quote || closing_quote == source + name_start) return false;
            size_t name_length = (size_t)(closing_quote - (source + name_start));
            if (name_length >= WCL_MAX_INPUT) return false;
            char name[WCL_MAX_INPUT];
            char value[WCL_MAX_INPUT];
            memcpy(name, source + name_start, name_length);
            name[name_length] = '\0';

            if ((name[0] == 'F' || name[0] == 'f') && name[1] == '$') {
                if (!ExpandSymbolsInternal(ctx, name, value, sizeof(value), depth + 1)) return false;
            } else {
                const char *symbol = GetSymbol(ctx, name);
                if (!symbol) return false;
                if (recursive_substitution) {
                    if (!ExpandSymbolsInternal(ctx, symbol, value, sizeof(value), depth + 1)) return false;
                } else {
                    CopyString(value, sizeof(value), symbol);
                }
            }
            size_t value_length = strlen(value);
            if (value_length >= capacity - written) return false;
            memcpy(dest + written, value, value_length);
            written += value_length;
            i = (size_t)(closing_quote - source) + 1;
            continue;
        }

        if (source[i] != '&') {
            if (written + 1 >= capacity) return false;
            if (source[i] == '"') in_double_quotes = !in_double_quotes;
            dest[written++] = source[i++];
            continue;
        }
        if (source[i + 1] == '&') {
            if (written + 1 >= capacity) return false;
            dest[written++] = '&';
            i += 2;
            continue;
        }
        size_t start = i + 1;
        size_t end = start;
        while (IsSymbolNameChar((unsigned char)source[end])) end++;
        if (end == start) {
            if (written + 1 >= capacity) return false;
            dest[written++] = source[i++];
            continue;
        }
        size_t name_length = end - start;
        if (name_length >= 64) return false;
        char name[64];
        memcpy(name, source + start, name_length);
        name[name_length] = '\0';
        const char *value = GetSymbol(ctx, name);
        if (!value) return false;
        size_t value_length = strlen(value);
        if (value_length >= capacity - written) return false;
        memcpy(dest + written, value, value_length);
        written += value_length;
        i = end;
    }
    dest[written] = '\0';
    return true;
}

bool ExpandSymbols(ExecutionContext *ctx, const char *source, char *dest, size_t capacity) {
    return ExpandSymbolsInternal(ctx, source, dest, capacity, 0);
}

typedef struct NumericExpression {
    const char *cursor;
    bool valid;
} NumericExpression;

static void SkipNumericSpaces(NumericExpression *expression) {
    while (isspace((unsigned char)*expression->cursor)) expression->cursor++;
}

static bool ParseNumericExpression(NumericExpression *expression, long long *value);

static bool ParseNumericFactor(NumericExpression *expression, long long *value) {
    SkipNumericSpaces(expression);
    if (*expression->cursor == '+') {
        expression->cursor++;
        return ParseNumericFactor(expression, value);
    }
    if (*expression->cursor == '-') {
        expression->cursor++;
        if (!ParseNumericFactor(expression, value) || *value == LLONG_MIN) return false;
        *value = -*value;
        return true;
    }
    if (*expression->cursor == '(') {
        expression->cursor++;
        if (!ParseNumericExpression(expression, value)) return false;
        SkipNumericSpaces(expression);
        if (*expression->cursor != ')') return false;
        expression->cursor++;
        return true;
    }

    errno = 0;
    char *end = NULL;
    long long parsed = strtoll(expression->cursor, &end, 10);
    if (end == expression->cursor || errno == ERANGE) return false;
    expression->cursor = end;
    *value = parsed;
    return true;
}

static bool ParseNumericTerm(NumericExpression *expression, long long *value) {
    if (!ParseNumericFactor(expression, value)) return false;
    for (;;) {
        SkipNumericSpaces(expression);
        char op = *expression->cursor;
        if (op != '*' && op != '/' && op != '%') return true;
        expression->cursor++;
        long long right;
        if (!ParseNumericFactor(expression, &right)) return false;
        if (op == '*') {
            if (*value > 0) {
                if ((right > 0 && *value > LLONG_MAX / right) ||
                    (right < 0 && right < LLONG_MIN / *value)) return false;
            } else if (*value < 0) {
                if ((right > 0 && *value < LLONG_MIN / right) ||
                    (right < 0 && *value < LLONG_MAX / right)) return false;
            }
            *value *= right;
        } else {
            if (!right || (*value == LLONG_MIN && right == -1)) return false;
            if (op == '/') *value /= right;
            else *value %= right;
        }
    }
}

static bool ParseNumericExpression(NumericExpression *expression, long long *value) {
    if (!ParseNumericTerm(expression, value)) return false;
    for (;;) {
        SkipNumericSpaces(expression);
        char op = *expression->cursor;
        if (op != '+' && op != '-') return true;
        expression->cursor++;
        long long right;
        if (!ParseNumericTerm(expression, &right)) return false;
        if (op == '+') {
            if ((right > 0 && *value > LLONG_MAX - right) ||
                (right < 0 && *value < LLONG_MIN - right)) return false;
            *value += right;
        } else {
            if ((right < 0 && *value > LLONG_MAX + right) ||
                (right > 0 && *value < LLONG_MIN + right)) return false;
            *value -= right;
        }
    }
}

static bool EvaluateNumericExpression(const char *text, long long *value) {
    NumericExpression expression = { text, true };
    if (!ParseNumericExpression(&expression, value)) return false;
    SkipNumericSpaces(&expression);
    return *expression.cursor == '\0';
}

bool IsTopLevelStringFunction(const char *expression) {
    while (isspace((unsigned char)*expression)) expression++;
    if (_strnicmp(expression, "F$", 2) == 0) return true;
    return false;
}

bool TryAssignSymbol(ExecutionContext *ctx, const char *line, bool *handled) {
    *handled = false;
    const char *equals = NULL;
    bool quoted = false;
    for (const char *p = line; *p; ++p) {
        if (*p == '"') quoted = !quoted;
        else if (*p == '=' && !quoted) {
            equals = p;
            break;
        }
    }
    if (!equals) return true;

    /* Detect DCL assignment operators: :=, :==, ==, = */
    const char *op_start = equals;
    if (op_start > line && *(op_start - 1) == ':') op_start--;
    const char *op_end = equals + 1;
    if (*op_end == '=') op_end++;

    bool is_command_assign = (op_start < equals && *op_start == ':'); /* := or :== */

    const char *name_start = line;
    const char *name_end = op_start;
    while (name_start < name_end && isspace((unsigned char)*name_start)) name_start++;
    while (name_end > name_start && isspace((unsigned char)name_end[-1])) name_end--;
    size_t name_length = (size_t)(name_end - name_start);
    if (!name_length || name_length >= 64) return true;

    char name[64];
    memcpy(name, name_start, name_length);
    name[name_length] = '\0';
    if (!IsValidSymbolName(name)) return true;
    *handled = true;

    const char *value_start = op_end;
    while (*value_start && isspace((unsigned char)*value_start)) value_start++;
    const char *value_end = line + strlen(line);
    while (value_end > value_start && isspace((unsigned char)value_end[-1])) value_end--;
    bool quoted_value = value_end - value_start >= 2 &&
        value_start[0] == '"' && value_end[-1] == '"';
    if (quoted_value) {
        value_start++;
        value_end--;
    }
    size_t value_length = (size_t)(value_end - value_start);
    if (value_length >= WCL_MAX_INPUT) return false;
    char raw_value[WCL_MAX_INPUT];
    char expanded_value[WCL_MAX_INPUT];
    char numeric_value[WCL_MAX_INPUT];
    memcpy(raw_value, value_start, value_length);
    raw_value[value_length] = '\0';
    if (!ExpandSymbols(ctx, raw_value, expanded_value, sizeof(expanded_value))) return false;
    bool string_result = quoted_value || is_command_assign || IsTopLevelStringFunction(raw_value);
    if (!string_result && (isdigit((unsigned char)expanded_value[0]) ||
        expanded_value[0] == '-' || expanded_value[0] == '+' || expanded_value[0] == '(')) {
        long long number;
        if (!EvaluateNumericExpression(expanded_value, &number)) return false;
        snprintf(numeric_value, sizeof(numeric_value), "%lld", number);
        return SetSymbol(ctx, name, numeric_value);
    }
    return SetSymbol(ctx, name, expanded_value);
}

/* ----------------------------------------------------------------------------
 * Qualifier Parser Helper Methods
 * ---------------------------------------------------------------------------- */
