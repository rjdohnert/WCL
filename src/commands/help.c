#include "../include/wcl_internal.h"
#include "help.h"

typedef struct HelpCommand {
    struct ICommand base;
    CommandRegistry *registry;
} HelpCommand;

int HelpCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)quals;
    HelpCommand *hcmd = (HelpCommand*)self;

    if (argc == 1) {
        printf("\n===============================================================================\n");
        printf("                      WCL (WINDOWS COMMAND LANGUAGE) HELP                      \n");
        printf("===============================================================================\n\n");
        printf("WCL Version: %s\n\n", WCL_VERSION);
        printf("WCL is an object-oriented OpenVMS-style CLI shell tailored specifically for native\n");
        printf("Win32 administration. Commands and qualifiers can be uniquely abbreviated.\n");
        printf("Commands not built into WCL are launched via the Windows application search path.\n");
        printf("File specifications use ;N for an explicit version, ;0 or no version for the latest\n");
        printf("input version, and the next version for new output. Wildcards match all versions.\n");
        printf("Command procedures support @files, P1-P8 parameters, symbols, IF/ELSE/ENDIF,\n");
        printf("GOTO, GOSUB/RETURN, and ON ERROR handlers. See HELP PROCEDURE.\n\n");
        printf("String lexical functions are available in procedure text. See HELP LEXICAL.\n\n");
        printf("Available Commands:\n\n");

        for (int i = 0; i < hcmd->registry->count; ++i) {
            ICommand *c = hcmd->registry->commands[i];
            printf("  %-12s - %s\n", c->vptr->GetVerb(c), c->vptr->GetSummary(c));
        }
        printf("\nType \"HELP <command>\" for specific qualifier documentation and usage syntax.\n\n");
        return 0;
    }

    if (_stricmp(argv[1], "SHOW") == 0) {
        PrintShowCatalogHelp();
        return 0;
    }

    if (_stricmp(argv[1], "PROCEDURE") == 0 || _stricmp(argv[1], "SCRIPT") == 0) {
        printf("\nDCL COMMAND PROCEDURES\n\n"
               "  @file-spec [P1 ... P8]  Runs a text command procedure.\n"
               "  name = value            Assigns a process symbol.\n"
               "  &name                   Substitutes a symbol in command text.\n"
               "  'name'                  DCL symbol substitution; use ''name' inside a quoted string.\n"
               "  command -               A trailing hyphen continues a command on the next line.\n"
               "  label:                  Declares a branch target.\n"
               "  IF expr THEN command    Conditional execution; ELSE/ENDIF blocks are supported.\n"
               "  GOTO label              Branches to a procedure label.\n"
               "  GOSUB label / RETURN    Calls and returns from a procedure subroutine.\n"
               "  ON ERROR GOTO label     Branches after a command failure.\n"
               "  ON ERROR THEN command   Runs one command after a command failure.\n"
               "  SET NOON / SET ON       Controls stop-on-error behavior.\n"
               "  SET VERIFY / SET NOVERIFY Displays or suppresses procedure command lines.\n\n"
               "  Conditions support DCL odd-integer and Y/T truth values, balanced parentheses,\n"
               "  numeric and string comparisons, .AND., .OR., and .NOT.\n"
               "  Limits: 8 nested procedures, 8 parameters, 4096 lines, and 2047 characters per command.\n\n"
               "  OPEN/READ input file; READ input record; CLOSE input reads file records.\n"
               "  OPEN/WRITE output file; WRITE output record writes a record; OPEN/APPEND adds records.\n"
               "  OPEN/NETWORK chan host:port connects to TCP network sockets for READ and WRITE.\n"
               "  INQUIRE symbol [prompt] prompts user interactively; WAIT delta-time pauses execution.\n\n"
               "  SPAWN/NOWAIT launches background jobs; SYNCHRONIZE [entry] waits for completion.\n"
               "  SUBMIT procedure P1 ... /AFTER=YYYY-MM-DDTHH:MM schedules a one-time local task for the current user.\n"
               "  SHOW QUEUE lists WCL tasks; DELETE/ENTRY task-id cancels one. See HELP SUBMIT.\n\n"
               "  F$ lexical functions can be used in assignments, conditions, and command text; see HELP LEXICAL.\n\n"
               "  Conditions support .EQ., .NE., .GT., .GE., .LT., .LE., .EQS., .NES.,\n"
               "  .AND., .OR., and .NOT. This is a DCL-compatible subset, not full VMS DCL.\n\n");
        return 0;
    }

    if (_stricmp(argv[1], "LEXICAL") == 0 || _stricmp(argv[1], "FUNCTIONS") == 0) {
        printf("\nDCL STRING & SYSTEM LEXICAL FUNCTIONS\n\n"
               "  F$LENGTH(string)                 Returns the string length.\n"
               "  F$EXTRACT(start,length,string)   Returns a zero-based substring.\n"
               "  F$LOCATE(substring,string)       Returns its zero-based position, or string length if absent.\n"
               "  F$ELEMENT(index,delimiter,string) Returns a zero-based element; returns delimiter if absent.\n"
               "  F$EDIT(string,edit-list)         Applies comma- or space-separated edit operations.\n"
               "  F$INTEGER(expression)            Converts a decimal expression to a signed 32-bit integer string.\n"
               "  F$STRING(expression)             Converts an integer expression to decimal text; string values pass through.\n"
               "  F$VERIFY([procedure[,image]])    Returns the previous procedure setting and optionally changes verification.\n"
               "  F$SEARCH(file-spec[,stream-id])  Returns the next matching file name, or an empty string.\n"
               "  F$ENVIRONMENT(item)              Returns supported process or procedure environment values.\n"
               "  F$TIME()                         Returns the current system timestamp in OpenVMS format.\n"
               "  F$PRIVILEGE(priv)                Returns TRUE if token holds privilege or ADMIN/ELEVATED state.\n"
               "  F$GETSYI(item)                   Returns Windows system info (NODENAME, VERSION, CPU_COUNT, TOTAL_PHYSICAL_MEM, FREE_RAM_MB, UPTIME, ARCH).\n"
               "  F$GETJPI([pid],item)             Returns process metrics (PID, USERNAME, IMAGENAME, WSSIZE, WSPEAK, CPU_TIME, MEM_BYTES, HANDLES).\n"
               "  F$GETDVI(device,item)            Returns drive metrics (FREE_MB, TOTAL_MB, FREEBLOCKS, MAXBLOCK, FS_TYPE, MOUNTED).\n"
               "  F$FILE_ATTRIBUTES(file,item)     Returns file attributes (SIZE, SIZE_KB, EOF, DIRECTORY, READONLY).\n"
               "  F$PARSE(file[,field])            Parses Windows path (DEVICE, DIRECTORY, NAME, TYPE).\n"
               "  F$TRNLNM(logical)                Translates a logical name or Windows environment variable.\n"
               "  F$REGISTRY(key,value[,default])  Reads a Windows Registry value (HKLM, HKCU, HKCR, HKU, HKCC).\n\n"
               "  F$EDIT supports UPCASE/UPPERCASE, LOWERCASE, TRIM, COLLAPSE (remove whitespace),\n"
               "  COMPRESS (reduce whitespace runs), and UNCOMMENT (remove text from ! onward).\n"
               "  F$ENVIRONMENT supports DEFAULT, INTERACTIVE, JOBTYPE, PROCEDURE, PROMPT,\n"
               "  VERIFY_PROCEDURE, and VERIFY_IMAGE.\n"
               "  F$SEARCH supports streams 0-7; an omitted stream-id selects stream 0. Each stream advances\n"
               "  independently and closes when exhausted or when a different specification is used.\n"
               "  Function arguments may be quoted; functions may be nested. Results are substituted\n"
               "  in command procedures, symbol assignments, conditions, and command text.\n\n"
               "  Example: count = F$LENGTH(\"hello\")\n"
               "           node = F$GETSYI(\"NODENAME\")\n"
               "           now = F$TIME()\n"
               "           free = F$GETDVI(\"C:\", \"FREE_MB\")\n"
               "           part = F$EXTRACT(1,3,\"hello\")\n"
               "           clean = F$EDIT(\"  Hello   WCL  \",\"TRIM,COMPRESS,UPCASE\")\n\n"
               "  This is a supported DCL-oriented subset for Windows administration.\n\n");
        return 0;
    }

    bool ambiguous = false;
    ICommand *target = hcmd->registry->Resolve(hcmd->registry, argv[1], &ambiguous);
    if (ambiguous) {
        ctx->PrintStatus(ctx, 'E', "AMBCMD", "Specified help topic is ambiguous");
        return 1;
    }
    if (!target) {
        ctx->PrintStatus(ctx, 'E', "NOTOPIC", "No help information available on requested subject");
        return 1;
    }

    printf("\n%s\n\n%s\n\n", target->vptr->GetVerb(target), target->vptr->GetHelpTopic(target));
    return 0;
}

const char* HelpCmd_GetVerb(ICommand *self) { (void)self; return "HELP"; }
const char* HelpCmd_GetSummary(ICommand *self) { (void)self; return "Displays online documentation on WCL and system commands."; }
const char* HelpCmd_GetHelp(ICommand *self) {
    (void)self;
    return "HELP [command-name | PROCEDURE | LEXICAL]\n"
           "  Displays built-in command documentation, command-procedure syntax, or lexical-function details.\n"
           "  DCL help-library selection, search, paging, and width qualifiers have no built-in Windows help-library mapping.";
}

static const ICommandVTable HelpCmd_VTable = {
    { NULL, NULL },
    HelpCmd_Execute,
    HelpCmd_GetVerb,
    HelpCmd_GetSummary,
    HelpCmd_GetHelp
};

void RegisterHelpCommand(CommandRegistry *registry) {
    static HelpCommand command = { { &HelpCmd_VTable, 1 }, NULL };
    command.registry = registry;
    registry->Register(registry, (ICommand*)&command);
}

/* ----------------------------------------------------------------------------
 * Utilities: Prefix Matching & Timestamp Formatting
 * ---------------------------------------------------------------------------- */
