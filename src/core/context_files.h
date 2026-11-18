#ifndef WCL_CORE_CONTEXT_FILES_H
#define WCL_CORE_CONTEXT_FILES_H

#include "../include/wcl_internal.h"

bool QualifierList_HasQualifier(QualifierList *self, const char *name);
const char* QualifierList_GetQualifierValue(QualifierList *self, const char *name);
void QualifierList_Init(QualifierList *self);
void Context_SetPrompt(ExecutionContext *self, const char *prompt);
bool Context_SetLogical(ExecutionContext *self, const char *logical, const char *equiv);
const char* Context_GetLogical(ExecutionContext *self, const char *logical);
bool ExpandLogicalName(ExecutionContext *ctx, const char *file_spec, char *expanded, size_t capacity);
bool IsNumericVersion(const char *text, unsigned long *version);
bool FindHighestFileVersion(const char *base, unsigned long *highest);
bool HasFileWildcard(const char *file_spec);
bool BuildMatchedFilePath(const char *pattern, const char *file_name, char *path, size_t capacity);
bool ResolveFileSpecification(ExecutionContext *ctx, const char *file_spec, bool output, char *resolved, size_t capacity);
void ResetFileSearchStream(FileSearchStream *stream);
bool GetLexicalSearchResult(ExecutionContext *ctx, const char *file_spec, int stream_id, char *result, size_t capacity);
FileChannel* FindFileChannel(ExecutionContext *ctx, const char *name);
bool CloseAllFileChannels(ExecutionContext *ctx);
bool CloseAllFileSearchStreams(ExecutionContext *ctx);
bool CloseProcedureFileChannels(ExecutionContext *ctx, int script_depth);
void Context_PrintStatus(ExecutionContext *self, char severity, const char *ident, const char *text);
int ConfirmMutation(ExecutionContext *ctx, const char *operation, const char *target);
void ReportDryRun(ExecutionContext *ctx, const char *operation, const char *target);
bool DirectoryVersionMatches(const char *search_path, const char *file_name, bool all_versions, unsigned long version_limit);


#endif /* WCL_CORE_CONTEXT_FILES_H */
