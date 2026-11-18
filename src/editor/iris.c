#define _CRT_SECURE_NO_WARNINGS
#include "../include/wcl_internal.h"
#include "iris.h"
#include "../core/symbols_lexicals.h"

/* ----------------------------------------------------------------------------
 * IRIS Editor Buffer Structure
 * ---------------------------------------------------------------------------- */
typedef struct EditorBuffer {
    char filename[MAX_PATH];
    char **lines;
    int line_count;
    int line_capacity;
    bool modified;
    bool readonly;
    int cursor_line;  /* 0-based */
    int cursor_col;   /* 0-based */
    int top_line;     /* 0-based first visible line */
    bool select_active;
    int sel_start_line;
    int sel_start_col;
    char message[128];
} EditorBuffer;

static char *g_clipboard = NULL;

static void Buffer_Init(EditorBuffer *buf, const char *filename) {
    memset(buf, 0, sizeof(*buf));
    if (filename && filename[0]) {
        CopyString(buf->filename, sizeof(buf->filename), filename);
    } else {
        CopyString(buf->filename, sizeof(buf->filename), "MAIN.TXT");
    }
    buf->line_capacity = 64;
    buf->lines = (char**)calloc(buf->line_capacity, sizeof(char*));
    buf->lines[0] = _strdup("");
    buf->line_count = 1;
}

static void Buffer_Free(EditorBuffer *buf) {
    if (buf->lines) {
        for (int i = 0; i < buf->line_count; ++i) {
            free(buf->lines[i]);
        }
        free(buf->lines);
        buf->lines = NULL;
    }
    buf->line_count = 0;
    if (g_clipboard) {
        free(g_clipboard);
        g_clipboard = NULL;
    }
}

static void Buffer_EnsureCapacity(EditorBuffer *buf, int count) {
    if (count >= buf->line_capacity) {
        int new_cap = buf->line_capacity * 2;
        if (new_cap <= count) new_cap = count + 32;
        char **new_lines = (char**)realloc(buf->lines, new_cap * sizeof(char*));
        if (new_lines) {
            buf->lines = new_lines;
            buf->line_capacity = new_cap;
        }
    }
}

static bool Buffer_LoadFile(EditorBuffer *buf, const char *filepath) {
    FILE *f = fopen(filepath, "r");
    if (!f) return false;

    /* Clear current lines */
    for (int i = 0; i < buf->line_count; ++i) free(buf->lines[i]);
    buf->line_count = 0;

    char line_buf[WCL_MAX_INPUT];
    while (fgets(line_buf, sizeof(line_buf), f)) {
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\r' || line_buf[len - 1] == '\n')) {
            line_buf[--len] = '\0';
        }
        Buffer_EnsureCapacity(buf, buf->line_count + 1);
        buf->lines[buf->line_count++] = _strdup(line_buf);
    }
    fclose(f);

    if (buf->line_count == 0) {
        Buffer_EnsureCapacity(buf, 1);
        buf->lines[0] = _strdup("");
        buf->line_count = 1;
    }

    CopyString(buf->filename, sizeof(buf->filename), filepath);
    buf->modified = false;
    buf->cursor_line = 0;
    buf->cursor_col = 0;
    buf->top_line = 0;
    snprintf(buf->message, sizeof(buf->message), "%d lines read from %s", buf->line_count, filepath);
    return true;
}

static bool Buffer_SaveFile(EditorBuffer *buf, const char *filepath) {
    const char *target = (filepath && filepath[0]) ? filepath : buf->filename;
    FILE *f = fopen(target, "w");
    if (!f) return false;

    for (int i = 0; i < buf->line_count; ++i) {
        fputs(buf->lines[i], f);
        fputc('\n', f);
    }
    fclose(f);

    if (target != buf->filename) {
        CopyString(buf->filename, sizeof(buf->filename), target);
    }
    buf->modified = false;
    snprintf(buf->message, sizeof(buf->message), "%d lines written to %s", buf->line_count, target);
    return true;
}

static void Buffer_InsertChar(EditorBuffer *buf, char ch) {
    if (buf->readonly) return;
    char *cur_line = buf->lines[buf->cursor_line];
    size_t len = strlen(cur_line);
    if (buf->cursor_col > (int)len) buf->cursor_col = (int)len;

    char *new_line = (char*)malloc(len + 2);
    if (!new_line) return;
    memcpy(new_line, cur_line, buf->cursor_col);
    new_line[buf->cursor_col] = ch;
    memcpy(new_line + buf->cursor_col + 1, cur_line + buf->cursor_col, len - buf->cursor_col + 1);

    free(cur_line);
    buf->lines[buf->cursor_line] = new_line;
    buf->cursor_col++;
    buf->modified = true;
}

