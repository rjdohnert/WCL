#define _CRT_SECURE_NO_WARNINGS
#include "history.h"
#include "symbols_lexicals.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *s_history[WCL_HISTORY_MAX];
static int s_history_count = 0;
static char s_hist_file[MAX_PATH] = "";
static char s_ini_file[MAX_PATH] = "";
static CommandRegistry *s_registry = NULL;

static void HandleTabCompletion(ExecutionContext *ctx, char *line, int *p_len, int *p_pos, size_t buffer_size) {
    int pos = *p_pos;
    int len = *p_len;
    if (pos < 0) return;

    /* Find token boundary */
    int token_start = pos;
    while (token_start > 0 && !isspace((unsigned char)line[token_start - 1]) && line[token_start - 1] != '/') {
        token_start--;
    }
    bool is_qualifier = false;
    if (token_start > 0 && line[token_start - 1] == '/') {
        token_start--;
        is_qualifier = true;
    } else if (line[token_start] == '/') {
        is_qualifier = true;
    }

    char token[MAX_PATH] = {0};
    int token_len = pos - token_start;
    if (token_len >= (int)sizeof(token)) token_len = (int)sizeof(token) - 1;
    strncpy(token, line + token_start, token_len);
    token[token_len] = '\0';

    char completion[MAX_PATH] = {0};
    bool found = false;

    if (is_qualifier) {
        /* Complete DCL qualifier */
        const char *qual_token = token + 1; /* skip '/' */
        size_t qlen = strlen(qual_token);
        static const char *known_quals[] = {
            "CONFIRM", "NOCONFIRM", "DRYRUN", "LOG", "NOLOG", "PAGE", "NOPAGE",
            "OUTPUT", "FULL", "BRIEF", "SIZE", "EXACT", "HEADING", "NUMBERS",
            "VALUES", "DATA", "TYPE", "TREE", "NETWORK", "SOCKET", "NOWAIT",
            "NOTIFY", "ALL", "SYMBOL", "ENTRY", "WIDTH", "TITLE", "WRAP", "NOWRAP",
            "PROTECTION", "PRIORITY", "AFFINITY", "SUSPEND", "RESUME", "KEEP",
            "READ", "WRITE", "APPEND", "REPLACE", "CREATE", "DIRECTORY", "HELP"
        };
        int matches = 0;
        char best_match[64] = {0};
        for (size_t i = 0; i < sizeof(known_quals)/sizeof(known_quals[0]); ++i) {
            if (_strnicmp(known_quals[i], qual_token, qlen) == 0) {
                matches++;
                if (matches == 1) snprintf(best_match, sizeof(best_match), "%s", known_quals[i]);
            }
        }
        if (matches == 1) {
            snprintf(completion, sizeof(completion), "/%s ", best_match);
            found = true;
        }
    } else if (token_start == 0) {
        /* Complete command verb */
        size_t tlen = strlen(token);
        if (tlen > 0 && s_registry) {
            int matches = 0;
            char best_verb[64] = {0};
            for (int i = 0; i < s_registry->count; ++i) {
                const char *verb = s_registry->commands[i]->vptr->GetVerb(s_registry->commands[i]);
                if (verb && _strnicmp(verb, token, tlen) == 0) {
                    matches++;
                    if (matches == 1) snprintf(best_verb, sizeof(best_verb), "%s", verb);
                }
            }
            if (matches == 1) {
                snprintf(completion, sizeof(completion), "%s ", best_verb);
                found = true;
            }
        }
    } else {
        /* Complete file path */
        char search_pattern[MAX_PATH];
        char dir_part[MAX_PATH] = "";
        const char *file_part = token;
        const char *last_slash = strrchr(token, '\\');
        if (!last_slash) last_slash = strrchr(token, '/');
        if (last_slash) {
            size_t dlen = last_slash - token + 1;
            strncpy(dir_part, token, dlen);
            dir_part[dlen] = '\0';
            file_part = last_slash + 1;
        }
        snprintf(search_pattern, sizeof(search_pattern), "%s%s*", dir_part, file_part);

        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(search_pattern, &fd);
        if (hFind != INVALID_HANDLE_VALUE) {
            int matches = 0;
            char best_name[MAX_PATH] = {0};
            bool is_dir = false;
            do {
                if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                    matches++;
                    if (matches == 1) {
                        snprintf(best_name, sizeof(best_name), "%s", fd.cFileName);
                        is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    }
                }
            } while (FindNextFileA(hFind, &fd));
            FindClose(hFind);

            if (matches == 1) {
                snprintf(completion, sizeof(completion), "%s%s%s", dir_part, best_name, is_dir ? "\\" : " ");
                found = true;
            }
        }
    }

    if (found) {
        int comp_len = (int)strlen(completion);
        int tail_len = len - pos;
        if (token_start + comp_len + tail_len < (int)buffer_size && token_start + comp_len + tail_len < (int)WCL_MAX_INPUT) {
            memmove(line + token_start + comp_len, line + pos, tail_len + 1);
            memcpy(line + token_start, completion, comp_len);
            len = token_start + comp_len + tail_len;
            pos = token_start + comp_len;
            *p_len = len;
            *p_pos = pos;
            printf("\r%s%s\x1b[K", ctx->current_prompt, line);
            for (int i = 0; i < len - pos; ++i) printf("\b");
            fflush(stdout);
        }
    }
}

