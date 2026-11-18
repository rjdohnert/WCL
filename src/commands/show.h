#ifndef WCL_COMMANDS_SHOW_H
#define WCL_COMMANDS_SHOW_H

#include "../include/wcl_internal.h"

typedef struct ShowOptionDefinition ShowOptionDefinition;

bool FormatNetworkAddress(const SOCKADDR *address, char *text, size_t capacity);
bool GetNetworkAdapterName(const IP_ADAPTER_ADDRESSES *adapter, char *name, size_t capacity);
bool PrintNetworkAddress(FILE *output, const SOCKADDR *address, bool *printed);
bool PrintNetworkAdapterAddresses(FILE *output, const IP_ADAPTER_ADDRESSES *adapter, bool full);
int ShowNetworkStatus(ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
bool ShowServiceState(const char *service_name, char *state, size_t capacity);
int ShowCmd_ExecuteCore(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
int ShowCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* ShowCmd_GetVerb(ICommand *self);
const char* ShowCmd_GetSummary(ICommand *self);
const char* ShowCmd_GetHelp(ICommand *self);

const ShowOptionDefinition *FindShowOption(const char *name, bool *ambiguous);
const ShowOptionDefinition *FindShowInvocationOption(const ShowOptionDefinition *option, int argc, char **argv, QualifierList *quals, bool *ambiguous);
bool ShowQualifierIsValid(const ShowOptionDefinition *option, const char *name);
const char *ShowQualifierBaseName(const ShowOptionDefinition *option, const char *name);
void RegisterShowCommand(CommandRegistry *registry);
void PrintShowCatalogHelp(void);

#endif /* WCL_COMMANDS_SHOW_H */