static void Buffer_SplitLine(EditorBuffer *buf) {
    if (buf->readonly) return;
    char *cur_line = buf->lines[buf->cursor_line];
    size_t len = strlen(cur_line);
    if (buf->cursor_col > (int)len) buf->cursor_col = (int)len;

    char *first_part = (char*)malloc(buf->cursor_col + 1);
    char *second_part = (char*)malloc(len - buf->cursor_col + 1);
    if (!first_part || !second_part) {
        if (first_part) free(first_part);
        if (second_part) free(second_part);
        return;
    }

    memcpy(first_part, cur_line, buf->cursor_col);
    first_part[buf->cursor_col] = '\0';

    memcpy(second_part, cur_line + buf->cursor_col, len - buf->cursor_col + 1);

    free(cur_line);
    buf->lines[buf->cursor_line] = first_part;

    Buffer_EnsureCapacity(buf, buf->line_count + 1);
    for (int i = buf->line_count; i > buf->cursor_line + 1; --i) {
        buf->lines[i] = buf->lines[i - 1];
    }
    buf->lines[buf->cursor_line + 1] = second_part;
    buf->line_count++;

    buf->cursor_line++;
    buf->cursor_col = 0;
    buf->modified = true;
}

static void Buffer_Backspace(EditorBuffer *buf) {
    if (buf->readonly) return;
    if (buf->cursor_col > 0) {
        char *cur_line = buf->lines[buf->cursor_line];
        size_t len = strlen(cur_line);
        memmove(cur_line + buf->cursor_col - 1, cur_line + buf->cursor_col, len - buf->cursor_col + 1);
        buf->cursor_col--;
        buf->modified = true;
    } else if (buf->cursor_line > 0) {
        /* Join with previous line */
        char *prev_line = buf->lines[buf->cursor_line - 1];
        char *cur_line = buf->lines[buf->cursor_line];
        size_t prev_len = strlen(prev_line);
        size_t cur_len = strlen(cur_line);

        char *joined = (char*)malloc(prev_len + cur_len + 1);
        if (!joined) return;
        memcpy(joined, prev_line, prev_len);
        memcpy(joined + prev_len, cur_line, cur_len + 1);

        free(prev_line);
        free(cur_line);
        buf->lines[buf->cursor_line - 1] = joined;

        for (int i = buf->cursor_line; i < buf->line_count - 1; ++i) {
            buf->lines[i] = buf->lines[i + 1];
        }
        buf->line_count--;
        buf->cursor_line--;
        buf->cursor_col = (int)prev_len;
        buf->modified = true;
    }
}

static void Buffer_DeleteChar(EditorBuffer *buf) {
    if (buf->readonly) return;
    char *cur_line = buf->lines[buf->cursor_line];
    size_t len = strlen(cur_line);
    if (buf->cursor_col < (int)len) {
        memmove(cur_line + buf->cursor_col, cur_line + buf->cursor_col + 1, len - buf->cursor_col);
        buf->modified = true;
    } else if (buf->cursor_line < buf->line_count - 1) {
        /* Join with next line */
        char *next_line = buf->lines[buf->cursor_line + 1];
        size_t next_len = strlen(next_line);
        char *joined = (char*)malloc(len + next_len + 1);
        if (!joined) return;
        memcpy(joined, cur_line, len);
        memcpy(joined + len, next_line, next_len + 1);

        free(cur_line);
        free(next_line);
        buf->lines[buf->cursor_line] = joined;

        for (int i = buf->cursor_line + 1; i < buf->line_count - 1; ++i) {
            buf->lines[i] = buf->lines[i + 1];
        }
        buf->line_count--;
        buf->modified = true;
    }
}

/* ----------------------------------------------------------------------------
 * Clipboard (Select, Copy, Cut, Paste)
 * ---------------------------------------------------------------------------- */
