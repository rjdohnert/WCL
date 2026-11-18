#include "../include/wcl_internal.h"
#include "file_operations.h"
#include <conio.h>

typedef struct TypeCommand {
    struct ICommand base;
} TypeCommand;

int TypeFileContents(ExecutionContext *ctx, const char *path, FILE *output,
    unsigned long tail_count, bool paging, const char *page_mode) {
    FILE *input = NULL;
    bool is_stdin = (_stricmp(path, "SYS$INPUT") == 0 || strcmp(path, "-") == 0);
    if (is_stdin) {
        input = stdin;
    } else if (fopen_s(&input, path, "rb") != 0) {
        ctx->PrintStatus(ctx, 'E', "OPENIN", "Error opening file for read verification");
        return 1;
    }

    /* Paging mode */
    if (paging && output == stdout) {
        bool scroll = (page_mode && _stricmp(page_mode, "SCROLL") == 0);
        bool clear = !scroll;

        /* If reading from null device (NL: / NUL) */
        if (_stricmp(path, "NUL") == 0 || _stricmp(path, "NL:") == 0 || _stricmp(path, "NL") == 0) {
            if (!is_stdin) fclose(input);
            if (clear) ClearConsoleScreen(ctx);
            return 0;
        }

        size_t capacity = 32;
        size_t line_count = 0;
        char **lines = (char**)malloc(capacity * sizeof(*lines));
        if (!lines) {
            if (!is_stdin) fclose(input);
            ctx->PrintStatus(ctx, 'E', "TYPEMEM", "Unable to allocate TYPE/PAGE line storage");
            return 1;
        }

        char buffer[WCL_MAX_INPUT];
        while (fgets(buffer, sizeof(buffer), input)) {
            size_t length = strlen(buffer);
            while (length && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')) {
                buffer[--length] = '\0';
            }
            if (line_count == capacity) {
                size_t new_cap = capacity * 2;
                char **new_lines = (char**)realloc(lines, new_cap * sizeof(*new_lines));
                if (!new_lines) {
                    for (size_t i = 0; i < line_count; ++i) free(lines[i]);
                    free(lines);
                    if (!is_stdin) fclose(input);
                    ctx->PrintStatus(ctx, 'E', "TYPEMEM", "Unable to allocate TYPE/PAGE line storage");
                    return 1;
                }
                lines = new_lines;
                capacity = new_cap;
            }
            lines[line_count] = _strdup(buffer);
            if (!lines[line_count]) {
                for (size_t i = 0; i < line_count; ++i) free(lines[i]);
                free(lines);
                if (!is_stdin) fclose(input);
                ctx->PrintStatus(ctx, 'E', "TYPEMEM", "Unable to allocate TYPE/PAGE line storage");
                return 1;
            }
            line_count++;
        }
        if (!is_stdin) fclose(input);
        if (is_stdin) clearerr(stdin);

        size_t start_idx = 0;
        if (tail_count && tail_count < line_count) {
            start_idx = line_count - tail_count;
        }

        if (line_count == 0) {
            if (clear) ClearConsoleScreen(ctx);
            free(lines);
            return 0;
        }

        HANDLE hOut = ctx ? ctx->hConsoleOut : GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE hIn = ctx ? ctx->hConsoleIn : GetStdHandle(STD_INPUT_HANDLE);
        DWORD out_mode = 0, in_mode = 0;
        bool interactive = GetConsoleMode(hOut, &out_mode) && GetConsoleMode(hIn, &in_mode);

        if (!interactive) {
            if (clear) ClearConsoleScreen(ctx);
            for (size_t i = start_idx; i < line_count; ++i) {
                printf("%s\n", lines[i]);
            }
            for (size_t i = 0; i < line_count; ++i) free(lines[i]);
            free(lines);
            return 0;
        }

        CONSOLE_SCREEN_BUFFER_INFO screen;
        size_t page_height = 24;
        size_t screen_width = 80;
        if (GetConsoleScreenBufferInfo(hOut, &screen)) {
            page_height = screen.srWindow.Bottom - screen.srWindow.Top;
            if (page_height < 1) page_height = 1;
            screen_width = screen.srWindow.Right - screen.srWindow.Left + 1;
        }

        bool save_pages = (page_mode && (_stricmp(page_mode, "SAVE") == 0 || _strnicmp(page_mode, "SAVE=", 5) == 0));
        size_t saved_page_limit = 5;
        if (page_mode && _strnicmp(page_mode, "SAVE=", 5) == 0) {
            unsigned long req = strtoul(page_mode + 5, NULL, 10);
            if (req >= 1 && req <= 5) saved_page_limit = req;
        }
        size_t saved_page_starts[5] = { 0 };
        size_t saved_page_count = 0;
        size_t saved_page_index = 0;

        size_t cur = start_idx;
        while (cur < line_count) {
            if (save_pages && (!saved_page_count || saved_page_starts[saved_page_index] != cur)) {
                if (saved_page_index + 1 < saved_page_count) saved_page_count = saved_page_index + 1;
                if (saved_page_count == saved_page_limit) {
                    for (size_t i = 1; i < saved_page_count; ++i)
                        saved_page_starts[i - 1] = saved_page_starts[i];
                    --saved_page_count;
                    if (saved_page_index) --saved_page_index;
                }
                saved_page_starts[saved_page_count] = cur;
                saved_page_index = saved_page_count++;
            }
            if (clear) ClearConsoleScreen(ctx);
            size_t end = cur;
            size_t rows = 0;
            do {
                size_t line_len = strlen(lines[end]);
                size_t line_rows = (screen_width && line_len > screen_width) ?
                    (line_len + screen_width - 1) / screen_width : 1;
                if (rows && !scroll && rows + line_rows > page_height) break;
                rows += line_rows;
                ++end;
            } while (end < line_count && !scroll && rows < page_height);

            for (size_t i = cur; i < end; ++i) {
                printf("%s\n", lines[i]);
            }
            if (end >= line_count) break;

            printf(save_pages ?
                "\n-- More -- [Enter/Space: next, B: previous, Q: quit] " :
                "\n-- More -- [Enter/Space: next, Q: quit] ");
            fflush(stdout);

            int key = _getch();
            if (key == 0 || key == 224) key = _getch();
            printf("\r                                                      \r");
            fflush(stdout);

            if (key == 'q' || key == 'Q' || key == 27) break;
            if (save_pages && (key == 'b' || key == 'B' || key == 72 || key == 73)) {
                if (saved_page_index) cur = saved_page_starts[--saved_page_index];
                else cur = start_idx;
            } else if (save_pages && saved_page_index + 1 < saved_page_count) {
                cur = saved_page_starts[++saved_page_index];
            } else {
                cur = end;
            }
        }

        for (size_t i = 0; i < line_count; ++i) free(lines[i]);
        free(lines);
        return 0;
    }

    bool success = true;
    size_t total_bytes = 0;
    if (!tail_count) {
        char buffer[8192];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
            total_bytes += bytes_read;
            if (output && fwrite(buffer, 1, bytes_read, output) != bytes_read) {
                success = false;
                break;
            }
        }
        if (ferror(input)) success = false;
    } else {
        char **lines = (char**)calloc(tail_count, sizeof(*lines));
        if (!lines) {
            if (!is_stdin) fclose(input);
            else clearerr(stdin);
            ctx->PrintStatus(ctx, 'E', "TYPEMEM", "Unable to allocate TYPE/TAIL line storage");
            return 1;
        }
        char line[WCL_MAX_INPUT];
        unsigned long line_count = 0;
        while (fgets(line, sizeof(line), input)) {
            size_t length = strlen(line);
            total_bytes += length;
            char *copy = (char*)malloc(length + 1);
            if (!copy) {
                success = false;
                break;
            }
            memcpy(copy, line, length + 1);
            unsigned long slot = line_count % tail_count;
            free(lines[slot]);
            lines[slot] = copy;
            ++line_count;
        }
        if (ferror(input)) success = false;
        unsigned long first = line_count > tail_count ? line_count - tail_count : 0;
        for (unsigned long i = first; success && output && i < line_count; ++i) {
            const char *line_text = lines[i % tail_count];
            if (line_text) {
                size_t length = strlen(line_text);
                bool terminated = length && line_text[length - 1] == '\n';
                while (length && (line_text[length - 1] == '\n' ||
                    line_text[length - 1] == '\r')) {
                    --length;
                }
                if ((length && fwrite(line_text, 1, length, output) != length) ||
                    (terminated && fputc('\n', output) == EOF)) {
                    success = false;
                }
            }
        }
        for (unsigned long i = 0; i < tail_count; ++i) free(lines[i]);
        free(lines);
    }
    if (!is_stdin && fclose(input) != 0) success = false;
    if (is_stdin) clearerr(stdin);
    if (success && output == stdout && total_bytes > 0 && fputc('\n', output) == EOF) success = false;
    if (!success) {
        ctx->PrintStatus(ctx, 'E', "TYPEFAIL", "Unable to read the input or write TYPE output");
        return 1;
    }
    return 0;
}

int TypeCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "MISSINGARG", "File specification required");
        return 1;
    }

    char path[MAX_PATH];
    if (_stricmp(argv[1], "SYS$INPUT") == 0 || strcmp(argv[1], "-") == 0) {
        CopyString(path, sizeof(path), "SYS$INPUT");
    } else if (!ResolveFileSpecification(ctx, argv[1], false, path, sizeof(path))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }

    const Qualifier *output_qualifier = NULL;
    const Qualifier *tail_qualifier = NULL;
    bool no_output = quals->HasQualifier(quals, "NOOUTPUT");
    bool paging = quals->HasQualifier(quals, "PAGE");
    const char *page_mode = GetQualifierValueByPrefix(quals, "PAGE");
    for (int i = 0; i < quals->count; ++i) {
        if (MatchesPrefix(quals->items[i].name, "OUTPUT")) output_qualifier = &quals->items[i];
        if (MatchesPrefix(quals->items[i].name, "TAIL")) tail_qualifier = &quals->items[i];
    }
    unsigned long tail_count = 0;
    if (tail_qualifier) {
        tail_count = 10;
        if (tail_qualifier->has_value &&
            (!IsNumericVersion(tail_qualifier->value, &tail_count) ||
             !tail_count || tail_count > 10000)) {
            ctx->PrintStatus(ctx, 'E', "BADTAIL", "/TAIL requires a line count from 1 through 10000");
            return 1;
        }
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        if (output_qualifier && output_qualifier->has_value &&
            output_qualifier->value[0] && !no_output) {
            if (HasFileWildcard(path)) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "TYPE/OUTPUT with wildcard input is not supported by this Windows mapping");
                return 1;
            }
            char output_path[MAX_PATH];
            char input_full[MAX_PATH];
            char output_full[MAX_PATH];
            if (!ResolveFileSpecification(ctx, output_qualifier->value, true,
                output_path, sizeof(output_path)) ||
                !GetFullPathNameA(path, sizeof(input_full), input_full, NULL) ||
                !GetFullPathNameA(output_path, sizeof(output_full), output_full, NULL)) {
                ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "TYPE output specification is too long");
                return 1;
            }
            if (_stricmp(input_full, output_full) == 0) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL", "TYPE/OUTPUT cannot overwrite its input file");
                return 1;
            }
            char target[WCL_MAX_INPUT];
            int length = snprintf(target, sizeof(target), "%s -> %s", path, output_path);
            ReportDryRun(ctx, "type to output", length >= 0 &&
                (size_t)length < sizeof(target) ? target : output_path);
        } else {
            ReportDryRun(ctx, "type file", path);
        }
        return 0;
    }

    FILE *output = no_output ? NULL : stdout;
    bool close_output = false;
    if (output_qualifier && output_qualifier->has_value && output_qualifier->value[0] && !no_output) {
        if (HasFileWildcard(path)) {
            ctx->PrintStatus(ctx, 'E', "IVQUAL", "TYPE/OUTPUT with wildcard input is not supported by this Windows mapping");
            return 1;
        }
        char output_path[MAX_PATH];
        char input_full[MAX_PATH];
        char output_full[MAX_PATH];
        if (!ResolveFileSpecification(ctx, output_qualifier->value, true, output_path, sizeof(output_path)) ||
            !GetFullPathNameA(path, sizeof(input_full), input_full, NULL) ||
            !GetFullPathNameA(output_path, sizeof(output_full), output_full, NULL)) {
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "TYPE output specification is too long");
            return 1;
        }
        if (_stricmp(input_full, output_full) == 0) {
            ctx->PrintStatus(ctx, 'E', "IVQUAL", "TYPE/OUTPUT cannot overwrite its input file");
            return 1;
        }
        if (quals->HasQualifier(quals, "CONFIRM")) {
            int confirmed = ConfirmMutation(ctx, "Write TYPE output", output_path);
            if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        }
        if (fopen_s(&output, output_path, "wb") != 0 || !output) {
            ctx->PrintStatus(ctx, 'E', "TYPEOUTPUT", "Unable to open TYPE output file");
            return 1;
        }
        close_output = true;
    } else if (quals->HasQualifier(quals, "CONFIRM") && !HasFileWildcard(path)) {
        int confirmed = ConfirmMutation(ctx, "Type", path);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    if (!HasFileWildcard(path)) {
        int result = TypeFileContents(ctx, path, output, tail_count, paging, page_mode);
        if (close_output && fclose(output) != 0) result = 1;
        return result;
    }

    WIN32_FIND_DATAA data;
    HANDLE search = FindFirstFileA(path, &data);
    if (search == INVALID_HANDLE_VALUE) {
        ctx->PrintStatus(ctx, 'E', "NOFILES", "No files matched the TYPE specification");
        return 1;
    }
    int matched = 0;
    bool failed = false;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        char matched_path[MAX_PATH];
        if (!BuildMatchedFilePath(path, data.cFileName, matched_path, sizeof(matched_path))) {
            failed = true;
            continue;
        }
        ++matched;
        if (quals->HasQualifier(quals, "CONFIRM")) {
            int confirmed = ConfirmMutation(ctx, "Type", matched_path);
            if (confirmed < 0) {
                failed = true;
                break;
            }
            if (confirmed == 0) continue;
        }
        if (TypeFileContents(ctx, matched_path, output, tail_count, paging, page_mode) != 0) {
            failed = true;
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);
    if (close_output && fclose(output) != 0) failed = true;
    if (!matched) {
        ctx->PrintStatus(ctx, 'W', "NOFILES", "No files matched the TYPE specification");
        return 1;
    }
    return failed ? 1 : 0;
}

