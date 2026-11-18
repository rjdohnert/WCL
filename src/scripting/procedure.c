#include "../include/wcl_internal.h"

char* GetScriptText(const char *line, char *buffer) {
    CopyString(buffer, WCL_MAX_INPUT, line);
    char *text = TrimWhitespace(buffer);
    if (text[0] == '$') {
        text = TrimWhitespace(text + 1);
        if (text[0] == '!') return NULL;
    }
    return text[0] ? text : NULL;
}

bool IsScriptLabel(const char *text, char *label, size_t capacity) {
    size_t length = strlen(text);
    if (length < 2 || text[length - 1] != ':') return false;
    length--;
    for (size_t i = 0; i < length; ++i) {
        if (isspace((unsigned char)text[i])) return false;
    }
    if (length >= capacity) return false;
    memcpy(label, text, length);
    label[length] = '\0';
    return IsValidSymbolName(label);
}

bool FindScriptLabel(char **lines, size_t line_count, const char *name, size_t *line_index) {
    for (size_t i = 0; i < line_count; ++i) {
        char buffer[WCL_MAX_INPUT];
        char label[64];
        char *text = GetScriptText(lines[i], buffer);
        if (text && IsScriptLabel(text, label, sizeof(label)) && _stricmp(label, name) == 0) {
            *line_index = i;
            return true;
        }
    }
    return false;
}

const char* FindThenKeyword(const char *text) {
    bool quoted = false;
    int depth = 0;
    for (const char *p = text; *p; ++p) {
        if (*p == '"') quoted = !quoted;
        else if (!quoted && *p == '(') depth++;
        else if (!quoted && *p == ')' && depth > 0) depth--;
        if (!quoted && depth == 0 && _strnicmp(p, "THEN", 4) == 0 &&
            (p == text || isspace((unsigned char)p[-1])) &&
            (p[4] == '\0' || isspace((unsigned char)p[4]))) return p;
    }
    return NULL;
}

const char* AfterScriptWord(const char *text, const char *word) {
    size_t length = strlen(word);
    if (_strnicmp(text, word, length) != 0 ||
        (text[length] && !isspace((unsigned char)text[length]))) return NULL;
    text += length;
    while (*text && isspace((unsigned char)*text)) text++;
    return text;
}

bool ParseProcedureInvocation(const char *text, char *file_spec, size_t file_capacity,
    int *parameter_count, char **parameters) {
    *parameter_count = 0;
    while (*text && isspace((unsigned char)*text)) text++;
    bool quoted = *text == '"';
    if (quoted) text++;
    size_t file_length = 0;
    while (*text && (quoted ? *text != '"' : !isspace((unsigned char)*text))) {
        if (file_length + 1 >= file_capacity) return false;
        file_spec[file_length++] = *text++;
    }
    if (quoted) {
        if (*text != '"') return false;
        text++;
    }
    file_spec[file_length] = '\0';
    if (!file_length) return false;

    while (*text) {
        while (*text && isspace((unsigned char)*text)) text++;
        if (!*text) break;
        if (*parameter_count >= 8) return false;
        char parameter[MAX_PATH];
        size_t length = 0;
        quoted = *text == '"';
        if (quoted) text++;
        while (*text && (quoted ? *text != '"' : !isspace((unsigned char)*text))) {
            if (length + 1 >= sizeof(parameter)) return false;
            parameter[length++] = *text++;
        }
        if (quoted) {
            if (*text != '"') return false;
            text++;
        }
        parameter[length] = '\0';
        parameters[*parameter_count] = _strdup(parameter);
        if (!parameters[*parameter_count]) return false;
        (*parameter_count)++;
    }
    return true;
}

