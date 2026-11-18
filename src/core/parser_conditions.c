#include "../include/wcl_internal.h"

bool MatchesPrefix(const char *input, const char *target) {
    size_t inLen = strlen(input);
    size_t targetLen = strlen(target);
    if (inLen > targetLen || inLen == 0) return false;
    for (size_t i = 0; i < inLen; ++i) {
        if (toupper((unsigned char)input[i]) != toupper((unsigned char)target[i])) return false;
    }
    return true;
}

bool MatchesWildcard(const char *pattern, const char *text) {
    const char *star = NULL;
    const char *retry = NULL;
    while (*text) {
        if (*pattern == '?' || toupper((unsigned char)*pattern) == toupper((unsigned char)*text)) {
            pattern++;
            text++;
        } else if (*pattern == '*') {
            star = pattern++;
            retry = text;
        } else if (star) {
            pattern = star + 1;
            text = ++retry;
        } else {
            return false;
        }
    }
    while (*pattern == '*') pattern++;
    return *pattern == '\0';
}

void FormatVmsTime(const FILETIME *ft, char *buf, size_t len) {
    FILETIME local_ft;
    FileTimeToLocalFileTime(ft, &local_ft);
    SYSTEMTIME st;
    FileTimeToSystemTime(&local_ft, &st);

    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };
    int monthIndex = (st.wMonth >= 1 && st.wMonth <= 12) ? st.wMonth - 1 : 0;

    snprintf(buf, len, "%02d-%s-%04d %02d:%02d:%02d",
        st.wDay, months[monthIndex], st.wYear, st.wHour, st.wMinute, st.wSecond);
}

void FormatFileSizeMetric(ULONGLONG bytes, char *buf, size_t len) {
    if (bytes == 0) {
        snprintf(buf, len, "0 KB");
    } else if (bytes < 1024 * 1024) {
        double kb = (double)bytes / 1024.0;
        if (kb < 0.01) kb = 0.01;
        snprintf(buf, len, "%.2f KB", kb);
    } else if (bytes < 1024ULL * 1024 * 1024) {
        double mb = (double)bytes / (1024.0 * 1024.0);
        snprintf(buf, len, "%.2f MB", mb);
    } else {
        double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
        snprintf(buf, len, "%.2f GB", gb);
    }
}

/* ----------------------------------------------------------------------------
 * Lexer and Parser (Qualifiers & Parameter Splitting)
 * ---------------------------------------------------------------------------- */
bool ParseCommandLine(char *line, int *argc, char **argv, QualifierList *quals) {
    *argc = 0;
    quals->count = 0;

    char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;

    while (*p) {
        if (*p == '/') {
            p++;
            int qidx = 0;
            if (quals->count >= WCL_MAX_QUALIFIERS) return false;
            Qualifier *q = &quals->items[quals->count];
            while (*p && !isspace((unsigned char)*p) && *p != '/' && *p != '=' && qidx < (int)sizeof(q->name) - 1) {
                q->name[qidx++] = *p++;
            }
            if (qidx == 0 || (*p && !isspace((unsigned char)*p) && *p != '/' && *p != '=')) return false;
            q->name[qidx] = '\0';
            q->has_value = false;
            q->value[0] = '\0';

            if (*p == '=') {
                p++;
                int vidx = 0;
                bool quoted_value = *p == '"';
                if (quoted_value) p++;
                while (*p && (quoted_value ? *p != '"' :
                    !isspace((unsigned char)*p) && *p != '/') && vidx < MAX_PATH - 1) {
                    q->value[vidx++] = *p++;
                }
                if (quoted_value) {
                    if (*p != '"') return false;
                    p++;
                }
                if (*p && !isspace((unsigned char)*p) && *p != '/') return false;
                q->value[vidx] = '\0';
                q->has_value = true;
            }
            quals->count++;
        } else if (*p == '|' || *p == ';') {
            if (*argc >= WCL_MAX_ARGS) return false;
            char op[3] = {0};
            op[0] = *p++;
            if (op[0] == '|' && *p == '|') {
                op[1] = *p++;
            }
            argv[*argc] = _strdup(op);
            if (!argv[*argc]) return false;
            (*argc)++;
        } else if (*p == '&' && *(p + 1) == '&') {
            if (*argc >= WCL_MAX_ARGS) return false;
            argv[*argc] = _strdup("&&");
            if (!argv[*argc]) return false;
            (*argc)++;
            p += 2;
        } else {
            if (*argc >= WCL_MAX_ARGS) return false;
            char arg_tok[MAX_PATH];
            int aidx = 0;
            bool in_url = (_strnicmp(p, "http://", 7) == 0 || _strnicmp(p, "https://", 8) == 0 || _strnicmp(p, "ftp://", 6) == 0);
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && aidx < MAX_PATH - 1) {
                    arg_tok[aidx++] = *p++;
                }
                if (*p != '"') return false;
                p++;
            } else if (in_url) {
                while (*p && !isspace((unsigned char)*p) && aidx < MAX_PATH - 1) {
                    arg_tok[aidx++] = *p++;
                }
            } else {
                while (*p && !isspace((unsigned char)*p) && *p != '/' && *p != '|' && *p != ';' && aidx < MAX_PATH - 1) {
                    arg_tok[aidx++] = *p++;
                }
                if (*p && !isspace((unsigned char)*p) && *p != '/' && *p != '|' && *p != ';') return false;
            }
            arg_tok[aidx] = '\0';
            argv[*argc] = _strdup(arg_tok);
            if (!argv[*argc]) return false;
            (*argc)++;
        }
        while (*p && isspace((unsigned char)*p)) p++;
    }
    return true;
}