static void Iris_Copy(EditorBuffer *buf) {
    if (g_clipboard) { free(g_clipboard); g_clipboard = NULL; }
    if (buf->select_active) {
        int start_l = buf->sel_start_line, end_l = buf->cursor_line;
        int start_c = buf->sel_start_col, end_c = buf->cursor_col;
        if (start_l > end_l || (start_l == end_l && start_c > end_c)) {
            int tl = start_l; start_l = end_l; end_l = tl;
            int tc = start_c; start_c = end_c; end_c = tc;
        }

        /* Calculate total length */
        size_t total = 0;
        for (int l = start_l; l <= end_l; ++l) {
            total += strlen(buf->lines[l]) + 2;
        }
        g_clipboard = (char*)malloc(total + 1);
        if (!g_clipboard) return;
        g_clipboard[0] = '\0';

        for (int l = start_l; l <= end_l; ++l) {
            const char *line = buf->lines[l];
            int from = (l == start_l) ? start_c : 0;
            int to = (l == end_l) ? end_c : (int)strlen(line);
            if (from < (int)strlen(line)) {
                if (to > (int)strlen(line)) to = (int)strlen(line);
                if (to > from) {
                    strncat(g_clipboard, line + from, (size_t)(to - from));
                }
            }
            if (l < end_l) strcat(g_clipboard, "\n");
        }
        buf->select_active = false;
        snprintf(buf->message, sizeof(buf->message), "Selection copied to pasteboard");
    } else {
        /* Copy current line */
        if (buf->cursor_line >= 0 && buf->cursor_line < buf->line_count && buf->lines[buf->cursor_line]) {
            g_clipboard = _strdup(buf->lines[buf->cursor_line]);
            snprintf(buf->message, sizeof(buf->message), "Line %d copied to pasteboard", buf->cursor_line + 1);
        }
    }
}

static void Iris_Cut(EditorBuffer *buf) {
    if (buf->readonly) return;
    Iris_Copy(buf);
    if (buf->line_count > 1) {
        free(buf->lines[buf->cursor_line]);
        for (int i = buf->cursor_line; i < buf->line_count - 1; ++i) {
            buf->lines[i] = buf->lines[i + 1];
        }
        buf->line_count--;
        if (buf->cursor_line >= buf->line_count) buf->cursor_line = buf->line_count - 1;
        buf->cursor_col = 0;
        buf->modified = true;
        snprintf(buf->message, sizeof(buf->message), "Line cut to pasteboard");
    } else {
        free(buf->lines[0]);
        buf->lines[0] = _strdup("");
        buf->cursor_col = 0;
        buf->modified = true;
    }
}

static void Iris_Paste(EditorBuffer *buf) {
    if (buf->readonly || !g_clipboard) return;
    char *dup = _strdup(g_clipboard);
    if (!dup) return;

    char *line = strtok(dup, "\r\n");
    while (line) {
        size_t len = strlen(line);
        for (size_t i = 0; i < len; ++i) {
            Buffer_InsertChar(buf, line[i]);
        }
        line = strtok(NULL, "\r\n");
        if (line) Buffer_SplitLine(buf);
    }
    free(dup);
    snprintf(buf->message, sizeof(buf->message), "Pasted from pasteboard");
}

/* ----------------------------------------------------------------------------
 * Find and Replace
 * ---------------------------------------------------------------------------- */
static bool Iris_Find(EditorBuffer *buf, const char *needle) {
    if (!needle || !needle[0]) return false;
    for (int l = buf->cursor_line; l < buf->line_count; ++l) {
        const char *line = buf->lines[l];
        int start_pos = (l == buf->cursor_line) ? buf->cursor_col + 1 : 0;
        if (start_pos < (int)strlen(line)) {
            char *found = strstr(line + start_pos, needle);
            if (found) {
                buf->cursor_line = l;
                buf->cursor_col = (int)(found - line);
                snprintf(buf->message, sizeof(buf->message), "Found \"%s\" at line %d, col %d", needle, l + 1, buf->cursor_col + 1);
                return true;
            }
        }
    }
    snprintf(buf->message, sizeof(buf->message), "String \"%s\" not found", needle);
    return false;
}

static int Iris_Replace(EditorBuffer *buf, const char *old_str, const char *new_str) {
    if (buf->readonly || !old_str || !old_str[0]) return 0;
    const char *replacement = new_str ? new_str : "";
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(replacement);
    int replaces = 0;

    for (int l = 0; l < buf->line_count; ++l) {
        char *p = buf->lines[l];
        char *match = NULL;
        while ((match = strstr(p, old_str)) != NULL) {
            size_t prefix_len = (size_t)(match - buf->lines[l]);
            size_t suffix_len = strlen(match + old_len);
            char *new_line = (char*)malloc(prefix_len + new_len + suffix_len + 1);
            if (!new_line) break;

            memcpy(new_line, buf->lines[l], prefix_len);
            memcpy(new_line + prefix_len, replacement, new_len);
            memcpy(new_line + prefix_len + new_len, match + old_len, suffix_len + 1);

            free(buf->lines[l]);
            buf->lines[l] = new_line;
            p = new_line + prefix_len + new_len;
            replaces++;
            buf->modified = true;
        }
    }
    snprintf(buf->message, sizeof(buf->message), "Replaced %d occurrence%s of \"%s\"", replaces, replaces == 1 ? "" : "s", old_str);
    return replaces;
}

