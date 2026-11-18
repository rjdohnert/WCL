#ifndef WCL_COMMANDS_PROCESS_H
#define WCL_COMMANDS_PROCESS_H

#include "../include/wcl_internal.h"

int DefineCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
int DeassignCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* DeassignCmd_GetVerb(ICommand *self);
const char* DeassignCmd_GetSummary(ICommand *self);
const char* DeassignCmd_GetHelp(ICommand *self);
const char* DefineCmd_GetVerb(ICommand *self);
const char* DefineCmd_GetSummary(ICommand *self);
const char* DefineCmd_GetHelp(ICommand *self);
const char* AssignCmd_GetVerb(ICommand *self);
const char* AssignCmd_GetSummary(ICommand *self);
const char* AssignCmd_GetHelp(ICommand *self);
int SetCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* SetCmd_GetVerb(ICommand *self);
const char* SetCmd_GetSummary(ICommand *self);
const char* SetCmd_GetHelp(ICommand *self);
int SpawnCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* SpawnCmd_GetVerb(ICommand *self);
const char* SpawnCmd_GetSummary(ICommand *self);
const char* SpawnCmd_GetHelp(ICommand *self);

void RegisterProcessCommands(CommandRegistry *registry);

#endif /* WCL_COMMANDS_PROCESS_H */
