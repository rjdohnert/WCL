#ifndef WCL_CORE_PARSER_CONDITIONS_H
#define WCL_CORE_PARSER_CONDITIONS_H

#include "../include/wcl_internal.h"

bool MatchesPrefix(const char *input, const char *target);
bool MatchesWildcard(const char *pattern, const char *text);
void FormatVmsTime(const FILETIME *ft, char *buf, size_t len);
void FormatFileSizeMetric(ULONGLONG bytes, char *buf, size_t len);
bool ParseCommandLine(char *line, int *argc, char **argv, QualifierList *quals);
char* TrimWhitespace(char *text);
const char* FindDclOperator(const char *text, const char *op);
bool IsWrappedCondition(const char *text, size_t length);
bool EvaluateConditionTruth(ExecutionContext *ctx, char *text, bool *result);
bool EvaluateCondition(ExecutionContext *ctx, const char *expression, bool *result, int depth);


#endif /* WCL_CORE_PARSER_CONDITIONS_H */