const char* TypeCmd_GetVerb(ICommand *self) { (void)self; return "TYPE"; }
const char* TypeCmd_GetSummary(ICommand *self) { (void)self; return "Displays the contents of an ASCII text file."; }
const char* TypeCmd_GetHelp(ICommand *self) {
    (void)self;
    return "TYPE file-spec [/OUTPUT[=filespec] | /NOOUTPUT] [/TAIL[=count]] [/CONFIRM] [/PAGE[=mode]] [/NOPAGE]\n"
           "  Displays the latest version of an exact file name; wildcard specifications display every matching file version.\n"
           "  /OUTPUT=filespec redirects one exact input file to a Windows file; /OUTPUT alone selects the console;\n"
           "  /NOOUTPUT suppresses displayed bytes. WCL extension /TAIL shows the last 10 lines by default or the requested count.\n"
           "  /PAGE displays output one screen at a time (supports CLEAR_SCREEN, SCROLL, or SAVE[=n]); /PAGE NL: clears the screen.\n"
           "  /CONFIRM prompts before each type operation; noninteractive requests fail closed.\n"
           "  WCL /DRYRUN previews without reading or writing. DCL file-date, RMS-record, and protection qualifiers\n"
           "  have no direct Windows mapping and are rejected.";
}

static const ICommandVTable TypeCmd_VTable = {
    { NULL, NULL },
    TypeCmd_Execute,
    TypeCmd_GetVerb,
    TypeCmd_GetSummary,
    TypeCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: COPY
 * ---------------------------------------------------------------------------- */
typedef struct CopyCommand {
    struct ICommand base;
} CopyCommand;

int CopyFileToDirectory(ExecutionContext *ctx, const char *source, const char *directory,
    bool log_operation) {
    const char *file_name = source;
    for (const char *p = source; *p; ++p) {
        if (*p == '\\' || *p == '/') file_name = p + 1;
    }
    char base_name[MAX_PATH];
    CopyString(base_name, sizeof(base_name), file_name);
    char *version_separator = strrchr(base_name, ';');
    unsigned long ignored_version;
    if (version_separator && IsNumericVersion(version_separator + 1, &ignored_version)) {
        *version_separator = '\0';
    }

    char destination[MAX_PATH];
    CopyString(destination, sizeof(destination), directory);
    size_t length = strlen(destination);
    if (length && destination[length - 1] != '\\' && destination[length - 1] != '/' &&
        !AppendString(destination, sizeof(destination), "\\")) return 1;
    if (!AppendString(destination, sizeof(destination), base_name)) return 1;

    char versioned_destination[MAX_PATH];
    if (!ResolveFileSpecification(ctx, destination, true, versioned_destination, sizeof(versioned_destination)) ||
        !CopyFileA(source, versioned_destination, TRUE)) {
        ctx->PrintStatus(ctx, 'E', "COPYFAIL", "File could not be copied to the destination directory");
        return 1;
    }
    if (log_operation) printf("%s copied to %s\n", source, versioned_destination);
    return 0;
}

int CopyCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Missing source or target parameter for COPY");
        return 1;
    }

    if (_strnicmp(argv[1], "http://", 7) == 0 ||
        _strnicmp(argv[1], "https://", 8) == 0 ||
        _strnicmp(argv[1], "ftp://", 6) == 0) {
        const char *url = argv[1];
        char dst_path[MAX_PATH];
        if (!ResolveFileSpecification(ctx, argv[2], false, dst_path, sizeof(dst_path))) {
            CopyString(dst_path, sizeof(dst_path), argv[2]);
        }

        DWORD attr = GetFileAttributesA(dst_path);
        if ((attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) ||
            dst_path[strlen(dst_path) - 1] == '\\' || dst_path[strlen(dst_path) - 1] == '/' ||
            strcmp(dst_path, ".") == 0) {
            const char *last_slash = strrchr(url, '/');
            const char *fname = (last_slash && *(last_slash + 1)) ? last_slash + 1 : "download.dat";
            char clean_fname[MAX_PATH];
            CopyString(clean_fname, sizeof(clean_fname), fname);
            char *q = strchr(clean_fname, '?');
            if (q) *q = '\0';
            char combined[MAX_PATH];
            snprintf(combined, sizeof(combined), "%s\\%s", dst_path, clean_fname);
            CopyString(dst_path, sizeof(dst_path), combined);
        }

        if (quals->HasQualifier(quals, "DRYRUN")) {
            char target[WCL_MAX_INPUT];
            snprintf(target, sizeof(target), "%s -> %s", url, dst_path);
            ReportDryRun(ctx, "download", target);
            return 0;
        }

        if (quals->HasQualifier(quals, "CONFIRM")) {
            int confirmed = ConfirmMutation(ctx, "Download", url);
            if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        }

        bool downloaded = false;
        HMODULE hUrlmon = LoadLibraryA("urlmon.dll");
        if (hUrlmon) {
            typedef HRESULT (WINAPI *URLDownloadToFileAFn)(LPUNKNOWN, LPCSTR, LPCSTR, DWORD, LPVOID);
            URLDownloadToFileAFn pfn = (URLDownloadToFileAFn)GetProcAddress(hUrlmon, "URLDownloadToFileA");
            if (pfn) {
                HRESULT hr = pfn(NULL, url, dst_path, 0, NULL);
                if (SUCCEEDED(hr)) downloaded = true;
            }
            FreeLibrary(hUrlmon);
        }

        if (!downloaded) {
            char safe_url[WCL_MAX_INPUT];
            char safe_dst[MAX_PATH * 2];
            size_t u = 0, d = 0;
            for (const char *p = url; *p && u + 2 < sizeof(safe_url); ++p) {
                if (*p == '\'') { safe_url[u++] = '\''; safe_url[u++] = '\''; }
                else if (*p != '"' && *p != '`' && *p != '\r' && *p != '\n') safe_url[u++] = *p;
            }
            safe_url[u] = '\0';
            for (const char *p = dst_path; *p && d + 2 < sizeof(safe_dst); ++p) {
                if (*p == '\'') { safe_dst[d++] = '\''; safe_dst[d++] = '\''; }
                else if (*p != '"' && *p != '`' && *p != '\r' && *p != '\n') safe_dst[d++] = *p;
            }
            safe_dst[d] = '\0';

            char dl_cmd[WCL_MAX_INPUT * 2];
            snprintf(dl_cmd, sizeof(dl_cmd),
                "powershell.exe -NoProfile -NonInteractive -Command \"try { Invoke-WebRequest -Uri '%s' -OutFile '%s' -UseBasicParsing; exit 0 } catch { exit 1 }\"",
                safe_url, safe_dst);
            STARTUPINFOA si = { sizeof(si) };
            PROCESS_INFORMATION pi;
            if (CreateProcessA(NULL, dl_cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, 30000);
                DWORD code = 1;
                GetExitCodeProcess(pi.hProcess, &code);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                if (code == 0) downloaded = true;
            }
        }

        if (downloaded) {
            if (quals->HasQualifier(quals, "LOG")) {
                printf("%%COPY-S-COPIED, %s -> %s\n", url, dst_path);
            }
            return 0;
        } else {
            ctx->PrintStatus(ctx, 'E', "NETDOWNLOAD", "Failed to download remote network URL resource");
            return 1;
        }
    }

    char src[MAX_PATH];
    char dst[MAX_PATH];
    if (!ResolveFileSpecification(ctx, argv[1], false, src, sizeof(src)) ||
        !ExpandLogicalName(ctx, argv[2], dst, sizeof(dst))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        char target[WCL_MAX_INPUT];
        int length = snprintf(target, sizeof(target), "%s -> %s", src, dst);
        ReportDryRun(ctx, "copy", length >= 0 && (size_t)length < sizeof(target) ? target : "files");
        return 0;
    }
    DWORD destination_attributes = GetFileAttributesA(dst);
    if (destination_attributes != INVALID_FILE_ATTRIBUTES &&
        (destination_attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        int copied = 0;
        bool failed = false;
        bool cancelled = false;
        if (HasFileWildcard(src)) {
            WIN32_FIND_DATAA data;
            HANDLE search = FindFirstFileA(src, &data);
            if (search == INVALID_HANDLE_VALUE) {
                ctx->PrintStatus(ctx, 'E', "NOFILES", "No files matched the COPY source specification");
                return 1;
            }
            do {
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                char matched_source[MAX_PATH];
                if (!BuildMatchedFilePath(src, data.cFileName, matched_source, sizeof(matched_source))) {
                    failed = true;
                    continue;
                }
                if (quals->HasQualifier(quals, "CONFIRM")) {
                    int confirmed = ConfirmMutation(ctx, "Copy", matched_source);
                    if (confirmed < 0) {
                        failed = true;
                        break;
                    }
                    if (confirmed == 0) {
                        cancelled = true;
                        continue;
                    }
                }
                if (CopyFileToDirectory(ctx, matched_source, dst,
                        quals->HasQualifier(quals, "LOG")) != 0) {
                    failed = true;
                } else {
                    ++copied;
                }
            } while (FindNextFileA(search, &data));
            FindClose(search);
        } else {
            if (quals->HasQualifier(quals, "CONFIRM")) {
                int confirmed = ConfirmMutation(ctx, "Copy", src);
                if (confirmed < 0) return 1;
                if (confirmed == 0) return 0;
            }
            if (ResolveFileSpecification(ctx, src, false, src, sizeof(src)) &&
                CopyFileToDirectory(ctx, src, dst, quals->HasQualifier(quals, "LOG")) == 0) {
                copied = 1;
            } else {
                failed = true;
            }
        }
        if (!copied) {
            if (cancelled && !failed) return 0;
            if (!failed) ctx->PrintStatus(ctx, 'E', "NOFILES", "No files matched the COPY source specification");
            return 1;
        }
        if (failed) {
            ctx->PrintStatus(ctx, 'E', "COPYFAIL", "One or more matching files could not be copied");
            return 1;
        }
        ctx->PrintStatus(ctx, 'S', "COPIED", "Matching files copied to destination directory");
        return 0;
    }

    if (HasFileWildcard(src) || !ResolveFileSpecification(ctx, argv[2], true, dst, sizeof(dst))) {
        ctx->PrintStatus(ctx, 'E', "BADSPEC", "Wildcard COPY sources require an existing destination directory");
        return 1;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Copy", src);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }
    if (!CopyFileA(src, dst, FALSE)) {
        ctx->PrintStatus(ctx, 'E', "COPYFAIL", "File duplicate operation failed");
        return 1;
    }

    if (quals->HasQualifier(quals, "LOG")) printf("%s copied to %s\n", src, dst);
    ctx->PrintStatus(ctx, 'S', "COPIED", "File transfer operation completed successfully");
    return 0;
}

const char* CopyCmd_GetVerb(ICommand *self) { (void)self; return "COPY"; }
const char* CopyCmd_GetSummary(ICommand *self) { (void)self; return "Copies an input file to a designated target file-spec."; }
const char* CopyCmd_GetHelp(ICommand *self) {
    (void)self;
    return "COPY input-file output-file\n"
           "  Reads the latest source version and creates the next destination version. A wildcard source can be copied to an existing directory.\n"
    "  /CONFIRM prompts before copying; /NOCONFIRM is the default. /LOG reports each copied path; /NOLOG is the default.\n"
    "  WCL /DRYRUN previews without writing. VMS RMS record-format, ownership, protection, ACL, version-policy,\n"
    "  and file-date qualifiers have no direct Windows mapping; unimplemented DCL qualifiers are rejected.";
}

static const ICommandVTable CopyCmd_VTable = {
    { NULL, NULL },
    CopyCmd_Execute,
    CopyCmd_GetVerb,
    CopyCmd_GetSummary,
    CopyCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: APPEND
 * ---------------------------------------------------------------------------- */
typedef struct AppendCommand {
    struct ICommand base;
} AppendCommand;

int AppendCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: APPEND source-file target-file");
        return 1;
    }

    char source[MAX_PATH];
    char target[MAX_PATH];
    if (!ResolveFileSpecification(ctx, argv[1], false, source, sizeof(source)) ||
        !ResolveFileSpecification(ctx, argv[2], false, target, sizeof(target))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }
    if (HasFileWildcard(source) || HasFileWildcard(target)) {
        ctx->PrintStatus(ctx, 'E', "BADSPEC",
            "APPEND supports one exact source and target; wildcard file sets are not mapped");
        return 1;
    }
    DWORD target_attributes = GetFileAttributesA(target);
    if (target_attributes == INVALID_FILE_ATTRIBUTES &&
        !ResolveFileSpecification(ctx, argv[2], true, target, sizeof(target))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Resolved target specification exceeds the supported length");
        return 1;
    }

    char source_full[MAX_PATH];
    char target_full[MAX_PATH];
    if (!GetFullPathNameA(source, sizeof(source_full), source_full, NULL) ||
        !GetFullPathNameA(target, sizeof(target_full), target_full, NULL) ||
        _stricmp(source_full, target_full) == 0) {
        ctx->PrintStatus(ctx, 'E', "BADSPEC", "Source and target must be different valid file specifications");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        char target_text[WCL_MAX_INPUT];
        int length = snprintf(target_text, sizeof(target_text), "%s -> %s", source, target);
        ReportDryRun(ctx, "append", length >= 0 && (size_t)length < sizeof(target_text) ?
            target_text : "files");
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Append", target);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    FILE *input = NULL;
    FILE *output = NULL;
    if (fopen_s(&input, source, "rb") != 0) {
        ctx->PrintStatus(ctx, 'E', "OPENIN", "Unable to open source file for append");
        return 1;
    }
    if (fopen_s(&output, target, "ab") != 0) {
        fclose(input);
        ctx->PrintStatus(ctx, 'E', "OPENOUT", "Unable to open target file for append");
        return 1;
    }

    unsigned char buffer[8192];
    bool success = true;
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (fwrite(buffer, 1, bytes_read, output) != bytes_read) {
            success = false;
            break;
        }
    }
    if (ferror(input)) success = false;
    if (fclose(input) != 0) success = false;
    if (fclose(output) != 0) success = false;
    if (!success) {
        ctx->PrintStatus(ctx, 'E', "APPENDFAIL", "File append failed while reading or writing data");
        return 1;
    }
    if (quals->HasQualifier(quals, "LOG")) printf("%s appended to %s\n", source, target);
    ctx->PrintStatus(ctx, 'S', "APPENDED", "Source file appended to target");
    return 0;
}