bool HandleProcedureControl(ExecutionContext *ctx, char **lines, size_t line_count, const char *text,
    size_t *pc, size_t *gosub_return, size_t *gosub_if_depth, size_t *gosub_depth, size_t gosub_capacity,
    size_t *if_depth, int *status, bool *handled) {
    *handled = false;
    const char *target_text = AfterScriptWord(text, "GOTO");
    bool is_gosub = false;
    if (!target_text) {
        target_text = AfterScriptWord(text, "GOSUB");
        is_gosub = target_text != NULL;
    }
    if (target_text) {
        *handled = true;
        char target_name[64];
        char expanded_name[64];
        CopyString(target_name, sizeof(target_name), target_text);
        char *trimmed = TrimWhitespace(target_name);
        if (!ExpandSymbols(ctx, trimmed, expanded_name, sizeof(expanded_name)) ||
            !expanded_name[0] || strlen(expanded_name) >= sizeof(target_name)) {
            ctx->PrintStatus(ctx, 'E', "NOLABEL", "Branch target label is invalid");
            ctx->last_status = 1;
            return false;
        }
        size_t target;
        if ((is_gosub && *gosub_depth >= gosub_capacity) ||
            !FindScriptLabel(lines, line_count, expanded_name, &target)) {
            ctx->PrintStatus(ctx, 'E', is_gosub ? "GOSUBFAIL" : "NOLABEL",
                is_gosub ? "GOSUB target was not found or the call stack is full" : "GOTO target label was not found");
            ctx->last_status = 1;
            return false;
        }
        if (is_gosub) {
            gosub_return[*gosub_depth] = *pc;
            gosub_if_depth[*gosub_depth] = *if_depth;
            (*gosub_depth)++;
        }
        *pc = target;
        *if_depth = 0;
        return true;
    }

    const char *return_text = AfterScriptWord(text, "RETURN");
    if (return_text) {
        *handled = true;
        char return_buffer[64];
        char expanded_status[64];
        CopyString(return_buffer, sizeof(return_buffer), return_text);
        char *value = TrimWhitespace(return_buffer);
        if (*value) {
            if (!ExpandSymbols(ctx, value, expanded_status, sizeof(expanded_status))) {
                ctx->PrintStatus(ctx, 'E', "PROCSYNTAX", "RETURN status contains an undefined symbol");
                ctx->last_status = 1;
                return false;
            }
            char *end = NULL;
            long code = strtol(expanded_status, &end, 10);
            if (!end || *TrimWhitespace(end) || code < 0 || (unsigned long)code > MAXDWORD) {
                ctx->PrintStatus(ctx, 'E', "PROCSYNTAX", "RETURN status must be a non-negative integer");
                ctx->last_status = 1;
                return false;
            }
            ctx->last_status = (DWORD)code;
            *status = (int)code;
        } else {
            *status = (int)ctx->last_status;
        }
        if (*gosub_depth) {
            size_t frame = --(*gosub_depth);
            *pc = gosub_return[frame];
            *if_depth = gosub_if_depth[frame];
        } else {
            ctx->procedure_exit_requested = true;
        }
    }
    return true;
}