/* ----------------------------------------------------------------------------
 * Command Execution / Script Engine
 * ---------------------------------------------------------------------------- */
static const char* Iris_NextToken(const char *p, char *out, size_t out_size) {
    if (!p) return NULL;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) {
        if (out_size > 0) out[0] = '\0';
        return NULL;
    }
    size_t idx = 0;
    if (*p == '"') {
        p++; /* skip opening quote */
        while (*p && *p != '"' && idx + 1 < out_size) {
            out[idx++] = *p++;
        }
        if (*p == '"') p++; /* skip closing quote */
    } else {
        while (*p && !isspace((unsigned char)*p) && idx + 1 < out_size) {
            out[idx++] = *p++;
        }
    }
    out[idx] = '\0';
    return p;
}

static int Iris_ExecuteScriptLine(EditorBuffer *buf, const char *cmd_line, bool *should_exit);

static int Iris_ExecuteScriptFile(EditorBuffer *buf, const char *script_path, bool *should_exit) {
    FILE *f = fopen(script_path, "r");
    if (!f) {
        snprintf(buf->message, sizeof(buf->message), "Unable to open script file %s", script_path);
        return 1;
    }
    char line[WCL_MAX_INPUT];
    int res = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) line[--len] = '\0';
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '!' || *p == '\0') continue;
        res = Iris_ExecuteScriptLine(buf, p, should_exit);
        if (*should_exit) break;
    }
    fclose(f);
    snprintf(buf->message, sizeof(buf->message), "Script %s executed", script_path);
    return res;
}

static int Iris_ExecuteScriptLine(EditorBuffer *buf, const char *cmd_line, bool *should_exit) {
    if (!cmd_line || !cmd_line[0]) return 0;

    char verb[64] = "";
    char arg1[MAX_PATH] = "";
    char arg2[MAX_PATH] = "";

    const char *p = Iris_NextToken(cmd_line, verb, sizeof(verb));
    p = Iris_NextToken(p, arg1, sizeof(arg1));
    p = Iris_NextToken(p, arg2, sizeof(arg2));

    if (MatchesPrefix(verb, "SAVE") || MatchesPrefix(verb, "WRITE")) {
        Buffer_SaveFile(buf, arg1[0] ? arg1 : NULL);
        return 0;
    }
    if (MatchesPrefix(verb, "OPEN") || MatchesPrefix(verb, "GET")) {
        if (arg1[0]) Buffer_LoadFile(buf, arg1);
        return 0;
    }
    if (MatchesPrefix(verb, "RENAME")) {
        if (arg1[0]) {
            CopyString(buf->filename, sizeof(buf->filename), arg1);
            snprintf(buf->message, sizeof(buf->message), "Buffer target renamed to %s", arg1);
        }
        return 0;
    }
    if (MatchesPrefix(verb, "SELECT")) {
        buf->select_active = true;
        buf->sel_start_line = buf->cursor_line;
        buf->sel_start_col = buf->cursor_col;
        snprintf(buf->message, sizeof(buf->message), "Selection started at line %d", buf->cursor_line + 1);
        return 0;
    }
    if (MatchesPrefix(verb, "COPY")) {
        Iris_Copy(buf);
        return 0;
    }
    if (MatchesPrefix(verb, "CUT") || MatchesPrefix(verb, "REMOVE")) {
        Iris_Cut(buf);
        return 0;
    }
    if (MatchesPrefix(verb, "PASTE")) {
        Iris_Paste(buf);
        return 0;
    }
    if (MatchesPrefix(verb, "INSERT")) {
        if (arg1[0] && !MatchesPrefix(arg1, "HERE")) {
            for (size_t i = 0; arg1[i]; ++i) {
                Buffer_InsertChar(buf, arg1[i]);
            }
        } else {
            Iris_Paste(buf);
        }
        return 0;
    }
    if (MatchesPrefix(verb, "FIND")) {
        if (arg1[0]) Iris_Find(buf, arg1);
        return 0;
    }
    if (MatchesPrefix(verb, "REPLACE")) {
        if (arg1[0]) Iris_Replace(buf, arg1, arg2[0] ? arg2 : "");
        return 0;
    }
    if (MatchesPrefix(verb, "TOP")) {
        buf->cursor_line = 0;
        buf->cursor_col = 0;
        buf->top_line = 0;
        return 0;
    }
    if (MatchesPrefix(verb, "BOTTOM")) {
        buf->cursor_line = buf->line_count - 1;
        buf->cursor_col = 0;
        return 0;
    }
    if (MatchesPrefix(verb, "GOTO") || MatchesPrefix(verb, "LINE")) {
        if (arg1[0]) {
            int l = atoi(arg1) - 1;
            if (l < 0) l = 0;
            if (l >= buf->line_count) l = buf->line_count - 1;
            buf->cursor_line = l;
            buf->cursor_col = 0;
        }
        return 0;
    }
    if (MatchesPrefix(verb, "SCRIPT")) {
        if (arg1[0]) Iris_ExecuteScriptFile(buf, arg1, should_exit);
        return 0;
    }
    if (MatchesPrefix(verb, "EVAL")) {
        const char *p = strchr(cmd_line, ' ');
        if (p) Iris_ExecuteScriptLine(buf, p + 1, should_exit);
        return 0;
    }
    if (MatchesPrefix(verb, "EXIT")) {
        Buffer_SaveFile(buf, NULL);
        *should_exit = true;
        return 0;
    }
    if (MatchesPrefix(verb, "QUIT")) {
        *should_exit = true;
        return 0;
    }
    if (MatchesPrefix(verb, "HELP")) {
        snprintf(buf->message, sizeof(buf->message), "Type commands: OPEN, SAVE, RENAME, COPY, CUT, PASTE, FIND, REPLACE, SCRIPT, EXIT");
        return 0;
    }

    snprintf(buf->message, sizeof(buf->message), "Unknown IRIS command: %s", verb);
    return 1;
}