const char* AppendCmd_GetVerb(ICommand *self) { (void)self; return "APPEND"; }
const char* AppendCmd_GetSummary(ICommand *self) { (void)self; return "Appends one file's contents to another."; }
const char* AppendCmd_GetHelp(ICommand *self) {
    (void)self;
    return "APPEND source-file target-file\n"
           "  Appends the latest source version to the latest target version. If the target does not exist, a new target version is created.\n"
    "  /CONFIRM prompts before appending; /NOCONFIRM is the default. /LOG reports the completed append; /NOLOG is the default.\n"
    "  WCL /DRYRUN previews without writing. VMS RMS record-format, ownership, protection, ACL, and version-policy\n"
    "  qualifiers have no direct Windows mapping; unimplemented DCL qualifiers are rejected. Multiple-source and wildcard APPEND is unsupported.";
}

static const ICommandVTable AppendCmd_VTable = {
    { NULL, NULL },
    AppendCmd_Execute,
    AppendCmd_GetVerb,
    AppendCmd_GetSummary,
    AppendCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: DELETE
 * ---------------------------------------------------------------------------- */
typedef struct DeleteCommand {
    struct ICommand base;
} DeleteCommand;

int DeleteCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (quals->HasQualifier(quals, "ENTRY")) {
        if (quals->HasQualifier(quals, "SYMBOL")) {
            ctx->PrintStatus(ctx, 'E', "IVQUAL", "/ENTRY and /SYMBOL cannot be combined");
            return 1;
        }
        if (argc != 2) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "DELETE/ENTRY requires one WCL task identifier");
            return 1;
        }
        if (!IsWclTaskName(argv[1])) {
            ctx->PrintStatus(ctx, 'E', "BADENTRY", "Only WCL-created task identifiers can be deleted");
            return 1;
        }
        if (quals->HasQualifier(quals, "DRYRUN")) {
            ReportDryRun(ctx, "delete scheduled task", argv[1]);
            return 0;
        }
        if (quals->HasQualifier(quals, "CONFIRM")) {
            int confirmed = ConfirmMutation(ctx, "Delete scheduled task", argv[1]);
            if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        }
        int deleted = DeleteWclScheduledTask(ctx, argv[1]);
        if (!deleted && quals->HasQualifier(quals, "LOG")) {
            printf("Scheduled task %s deleted\n", argv[1]);
        }
        return deleted;
    }

    if (quals->HasQualifier(quals, "SYMBOL")) {
        if (argc < 2) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "Symbol name or wildcard pattern required");
            return 1;
        }
        int matched = 0;
        if (quals->HasQualifier(quals, "DRYRUN")) {
            for (int i = 0; i < ctx->symbol_count; ++i) {
                if (MatchesWildcard(argv[1], ctx->symbols[i].name)) matched++;
            }
            char count_text[32];
            snprintf(count_text, sizeof(count_text), "%d matching symbol%s",
                matched, matched == 1 ? "" : "s");
            ReportDryRun(ctx, "delete", count_text);
            return matched ? 0 : 1;
        }
        int deleted = 0;
        for (int i = 0; i < ctx->symbol_count;) {
            if (!MatchesWildcard(argv[1], ctx->symbols[i].name)) {
                i++;
                continue;
            }
            if (quals->HasQualifier(quals, "CONFIRM")) {
                int confirmed = ConfirmMutation(ctx, "Delete symbol", ctx->symbols[i].name);
                if (confirmed <= 0) {
                    if (confirmed < 0) return 1;
                    i++;
                    continue;
                }
            }
            char deleted_name[64];
            CopyString(deleted_name, sizeof(deleted_name), ctx->symbols[i].name);
            DeleteSymbol(ctx, deleted_name);
            if (quals->HasQualifier(quals, "LOG")) printf("Symbol %s deleted\n", deleted_name);
            deleted++;
        }
        if (!deleted) {
            ctx->PrintStatus(ctx, 'W', "NOSYMBOL", "No symbols matched the requested name");
            return 1;
        }
        printf("%d symbol%s deleted.\n", deleted, deleted == 1 ? "" : "s");
        return 0;
    }

    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "File specification required");
        return 1;
    }

    char target[MAX_PATH];
    if (!ResolveFileSpecification(ctx, argv[1], false, target, sizeof(target))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, "delete", target);
        return 0;
    }

    if (HasFileWildcard(target)) {
        WIN32_FIND_DATAA data;
        HANDLE search = FindFirstFileA(target, &data);
        if (search == INVALID_HANDLE_VALUE) {
            ctx->PrintStatus(ctx, 'W', "NOFILES", "No files matched the delete specification");
            return 1;
        }

        char directory[MAX_PATH];
        CopyString(directory, sizeof(directory), target);
        char *separator = strrchr(directory, '\\');
        char *forward_separator = strrchr(directory, '/');
        if (forward_separator && (!separator || forward_separator > separator)) separator = forward_separator;
        if (separator) separator[1] = '\0';
        else directory[0] = '\0';

        int deleted = 0;
        bool failed = false;
        do {
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            char match[MAX_PATH];
            int length = snprintf(match, sizeof(match), "%s%s", directory, data.cFileName);
            if (length < 0 || (size_t)length >= sizeof(match)) {
                failed = true;
                continue;
            }
            if (quals->HasQualifier(quals, "CONFIRM")) {
                int confirmed = ConfirmMutation(ctx, "Delete", match);
                if (confirmed < 0) {
                    failed = true;
                    break;
                }
                if (confirmed == 0) continue;
            }
            if (!DeleteFileA(match)) failed = true;
            else {
                if (quals->HasQualifier(quals, "LOG")) printf("%s deleted\n", match);
                deleted++;
            }
        } while (FindNextFileA(search, &data));
        FindClose(search);

        if (!deleted) {
            ctx->PrintStatus(ctx, 'W', "NOFILES", "No files were deleted");
            return 1;
        }
        if (failed) {
            ctx->PrintStatus(ctx, 'E', "DELFAIL", "One or more matching files could not be deleted");
            return 1;
        }
        ctx->PrintStatus(ctx, 'S', "DELETED", "Matching files successfully purged from volume");
        return 0;
    }

    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Delete", target);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    if (!DeleteFileA(target)) {
        ctx->PrintStatus(ctx, 'E', "DELFAIL", "Unable to remove specified file system object");
        return 1;
    }

    if (quals->HasQualifier(quals, "LOG")) printf("%s deleted\n", target);
    ctx->PrintStatus(ctx, 'S', "DELETED", "File successfully purged from volume");
    return 0;
}