int ExecuteProcedure(ExecutionContext *ctx, CommandRegistry *registry, const char *file_spec,
    int parameter_count, char **parameters) {
    if (ctx->script_depth >= WCL_MAX_SCRIPT_DEPTH || parameter_count > 8) {
        ctx->PrintStatus(ctx, 'E', "PROCLIMIT", "Command procedure nesting or parameter limit exceeded");
        return 1;
    }

    char path[MAX_PATH];
    if (!ResolveFileSpecification(ctx, file_spec, false, path, sizeof(path))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "Expanded procedure file specification exceeds the supported length");
        return 1;
    }
    char absolute_path[MAX_PATH];
    DWORD absolute_length = GetFullPathNameA(path, sizeof(absolute_path), absolute_path, NULL);
    if (!absolute_length || absolute_length >= sizeof(absolute_path)) {
        ctx->PrintStatus(ctx, 'E', "BADSPEC", "Unable to resolve the command procedure path");
        return 1;
    }
    FILE *file = NULL;
    if (fopen_s(&file, absolute_path, "r") != 0) {
        ctx->PrintStatus(ctx, 'E', "PROCOPEN", "Unable to open command procedure");
        return 1;
    }

    char **lines = (char**)calloc(WCL_MAX_SCRIPT_LINES, sizeof(char*));
    if (!lines) {
        fclose(file);
        ctx->PrintStatus(ctx, 'E', "PROCMEM", "Unable to allocate command procedure storage");
        return 1;
    }
    size_t line_count = 0;
    char line[WCL_MAX_INPUT];
    char logical_line[WCL_MAX_INPUT];
    size_t logical_length = 0;
    bool continuation_pending = false;
    bool read_ok = true;
    while (fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        if (length == sizeof(line) - 1 && line[length - 1] != '\n') {
            int next = fgetc(file);
            if (next != '\n' && next != EOF) {
                while (next != '\n' && next != EOF) next = fgetc(file);
                read_ok = false;
                break;
            }
        }
        while (length && (line[length - 1] == '\r' || line[length - 1] == '\n')) {
            line[--length] = '\0';
        }

        char *segment = continuation_pending ? TrimWhitespace(line) : line;
        if (continuation_pending && segment[0] == '$') segment = TrimWhitespace(segment + 1);
        size_t segment_length = strlen(segment);
        while (segment_length && isspace((unsigned char)segment[segment_length - 1])) {
            segment[--segment_length] = '\0';
        }
        bool continues = segment_length && segment[segment_length - 1] == '-';
        if (continues) segment[--segment_length] = '\0';

        if (segment_length >= sizeof(logical_line) - logical_length) {
            read_ok = false;
            break;
        }
        memcpy(logical_line + logical_length, segment, segment_length);
        logical_length += segment_length;
        logical_line[logical_length] = '\0';
        if (continues) {
            continuation_pending = true;
            continue;
        }
        if (line_count >= WCL_MAX_SCRIPT_LINES) {
            read_ok = false;
            break;
        }
        lines[line_count] = _strdup(logical_line);
        if (!lines[line_count]) {
            read_ok = false;
            break;
        }
        line_count++;
        logical_length = 0;
        continuation_pending = false;
    }
    if (continuation_pending) read_ok = false;
    if (ferror(file)) read_ok = false;
    if (fclose(file) != 0) read_ok = false;
    if (!read_ok) {
        for (size_t i = 0; i < line_count; ++i) free(lines[i]);
        free(lines);
        ctx->PrintStatus(ctx, 'E', "PROCREAD", "Unable to read procedure or procedure exceeds supported limits");
        return 1;
    }

    char parameter_names[8][4];
    char old_parameters[8][WCL_MAX_INPUT];
    bool had_parameter[8] = { false };
    for (int i = 0; i < 8; ++i) {
        snprintf(parameter_names[i], sizeof(parameter_names[i]), "P%d", i + 1);
        const char *old = GetSymbol(ctx, parameter_names[i]);
        if (old) {
            had_parameter[i] = true;
            CopyString(old_parameters[i], sizeof(old_parameters[i]), old);
        }
        const char *value = i < parameter_count ? parameters[i] : "";
        if (!SetSymbol(ctx, parameter_names[i], value)) {
            for (int j = 0; j < i; ++j) {
                if (had_parameter[j]) SetSymbol(ctx, parameter_names[j], old_parameters[j]);
                else DeleteSymbol(ctx, parameter_names[j]);
            }
            for (size_t j = 0; j < line_count; ++j) free(lines[j]);
            free(lines);
            ctx->PrintStatus(ctx, 'E', "PROCPARAM", "Unable to bind procedure parameters");
            return 1;
        }
    }

    typedef struct ScriptIfFrame {
        bool parent_active;
        bool condition;
        bool in_else;
    } ScriptIfFrame;
    ScriptIfFrame if_stack[64];
    size_t if_depth = 0;
    size_t gosub_return[64];
    size_t gosub_if_depth[64];
    size_t gosub_depth = 0;
    char error_label[64] = {0};
    char error_command[WCL_MAX_INPUT] = {0};
    size_t pc = 0;
    int status = 0;
    bool saved_abort_on_error = ctx->abort_on_error;
    bool saved_exit_request = ctx->procedure_exit_requested;
    CopyString(ctx->procedure_paths[ctx->script_depth],
        sizeof(ctx->procedure_paths[ctx->script_depth]), absolute_path);
    ctx->script_depth++;
    ctx->abort_on_error = true;
    ctx->procedure_exit_requested = false;

    while (pc < line_count && ctx->running && !ctx->procedure_exit_requested) {
        size_t current_line = pc++;
        char script_buffer[WCL_MAX_INPUT];
        char *text = GetScriptText(lines[current_line], script_buffer);
        if (!text) continue;

        char label[64];
        if (IsScriptLabel(text, label, sizeof(label))) continue;
        if (ctx->verify_procedure) printf("$ %s\n", text);

        bool active = if_depth == 0 ||
            (if_stack[if_depth - 1].parent_active &&
             (if_stack[if_depth - 1].in_else ? !if_stack[if_depth - 1].condition : if_stack[if_depth - 1].condition));

        const char *if_expression = AfterScriptWord(text, "IF");
        if (if_expression) {
            const char *then = FindThenKeyword(if_expression);
            if (!then || if_depth >= sizeof(if_stack) / sizeof(if_stack[0])) {
                ctx->PrintStatus(ctx, 'E', "PROCSYNTAX", "Malformed or excessively nested IF statement");
                status = 1;
                ctx->last_status = 1;
                if (ctx->abort_on_error) break;
                continue;
            }
            char condition[WCL_MAX_INPUT];
            size_t condition_length = (size_t)(then - if_expression);
            memcpy(condition, if_expression, condition_length);
            condition[condition_length] = '\0';
            bool condition_value = false;
            if (active) {
                char expanded_condition[WCL_MAX_INPUT];
                if (!ExpandSymbols(ctx, condition, expanded_condition, sizeof(expanded_condition)) ||
                    !EvaluateCondition(ctx, expanded_condition, &condition_value, 0)) {
                    ctx->PrintStatus(ctx, 'E', "PROCCOND", "Unable to evaluate IF expression");
                    status = 1;
                    ctx->last_status = 1;
                    if (ctx->abort_on_error) break;
                    condition_value = false;
                }
            }
            const char *then_command = then + 4;
            while (*then_command && isspace((unsigned char)*then_command)) then_command++;
            bool ran_then_command = false;
            if (*then_command) {
                if (active && condition_value) {
                    bool handled_control;
                    if (!HandleProcedureControl(ctx, lines, line_count, then_command, &pc,
                        gosub_return, gosub_if_depth, &gosub_depth,
                        sizeof(gosub_return) / sizeof(gosub_return[0]),
                        &if_depth, &status, &handled_control)) {
                        status = 1;
                    }
                    if (!handled_control) status = ExecuteCommandLine(ctx, registry, then_command, true);
                    ran_then_command = true;
                }
            } else {
                if_stack[if_depth++] = (ScriptIfFrame){ active, condition_value, false };
            }
            if (ran_then_command && status != 0 && ctx->abort_on_error) break;
            continue;
        }

        const char *else_command = AfterScriptWord(text, "ELSE");
        if (else_command) {
            if (!if_depth || if_stack[if_depth - 1].in_else) {
                ctx->PrintStatus(ctx, 'E', "PROCSYNTAX", "ELSE without a matching IF");
                status = 1;
                ctx->last_status = 1;
                if (ctx->abort_on_error) break;
                continue;
            }
            ScriptIfFrame *frame = &if_stack[if_depth - 1];
            frame->in_else = true;
            bool ran_else_command = false;
            if (*else_command && frame->parent_active && !frame->condition) {
                bool handled_control;
                if (!HandleProcedureControl(ctx, lines, line_count, else_command, &pc,
                    gosub_return, gosub_if_depth, &gosub_depth,
                    sizeof(gosub_return) / sizeof(gosub_return[0]),
                    &if_depth, &status, &handled_control)) {
                    status = 1;
                }
                if (!handled_control) status = ExecuteCommandLine(ctx, registry, else_command, true);
                ran_else_command = true;
            }
            if (ran_else_command && status != 0 && ctx->abort_on_error) break;
            continue;
        }

        const char *endif_tail = AfterScriptWord(text, "ENDIF");
        if (endif_tail) {
            if (*endif_tail || !if_depth) {
                ctx->PrintStatus(ctx, 'E', "PROCSYNTAX", "ENDIF without a matching IF");
                status = 1;
                ctx->last_status = 1;
                if (ctx->abort_on_error) break;
            } else {
                if_depth--;
            }
            continue;
        }

        if (!active) continue;

        bool handled_control;
        if (!HandleProcedureControl(ctx, lines, line_count, text, &pc, gosub_return, gosub_if_depth, &gosub_depth,
            sizeof(gosub_return) / sizeof(gosub_return[0]), &if_depth, &status, &handled_control)) {
            status = 1;
            if (ctx->abort_on_error) break;
            continue;
        }
        if (handled_control) continue;

        const char *on_tail = AfterScriptWord(text, "ON");
        const char *error_tail = on_tail ? AfterScriptWord(on_tail, "ERROR") : NULL;
        if (error_tail) {
            bool invalid_handler = false;
            const char *goto_tail = AfterScriptWord(error_tail, "GOTO");
            const char *then_tail = AfterScriptWord(error_tail, "THEN");
            if (goto_tail && *goto_tail && strlen(goto_tail) < sizeof(error_label)) {
                CopyString(error_label, sizeof(error_label), goto_tail);
                error_command[0] = '\0';
            } else if (then_tail && *then_tail) {
                if (_stricmp(then_tail, "CONTINUE") == 0) {
                    error_label[0] = '\0';
                    error_command[0] = '\0';
                    ctx->abort_on_error = false;
                } else if (strlen(then_tail) < sizeof(error_command)) {
                    CopyString(error_command, sizeof(error_command), then_tail);
                    error_label[0] = '\0';
                } else {
                    ctx->PrintStatus(ctx, 'E', "PROCSYNTAX", "ON ERROR handler is too long");
                    status = 1;
                    ctx->last_status = 1;
                    invalid_handler = true;
                }
            } else {
                ctx->PrintStatus(ctx, 'E', "PROCSYNTAX", "Use ON ERROR GOTO label or ON ERROR THEN command");
                status = 1;
                ctx->last_status = 1;
                invalid_handler = true;
            }
            if (invalid_handler && ctx->abort_on_error) break;
            continue;
        }

        status = ExecuteCommandLine(ctx, registry, text, true);
        if (status != 0) {
            if (error_label[0]) {
                size_t target;
                char failed_label[64];
                CopyString(failed_label, sizeof(failed_label), error_label);
                error_label[0] = '\0';
                if (FindScriptLabel(lines, line_count, failed_label, &target)) {
                    pc = target;
                    if_depth = 0;
                    continue;
                }
            } else if (error_command[0]) {
                char handler[WCL_MAX_INPUT];
                CopyString(handler, sizeof(handler), error_command);
                error_command[0] = '\0';
                bool handled;
                bool control_ok = HandleProcedureControl(ctx, lines, line_count, handler, &pc, gosub_return,
                    gosub_if_depth, &gosub_depth, sizeof(gosub_return) / sizeof(gosub_return[0]),
                    &if_depth, &status, &handled);
                if (!control_ok) {
                    status = 1;
                } else if (handled) {
                    continue;
                }
                if (!handled) status = ExecuteCommandLine(ctx, registry, handler, true);
                if (status == 0) continue;
            }
            if (ctx->abort_on_error) break;
        }
    }

    if (if_depth && status == 0 && ctx->running && !ctx->procedure_exit_requested) {
        ctx->PrintStatus(ctx, 'E', "PROCSYNTAX", "Command procedure ended with an unterminated IF block");
        status = 1;
        ctx->last_status = 1;
    }

    for (int i = 0; i < 8; ++i) {
        if (had_parameter[i]) SetSymbol(ctx, parameter_names[i], old_parameters[i]);
        else DeleteSymbol(ctx, parameter_names[i]);
    }
    if (!CloseProcedureFileChannels(ctx, ctx->script_depth)) {
        ctx->PrintStatus(ctx, 'E', "CLOSEFAIL", "Unable to close one or more procedure file channels cleanly");
        status = 1;
        ctx->last_status = 1;
    }
    ctx->script_depth--;
    ctx->procedure_paths[ctx->script_depth][0] = '\0';
    ctx->abort_on_error = saved_abort_on_error;
    ctx->procedure_exit_requested = saved_exit_request;
    for (size_t i = 0; i < line_count; ++i) free(lines[i]);
    free(lines);
    return status;
}

