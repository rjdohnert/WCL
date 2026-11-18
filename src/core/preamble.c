#include "preamble.h"

void InitializeExecutionContext(ExecutionContext *ctx) {
    ZeroMemory(ctx, sizeof(*ctx));
    ctx->hConsoleOut = GetStdHandle(STD_OUTPUT_HANDLE);
    ctx->hConsoleIn = GetStdHandle(STD_INPUT_HANDLE);
    ctx->running = true;
    ctx->abort_on_error = true;
    ctx->SetPrompt = Context_SetPrompt;
    ctx->SetLogical = Context_SetLogical;
    ctx->GetLogical = Context_GetLogical;
    ctx->PrintStatus = Context_PrintStatus;
    ctx->SetPrompt(ctx, "$ ");

    CONSOLE_SCREEN_BUFFER_INFO console_info;
    if (GetConsoleScreenBufferInfo(ctx->hConsoleOut, &console_info)) {
        ctx->default_text_attr = console_info.wAttributes;
    }

    /* UTF-8 Console resilience for Unicode filenames and international environments */
    ctx->orig_output_cp = GetConsoleOutputCP();
    ctx->orig_input_cp = GetConsoleCP();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    /* OpenVMS null device mappings */
    ctx->SetLogical(ctx, "NL", "NUL");
    ctx->SetLogical(ctx, "NLA0", "NUL");
}

void ClearConsoleScreen(ExecutionContext *ctx) {
    HANDLE output = (ctx && ctx->hConsoleOut) ? ctx->hConsoleOut : GetStdHandle(STD_OUTPUT_HANDLE);
    if (!output || output == INVALID_HANDLE_VALUE) output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO screen;
    if (output && GetConsoleScreenBufferInfo(output, &screen)) {
        DWORD cells = (DWORD)screen.dwSize.X * (DWORD)screen.dwSize.Y;
        DWORD written = 0;
        COORD origin = { 0, 0 };
        WORD attr = (ctx && ctx->default_text_attr) ? ctx->default_text_attr : screen.wAttributes;
        FillConsoleOutputCharacterA(output, ' ', cells, origin, &written);
        FillConsoleOutputAttribute(output, attr, cells, origin, &written);
        SetConsoleCursorPosition(output, origin);
    }
    printf("\033[2J\033[H");
    fflush(stdout);
}

bool ShutdownExecutionContext(ExecutionContext *ctx) {
    bool succeeded = true;
    if (ctx->orig_output_cp) SetConsoleOutputCP(ctx->orig_output_cp);
    if (ctx->orig_input_cp) SetConsoleCP(ctx->orig_input_cp);
    if (!CloseAllFileChannels(ctx)) {
        ctx->PrintStatus(ctx, 'E', "CLOSEFAIL", "One or more file channels could not be closed cleanly");
        succeeded = false;
    }
    if (!CloseAllFileSearchStreams(ctx)) {
        ctx->PrintStatus(ctx, 'E', "SEARCHCLOSE", "One or more file search streams could not be closed cleanly");
        succeeded = false;
    }
    return succeeded;
}