const char* DeleteCmd_GetVerb(ICommand *self) { (void)self; return "DELETE"; }
const char* DeleteCmd_GetSummary(ICommand *self) { (void)self; return "Removes file entries from directories."; }
const char* DeleteCmd_GetHelp(ICommand *self) {
    (void)self;
    return "DELETE file-spec | DELETE/ENTRY[/CONFIRM] WCL-task-id\n"
           "  An exact name without a version selects the latest version; wildcard file specifications delete all matches.\n"
           "  Qualifiers:\n"
           "    /CONFIRM    Prompts Y/N before each deletion; noninteractive requests fail closed.\n"
           "    /NOCONFIRM  Performs the deletion without prompting (DCL-style default).\n"
           "    /LOG        Reports each deleted target; /NOLOG is the default.\n"
           "    /DRYRUN     WCL extension: previews matching targets and performs no deletion.\n"
           "    /SYMBOL     Deletes matching DCL symbols instead of files.\n"
           "      /ENTRY      WCL extension: deletes only a task with the WCL-generated identifier shown by SHOW QUEUE.\n"
           "  VMS file-date, ACL, protection, and queue-specific qualifiers without a Windows mapping are rejected.";
}

static const ICommandVTable DeleteCmd_VTable = {
    { NULL, NULL },
    DeleteCmd_Execute,
    DeleteCmd_GetVerb,
    DeleteCmd_GetSummary,
    DeleteCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: RENAME
 * ---------------------------------------------------------------------------- */
typedef struct RenameCommand {
    struct ICommand base;
} RenameCommand;

int RenameCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Missing old-name or new-name");
        return 1;
    }

    char source[MAX_PATH];
    char target[MAX_PATH];
    if (!ResolveFileSpecification(ctx, argv[1], false, source, sizeof(source)) ||
        !ResolveFileSpecification(ctx, argv[2], true, target, sizeof(target))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }
    if (HasFileWildcard(source) || HasFileWildcard(target)) {
        ctx->PrintStatus(ctx, 'E', "BADSPEC",
            "RENAME supports one exact source and target; wildcard renaming is not mapped");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        char target_text[WCL_MAX_INPUT];
        int length = snprintf(target_text, sizeof(target_text), "%s -> %s", source, target);
        ReportDryRun(ctx, "rename", length >= 0 && (size_t)length < sizeof(target_text) ?
            target_text : "file");
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Rename", source);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }
    if (!MoveFileA(source, target)) {
        ctx->PrintStatus(ctx, 'E', "RENFAIL", "Unable to rename file-spec");
        return 1;
    }
    if (quals->HasQualifier(quals, "LOG")) printf("%s renamed to %s\n", source, target);
    ctx->PrintStatus(ctx, 'S', "RENAMED", "File renamed successfully");
    return 0;
}