char* TrimWhitespace(char *text) {
    while (*text && isspace((unsigned char)*text)) text++;
    size_t length = strlen(text);
    while (length && isspace((unsigned char)text[length - 1])) text[--length] = '\0';
    return text;
}

const char* FindDclOperator(const char *text, const char *op) {
    bool quoted = false;
    int depth = 0;
    size_t op_length = strlen(op);
    for (const char *p = text; *p; ++p) {
        if (*p == '"') quoted = !quoted;
        else if (!quoted && *p == '(') depth++;
        else if (!quoted && *p == ')' && depth > 0) depth--;
        if (!quoted && depth == 0 && _strnicmp(p, op, op_length) == 0) return p;
    }
    return NULL;
}

bool IsWrappedCondition(const char *text, size_t length) {
    if (length < 2 || text[0] != '(' || text[length - 1] != ')') return false;
    int depth = 0;
    bool quoted = false;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == '"') quoted = !quoted;
        else if (!quoted && text[i] == '(') depth++;
        else if (!quoted && text[i] == ')') {
            if (--depth < 0) return false;
            if (depth == 0 && i != length - 1) return false;
        }
    }
    return depth == 0 && !quoted;
}

bool EvaluateConditionTruth(ExecutionContext *ctx, char *text, bool *result) {
    text = TrimWhitespace(text);
    size_t length = strlen(text);
    bool quoted = length >= 2 && text[0] == '"' && text[length - 1] == '"';
    if (quoted) {
        text[length - 1] = '\0';
        text++;
    }

    char value[WCL_MAX_INPUT];
    const char *symbol = quoted ? NULL : GetSymbol(ctx, text);
    CopyString(value, sizeof(value), symbol ? symbol : text);
    char *value_text = TrimWhitespace(value);
    if (*value_text == 'Y' || *value_text == 'y' || *value_text == 'T' || *value_text == 't') {
        *result = true;
        return true;
    }

    errno = 0;
    char *end = NULL;
    long long number = strtoll(value_text, &end, 10);
    if (end != value_text && end && *end == '\0') {
        if (errno == ERANGE) return false;
        *result = number % 2 != 0;
        return true;
    }
    *result = false;
    return true;
}

