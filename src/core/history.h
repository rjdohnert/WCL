#ifndef WCL_CORE_HISTORY_H
#define WCL_CORE_HISTORY_H

#include "../include/wcl_internal.h"
#include "registry.h"

#define WCL_HISTORY_MAX 500

void History_Init(ExecutionContext *ctx, CommandRegistry *registry);
bool History_ReadLine(ExecutionContext *ctx, char *buffer, size_t buffer_size);
void History_Add(const char *command);
int History_GetCount(void);
const char* History_Get(int index);
void History_Cleanup(void);

#endif /* WCL_CORE_HISTORY_H */
