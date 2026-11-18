#ifndef WCL_CORE_SYMBOLS_LEXICALS_H
#define WCL_CORE_SYMBOLS_LEXICALS_H

#include "../include/wcl_internal.h"

void CopyString(char *dest, size_t capacity, const char *src);
bool AppendString(char *dest, size_t capacity, const char *src);
bool IsSymbolNameChar(unsigned char c);
bool IsValidSymbolName(const char *name);
const char* GetSymbol(ExecutionContext *ctx, const char *name);
bool SetSymbol(ExecutionContext *ctx, const char *name, const char *value);
bool DeleteSymbol(ExecutionContext *ctx, const char *name);
bool ParseLexicalInteger(const char *text, long long *value);
bool IsQuotedLexicalArgument(const char *source);
bool ParseLexicalArguments(const char *open_paren, char arguments[3][WCL_MAX_INPUT], int *argument_count, const char **after_function);
bool ExpandLexicalArgument(ExecutionContext *ctx, const char *source, char *value, size_t capacity, int depth);
bool ApplyLexicalEdit(char *value, const char *edit_list);
bool EvaluateLexicalEnvironment(ExecutionContext *ctx, const char *item, char *result, size_t capacity);
bool ExpandSymbolsInternal(ExecutionContext *ctx, const char *source, char *dest, size_t capacity, int depth);
bool ExpandSymbols(ExecutionContext *ctx, const char *source, char *dest, size_t capacity);
bool IsTopLevelStringFunction(const char *expression);
bool TryAssignSymbol(ExecutionContext *ctx, const char *line, bool *handled);


#endif /* WCL_CORE_SYMBOLS_LEXICALS_H */