const char* RenameCmd_GetVerb(ICommand *self) { (void)self; return "RENAME"; }
const char* RenameCmd_GetSummary(ICommand *self) { (void)self; return "Modifies the name of an existing directory item."; }
const char* RenameCmd_GetHelp(ICommand *self) {
    (void)self;
    return "RENAME from-file to-file\n"
           "  Renames the latest source version. An unversioned destination creates the next destination version; wildcard renaming is unsupported.\n"
    "  /CONFIRM prompts Y/N before renaming; /NOCONFIRM is the default. /LOG reports the new name; /NOLOG is the default.\n"
    "  WCL /DRYRUN previews without changing names. VMS file ownership, protection, ACL, and record-attribute qualifiers\n"
    "  have no direct Windows mapping; unimplemented DCL qualifiers are rejected.";
}

static const ICommandVTable RenameCmd_VTable = {
    { NULL, NULL },
    RenameCmd_Execute,
    RenameCmd_GetVerb,
    RenameCmd_GetSummary,
    RenameCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: CREATE (Supports /DIRECTORY qualifier)
 * ---------------------------------------------------------------------------- */
typedef struct CreateCommand {
    struct ICommand base;
} CreateCommand;

int CreateCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Target specification missing");
        return 1;
    }

    char target[MAX_PATH];
    bool create_directory = quals->HasQualifier(quals, "DIRECTORY");
    if (!(create_directory ? ExpandLogicalName(ctx, argv[1], target, sizeof(target)) :
        ResolveFileSpecification(ctx, argv[1], true, target, sizeof(target)))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }

    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, create_directory ? "create directory" : "create file", target);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, create_directory ? "Create directory" : "Create file", target);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    if (create_directory) {
        if (!CreateDirectoryA(target, NULL)) {
            ctx->PrintStatus(ctx, 'E', "MKDIRFAIL", "Unable to create specified directory node");
            return 1;
        }
        if (quals->HasQualifier(quals, "LOG")) printf("Directory %s created\n", target);
        ctx->PrintStatus(ctx, 'S', "CREATED", "Directory successfully created");
        return 0;
    }

    /* Standard Interactive File Creation */
    FILE *fp = NULL;
    if (fopen_s(&fp, target, "w") != 0) {
        ctx->PrintStatus(ctx, 'E', "CREATEFAIL", "Failed to open target file for output");
        return 1;
    }

    printf("Enter data lines. Finish by typing Ctrl+Z on an empty line or EOF:\n");
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        if (fputs(line, fp) == EOF) {
            fclose(fp);
            ctx->PrintStatus(ctx, 'E', "WRITEFAIL", "Failed while writing file data");
            return 1;
        }
    }
    if (ferror(stdin)) {
        fclose(fp);
        ctx->PrintStatus(ctx, 'E', "READFAIL", "Failed while reading file data from standard input");
        clearerr(stdin);
        return 1;
    }
    if (fclose(fp) != 0) {
        ctx->PrintStatus(ctx, 'E', "WRITEFAIL", "Failed to flush or close the created file");
        return 1;
    }
    clearerr(stdin);
    if (quals->HasQualifier(quals, "LOG")) printf("File %s created\n", target);
    ctx->PrintStatus(ctx, 'S', "CREATED", "File constructed successfully");
    return 0;
}

const char* CreateCmd_GetVerb(ICommand *self) { (void)self; return "CREATE"; }
const char* CreateCmd_GetSummary(ICommand *self) { (void)self; return "Constructs a file from input stream or makes a directory."; }
const char* CreateCmd_GetHelp(ICommand *self) {
    (void)self;
    return "CREATE [file-spec | dir-spec]\n"
           "  An unversioned file specification creates the next file version; /DIRECTORY uses the literal directory name.\n"
           "  Qualifiers:\n"
           "    /DIRECTORY     Creates a directory instead of a file.\n"
           "    /NODIRECTORY   Creates a file (default).\n"
           "    /LOG           Reports the created path; /NOLOG is the default.\n"
           "  WCL safety extensions: /CONFIRM prompts Y/N; /NOCONFIRM is the default;\n"
           "  /DRYRUN previews the target without creating it. VMS ownership, protection, ACL, and RMS file-definition\n"
           "  qualifiers have no direct Windows mapping; unimplemented DCL qualifiers are rejected.";
}