/* ----------------------------------------------------------------------------
 * Interactive Screen Rendering & Terminal Loop
 * ---------------------------------------------------------------------------- */
static void Iris_RenderHelp(HANDLE hOut, int screen_rows, int screen_cols) {
    (void)screen_rows; (void)screen_cols;
    COORD home = { 0, 0 };
    SetConsoleCursorPosition(hOut, home);

    printf("\x1b[2J\x1b[H"); /* Clear screen */
    printf("============================= IRIS EDITOR HELP =============================\n");
    printf("IRIS is an interactive, screen-oriented text editor inspired by DEC EVE.\n\n");
    printf("Navigation & Editing Keys:\n");
    printf("  Arrows / Home / End        Move cursor (character, line, start, end)\n");
    printf("  Page Up / Page Down        Scroll buffer up or down by one screen\n");
    printf("  Ctrl+B or Esc              Activate the 'Command: ' prompt\n");
    printf("  Ctrl+K                     Cut (Kill) current line or active selection\n");
    printf("  Ctrl+Y                     Paste (Yank) clipboard contents into buffer\n");
    printf("  Ctrl+S                     Quick Save buffer to current file\n");
    printf("  Ctrl+Z                     Exit and Save changes to disk\n");
    printf("  Ctrl+Q                     Quit and Discard modifications\n");
    printf("  Ctrl+G                     Toggle this Help screen\n\n");
    printf("IRIS Built-In Commands (at 'Command: ' prompt):\n");
    printf("  OPEN <filespec>            Loads a file into the current buffer\n");
    printf("  SAVE [filespec]            Writes buffer contents to disk (versioned)\n");
    printf("  RENAME <filespec>          Changes the active buffer's target file name\n");
    printf("  SELECT                     Sets selection start mark (highlight mode)\n");
    printf("  COPY                       Copies selected text to the IRIS pasteboard\n");
    printf("  CUT / REMOVE               Deletes selected text and stores in pasteboard\n");
    printf("  PASTE / INSERT HERE        Inserts pasteboard text at cursor position\n");
    printf("  FIND <text>                Searches for target string forward\n");
    printf("  REPLACE <old> <new>        Replaces text throughout buffer\n");
    printf("  SCRIPT <file.iris>         Executes an IRIS automation script file\n");
    printf("  EVAL <command>             Executes a single IRIS editing command script\n");
    printf("  EXIT                       Saves buffer and returns to WCL\n");
    printf("  QUIT                       Exits without saving\n");
    printf("============================================================================\n");
    printf("Press Esc or RETURN to return to editing buffer...\n");
    fflush(stdout);
}

