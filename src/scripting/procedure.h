#ifndef WCL_SCRIPTING_PROCEDURE_H
#define WCL_SCRIPTING_PROCEDURE_H

#include "../include/wcl_internal.h"

char* GetScriptText(const char *line, char *buffer);
bool IsScriptLabel(const char *text, char *label, size_t capacity);
bool FindScriptLabel(char **lines, size_t line_count, const char *name, size_t *line_index);
const char* FindThenKeyword(const char *text);
const char* AfterScriptWord(const char *text, const char *word);
bool ParseProcedureInvocation(const char *text, char *file_spec, size_t file_capacity, int *parameter_count, char **parameters);
bool HandleProcedureControl(ExecutionContext *ctx, char **lines, size_t line_count, const char *text, size_t *pc, size_t *gosub_return, size_t *gosub_if_depth, size_t *gosub_depth, size_t gosub_capacity, size_t *if_depth, int *status, bool *handled);
int ExecuteProcedure(ExecutionContext *ctx, CommandRegistry *registry, const char *file_spec, int parameter_count, char **parameters);


#endif /* WCL_SCRIPTING_PROCEDURE_H */
