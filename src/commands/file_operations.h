#ifndef WCL_COMMANDS_FILE_OPERATIONS_H
#define WCL_COMMANDS_FILE_OPERATIONS_H

#include "../include/wcl_internal.h"

int TypeFileContents(ExecutionContext *ctx, const char *path, FILE *output,
    unsigned long tail_count, bool paging, const char *page_mode);
int TypeCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* TypeCmd_GetVerb(ICommand *self);
const char* TypeCmd_GetSummary(ICommand *self);
const char* TypeCmd_GetHelp(ICommand *self);
int CopyFileToDirectory(ExecutionContext *ctx, const char *source, const char *directory, bool log_operation);
int CopyCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* CopyCmd_GetVerb(ICommand *self);
const char* CopyCmd_GetSummary(ICommand *self);
const char* CopyCmd_GetHelp(ICommand *self);
int AppendCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* AppendCmd_GetVerb(ICommand *self);
const char* AppendCmd_GetSummary(ICommand *self);
const char* AppendCmd_GetHelp(ICommand *self);
int DeleteCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* DeleteCmd_GetVerb(ICommand *self);
const char* DeleteCmd_GetSummary(ICommand *self);
const char* DeleteCmd_GetHelp(ICommand *self);
int RenameCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* RenameCmd_GetVerb(ICommand *self);
const char* RenameCmd_GetSummary(ICommand *self);
const char* RenameCmd_GetHelp(ICommand *self);
int CreateCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* CreateCmd_GetVerb(ICommand *self);
const char* CreateCmd_GetSummary(ICommand *self);
const char* CreateCmd_GetHelp(ICommand *self);
int SearchFileContents(ExecutionContext *ctx, const char *file_path, const char *pattern, FILE *output, bool exact, bool show_numbers, bool show_heading, unsigned long *skip_matches, unsigned long limit_matches, unsigned long *displayed, int *total_matches);
int SearchCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* SearchCmd_GetVerb(ICommand *self);
const char* SearchCmd_GetSummary(ICommand *self);
const char* SearchCmd_GetHelp(ICommand *self);
int PrintCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* PrintCmd_GetVerb(ICommand *self);
const char* PrintCmd_GetSummary(ICommand *self);
const char* PrintCmd_GetHelp(ICommand *self);

void RegisterFileOperationCommands(CommandRegistry *registry);

#endif /* WCL_COMMANDS_FILE_OPERATIONS_H */
