#ifndef WCL_PREAMBLE_H
#define WCL_PREAMBLE_H

#include "../include/wcl_internal.h"

void InitializeExecutionContext(ExecutionContext *ctx);
bool ShutdownExecutionContext(ExecutionContext *ctx);
void ClearConsoleScreen(ExecutionContext *ctx);

#endif
