#ifndef WCL_COMMANDS_DCL_EXTENDED_H
#define WCL_COMMANDS_DCL_EXTENDED_H

#include "../include/wcl_internal.h"
#include "../core/registry.h"

int ConnectCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
void RegisterDclExtendedCommands(CommandRegistry *registry);

#endif /* WCL_COMMANDS_DCL_EXTENDED_H */
