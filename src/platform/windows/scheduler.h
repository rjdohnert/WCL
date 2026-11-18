#ifndef WCL_PLATFORM_WINDOWS_SCHEDULER_H
#define WCL_PLATFORM_WINDOWS_SCHEDULER_H

#include "../../include/wcl_internal.h"

bool AppendRepeated(char *dest, size_t capacity, char value, size_t count);
bool AppendWindowsArgument(char *command_line, size_t capacity, const char *argument);
bool GetTaskSchedulerPath(char *path, size_t capacity);
bool RunTaskSchedulerCommand(const char *arguments, char *output, size_t output_capacity, DWORD *exit_code);
bool IsWclTaskName(const char *name);
bool ParseTaskCsvFirstField(const char *line, char *field, size_t capacity, const char **remaining);
int ShowWclTaskQueue(ExecutionContext *ctx);
int DeleteWclScheduledTask(ExecutionContext *ctx, const char *task_name);
int RunCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* RunCmd_GetVerb(ICommand *self);
const char* RunCmd_GetSummary(ICommand *self);
const char* RunCmd_GetHelp(ICommand *self);
bool AppendXmlText(char *xml, size_t capacity, const char *text);
bool WriteTaskXml(const char *path, const char *xml);
bool ParseSubmitTime(const char *text, char *boundary, size_t boundary_capacity);
int CreateScheduledTask(ExecutionContext *ctx, const char *procedure, int parameter_count, char **parameters, const char *scheduled_time);
int SubmitCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals);
const char* SubmitCmd_GetVerb(ICommand *self);
const char* SubmitCmd_GetSummary(ICommand *self);
const char* SubmitCmd_GetHelp(ICommand *self);

void RegisterSchedulerCommands(CommandRegistry *registry);

#endif /* WCL_PLATFORM_WINDOWS_SCHEDULER_H */