static void GetUserHomeDirectory(char *out_path, size_t capacity) {
    DWORD len = GetEnvironmentVariableA("USERPROFILE", out_path, (DWORD)capacity);
    if (!len || len >= capacity) {
        char drive[32] = "", path[MAX_PATH] = "";
        GetEnvironmentVariableA("HOMEDRIVE", drive, sizeof(drive));
        GetEnvironmentVariableA("HOMEPATH", path, sizeof(path));
        if (drive[0] && path[0]) {
            snprintf(out_path, capacity, "%s%s", drive, path);
        } else {
            GetCurrentDirectoryA((DWORD)capacity, out_path);
        }
    }
}

void History_Init(ExecutionContext *ctx, CommandRegistry *registry) {
    s_registry = registry;
    char home_dir[MAX_PATH] = "";
    GetUserHomeDirectory(home_dir, sizeof(home_dir));

    snprintf(s_ini_file, sizeof(s_ini_file), "%s\\wcl.ini", home_dir);
    snprintf(s_hist_file, sizeof(s_hist_file), "%s\\.wcl_history", home_dir);

    /* 1. Create default wcl.ini if it does not exist */
    DWORD ini_attr = GetFileAttributesA(s_ini_file);
    if (ini_attr == INVALID_FILE_ATTRIBUTES) {
        FILE *f = fopen(s_ini_file, "w");
        if (f) {
            fprintf(f,
                "! ============================================================================\n"
                "! Windows Command Language (WCL) User Initialization File\n"
                "! Location: %%USERPROFILE%%\\wcl.ini\n"
                "! Automatically executed at shell startup.\n"
                "! ============================================================================\n"
                "! Default Command Aliases & Symbols\n"
                "LS :== DIRECTORY\n"
                "LL :== DIRECTORY /FULL\n"
                "CAT :== TYPE\n"
                "CP :== COPY\n"
                "MV :== RENAME\n"
                "RM :== DELETE\n"
                "CLEAR :== TYPE/PAGE NL:\n"
                "CLS :== TYPE/PAGE NL:\n"
                "VI :== IRIS\n"
                "VIM :== IRIS\n"
                "EVE :== IRIS\n\n"
                "! Environment Configuration\n"
                "! SET PROMPT \"$ \"\n");
            fclose(f);
        }
    }

    /* 2. Execute wcl.ini */
    FILE *fini = fopen(s_ini_file, "r");
    if (fini) {
        char line[WCL_MAX_INPUT];
        while (fgets(line, sizeof(line), fini)) {
            size_t l = strlen(line);
            while (l > 0 && (line[l - 1] == '\r' || line[l - 1] == '\n')) line[--l] = '\0';
            char *p = line;
            while (*p && isspace((unsigned char)*p)) p++;
            if (p[0] && p[0] != '!') {
                ExecuteCommandLine(ctx, registry, p, true);
            }
        }
        fclose(fini);
    }

    /* 3. Load previous history from .wcl_history */
    FILE *fhist = fopen(s_hist_file, "r");
    if (fhist) {
        char line[WCL_MAX_INPUT];
        while (fgets(line, sizeof(line), fhist) && s_history_count < WCL_HISTORY_MAX) {
            size_t l = strlen(line);
            while (l > 0 && (line[l - 1] == '\r' || line[l - 1] == '\n')) line[--l] = '\0';
            const char *cmd_text = line;
            /* Parse [DD-MMM-YYYY HH:MM:SS] prefix */
            if (line[0] == '[') {
                char *close_bracket = strchr(line, ']');
                if (close_bracket && *(close_bracket + 1)) {
                    cmd_text = close_bracket + 1;
                    while (*cmd_text && isspace((unsigned char)*cmd_text)) cmd_text++;
                }
            }
            if (cmd_text[0]) {
                char *dup = _strdup(cmd_text);
                if (dup) s_history[s_history_count++] = dup;
            }
        }
        fclose(fhist);
    }
}