bool EvaluateCondition(ExecutionContext *ctx, const char *expression, bool *result, int depth) {
    if (depth > 16) return false;
    char buffer[WCL_MAX_INPUT];
    CopyString(buffer, sizeof(buffer), expression);
    char *text = TrimWhitespace(buffer);
    size_t length = strlen(text);
    while (IsWrappedCondition(text, length)) {
        text[length - 1] = '\0';
        text = TrimWhitespace(text + 1);
        length = strlen(text);
    }

    const char *op = FindDclOperator(text, ".OR.");
    if (op) {
        char left[WCL_MAX_INPUT];
        size_t left_length = (size_t)(op - text);
        if (left_length >= sizeof(left)) return false;
        memcpy(left, text, left_length);
        left[left_length] = '\0';
        bool left_value, right_value;
        if (!EvaluateCondition(ctx, left, &left_value, depth + 1)) return false;
        if (left_value) {
            *result = true;
            return true;
        }
        return EvaluateCondition(ctx, op + 4, result, depth + 1);
    }

    op = FindDclOperator(text, ".AND.");
    if (op) {
        char left[WCL_MAX_INPUT];
        size_t left_length = (size_t)(op - text);
        if (left_length >= sizeof(left)) return false;
        memcpy(left, text, left_length);
        left[left_length] = '\0';
        bool left_value;
        if (!EvaluateCondition(ctx, left, &left_value, depth + 1)) return false;
        if (!left_value) {
            *result = false;
            return true;
        }
        return EvaluateCondition(ctx, op + 5, result, depth + 1);
    }

    if (_strnicmp(text, ".NOT.", 5) == 0) {
        if (!EvaluateCondition(ctx, text + 5, result, depth + 1)) return false;
        *result = !*result;
        return true;
    }

    const char *operators[] = {
        ".EQS.", ".NES.", ".GEQ.", ".GTR.", ".LEQ.", ".LSS.",
        ".EQ.", ".NE.", ".GE.", ".GT.", ".LE.", ".LT."
    };
    const char *comparison = NULL;
    const char *comparison_op = NULL;
    for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); ++i) {
        comparison = FindDclOperator(text, operators[i]);
        if (comparison) {
            comparison_op = operators[i];
            break;
        }
    }
    if (!comparison) {
        return EvaluateConditionTruth(ctx, text, result);
    }

    char left[WCL_MAX_INPUT];
    char right[WCL_MAX_INPUT];
    size_t left_length = (size_t)(comparison - text);
    if (left_length >= sizeof(left)) return false;
    memcpy(left, text, left_length);
    left[left_length] = '\0';
    CopyString(right, sizeof(right), comparison + strlen(comparison_op));
    char *left_value = TrimWhitespace(left);
    char *right_value = TrimWhitespace(right);
    size_t left_value_length = strlen(left_value);
    size_t right_value_length = strlen(right_value);
    bool left_quoted = left_value_length >= 2 && left_value[0] == '"' &&
        left_value[left_value_length - 1] == '"';
    bool right_quoted = right_value_length >= 2 && right_value[0] == '"' &&
        right_value[right_value_length - 1] == '"';
    if (left_quoted) {
        left_value[left_value_length - 1] = '\0';
        left_value++;
    }
    if (right_quoted) {
        right_value[right_value_length - 1] = '\0';
        right_value++;
    }
    const char *left_symbol = left_quoted ? NULL : GetSymbol(ctx, left_value);
    const char *right_symbol = right_quoted ? NULL : GetSymbol(ctx, right_value);
    if (left_symbol) left_value = (char*)left_symbol;
    if (right_symbol) right_value = (char*)right_symbol;

    bool string_compare = strstr(comparison_op, "S") != NULL;
    if (string_compare) {
        int value = strcmp(left_value, right_value);
        *result = strcmp(comparison_op, ".EQS.") == 0 ? value == 0 : value != 0;
        return true;
    }

    errno = 0;
    char *left_end = NULL;
    char *right_end = NULL;
    long long left_number = strtoll(left_value, &left_end, 10);
    if (errno == ERANGE) return false;
    errno = 0;
    long long right_number = strtoll(right_value, &right_end, 10);
    if (errno == ERANGE || !left_value[0] || !right_value[0] || !left_end || *left_end ||
        !right_end || *right_end) return false;
    if (strcmp(comparison_op, ".EQ.") == 0) *result = left_number == right_number;
    else if (strcmp(comparison_op, ".NE.") == 0) *result = left_number != right_number;
    else if (strcmp(comparison_op, ".GT.") == 0 || strcmp(comparison_op, ".GTR.") == 0) *result = left_number > right_number;
    else if (strcmp(comparison_op, ".GE.") == 0 || strcmp(comparison_op, ".GEQ.") == 0) *result = left_number >= right_number;
    else if (strcmp(comparison_op, ".LT.") == 0 || strcmp(comparison_op, ".LSS.") == 0) *result = left_number < right_number;
    else *result = left_number <= right_number;
    return true;
}