static void Iris_Render(HANDLE hOut, EditorBuffer *buf, int screen_rows, int screen_cols) {
    int text_rows = screen_rows - 3;
    if (text_rows < 1) text_rows = 1;

    /* Adjust scrolling */
    if (buf->cursor_line < buf->top_line) buf->top_line = buf->cursor_line;
    if (buf->cursor_line >= buf->top_line + text_rows) {
        buf->top_line = buf->cursor_line - text_rows + 1;
    }

    COORD home = { 0, 0 };
    SetConsoleCursorPosition(hOut, home);

    /* Render lines */
    for (int r = 0; r < text_rows; ++r) {
        int l = buf->top_line + r;
        if (l < buf->line_count) {
            printf("%4d  %-*.*s\x1b[K\n", l + 1, screen_cols - 6, screen_cols - 6, buf->lines[l]);
        } else {
            printf("~     \x1b[K\n");
        }
    }

    /* Status Bar */
    printf("\x1b[7mBuffer: %-20.20s %s   Insert   Forward   Line: %-4d Col: %-3d\x1b[0m\x1b[K\n",
        buf->filename, buf->modified ? "Modified" : "Write   ", buf->cursor_line + 1, buf->cursor_col + 1);

    /* Message Line */
    printf("%-*.*s\x1b[K", screen_cols, screen_cols, buf->message[0] ? buf->message : "");
    fflush(stdout);

    /* Position cursor */
    int cur_y = buf->cursor_line - buf->top_line;
    int cur_x = buf->cursor_col + 6;
    if (cur_x >= screen_cols) cur_x = screen_cols - 1;
    COORD cur_pos = { (SHORT)cur_x, (SHORT)cur_y };
    SetConsoleCursorPosition(hOut, cur_pos);
}

