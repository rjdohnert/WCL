#ifndef WCL_PLATFORM_WINDOWS_VOLUME_OPERATIONS_H
#define WCL_PLATFORM_WINDOWS_VOLUME_OPERATIONS_H

#include "../../include/wcl_internal.h"

bool ResolveDriveRoot(const char *device, char *root, size_t capacity);
bool IsWindowsSystemVolume(const char *root);
int InitializeWindowsVolume(ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
int UnsupportedCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* UnsupportedCmd_GetVerb(ICommand *self);
const char* UnsupportedCmd_GetSummary(ICommand *self);
const char* UnsupportedCmd_GetHelp(ICommand *self);

void RegisterVolumeCommands(CommandRegistry *registry);

#endif /* WCL_PLATFORM_WINDOWS_VOLUME_OPERATIONS_H */
