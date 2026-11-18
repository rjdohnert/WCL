#ifndef WCL_CORE_EXECUTION_H
#define WCL_CORE_EXECUTION_H

#include "../include/wcl_internal.h"

int LaunchExternalApplication(ExecutionContext *ctx, const char *command);
int ExecuteCommandLine(ExecutionContext *ctx, CommandRegistry *registry, const char *line, bool from_script);

#endif /* WCL_CORE_EXECUTION_H */