static void Iris_RunInteractive(EditorBuffer *buf) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    DWORD orig_in_mode = 0, orig_out_mode = 0;
    GetConsoleMode(hIn, &orig_in_mode);
    GetConsoleMode(hOut, &orig_out_mode);

    /* Disable processed input (Ctrl+C, Ctrl+S/XOFF, Ctrl+Z) and line buffering */
    DWORD raw_in = orig_in_mode;
    raw_in &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_MOUSE_INPUT);
    raw_in |= ENABLE_WINDOW_INPUT;
    SetConsoleMode(hIn, raw_in);
    SetConsoleMode(hOut, orig_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOut, &csbi);
    int screen_rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    int screen_cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    if (screen_rows < 10) screen_rows = 24;
    if (screen_cols < 40) screen_cols = 80;

    bool running = true;
    bool in_help = false;

    while (running) {
        if (in_help) {
            Iris_RenderHelp(hOut, screen_rows, screen_cols);
        } else {
            Iris_Render(hOut, buf, screen_rows, screen_cols);
        }

        INPUT_RECORD ir;
        DWORD read_count = 0;
        if (!ReadConsoleInputA(hIn, &ir, 1, &read_count) || !read_count) break;

        if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            CHAR ch = ir.Event.KeyEvent.uChar.AsciiChar;
            DWORD ctrl = ir.Event.KeyEvent.dwControlKeyState;
            bool is_ctrl = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

            if (in_help) {
                in_help = false;
                continue;
            }

            buf->message[0] = '\0'; /* Clear message on action */

            /* Detect Ctrl combination reliably across Windows console variants */
            char ctrl_letter = 0;
            if (is_ctrl) {
                if (vk >= 'A' && vk <= 'Z') {
                    ctrl_letter = (char)tolower((unsigned char)vk);
                } else if (ch >= 1 && ch <= 26) {
                    ctrl_letter = (char)('a' + (ch - 1));
                }
            } else if (ch > 0 && ch <= 26 && ch != '\t' && ch != '\r' && ch != '\n' && ch != '\b') {
                ctrl_letter = (char)('a' + (ch - 1));
            }

            if (ctrl_letter != 0) {
                switch (ctrl_letter) {
                case 's':
                    Buffer_SaveFile(buf, NULL);
                    break;
                case 'z':
                    Buffer_SaveFile(buf, NULL);
                    running = false;
                    break;
                case 'q':
                    running = false;
                    break;
                case 'k':
                    Iris_Cut(buf);
                    break;
                case 'y':
                    Iris_Paste(buf);
                    break;
                case 'c':
                    Iris_Copy(buf);
                    break;
                case 'g':
                    in_help = true;
                    break;
                case 'a':
                    buf->select_active = !buf->select_active;
                    if (buf->select_active) {
                        buf->sel_start_line = buf->cursor_line;
                        buf->sel_start_col = buf->cursor_col;
                        snprintf(buf->message, sizeof(buf->message), "Selection mark set at line %d", buf->cursor_line + 1);
                    } else {
                        snprintf(buf->message, sizeof(buf->message), "Selection mark cancelled");
                    }
                    break;
                case 'b':
                    goto handle_command_prompt;
                }
                continue;
            }

            if (vk == VK_ESCAPE) {
            handle_command_prompt:;
                /* Command Prompt */
                COORD cmd_pos = { 0, (SHORT)(screen_rows - 1) };
                SetConsoleCursorPosition(hOut, cmd_pos);
                printf("\x1b[KCommand: ");
                fflush(stdout);

                char cmd_buf[128] = "";
                int idx = 0;
                while (1) {
                    INPUT_RECORD cir;
                    DWORD cr_count = 0;
                    if (!ReadConsoleInputA(hIn, &cir, 1, &cr_count) || !cr_count) break;
                    if (cir.EventType == KEY_EVENT && cir.Event.KeyEvent.bKeyDown) {
                        CHAR cch = cir.Event.KeyEvent.uChar.AsciiChar;
                        WORD cvk = cir.Event.KeyEvent.wVirtualKeyCode;
                        DWORD cctrl = cir.Event.KeyEvent.dwControlKeyState;
                        bool cis_ctrl = (cctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

                        if (cvk == VK_RETURN) { break; }
                        if (cvk == VK_ESCAPE || (cis_ctrl && (cvk == 'G' || cvk == 'C' || cch == 3 || cch == 7))) {
                            cmd_buf[0] = '\0';
                            break;
                        }
                        if (cvk == VK_BACK && idx > 0) {
                            cmd_buf[--idx] = '\0';
                            printf("\b \b");
                            fflush(stdout);
                        } else if (cch >= 32 && cch < 127 && idx < (int)sizeof(cmd_buf) - 1) {
                            cmd_buf[idx++] = cch;
                            cmd_buf[idx] = '\0';
                            putchar(cch);
                            fflush(stdout);
                        }
                    }
                }
                if (cmd_buf[0]) {
                    bool should_exit = false;
                    Iris_ExecuteScriptLine(buf, cmd_buf, &should_exit);
                    if (should_exit) running = false;
                }
            } else {
                switch (vk) {
                case VK_UP:
                    if (buf->cursor_line > 0) buf->cursor_line--;
                    break;
                case VK_DOWN:
                    if (buf->cursor_line < buf->line_count - 1) buf->cursor_line++;
                    break;
                case VK_LEFT:
                    if (buf->cursor_col > 0) buf->cursor_col--;
                    break;
                case VK_RIGHT:
                    if (buf->cursor_col < (int)strlen(buf->lines[buf->cursor_line])) buf->cursor_col++;
                    break;
                case VK_HOME:
                    buf->cursor_col = 0;
                    break;
                case VK_END:
                    buf->cursor_col = (int)strlen(buf->lines[buf->cursor_line]);
                    break;
                case VK_PRIOR: /* Page Up */
                    buf->cursor_line -= (screen_rows - 3);
                    if (buf->cursor_line < 0) buf->cursor_line = 0;
                    break;
                case VK_NEXT: /* Page Down */
                    buf->cursor_line += (screen_rows - 3);
                    if (buf->cursor_line >= buf->line_count) buf->cursor_line = buf->line_count - 1;
                    break;
                case VK_RETURN:
                    Buffer_SplitLine(buf);
                    break;
                case VK_BACK:
                    Buffer_Backspace(buf);
                    break;
                case VK_DELETE:
                    Buffer_DeleteChar(buf);
                    break;
                default:
                    if (ch >= 32 && ch < 127) {
                        Buffer_InsertChar(buf, ch);
                    }
                    break;
                }
            }
        }
    }

    SetConsoleMode(hIn, orig_in_mode);
    SetConsoleMode(hOut, orig_out_mode);
    printf("\x1b[2J\x1b[H"); /* Clear on exit */
}