static const ICommandVTable CreateCmd_VTable = {
    { NULL, NULL },
    CreateCmd_Execute,
    CreateCmd_GetVerb,
    CreateCmd_GetSummary,
    CreateCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: SEARCH
 * ---------------------------------------------------------------------------- */
typedef struct SearchCommand {
    struct ICommand base;
} SearchCommand;

int SearchFileContents(ExecutionContext *ctx, const char *file_path, const char *pattern,
    FILE *output, bool exact, bool show_numbers, bool show_heading, unsigned long *skip_matches,
    unsigned long limit_matches, unsigned long *displayed, int *total_matches) {
    FILE *file = NULL;
    bool is_stdin = (_stricmp(file_path, "SYS$INPUT") == 0 || strcmp(file_path, "-") == 0);
    if (is_stdin) {
        file = stdin;
    } else if (fopen_s(&file, file_path, "r") != 0) {
        ctx->PrintStatus(ctx, 'E', "FILENOTFND", "Specified search source not found");
        return 1;
    }

    char line[1024];
    int line_number = 0;
    if (show_heading && output &&
        fprintf(output, "\n******************************\n%s\n\n", file_path) < 0) {
        if (!is_stdin) fclose(file);
        else clearerr(stdin);
        ctx->PrintStatus(ctx, 'E', "WRITEFAIL", "Unable to write search heading");
        return 1;
    }
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        bool matches = false;
        if (exact) {
            matches = strstr(line, pattern) != NULL;
        } else {
            size_t pattern_length = strlen(pattern);
            for (const char *candidate = line; *candidate; ++candidate) {
                if (_strnicmp(candidate, pattern, pattern_length) == 0) {
                    matches = true;
                    break;
                }
            }
        }
        if (matches) {
            ++*total_matches;
            if (*skip_matches) {
                --*skip_matches;
                continue;
            }
            if (!limit_matches || *displayed < limit_matches) {
                if (output) {
                    int write_result = show_numbers ?
                        fprintf(output, "%5d  %s", line_number, line) :
                        fputs(line, output);
                    if (write_result < 0) {
                        if (!is_stdin) fclose(file);
                        else clearerr(stdin);
                        ctx->PrintStatus(ctx, 'E', "WRITEFAIL", "Unable to write search result");
                        return 1;
                    }
                }
                ++*displayed;
            }
        }
    }
    bool read_error = ferror(file) != 0;
    if (!is_stdin && fclose(file) != 0) read_error = true;
    if (is_stdin) clearerr(stdin);
    if (read_error) {
        ctx->PrintStatus(ctx, 'E', "READFAIL", "Error while reading search source");
        return 1;
    }
    return 0;
}

int SearchCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: SEARCH <file> <string>");
        return 1;
    }

    char filePath[MAX_PATH];
    if (_stricmp(argv[1], "SYS$INPUT") == 0 || strcmp(argv[1], "-") == 0) {
        CopyString(filePath, sizeof(filePath), "SYS$INPUT");
    } else if (!ResolveFileSpecification(ctx, argv[1], false, filePath, sizeof(filePath))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }
    const char *pattern = argv[2];

    bool exact = quals->HasQualifier(quals, "EXACT");
    bool show_numbers = quals->HasQualifier(quals, "NUMBERS");
    bool show_heading = !quals->HasQualifier(quals, "NOHEADING") &&
        (quals->HasQualifier(quals, "HEADING") || HasFileWildcard(filePath));
    bool suppress_output = quals->HasQualifier(quals, "NOOUTPUT");
    unsigned long skip_matches = 0;
    unsigned long limit_matches = 0;
    const Qualifier *output_qualifier = NULL;
    for (int i = 0; i < quals->count; ++i) {
        if (MatchesPrefix(quals->items[i].name, "SKIP")) {
            IsNumericVersion(quals->items[i].value, &skip_matches);
        } else if (MatchesPrefix(quals->items[i].name, "LIMIT")) {
            IsNumericVersion(quals->items[i].value, &limit_matches);
        } else if (MatchesPrefix(quals->items[i].name, "OUTPUT")) {
            output_qualifier = &quals->items[i];
        }
    }

    if (quals->HasQualifier(quals, "DRYRUN")) {
        if (output_qualifier && output_qualifier->has_value &&
            output_qualifier->value[0] && !suppress_output) {
            if (HasFileWildcard(filePath)) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL",
                    "SEARCH/OUTPUT with wildcard input is not supported by this Windows mapping");
                return 1;
            }
            char input_full[MAX_PATH];
            char output_path[MAX_PATH];
            char output_full[MAX_PATH];
            if (!GetFullPathNameA(filePath, sizeof(input_full), input_full, NULL) ||
                !ResolveFileSpecification(ctx, output_qualifier->value, true,
                    output_path, sizeof(output_path)) ||
                !GetFullPathNameA(output_path, sizeof(output_full), output_full, NULL)) {
                ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "SEARCH output specification is too long");
                return 1;
            }
            if (_stricmp(input_full, output_full) == 0) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL", "SEARCH/OUTPUT cannot overwrite its input file");
                return 1;
            }
            char target[WCL_MAX_INPUT];
            int length = snprintf(target, sizeof(target), "%s -> %s", filePath, output_path);
            ReportDryRun(ctx, "search to output", length >= 0 &&
                (size_t)length < sizeof(target) ? target : output_path);
        } else {
            ReportDryRun(ctx, "search file", filePath);
        }
        return 0;
    }

    FILE *output = suppress_output ? NULL : stdout;
    bool close_output = false;
    if (output_qualifier && output_qualifier->has_value &&
        output_qualifier->value[0] && !suppress_output) {
        if (HasFileWildcard(filePath)) {
            ctx->PrintStatus(ctx, 'E', "IVQUAL",
                "SEARCH/OUTPUT with wildcard input is not supported by this Windows mapping");
            return 1;
        } else {
            char input_full[MAX_PATH];
            char output_path[MAX_PATH];
            char output_full[MAX_PATH];
            if (!GetFullPathNameA(filePath, sizeof(input_full), input_full, NULL) ||
                !ResolveFileSpecification(ctx, output_qualifier->value, true,
                    output_path, sizeof(output_path)) ||
                !GetFullPathNameA(output_path, sizeof(output_full), output_full, NULL)) {
                ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "SEARCH output specification is too long");
                return 1;
            }
            if (_stricmp(input_full, output_full) == 0) {
                ctx->PrintStatus(ctx, 'E', "IVQUAL", "SEARCH/OUTPUT cannot overwrite its input file");
                return 1;
            }
        }
        char output_path[MAX_PATH];
        if (!ResolveFileSpecification(ctx, output_qualifier->value, true,
            output_path, sizeof(output_path))) {
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "SEARCH output specification is too long");
            return 1;
        }
        if (quals->HasQualifier(quals, "CONFIRM")) {
            int confirmed = ConfirmMutation(ctx, "Search and write output", output_path);
            if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        }
        if (fopen_s(&output, output_path, "w") != 0 || !output) {
            ctx->PrintStatus(ctx, 'E', "SEARCHOUTPUT", "Unable to open SEARCH output file");
            return 1;
        }
        close_output = true;
    }

    int matches = 0;
    unsigned long displayed = 0;
    bool failed = false;
    if (HasFileWildcard(filePath)) {
        WIN32_FIND_DATAA data;
        HANDLE search = FindFirstFileA(filePath, &data);
        if (search == INVALID_HANDLE_VALUE) {
            ctx->PrintStatus(ctx, 'E', "NOFILES", "No files matched the SEARCH specification");
            return 1;
        }
        int searched = 0;
        do {
            if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            char matched_path[MAX_PATH];
            if (!BuildMatchedFilePath(filePath, data.cFileName, matched_path, sizeof(matched_path))) {
                failed = true;
                ++searched;
                continue;
            }
            if (quals->HasQualifier(quals, "CONFIRM") && !close_output) {
                int confirmed = ConfirmMutation(ctx, "Search", matched_path);
                if (confirmed < 0) {
                    failed = true;
                    break;
                }
                if (confirmed == 0) {
                    ++searched;
                    continue;
                }
            }
            if (SearchFileContents(ctx, matched_path, pattern, output, exact, show_numbers,
                    show_heading, &skip_matches, limit_matches, &displayed, &matches) != 0) {
                failed = true;
            }
            ++searched;
        } while (FindNextFileA(search, &data));
        FindClose(search);
        if (!searched) {
            ctx->PrintStatus(ctx, 'W', "NOFILES", "No files matched the SEARCH specification");
            return 1;
        }
    } else {
        bool should_search = true;
        if (quals->HasQualifier(quals, "CONFIRM") && !close_output) {
            int confirmed = ConfirmMutation(ctx, "Search", filePath);
            if (confirmed < 0) failed = true;
            else if (confirmed == 0) should_search = false;
        }
        if (should_search && SearchFileContents(ctx, filePath, pattern, output, exact,
            show_numbers, show_heading, &skip_matches, limit_matches,
            &displayed, &matches) != 0) {
            failed = true;
        }
    }
    if (close_output && fclose(output) != 0) failed = true;
    printf("\nFound %d matching record%s.\n\n", matches, matches == 1 ? "" : "s");
    return failed ? 1 : 0;
}

