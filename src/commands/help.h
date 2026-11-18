#ifndef WCL_COMMANDS_HELP_H
#define WCL_COMMANDS_HELP_H

#include "../include/wcl_internal.h"

void PrintShowCatalogHelp(void);
int HelpCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* HelpCmd_GetVerb(ICommand *self);
const char* HelpCmd_GetSummary(ICommand *self);
const char* HelpCmd_GetHelp(ICommand *self);

void RegisterHelpCommand(CommandRegistry *registry);

#endif /* WCL_COMMANDS_HELP_H */