/* ----------------------------------------------------------------------------
 * WCL Command Integration
 * ---------------------------------------------------------------------------- */
typedef struct IrisCommand { struct ICommand base; } IrisCommand;

static int IrisCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    const char *filespec = (argc > 1 && argv[1][0] != '/') ? argv[1] : "MAIN.TXT";

    EditorBuffer buf;
    Buffer_Init(&buf, filespec);

    if (GetFileAttributesA(filespec) != INVALID_FILE_ATTRIBUTES) {
        Buffer_LoadFile(&buf, filespec);
    } else {
        snprintf(buf.message, sizeof(buf.message), "New buffer: %s", filespec);
    }

    if (quals->HasQualifier(quals, "READONLY")) buf.readonly = true;

    /* Execute /SCRIPT if specified */
    const char *script_file = GetQualifierValueByPrefix(quals, "SCRIPT");
    if (script_file && script_file[0]) {
        bool should_exit = false;
        Iris_ExecuteScriptFile(&buf, script_file, &should_exit);
        const char *out_file = GetQualifierValueByPrefix(quals, "OUTPUT");
        Buffer_SaveFile(&buf, (out_file && out_file[0]) ? out_file : NULL);
        Buffer_Free(&buf);
        return 0;
    }

    /* Check if interactive console is available */
    DWORD mode = 0;
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (!GetConsoleMode(hIn, &mode)) {
        /* Non-interactive pipe or batch execution */
        const char *out_file = GetQualifierValueByPrefix(quals, "OUTPUT");
        Buffer_SaveFile(&buf, (out_file && out_file[0]) ? out_file : NULL);
        printf("%%IRIS-S-BATCH, IRIS non-interactive processing complete for %s\n", buf.filename);
        Buffer_Free(&buf);
        return 0;
    }

    /* Interactive editing mode */
    Iris_RunInteractive(&buf);

    const char *out_file = GetQualifierValueByPrefix(quals, "OUTPUT");
    if (out_file && out_file[0]) {
        Buffer_SaveFile(&buf, out_file);
    }

    printf("%%IRIS-S-SAVED, %d lines in %s\n", buf.line_count, buf.filename);
    Buffer_Free(&buf);
    return 0;
}

static const char* IrisCmd_GetVerb(ICommand *self) { (void)self; return "IRIS"; }
static const char* IrisCmd_GetSummary(ICommand *self) { (void)self; return "Interactive screen editor based on DEC EVE / TPU."; }
static const char* IrisCmd_GetHelp(ICommand *self) {
    (void)self;
    return "IRIS [file-spec] [/SCRIPT=file] [/OUTPUT=file] [/READONLY]\n"
           "  Interactive full-screen text editor (DEC EVE clone).\n"
           "  Navigation: Arrows, Home, End, PageUp, PageDown.\n"
           "  Editing: Enter, Backspace, Delete.\n"
           "  Keys: Ctrl+B (Command: prompt), Ctrl+S (Save), Ctrl+Z (Exit), Ctrl+Q (Quit), Ctrl+G (Help).\n"
           "  Commands: OPEN, SAVE, RENAME, SELECT, COPY, CUT, PASTE, FIND, REPLACE, SCRIPT, EVAL, EXIT.";
}

static const char* EditCmd_GetVerb(ICommand *self) { (void)self; return "EDIT"; }
static const char* EditCmd_GetSummary(ICommand *self) { (void)self; return "Invokes the IRIS / EVE text editor."; }
static const char* EditCmd_GetHelp(ICommand *self) {
    (void)self;
    return "EDIT [file-spec] [/IRIS] [/TPU] [/EVE] [/SCRIPT=file] [/READONLY]\n"
           "  Invokes the IRIS screen editor (OpenVMS EVE clone).";
}

static const ICommandVTable Iris_VTable = { { NULL, NULL }, IrisCmd_Execute, IrisCmd_GetVerb, IrisCmd_GetSummary, IrisCmd_GetHelp };
static const ICommandVTable Edit_VTable = { { NULL, NULL }, IrisCmd_Execute, EditCmd_GetVerb, EditCmd_GetSummary, EditCmd_GetHelp };

void RegisterIrisCommand(CommandRegistry *registry) {
    static IrisCommand iris = { { &Iris_VTable, 1 } };
    static IrisCommand edit = { { &Edit_VTable, 1 } };
    registry->Register(registry, (ICommand*)&iris);
    registry->Register(registry, (ICommand*)&edit);
}