void History_Add(const char *command) {
    if (!command || !command[0]) return;
    while (isspace((unsigned char)*command)) command++;
    if (!*command) return;

    /* Do not store duplicate of immediate previous command */
    if (s_history_count > 0 && s_history[s_history_count - 1] &&
        strcmp(s_history[s_history_count - 1], command) == 0) return;

    char *dup = _strdup(command);
    if (!dup) return;

    if (s_history_count < WCL_HISTORY_MAX) {
        s_history[s_history_count++] = dup;
    } else {
        free(s_history[0]);
        memmove(&s_history[0], &s_history[1], sizeof(char*) * (WCL_HISTORY_MAX - 1));
        s_history[WCL_HISTORY_MAX - 1] = dup;
    }

    /* Append timestamped entry to .wcl_history */
    if (s_hist_file[0]) {
        FILE *f = fopen(s_hist_file, "a");
        if (f) {
            SYSTEMTIME st;
            GetLocalTime(&st);
            const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };
            fprintf(f, "[%02d-%s-%04d %02d:%02d:%02d] %s\n",
                st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, command);
            fclose(f);
        }
    }
}

bool History_ReadLine(ExecutionContext *ctx, char *buffer, size_t buffer_size) {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    /* If not an interactive console (piped / automated testing), use standard fgets */
    if (!GetConsoleMode(hIn, &mode)) {
        printf("%s", ctx->current_prompt);
        fflush(stdout);
        return fgets(buffer, (int)buffer_size, stdin) != NULL;
    }

    /* Print prompt */
    printf("%s", ctx->current_prompt);
    fflush(stdout);

    DWORD orig_in_mode = 0, orig_out_mode = 0;
    GetConsoleMode(hIn, &orig_in_mode);
    GetConsoleMode(hOut, &orig_out_mode);

    DWORD raw_in = orig_in_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    raw_in |= ENABLE_WINDOW_INPUT;
    SetConsoleMode(hIn, raw_in);
    SetConsoleMode(hOut, orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);

    char line[WCL_MAX_INPUT] = "";
    char temp_line[WCL_MAX_INPUT] = "";
    int len = 0;
    int pos = 0;
    int hist_idx = s_history_count;

    while (1) {
        INPUT_RECORD ir;
        DWORD count = 0;
        if (!ReadConsoleInputA(hIn, &ir, 1, &count) || !count) break;

        if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            CHAR ch = ir.Event.KeyEvent.uChar.AsciiChar;
            DWORD ctrl = ir.Event.KeyEvent.dwControlKeyState;
            bool is_ctrl = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

            if (vk == VK_RETURN) {
                putchar('\n');
                fflush(stdout);
                CopyString(buffer, buffer_size, line);
                SetConsoleMode(hIn, orig_in_mode);
                SetConsoleMode(hOut, orig_out_mode);
                return true;
            }

            if (vk == VK_TAB) {
                HandleTabCompletion(ctx, line, &len, &pos, sizeof(line));
                continue;
            }

            if (vk == VK_UP) {
                /* Recall older history entry */
                if (hist_idx == s_history_count) {
                    CopyString(temp_line, sizeof(temp_line), line);
                }
                if (hist_idx > 0) {
                    hist_idx--;
                    while (pos > 0) { printf("\b \b"); pos--; }
                    printf("\x1b[K");
                    CopyString(line, sizeof(line), s_history[hist_idx] ? s_history[hist_idx] : "");
                    len = (int)strlen(line);
                    pos = len;
                    printf("%s", line);
                    fflush(stdout);
                }
                continue;
            }

            if (vk == VK_DOWN) {
                /* Recall newer history entry */
                if (hist_idx < s_history_count) {
                    hist_idx++;
                    while (pos > 0) { printf("\b \b"); pos--; }
                    printf("\x1b[K");
                    if (hist_idx == s_history_count) {
                        CopyString(line, sizeof(line), temp_line);
                    } else {
                        CopyString(line, sizeof(line), s_history[hist_idx] ? s_history[hist_idx] : "");
                    }
                    len = (int)strlen(line);
                    pos = len;
                    printf("%s", line);
                    fflush(stdout);
                }
                continue;
            }

            if (vk == VK_LEFT) {
                if (pos > 0) {
                    pos--;
                    printf("\b");
                    fflush(stdout);
                }
                continue;
            }

            if (vk == VK_RIGHT) {
                if (pos < len) {
                    putchar(line[pos]);
                    pos++;
                    fflush(stdout);
                }
                continue;
            }

            if (vk == VK_HOME) {
                while (pos > 0) { printf("\b"); pos--; }
                fflush(stdout);
                continue;
            }

            if (vk == VK_END) {
                while (pos < len) { putchar(line[pos]); pos++; }
                fflush(stdout);
                continue;
            }

            if (vk == VK_BACK) {
                if (pos > 0) {
                    memmove(&line[pos - 1], &line[pos], len - pos + 1);
                    len--;
                    pos--;
                    printf("\b%s \b", &line[pos]);
                    for (int i = 0; i < len - pos; ++i) printf("\b");
                    fflush(stdout);
                }
                continue;
            }

            if (vk == VK_DELETE) {
                if (pos < len) {
                    memmove(&line[pos], &line[pos + 1], len - pos);
                    len--;
                    printf("%s \b", &line[pos]);
                    for (int i = 0; i < len - pos; ++i) printf("\b");
                    fflush(stdout);
                }
                continue;
            }

            if (vk == VK_ESCAPE || (is_ctrl && (vk == 'C' || ch == 3))) {
                /* Cancel line */
                putchar('\n');
                line[0] = '\0';
                len = 0;
                pos = 0;
                hist_idx = s_history_count;
                printf("%s", ctx->current_prompt);
                fflush(stdout);
                continue;
            }

            if (is_ctrl && (vk == 'U' || ch == 21)) {
                /* Erase line */
                while (pos > 0) { printf("\b \b"); pos--; }
                printf("\x1b[K");
                line[0] = '\0';
                len = 0;
                pos = 0;
                fflush(stdout);
                continue;
            }

            /* Regular character input */
            if (ch >= 32 && ch < 127 && len + 1 < (int)buffer_size && len + 1 < (int)sizeof(line)) {
                memmove(&line[pos + 1], &line[pos], len - pos + 1);
                line[pos] = ch;
                len++;
                printf("%s", &line[pos]);
                pos++;
                for (int i = 0; i < len - pos; ++i) printf("\b");
                fflush(stdout);
            }
        }
    }

    SetConsoleMode(hIn, orig_in_mode);
    SetConsoleMode(hOut, orig_out_mode);
    CopyString(buffer, buffer_size, line);
    return len > 0;
}

int History_GetCount(void) {
    return s_history_count;
}

const char* History_Get(int index) {
    if (index >= 0 && index < s_history_count) {
        return s_history[index];
    }
    return NULL;
}

void History_Cleanup(void) {
    for (int i = 0; i < s_history_count; ++i) {
        if (s_history[i]) {
            free(s_history[i]);
            s_history[i] = NULL;
        }
    }
    s_history_count = 0;
}
