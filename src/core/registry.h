#ifndef WCL_CORE_REGISTRY_H
#define WCL_CORE_REGISTRY_H

#include "../include/wcl_internal.h"

void Registry_Register(CommandRegistry *self, ICommand *cmd);
ICommand* Registry_Resolve(CommandRegistry *self, const char *verb, bool *ambiguous);
bool QualifierList_Validate(QualifierList *quals, ICommand *cmd, ExecutionContext *ctx, int argc, char **argv);
const char *GetQualifierValueByPrefix(QualifierList *quals, const char *name);


#endif /* WCL_CORE_REGISTRY_H */
