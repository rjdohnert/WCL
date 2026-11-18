#ifndef WCL_COMMANDS_DIRECTORY_H
#define WCL_COMMANDS_DIRECTORY_H

#include "../include/wcl_internal.h"

int DirCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* DirCmd_GetVerb(ICommand *self);
const char* DirCmd_GetSummary(ICommand *self);
const char* DirCmd_GetHelp(ICommand *self);

void RegisterDirectoryCommand(CommandRegistry *registry);

#endif /* WCL_COMMANDS_DIRECTORY_H */