const char* SearchCmd_GetVerb(ICommand *self) { (void)self; return "SEARCH"; }
const char* SearchCmd_GetSummary(ICommand *self) { (void)self; return "Scans records in files matching target string pattern."; }
const char* SearchCmd_GetHelp(ICommand *self) {
    (void)self;
    return "SEARCH file-spec string [/EXACT] [/NUMBERS] [/HEADING] [/SKIP=n] [/LIMIT=n] [/OUTPUT[=filespec]]\n"
           "  Scans the latest version of an exact file name; wildcard specifications scan every matching file version.\n"
           "  /EXACT matches case-sensitively; default /NOEXACT ignores case. /NUMBERS adds source line numbers;\n"
           "  default /NONUMBERS omits them. Headings are shown by default for wildcard input; /NOHEADING suppresses,\n"
           "  and /HEADING forces them. /SKIP=n skips that many matching records; /LIMIT=n caps displayed matches.\n"
           "  /OUTPUT=filespec writes results from one exact input file to a Windows file; wildcard output is not mapped.\n"
           "  /NOOUTPUT suppresses matched records.\n"
           "  /CONFIRM prompts before a requested output-file write; WCL /DRYRUN previews without reading or writing.\n"
           "  WCL-specific /SKIP and /LIMIT are bounded record controls. VMS RMS/indexed-file, date-selection, and paging\n"
           "  qualifiers without a direct Windows equivalent are rejected.";
}

static const ICommandVTable SearchCmd_VTable = {
    { NULL, NULL },
    SearchCmd_Execute,
    SearchCmd_GetVerb,
    SearchCmd_GetSummary,
    SearchCmd_GetHelp
};

/* ----------------------------------------------------------------------------
 * Built-in Command: PRINT
 * ---------------------------------------------------------------------------- */
typedef struct PrintCommand {
    struct ICommand base;
} PrintCommand;

typedef HINSTANCE (WINAPI *WclShellExecuteFn)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);

int PrintCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "File specification required");
        return 1;
    }

    char path[MAX_PATH];
    if (_stricmp(argv[1], "SYS$INPUT") == 0 || strcmp(argv[1], "-") == 0) {
        CopyString(path, sizeof(path), "SYS$INPUT");
    } else if (!ResolveFileSpecification(ctx, argv[1], false, path, sizeof(path))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded file specification exceeds the supported length");
        return 1;
    }
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        ctx->PrintStatus(ctx, 'E', "FILENOTFND", "Printable file was not found");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, "submit to the associated Windows print action", path);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Print", path);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    HMODULE shell = LoadLibraryA("shell32.dll");
    if (!shell) {
        ctx->PrintStatus(ctx, 'E', "PRINTFAIL", "Windows shell printing service is unavailable");
        return 1;
    }
    WclShellExecuteFn shell_execute = (WclShellExecuteFn)GetProcAddress(shell, "ShellExecuteA");
    if (!shell_execute) {
        FreeLibrary(shell);
        ctx->PrintStatus(ctx, 'E', "PRINTFAIL", "Windows shell printing service is unavailable");
        return 1;
    }
    INT_PTR result = (INT_PTR)shell_execute(NULL, "print", path, NULL, NULL, SW_HIDE);
    FreeLibrary(shell);
    if (result <= 32) {
        ctx->PrintStatus(ctx, 'E', "PRINTFAIL", "No associated Windows print action accepted the file");
        return 1;
    }
    ctx->PrintStatus(ctx, 'S', "PRINTQUEUED", "File submitted to its associated Windows print action");
    return 0;
}

const char* PrintCmd_GetVerb(ICommand *self) { (void)self; return "PRINT"; }
const char* PrintCmd_GetSummary(ICommand *self) { (void)self; return "Submits a file to its associated Windows print action."; }
const char* PrintCmd_GetHelp(ICommand *self) {
    (void)self;
    return "PRINT file-spec\n"
           "  Uses the registered Windows shell print action; it does not select an OpenVMS print queue.\n"
           "  WCL safety extensions: /CONFIRM prompts Y/N; /DRYRUN previews without invoking a print action.\n"
           "  VMS queue, form, job-parameter, retention, and page-range qualifiers have no direct Windows mapping and are rejected.";
}

static const ICommandVTable PrintCmd_VTable = {
    { NULL, NULL },
    PrintCmd_Execute,
    PrintCmd_GetVerb,
    PrintCmd_GetSummary,
    PrintCmd_GetHelp
};

void RegisterFileOperationCommands(CommandRegistry *registry) {
    static TypeCommand type = { { &TypeCmd_VTable, 1 } };
    static CopyCommand copy = { { &CopyCmd_VTable, 1 } };
    static DeleteCommand delete_command = { { &DeleteCmd_VTable, 1 } };
    static RenameCommand rename_command = { { &RenameCmd_VTable, 1 } };
    static CreateCommand create = { { &CreateCmd_VTable, 1 } };
    static SearchCommand search = { { &SearchCmd_VTable, 1 } };
    static AppendCommand append = { { &AppendCmd_VTable, 1 } };
    static PrintCommand print = { { &PrintCmd_VTable, 1 } };
    registry->Register(registry, (ICommand*)&type);
    registry->Register(registry, (ICommand*)&copy);
    registry->Register(registry, (ICommand*)&delete_command);
    registry->Register(registry, (ICommand*)&rename_command);
    registry->Register(registry, (ICommand*)&create);
    registry->Register(registry, (ICommand*)&search);
    registry->Register(registry, (ICommand*)&append);
    registry->Register(registry, (ICommand*)&print);
}

