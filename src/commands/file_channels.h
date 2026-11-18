#ifndef WCL_COMMANDS_FILE_CHANNELS_H
#define WCL_COMMANDS_FILE_CHANNELS_H

#include "../include/wcl_internal.h"

int OpenCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* OpenCmd_GetVerb(ICommand *self);
const char* OpenCmd_GetSummary(ICommand *self);
const char* OpenCmd_GetHelp(ICommand *self);
int ReadCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* ReadCmd_GetVerb(ICommand *self);
const char* ReadCmd_GetSummary(ICommand *self);
const char* ReadCmd_GetHelp(ICommand *self);
int CloseCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* CloseCmd_GetVerb(ICommand *self);
const char* CloseCmd_GetSummary(ICommand *self);
const char* CloseCmd_GetHelp(ICommand *self);
int WriteCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* WriteCmd_GetVerb(ICommand *self);
const char* WriteCmd_GetSummary(ICommand *self);
const char* WriteCmd_GetHelp(ICommand *self);
int ExitCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* ExitCmd_GetVerb(ICommand *self);
const char* ExitCmd_GetSummary(ICommand *self);
const char* ExitCmd_GetHelp(ICommand *self);
const char* ExitAliasCmd_GetVerb(ICommand *self);

void RegisterFileChannelCommands(CommandRegistry *registry);

#endif /* WCL_COMMANDS_FILE_CHANNELS_H */
