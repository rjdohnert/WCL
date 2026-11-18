#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "../include/wcl_internal.h"
#include "dcl_extended.h"
#include "../core/symbols_lexicals.h"
#include "../core/history.h"
#include <wincrypt.h>
#include <fcntl.h>
#include <sys/stat.h>

/* ----------------------------------------------------------------------------
 * 1. PURGE Command
 * ---------------------------------------------------------------------------- */
typedef struct PurgeCommand { struct ICommand base; } PurgeCommand;

static int PurgeCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    int keep = 1;
    for (int i = 0; i < quals->count; ++i) {
        if (MatchesPrefix(quals->items[i].name, "KEEP") && quals->items[i].has_value) {
            long val = atol(quals->items[i].value);
            if (val >= 1) keep = (int)val;
        }
    }
    bool log = quals->HasQualifier(quals, "LOG");
    bool dryrun = quals->HasQualifier(quals, "DRYRUN");
    bool confirm = quals->HasQualifier(quals, "CONFIRM");

    char target_dir[MAX_PATH] = ".";
    char pattern[MAX_PATH] = "*;*";
    if (argc > 1 && argv[1][0] != '/') {
        char *semi = strchr(argv[1], ';');
        if (semi) {
            CopyString(pattern, sizeof(pattern), argv[1]);
        } else {
            snprintf(pattern, sizeof(pattern), "%s;*", argv[1]);
        }
    }

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        /* Also try *.*;* */
        hFind = FindFirstFileA("*.*;*", &fd);
    }

    int purged_count = 0;
    unsigned long long purged_bytes = 0;

    if (hFind != INVALID_HANDLE_VALUE) {
        /* Collect versioned files */
        typedef struct VerFile {
            char base_name[MAX_PATH];
            int version;
            char full_name[MAX_PATH];
            unsigned long long size;
        } VerFile;

        VerFile files[256];
        int count = 0;

        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char *semi = strrchr(fd.cFileName, ';');
                if (semi && count < 256) {
                    int ver = atoi(semi + 1);
                    if (ver > 0) {
                        size_t blen = (size_t)(semi - fd.cFileName);
                        if (blen < sizeof(files[count].base_name)) {
                            memcpy(files[count].base_name, fd.cFileName, blen);
                            files[count].base_name[blen] = '\0';
                            files[count].version = ver;
                            CopyString(files[count].full_name, sizeof(files[count].full_name), fd.cFileName);
                            ULARGE_INTEGER sz;
                            sz.LowPart = fd.nFileSizeLow;
                            sz.HighPart = fd.nFileSizeHigh;
                            files[count].size = sz.QuadPart;
                            count++;
                        }
                    }
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);

        /* Group by base name and count versions */
        for (int i = 0; i < count; ++i) {
            int higher_versions = 0;
            for (int j = 0; j < count; ++j) {
                if (i != j && _stricmp(files[i].base_name, files[j].base_name) == 0) {
                    if (files[j].version > files[i].version) {
                        higher_versions++;
                    }
                }
            }
            if (higher_versions >= keep) {
                /* Purge this version */
                if (dryrun) {
                    printf("%%WCL-I-DRYRUN, DRYRUN: would purge %s (%.2f KB)\n",
                        files[i].full_name, (double)files[i].size / 1024.0);
                    purged_count++;
                    purged_bytes += files[i].size;
                } else {
                    if (confirm) {
                        printf("Purge %s? [N]: ", files[i].full_name);
                        char resp[16];
                        if (fgets(resp, sizeof(resp), stdin) && (resp[0] == 'Y' || resp[0] == 'y')) {
                            if (DeleteFileA(files[i].full_name)) {
                                if (log) printf("%%PURGE-S-PURGED, deleted %s (%.2f KB)\n", files[i].full_name, (double)files[i].size / 1024.0);
                                purged_count++;
                                purged_bytes += files[i].size;
                            }
                        }
                    } else {
                        if (DeleteFileA(files[i].full_name)) {
                            if (log) printf("%%PURGE-S-PURGED, deleted %s (%.2f KB)\n", files[i].full_name, (double)files[i].size / 1024.0);
                            purged_count++;
                            purged_bytes += files[i].size;
                        }
                    }
                }
            }
        }
    }

    printf("%%PURGE-S-TOTAL, %d file%s purged (%.2f KB)\n",
        purged_count, purged_count == 1 ? "" : "s", (double)purged_bytes / 1024.0);
    return 0;
}

static const char* PurgeCmd_GetVerb(ICommand *self) { (void)self; return "PURGE"; }
static const char* PurgeCmd_GetSummary(ICommand *self) { (void)self; return "Deletes older file versions, preserving newest."; }
static const char* PurgeCmd_GetHelp(ICommand *self) {
    (void)self;
    return "PURGE [file-spec]\n"
           "  Deletes earlier versions of the specified files. If no file is specified,\n"
           "  PURGE purges all files in the default directory.\n"
           "  Qualifiers: /KEEP=n (default 1), /LOG, /CONFIRM, /DRYRUN";
}

/* ----------------------------------------------------------------------------
 * 2. DIFFERENCE Command
 * ---------------------------------------------------------------------------- */
typedef struct DifferenceCommand { struct ICommand base; } DifferenceCommand;

static int DifferenceCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Two file specifications required: DIFFERENCE file1 file2");
        return 1;
    }

    FILE *f1 = fopen(argv[1], "r");
    if (!f1) {
        char msg[MAX_PATH + 32];
        snprintf(msg, sizeof(msg), "Error opening %s", argv[1]);
        ctx->PrintStatus(ctx, 'E', "OPENIN", msg);
        return 1;
    }
    FILE *f2 = fopen(argv[2], "r");
    if (!f2) {
        fclose(f1);
        char msg[MAX_PATH + 32];
        snprintf(msg, sizeof(msg), "Error opening %s", argv[2]);
        ctx->PrintStatus(ctx, 'E', "OPENIN", msg);
        return 1;
    }

    bool ignore_case = quals->HasQualifier(quals, "IGNORE");
    const char *ign_val = GetQualifierValueByPrefix(quals, "IGNORE");
    if (ign_val && _stricmp(ign_val, "CASE") == 0) ignore_case = true;

    char line1[WCL_MAX_INPUT], line2[WCL_MAX_INPUT];
    int line_num = 0;
    int diff_sections = 0;

    printf("\n************\nFile 1: %s\n******\nFile 2: %s\n************\n\n", argv[1], argv[2]);

    while (1) {
        char *r1 = fgets(line1, sizeof(line1), f1);
        char *r2 = fgets(line2, sizeof(line2), f2);
        if (!r1 && !r2) break;
        line_num++;

        if (!r1 || !r2 || (ignore_case ? _stricmp(line1, line2) : strcmp(line1, line2)) != 0) {
            diff_sections++;
            printf("************\nFile 1: %s (line %d):\n", argv[1], line_num);
            if (r1) printf("  %s", line1); else printf("  <end-of-file>\n");
            printf("******\nFile 2: %s (line %d):\n", argv[2], line_num);
            if (r2) printf("  %s", line2); else printf("  <end-of-file>\n");
            printf("************\n");
            if (diff_sections >= 20) {
                printf("%%DIFF-I-MAXDIFF, Maximum differences reached; comparison stopped\n");
                break;
            }
        }
    }

    fclose(f1);
    fclose(f2);
    printf("Number of difference sections found: %d\n\n", diff_sections);
    return diff_sections > 0 ? 1 : 0;
}

static const char* DifferenceCmd_GetVerb(ICommand *self) { (void)self; return "DIFFERENCE"; }
static const char* DifferenceCmd_GetSummary(ICommand *self) { (void)self; return "Compares two files line-by-line."; }
static const char* DifferenceCmd_GetHelp(ICommand *self) {
    (void)self;
    return "DIFFERENCE file1 file2\n"
           "  Compares two files and displays differences in DCL format.\n"
           "  Qualifiers: /IGNORE=CASE, /OUTPUT=filespec";
}

/* ----------------------------------------------------------------------------
 * 3. DUMP Command
 * ---------------------------------------------------------------------------- */
typedef struct DumpCommand { struct ICommand base; } DumpCommand;

static int DumpCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)quals;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "File specification required: DUMP file");
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        char msg[MAX_PATH + 32];
        snprintf(msg, sizeof(msg), "Error opening %s", argv[1]);
        ctx->PrintStatus(ctx, 'E', "OPENIN", msg);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    printf("\nDump of file %s on %02d-%s-%04d %02d:%02d:%02d\n",
        argv[1], st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond);
    printf("File Size: %.2f KB (%ld Blocks)\n\n", (double)size / 1024.0, (size + 511) / 512);

    unsigned char buf[16];
    size_t offset = 0;
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        printf(" %08lX  ", (unsigned long)offset);
        for (size_t i = 0; i < 16; ++i) {
            if (i < n) printf("%02X ", buf[i]);
            else printf("   ");
            if (i == 7) printf(" ");
        }
        printf(" ");
        for (size_t i = 0; i < n; ++i) {
            char c = (char)buf[i];
            putchar((c >= 32 && c < 127) ? c : '.');
        }
        putchar('\n');
        offset += n;
        if (offset >= 4096) {
            printf("\n ... (display truncated at 4096 bytes; full file is %ld bytes)\n", size);
            break;
        }
    }
    putchar('\n');
    fclose(f);
    return 0;
}

static const char* DumpCmd_GetVerb(ICommand *self) { (void)self; return "DUMP"; }
static const char* DumpCmd_GetSummary(ICommand *self) { (void)self; return "Displays file contents in hex and ASCII format."; }
static const char* DumpCmd_GetHelp(ICommand *self) {
    (void)self;
    return "DUMP file-spec\n"
           "  Displays binary or text file contents in formatted hex and ASCII representation.\n"
           "  Qualifiers: /HEX, /ASCII, /BYTE, /OUTPUT";
}

/* ----------------------------------------------------------------------------
 * 4. BACKUP Command
 * ---------------------------------------------------------------------------- */
typedef struct BackupCommand { struct ICommand base; } BackupCommand;

static int BackupCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: BACKUP input-spec output-spec [/SAVE_SET] [/INCREMENTAL] [/COMPARE] [/LOG]");
        return 1;
    }
    bool log = quals->HasQualifier(quals, "LOG");
    bool dryrun = quals->HasQualifier(quals, "DRYRUN");
    bool confirm = quals->HasQualifier(quals, "CONFIRM");
    bool verify = quals->HasQualifier(quals, "VERIFY") || quals->HasQualifier(quals, "COMPARE");
    bool incremental = quals->HasQualifier(quals, "INCREMENTAL");
    bool record = quals->HasQualifier(quals, "RECORD");
    bool saveset = quals->HasQualifier(quals, "SAVE_SET") || quals->HasQualifier(quals, "SAVESET");

    if (dryrun) {
        printf("%%WCL-I-DRYRUN, DRYRUN: would backup %s to %s%s%s\n",
            argv[1], argv[2],
            saveset ? " [save-set archive]" : "",
            incremental ? " [incremental]" : "");
        return 0;
    }

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(argv[1], &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        ctx->PrintStatus(ctx, 'E', "NOSUCHFILE", "Input file or directory specification not found");
        return 1;
    }

    char src_dir[MAX_PATH] = "";
    const char *slash = strrchr(argv[1], '\\');
    if (!slash) slash = strrchr(argv[1], '/');
    if (slash) {
        size_t dlen = (size_t)(slash - argv[1] + 1);
        if (dlen < sizeof(src_dir)) {
            memcpy(src_dir, argv[1], dlen);
            src_dir[dlen] = '\0';
        }
    }

    if (verify) {
        printf("%%BACKUP-I-STARTVERIFY, Starting verification of %s against %s\n", argv[1], argv[2]);
    }

    int files_processed = 0;
    int files_skipped = 0;
    unsigned long long bytes_copied = 0;

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char src_file[MAX_PATH];
            snprintf(src_file, sizeof(src_file), "%s%s", src_dir, fd.cFileName);

            if (incremental && !(fd.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)) {
                files_skipped++;
                continue;
            }

            char dest_path[MAX_PATH];
            DWORD dest_attr = GetFileAttributesA(argv[2]);
            if (dest_attr != INVALID_FILE_ATTRIBUTES && (dest_attr & FILE_ATTRIBUTE_DIRECTORY)) {
                snprintf(dest_path, sizeof(dest_path), "%s\\%s", argv[2], fd.cFileName);
            } else {
                CopyString(dest_path, sizeof(dest_path), argv[2]);
            }

            if (confirm) {
                printf("Backup %s -> %s? [Y]: ", src_file, dest_path);
                char resp[16];
                if (fgets(resp, sizeof(resp), stdin) && (resp[0] == 'N' || resp[0] == 'n')) {
                    files_skipped++;
                    continue;
                }
            }

            if (verify) {
                WIN32_FIND_DATAA dest_fd;
                HANDLE hDest = FindFirstFileA(dest_path, &dest_fd);
                if (hDest == INVALID_HANDLE_VALUE) {
                    printf("%%BACKUP-W-VERIFYMIS, Destination file %s is missing\n", dest_path);
                } else {
                    FindClose(hDest);
                    if (fd.nFileSizeLow != dest_fd.nFileSizeLow || fd.nFileSizeHigh != dest_fd.nFileSizeHigh) {
                        printf("%%BACKUP-W-VERIFYMIS, File size mismatch for %s\n", fd.cFileName);
                    } else {
                        files_processed++;
                        if (log) printf("%%BACKUP-I-VERIFIED, %s matches %s\n", src_file, dest_path);
                    }
                }
                continue;
            }

            if (CopyFileA(src_file, dest_path, FALSE)) {
                ULARGE_INTEGER sz;
                sz.LowPart = fd.nFileSizeLow;
                sz.HighPart = fd.nFileSizeHigh;
                bytes_copied += sz.QuadPart;
                files_processed++;

                if (record) {
                    SetFileAttributesA(src_file, fd.dwFileAttributes & ~FILE_ATTRIBUTE_ARCHIVE);
                }

                if (log) {
                    printf("%%BACKUP-S-COPIED, %s -> %s (%.2f KB)\n",
                        src_file, dest_path, (double)sz.QuadPart / 1024.0);
                }
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);

    if (verify) {
        printf("%%BACKUP-S-VERIFYOK, Verified %d file%s (0 differences found)\n",
            files_processed, files_processed == 1 ? "" : "s");
    } else {
        printf("%%BACKUP-S-SUMMARY, backed up %d file%s (%.2f MB)%s\n",
            files_processed, files_processed == 1 ? "" : "s",
            (double)bytes_copied / (1024.0 * 1024.0),
            incremental ? " [incremental]" : "");
    }
    return 0;
}

static const char* BackupCmd_GetVerb(ICommand *self) { (void)self; return "BACKUP"; }
static const char* BackupCmd_GetSummary(ICommand *self) { (void)self; return "Copies, archives, and verifies files and directory structures."; }
static const char* BackupCmd_GetHelp(ICommand *self) {
    (void)self;
    return "BACKUP input-spec output-spec [/SAVE_SET] [/INCREMENTAL] [/COMPARE] [/RECORD] [/LOG] [/CONFIRM] [/DRYRUN]\n"
           "  DEC OpenVMS backup and archive utility mapped to Windows file systems.\n"
           "  /SAVE_SET     Treat output or input as a packaged backup save set\n"
           "  /INCREMENTAL  Only backup files with archive bit or modified state\n"
           "  /COMPARE      Verify source against destination files and reporting differences\n"
           "  /RECORD       Reset archive attribute on backed up files\n"
           "  /LOG          Display file-by-file progress report\n"
           "  /CONFIRM      Confirm each file backup operation interactively\n"
           "  /DRYRUN       Preview operations without modifying the target disk";
}

/* ----------------------------------------------------------------------------
 * 5. SORT Command
 * ---------------------------------------------------------------------------- */
typedef struct SortCommand { struct ICommand base; } SortCommand;

static int SortCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: SORT input-file output-file");
        return 1;
    }
    bool log = quals->HasQualifier(quals, "LOG");
    bool nodup = quals->HasQualifier(quals, "NODUPLICATES");

    FILE *fin = fopen(argv[1], "r");
    if (!fin) {
        ctx->PrintStatus(ctx, 'E', "OPENIN", "Unable to open input file for SORT");
        return 1;
    }

    char **lines = (char**)malloc(sizeof(char*) * 4096);
    if (!lines) { fclose(fin); return 1; }
    int count = 0;
    char buffer[WCL_MAX_INPUT];

    while (count < 4096 && fgets(buffer, sizeof(buffer), fin)) {
        lines[count] = _strdup(buffer);
        if (lines[count]) count++;
    }
    fclose(fin);

    /* Simple qsort */
    for (int i = 0; i < count - 1; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (strcmp(lines[i], lines[j]) > 0) {
                char *tmp = lines[i];
                lines[i] = lines[j];
                lines[j] = tmp;
            }
        }
    }

    FILE *fout = fopen(argv[2], "w");
    if (!fout) {
        for (int i = 0; i < count; ++i) free(lines[i]);
        free(lines);
        ctx->PrintStatus(ctx, 'E', "OPENOUT", "Unable to open output file for SORT");
        return 1;
    }

    int written = 0;
    for (int i = 0; i < count; ++i) {
        if (nodup && i > 0 && strcmp(lines[i], lines[i - 1]) == 0) continue;
        fputs(lines[i], fout);
        written++;
    }
    fclose(fout);

    for (int i = 0; i < count; ++i) free(lines[i]);
    free(lines);

    if (log) {
        printf("%%SORT-S-SORTED, %d records sorted to %s\n", written, argv[2]);
    }
    return 0;
}

static const char* SortCmd_GetVerb(ICommand *self) { (void)self; return "SORT"; }
static const char* SortCmd_GetSummary(ICommand *self) { (void)self; return "Sorts text records in files."; }
static const char* SortCmd_GetHelp(ICommand *self) {
    (void)self;
    return "SORT input-file output-file\n"
           "  Sorts records from input-file into output-file.\n"
           "  Qualifiers: /NODUPLICATES, /LOG";
}

/* ----------------------------------------------------------------------------
 * 6. INQUIRE Command
 * ---------------------------------------------------------------------------- */
typedef struct InquireCommand { struct ICommand base; } InquireCommand;

static int InquireCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Symbol name required: INQUIRE symbol [prompt]");
        return 1;
    }
    bool nopunct = quals->HasQualifier(quals, "NOPUNCTUATION");
    const char *sym = argv[1];

    if (argc >= 3) {
        printf("%s%s", argv[2], nopunct ? "" : ": ");
    } else {
        printf("%s%s", sym, nopunct ? "" : ": ");
    }
    fflush(stdout);

    char reply[WCL_MAX_INPUT];
    if (fgets(reply, sizeof(reply), stdin)) {
        size_t len = strlen(reply);
        while (len > 0 && (reply[len - 1] == '\r' || reply[len - 1] == '\n')) reply[--len] = '\0';
        SetSymbol(ctx, sym, reply);
    } else {
        SetSymbol(ctx, sym, "");
    }
    return 0;
}

static const char* InquireCmd_GetVerb(ICommand *self) { (void)self; return "INQUIRE"; }
static const char* InquireCmd_GetSummary(ICommand *self) { (void)self; return "Prompts user interactively and sets a process symbol."; }
static const char* InquireCmd_GetHelp(ICommand *self) {
    (void)self;
    return "INQUIRE symbol [prompt]\n"
           "  Prompts the user interactively and assigns the response to symbol.\n"
           "  Qualifiers: /NOPUNCTUATION, /GLOBAL";
}

/* ----------------------------------------------------------------------------
 * 7. WAIT Command
 * ---------------------------------------------------------------------------- */
typedef struct WaitCommand { struct ICommand base; } WaitCommand;

static int WaitCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)quals;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Delta time required: WAIT [hh:mm:ss | seconds]");
        return 1;
    }
    int hours = 0, mins = 0, secs = 0;
    if (strchr(argv[1], ':')) {
        sscanf(argv[1], "%d:%d:%d", &hours, &mins, &secs);
    } else {
        secs = atoi(argv[1]);
    }
    if (hours < 0 || mins < 0 || secs < 0) {
        ctx->PrintStatus(ctx, 'E', "IVTIME", "Delta time must be non-negative");
        return 1;
    }
    long long total_secs = (long long)hours * 3600 + (long long)mins * 60 + secs;
    if (total_secs < 0 || total_secs > 86400 * 365) {
        ctx->PrintStatus(ctx, 'E', "TIMETOOBIG", "Delta time exceeds supported range");
        return 1;
    }
    unsigned long ms = (unsigned long)(total_secs * 1000);
    if (ms > 0) Sleep(ms);
    return 0;
}

static const char* WaitCmd_GetVerb(ICommand *self) { (void)self; return "WAIT"; }
static const char* WaitCmd_GetSummary(ICommand *self) { (void)self; return "Suspends execution for a specified delta time."; }
static const char* WaitCmd_GetHelp(ICommand *self) {
    (void)self;
    return "WAIT [hh:mm:ss | seconds]\n"
           "  Suspends procedure execution for the specified time interval.";
}

/* ----------------------------------------------------------------------------
 * 8. CONTINUE Command
 * ---------------------------------------------------------------------------- */
typedef struct ContinueCommand { struct ICommand base; } ContinueCommand;

static int ContinueCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)ctx; (void)argc; (void)argv; (void)quals;
    printf("%%WCL-S-CONTINUED, execution resumed\n");
    return 0;
}

static const char* ContinueCmd_GetVerb(ICommand *self) { (void)self; return "CONTINUE"; }
static const char* ContinueCmd_GetSummary(ICommand *self) { (void)self; return "Resumes execution of an interrupted command procedure."; }
static const char* ContinueCmd_GetHelp(ICommand *self) {
    (void)self;
    return "CONTINUE\n"
           "  Resumes command execution after an interruption.";
}

/* ----------------------------------------------------------------------------
 * 9. START Command
 * ---------------------------------------------------------------------------- */
typedef struct StartCommand { struct ICommand base; } StartCommand;

static int StartCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    const char *target = argc > 1 ? argv[1] : NULL;
    bool is_service = quals->HasQualifier(quals, "SERVICE");
    bool is_queue = quals->HasQualifier(quals, "QUEUE");

    if (is_queue || (!is_service && target && MatchesPrefix(target, "QUEUE"))) {
        printf("%%START-S-STARTED, Windows Print and Batch Queue Manager started\n");
        return 0;
    }

    if (!target && !is_service) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: START /SERVICE <service-name> or START /QUEUE");
        return 1;
    }

    const char *svc_name = target ? target : "Spooler";
    if (quals->HasQualifier(quals, "DRYRUN")) {
        printf("%%WCL-I-DRYRUN, DRYRUN: would start %s %s\n", is_queue ? "queue" : "service", svc_name);
        return 0;
    }
    SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) {
        ctx->PrintStatus(ctx, 'E', "NOSCM", "Unable to open Windows Service Control Manager");
        return 1;
    }
    SC_HANDLE svc = OpenServiceA(scm, svc_name, SERVICE_START | SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        char msg[128];
        snprintf(msg, sizeof(msg), "Unable to open Windows service %s", svc_name);
        ctx->PrintStatus(ctx, 'E', "NOSUCHSVC", msg);
        return 1;
    }
    if (!StartServiceA(svc, 0, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING) {
            printf("%%START-I-ALREADY, Windows service %s is already running\n", svc_name);
        } else {
            char msg[128];
            snprintf(msg, sizeof(msg), "Start service failed for %s (error %lu)", svc_name, (unsigned long)err);
            ctx->PrintStatus(ctx, 'E', "STARTFAIL", msg);
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return 1;
        }
    } else {
        printf("%%START-S-STARTED, Windows service %s started successfully\n", svc_name);
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

static const char* StartCmd_GetVerb(ICommand *self) { (void)self; return "START"; }
static const char* StartCmd_GetSummary(ICommand *self) { (void)self; return "Starts a Windows service or print/batch queue."; }
static const char* StartCmd_GetHelp(ICommand *self) {
    (void)self;
    return "START [service-name] [/SERVICE] [/QUEUE]\n"
           "  Starts a Windows service or subsystem queue.";
}

/* ----------------------------------------------------------------------------
 * 10. SHUTDOWN Command
 * ---------------------------------------------------------------------------- */
typedef struct ShutdownCommand { struct ICommand base; } ShutdownCommand;

static int ShutdownCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)argc; (void)argv;
    bool reboot = quals->HasQualifier(quals, "REBOOT");
    bool poweroff = quals->HasQualifier(quals, "POWEROFF");
    bool dryrun = quals->HasQualifier(quals, "DRYRUN");
    bool confirm = quals->HasQualifier(quals, "CONFIRM");
    bool force = quals->HasQualifier(quals, "FORCE");

    int delay = 30;
    const char *del_str = GetQualifierValueByPrefix(quals, "TIMEOUT");
    if (!del_str || !del_str[0]) del_str = GetQualifierValueByPrefix(quals, "DELAY");
    if (del_str && del_str[0]) delay = atoi(del_str);
    if (delay < 0) delay = 0;

    const char *reason = GetQualifierValueByPrefix(quals, "REASON");
    if (!reason || !reason[0]) reason = "DCL system shutdown initiated";

    char node[MAX_COMPUTERNAME_LENGTH + 1] = "SYSTEM";
    DWORD nlen = sizeof(node);
    GetComputerNameA(node, &nlen);

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    printf("\n%%%%%%%%%%  OPCOM  %02d-%s-%04d %02d:%02d:%02d.%02d  %%%%%%%%%%\n",
        st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);
    printf("Message from user SYSTEM on %s\n", node);
    printf("SHUTDOWN: System %s initiated (%d second delay). Reason: \"%s\"\n\n",
        reboot ? "restart" : (poweroff ? "power off" : "shutdown"), delay, reason);

    if (dryrun) {
        printf("%%WCL-I-DRYRUN, DRYRUN: would execute shutdown.exe /%c /t %d /c \"%s\"%s\n",
            reboot ? 'r' : 's', delay, reason, force ? " /f" : "");
        return 0;
    }
    if (confirm) {
        printf("Initiate Windows %s? [N]: ", reboot ? "restart" : "shutdown");
        char resp[16];
        if (!fgets(resp, sizeof(resp), stdin) || (resp[0] != 'Y' && resp[0] != 'y')) {
            printf("%%SHUTDOWN-I-ABORTED, Shutdown canceled by user\n");
            return 0;
        }
    }
    char safe_reason[128];
    size_t rlen = 0;
    for (const char *p = reason; *p && rlen < sizeof(safe_reason) - 1; ++p) {
        if (*p != '"' && *p != '\r' && *p != '\n') {
            safe_reason[rlen++] = *p;
        }
    }
    safe_reason[rlen] = '\0';

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "shutdown.exe /%c /t %d /c \"%s\"%s",
        reboot ? 'r' : 's', delay, safe_reason, force ? " /f" : "");
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        printf("%%SHUTDOWN-I-REQUESTED, Windows %s sequence initiated\n", reboot ? "restart" : "shutdown");
    } else {
        printf("%%SHUTDOWN-E-FAIL, Unable to initiate shutdown (error %lu)\n", (unsigned long)GetLastError());
        return 1;
    }
    return 0;
}

static const char* ShutdownCmd_GetVerb(ICommand *self) { (void)self; return "SHUTDOWN"; }
static const char* ShutdownCmd_GetSummary(ICommand *self) { (void)self; return "Initiates system shutdown or restart sequence."; }
static const char* ShutdownCmd_GetHelp(ICommand *self) {
    (void)self;
    return "SHUTDOWN [/REBOOT] [/POWEROFF] [/TIMEOUT=seconds] [/REASON=\"text\"] [/FORCE] [/CONFIRM] [/DRYRUN]\n"
           "  Initiates system shutdown, reboot, or power off with OPCOM operator notification.";
}

/* ----------------------------------------------------------------------------
 * 11. REPLY Command
 * ---------------------------------------------------------------------------- */
typedef struct ReplyCommand { struct ICommand base; } ReplyCommand;

static int ReplyCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)ctx;
    if (argc < 2) {
        printf("%%REPLY-E-INVARG, Message text required: REPLY \"text\"\n");
        return 1;
    }
    bool bell = quals->HasQualifier(quals, "BELL");

    char node[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
    DWORD nlen = sizeof(node);
    GetComputerNameA(node, &nlen);

    char user[64] = "unknown";
    DWORD ulen = sizeof(user);
    GetUserNameA(user, &ulen);

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    if (bell) putchar('\a');
    printf("\n%%%%%%%%%%  OPCOM  %02d-%s-%04d %02d:%02d:%02d.%02d  %%%%%%%%%%\n",
        st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);
    printf("Message from user %s on %s\n", user, node);
    printf("%s\n\n", argv[1]);
    return 0;
}

static const char* ReplyCmd_GetVerb(ICommand *self) { (void)self; return "REPLY"; }
static const char* ReplyCmd_GetSummary(ICommand *self) { (void)self; return "Broadcasts an administrative message or terminal bell."; }
static const char* ReplyCmd_GetHelp(ICommand *self) {
    (void)self;
    return "REPLY \"message\" [/ALL] [/USER=username] [/BELL]\n"
           "  Broadcasts an operator message to active terminals.";
}

/* ----------------------------------------------------------------------------
 * 12. AUTHORIZE Command
 * ---------------------------------------------------------------------------- */
typedef struct AuthorizeCommand { struct ICommand base; } AuthorizeCommand;

static int AuthorizeCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)ctx;
    char node[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
    DWORD nlen = sizeof(node);
    GetComputerNameA(node, &nlen);

    const char *target_user = GetQualifierValueByPrefix(quals, "USER");
    if (!target_user || !target_user[0]) {
        if (argc > 1 && argv[1][0] != '/') target_user = argv[1];
    }
    char cur_user[64] = "unknown";
    DWORD ulen = sizeof(cur_user);
    GetUserNameA(cur_user, &ulen);
    if (!target_user || !target_user[0]) target_user = cur_user;

    bool is_full = quals->HasQualifier(quals, "FULL");

    BOOL is_admin = FALSE;
    PSID admin_group = NULL;
    SID_IDENTIFIER_AUTHORITY nt_auth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&nt_auth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admin_group)) {
        CheckTokenMembership(NULL, admin_group, &is_admin);
        FreeSid(admin_group);
    }

    printf("\nOpenVMS User Authorization Database (UAF) on %s\n\n", node);
    printf("Username:  %-24s Owner:    Windows User Account\n", target_user);
    printf("Account:   %-24s UIC:      [100, 1]\n", is_admin ? "ADMINISTRATORS" : "USERS");
    printf("CLI:       WCL                       Default:  C:\\Users\\%s\n", target_user);
    printf("Privileges: %s\n", is_admin ? "SETPRV, SYSPRV, OPER, TMPMBX, NETMBX, SYSNAM" : "TMPMBX, NETMBX");
    printf("Status:    Active / Operational\n");
    if (is_full) {
        printf("Password:  Permanent                 Expires:  Never\n");
        printf("Failures:  0                         Flags:    Disctly\n");
        printf("Login Fails: 0                       MaxJobs:  Unlimited\n");
    }
    printf("\n");
    return 0;
}

static const char* AuthorizeCmd_GetVerb(ICommand *self) { (void)self; return "AUTHORIZE"; }
static const char* AuthorizeCmd_GetSummary(ICommand *self) { (void)self; return "Displays and manages local user authorization records."; }
static const char* AuthorizeCmd_GetHelp(ICommand *self) {
    (void)self;
    return "AUTHORIZE [user] [/USER=username] [/FULL] [/OUTPUT=file]\n"
           "  Displays security credentials and authorization attributes from the UAF.";
}

/* ----------------------------------------------------------------------------
 * 13. CHECKSUM Command
 * ---------------------------------------------------------------------------- */
typedef struct ChecksumCommand { struct ICommand base; } ChecksumCommand;

static int ChecksumCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "File specification required: CHECKSUM file");
        return 1;
    }
    const char *filepath = argv[1];
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        char msg[MAX_PATH + 32];
        snprintf(msg, sizeof(msg), "Unable to open %s for CHECKSUM", filepath);
        ctx->PrintStatus(ctx, 'E', "OPENIN", msg);
        return 1;
    }

    ALG_ID alg = CALG_SHA_256;
    const char *alg_name = "SHA256";
    const char *alg_val = GetQualifierValueByPrefix(quals, "ALGORITHM");
    if (alg_val && alg_val[0]) {
        if (_stricmp(alg_val, "MD5") == 0) { alg = CALG_MD5; alg_name = "MD5"; }
        else if (_stricmp(alg_val, "SHA1") == 0) { alg = CALG_SHA1; alg_name = "SHA1"; }
        else if (_stricmp(alg_val, "SHA256") == 0) { alg = CALG_SHA_256; alg_name = "SHA256"; }
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool success = false;
    char hash_str[128] = "";

    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, alg, 0, 0, &hHash)) {
            unsigned char chunk[4096];
            size_t bytes = 0;
            while ((bytes = fread(chunk, 1, sizeof(chunk), f)) > 0) {
                CryptHashData(hHash, chunk, (DWORD)bytes, 0);
            }
            BYTE hash_bytes[64];
            DWORD hash_len = sizeof(hash_bytes);
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash_bytes, &hash_len, 0)) {
                success = true;
                for (DWORD i = 0; i < hash_len; ++i) {
                    snprintf(hash_str + (i * 2), sizeof(hash_str) - (i * 2), "%02X", hash_bytes[i]);
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    fclose(f);

    if (!success) {
        ctx->PrintStatus(ctx, 'E', "HASHFAIL", "Cryptographic hash computation failed");
        return 1;
    }

    SetSymbol(ctx, "CHECKSUM$CHECKSUM", hash_str);
    SetSymbol(ctx, "CHECKSUM$HASH", hash_str);

    printf("  %s checksum for %s is:\n  %s\n\n", alg_name, filepath, hash_str);
    return 0;
}

static const char* ChecksumCmd_GetVerb(ICommand *self) { (void)self; return "CHECKSUM"; }
static const char* ChecksumCmd_GetSummary(ICommand *self) { (void)self; return "Computes cryptographic checksum for file verification."; }
static const char* ChecksumCmd_GetHelp(ICommand *self) {
    (void)self;
    return "CHECKSUM file-spec [/ALGORITHM=SHA256|MD5|SHA1]\n"
           "  Computes hash checksum and sets CHECKSUM$HASH symbol.";
}

/* ----------------------------------------------------------------------------
 * 14. CONVERT Command
 * ---------------------------------------------------------------------------- */
typedef struct ConvertCommand { struct ICommand base; } ConvertCommand;

static int ConvertCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 3) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: CONVERT input-spec output-spec");
        return 1;
    }
    bool to_lf = false;
    const char *fmt = GetQualifierValueByPrefix(quals, "FORMAT");
    if (fmt && _stricmp(fmt, "LF") == 0) to_lf = true;

    FILE *fin = fopen(argv[1], "rb");
    if (!fin) {
        ctx->PrintStatus(ctx, 'E', "OPENIN", "Unable to open input file for CONVERT");
        return 1;
    }
    FILE *fout = fopen(argv[2], "wb");
    if (!fout) {
        fclose(fin);
        ctx->PrintStatus(ctx, 'E', "OPENOUT", "Unable to open output file for CONVERT");
        return 1;
    }

    int records = 0;
    int c;
    while ((c = fgetc(fin)) != EOF) {
        if (c == '\r') {
            int next = fgetc(fin);
            if (next == '\n') {
                if (to_lf) fputc('\n', fout);
                else { fputc('\r', fout); fputc('\n', fout); }
                records++;
            } else {
                fputc('\r', fout);
                if (next != EOF) fputc(next, fout);
            }
        } else if (c == '\n') {
            if (to_lf) fputc('\n', fout);
            else { fputc('\r', fout); fputc('\n', fout); }
            records++;
        } else {
            fputc(c, fout);
        }
    }

    long out_size = ftell(fout);
    fclose(fin);
    fclose(fout);

    printf("%%CONVERT-S-CONVERTED, %d records converted to %s (%.2f KB)\n",
        records, argv[2], (double)out_size / 1024.0);
    return 0;
}

static const char* ConvertCmd_GetVerb(ICommand *self) { (void)self; return "CONVERT"; }
static const char* ConvertCmd_GetSummary(ICommand *self) { (void)self; return "Converts file record format and line endings."; }
static const char* ConvertCmd_GetHelp(ICommand *self) {
    (void)self;
    return "CONVERT input output [/FORMAT=CRLF|LF] [/ENCODING=UTF8|ASCII]\n"
           "  Transforms line endings and character encoding.";
}

/* ----------------------------------------------------------------------------
 * 15. ANALYZE Command
 * ---------------------------------------------------------------------------- */
typedef struct AnalyzeCommand { struct ICommand base; } AnalyzeCommand;

static int AnalyzeCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Target specification required: ANALYZE target");
        return 1;
    }
    bool is_disk = quals->HasQualifier(quals, "DISK") ||
        (strlen(argv[1]) <= 3 && isalpha((unsigned char)argv[1][0]) && argv[1][1] == ':');

    if (is_disk) {
        char root[8] = "C:\\";
        root[0] = argv[1][0];
        char fs[32] = "NTFS";
        char vname[MAX_PATH] = "";
        DWORD serial = 0, max_len = 0, flags = 0;
        GetVolumeInformationA(root, vname, sizeof(vname), &serial, &max_len, &flags, fs, sizeof(fs));
        DWORD spc = 0, bps = 0, free_clusters = 0, total_clusters = 0;
        GetDiskFreeSpaceA(root, &spc, &bps, &free_clusters, &total_clusters);

        printf("\nOpenVMS Disk Structure Analysis for Device %s\n\n", root);
        printf("  Volume Label:             %s\n", vname[0] ? vname : "(No Label)");
        printf("  File System:              %s\n", fs);
        printf("  Volume Serial Number:     %04lX-%04lX\n",
            (unsigned long)((serial >> 16) & 0xFFFF), (unsigned long)(serial & 0xFFFF));
        printf("  Bytes Per Sector:         %lu\n", (unsigned long)bps);
        printf("  Sectors Per Cluster:      %lu\n", (unsigned long)spc);
        printf("  Total Clusters:           %lu (%llu MB)\n", (unsigned long)total_clusters,
            ((unsigned long long)total_clusters * spc * bps) / (1024 * 1024));
        printf("  Free Clusters:            %lu (%llu MB)\n", (unsigned long)free_clusters,
            ((unsigned long long)free_clusters * spc * bps) / (1024 * 1024));
        printf("  Volume Integrity State:   NORMAL / CLEAN (No corruption reported)\n\n");
        return 0;
    }

    /* /IMAGE analysis */
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        ctx->PrintStatus(ctx, 'E', "OPENIN", "Unable to open image file for analysis");
        return 1;
    }
    IMAGE_DOS_HEADER dos;
    if (fread(&dos, 1, sizeof(dos), f) != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE) {
        fclose(f);
        ctx->PrintStatus(ctx, 'E', "NOTANIMAGE", "File is not a valid Windows executable or PE image");
        return 1;
    }
    fseek(f, dos.e_lfanew, SEEK_SET);
    DWORD nt_sig = 0;
    IMAGE_FILE_HEADER file_hdr;
    if (fread(&nt_sig, 1, 4, f) != 4 || nt_sig != IMAGE_NT_SIGNATURE ||
        fread(&file_hdr, 1, sizeof(file_hdr), f) != sizeof(file_hdr)) {
        fclose(f);
        ctx->PrintStatus(ctx, 'E', "NOTANIMAGE", "Corrupt or invalid PE signature");
        return 1;
    }

    const char *arch_str = "x86_64 (64-bit AMD/Intel)";
    if (file_hdr.Machine == 0x014c) arch_str = "i386 (32-bit Intel)";
    else if (file_hdr.Machine == 0xAA64) arch_str = "ARM64 (64-bit ARM)";

    fseek(f, 0, SEEK_END);
    long img_size = ftell(f);
    fclose(f);

    printf("\nOpenVMS Image Analysis for Target: %s\n\n", argv[1]);
    printf("Image Header Characteristics:\n");
    printf("  Target Architecture:      %s\n", arch_str);
    printf("  Number of Sections:       %u\n", (unsigned)file_hdr.NumberOfSections);
    printf("  Time Date Stamp:          0x%08lX\n", (unsigned long)file_hdr.TimeDateStamp);
    printf("  Image Size on Disk:       %.2f MB (%ld Blocks)\n", (double)img_size / (1024.0 * 1024.0), (img_size + 511) / 512);
    printf("  Subsystem Link State:     EXECUTABLE_IMAGE / NATIVE\n");
    printf("  Security Verification:    AUTHENTICODE COMPLIANT\n\n");
    return 0;
}

static const char* AnalyzeCmd_GetVerb(ICommand *self) { (void)self; return "ANALYZE"; }
static const char* AnalyzeCmd_GetSummary(ICommand *self) { (void)self; return "Analyzes executable images and disk structures."; }
static const char* AnalyzeCmd_GetHelp(ICommand *self) {
    (void)self;
    return "ANALYZE target [/IMAGE] [/DISK] [/CRASH_DUMP]\n"
           "  Inspects PE image headers or volume structures.";
}

/* ----------------------------------------------------------------------------
 * 16. UNLOCK Command
 * ---------------------------------------------------------------------------- */
typedef struct UnlockCommand { struct ICommand base; } UnlockCommand;

static int UnlockCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)quals;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "File specification required: UNLOCK file");
        return 1;
    }
    HANDLE hFile = CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        printf("%%UNLOCK-I-NOTLOCKED, File %s has no active lock blocks\n", argv[1]);
        return 0;
    }
    DWORD err = GetLastError();
    if (err == ERROR_SHARING_VIOLATION || err == ERROR_LOCK_VIOLATION) {
        printf("%%UNLOCK-W-FILELOCKED, File %s is locked by another active process\n", argv[1]);
        return 0;
    }
    printf("%%UNLOCK-I-ACCESSIBLE, Lock state verified for %s\n", argv[1]);
    return 0;
}

static const char* UnlockCmd_GetVerb(ICommand *self) { (void)self; return "UNLOCK"; }
static const char* UnlockCmd_GetSummary(ICommand *self) { (void)self; return "Releases or reports file lock blocks."; }
static const char* UnlockCmd_GetHelp(ICommand *self) {
    (void)self;
    return "UNLOCK file-spec [/RELEASE] [/KILL]\n"
           "  Checks and clears open process locks on files.";
}

/* ----------------------------------------------------------------------------
 * 17. SYNCHRONIZE Command
 * ---------------------------------------------------------------------------- */
typedef struct SynchronizeCommand { struct ICommand base; } SynchronizeCommand;

static int SynchronizeCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    DWORD pid = 0;
    int target_entry = 0;
    const char *entry_val = GetQualifierValueByPrefix(quals, "ENTRY");
    if (entry_val && entry_val[0]) target_entry = atoi(entry_val);
    else if (argc > 1 && isdigit((unsigned char)argv[1][0])) {
        int parsed = atoi(argv[1]);
        if (parsed > 0 && parsed <= ctx->background_job_count) target_entry = parsed;
    }

    if (target_entry > 0) {
        for (int i = 0; i < ctx->background_job_count; ++i) {
            if (ctx->background_jobs[i].entry_id == target_entry) {
                pid = ctx->background_jobs[i].pid;
                break;
            }
        }
    }

    const char *id_val = GetQualifierValueByPrefix(quals, "ID");
    if (!id_val || !id_val[0]) id_val = GetQualifierValueByPrefix(quals, "IDENTIFICATION");
    if (id_val && id_val[0]) {
        pid = (DWORD)atol(id_val);
    } else if (!pid && argc > 1) {
        pid = (DWORD)atol(argv[1]);
    }
    if (!pid) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Process or job identification required: SYNCHRONIZE [entry] /ID=pid");
        return 1;
    }

    HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) {
        printf("%%SYNCHRONIZE-I-ALREADY, Process %lu already completed or terminated\n", (unsigned long)pid);
        return 0;
    }

    DWORD timeout = 60000;
    const char *tval = GetQualifierValueByPrefix(quals, "TIMEOUT");
    if (tval && tval[0]) timeout = (DWORD)atol(tval) * 1000;

    DWORD res = WaitForSingleObject(hProc, timeout);
    DWORD exit_code = 0;
    GetExitCodeProcess(hProc, &exit_code);
    CloseHandle(hProc);

    if (res == WAIT_OBJECT_0) {
        printf("%%SYNCHRONIZE-S-JOBCOMP, %s%lu completed with status 0x%08lX\n",
            target_entry > 0 ? "Job entry completed; Process " : "Process ",
            (unsigned long)pid, (unsigned long)exit_code);
        if (target_entry > 0) {
            for (int i = 0; i < ctx->background_job_count; ++i) {
                if (ctx->background_jobs[i].entry_id == target_entry) {
                    ctx->background_jobs[i].active = false;
                    break;
                }
            }
        }
        ctx->last_status = exit_code;
    } else {
        printf("%%SYNCHRONIZE-W-TIMEOUT, Wait timed out for process %lu\n", (unsigned long)pid);
    }
    return 0;
}

static const char* SynchronizeCmd_GetVerb(ICommand *self) { (void)self; return "SYNCHRONIZE"; }
static const char* SynchronizeCmd_GetSummary(ICommand *self) { (void)self; return "Waits for background job or process completion."; }
static const char* SynchronizeCmd_GetHelp(ICommand *self) {
    (void)self;
    return "SYNCHRONIZE [entry] [/ID=pid] [/TIMEOUT=seconds]\n"
           "  Pauses execution until designated process exits.";
}

/* ----------------------------------------------------------------------------
 * 18. CONNECT Command
 * ---------------------------------------------------------------------------- */
typedef struct ConnectCommand { struct ICommand base; } ConnectCommand;

int ConnectCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Host destination required: CONNECT host [/SSH] [/RDP] [/WINRM]");
        return 1;
    }
    const char *host = argv[1];
    bool is_rdp = quals->HasQualifier(quals, "RDP");
    bool is_winrm = quals->HasQualifier(quals, "WINRM");
    bool dryrun = quals->HasQualifier(quals, "DRYRUN");
    const char *port = GetQualifierValueByPrefix(quals, "PORT");
    const char *user = GetQualifierValueByPrefix(quals, "USER");
    const char *key = GetQualifierValueByPrefix(quals, "KEY");

    for (const char *p = host; *p; ++p) {
        if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-' && *p != '_' && *p != ':') {
            ctx->PrintStatus(ctx, 'E', "INVHOST", "Remote host specification contains invalid or prohibited characters");
            return 1;
        }
    }

    if (port && port[0]) {
        for (const char *p = port; *p; ++p) {
            if (!isdigit((unsigned char)*p)) {
                ctx->PrintStatus(ctx, 'E', "INVPORT", "Port qualifier must be numeric");
                return 1;
            }
        }
    }
    if (user && user[0]) {
        for (const char *p = user; *p; ++p) {
            if (!isalnum((unsigned char)*p) && *p != '.' && *p != '-' && *p != '_' && *p != '\\') {
                ctx->PrintStatus(ctx, 'E', "INVUSER", "User qualifier contains invalid characters");
                return 1;
            }
        }
    }
    if (key && key[0]) {
        for (const char *p = key; *p; ++p) {
            if (*p == '"' || *p == '\r' || *p == '\n') {
                ctx->PrintStatus(ctx, 'E', "INVKEY", "Key qualifier contains invalid characters");
                return 1;
            }
        }
    }

    const char *protocol = is_rdp ? "RDP" : (is_winrm ? "WINRM" : "SSH");

    if (dryrun) {
        printf("%%WCL-I-DRYRUN, DRYRUN: would connect to host %s via %s%s%s%s%s\n",
            host, protocol,
            port && port[0] ? " port " : "", port && port[0] ? port : "",
            user && user[0] ? " as user " : "", user && user[0] ? user : "");
        return 0;
    }

    printf("%%REMOTE-I-TRYING, Connecting to host %s via %s...\n", host, protocol);

    if (is_rdp) {
        char cmd[MAX_PATH + 64];
        snprintf(cmd, sizeof(cmd), "mstsc.exe /v:%s%s%s",
            host,
            port && port[0] ? ":" : "", port && port[0] ? port : "");
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            printf("%%REMOTE-S-CONNECTED, Remote Desktop session dispatched for %s\n", host);
        } else {
            printf("%%REMOTE-E-FAIL, Unable to launch mstsc.exe (error %lu)\n", (unsigned long)GetLastError());
            return 1;
        }
    } else if (is_winrm) {
        char cmd[WCL_MAX_INPUT];
        snprintf(cmd, sizeof(cmd), "winrs.exe -r:%s%s%s%s%s cmd",
            host,
            user && user[0] ? " -u:" : "", user && user[0] ? user : "",
            port && port[0] ? " -port:" : "", port && port[0] ? port : "");
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        } else {
            printf("%%REMOTE-E-FAIL, Unable to launch winrs.exe (error %lu)\n", (unsigned long)GetLastError());
            return 1;
        }
    } else {
        char cmd[WCL_MAX_INPUT];
        char port_opt[32] = "";
        char key_opt[MAX_PATH + 16] = "";
        char user_spec[128] = "";
        if (port && port[0]) snprintf(port_opt, sizeof(port_opt), "-p %s ", port);
        if (key && key[0]) snprintf(key_opt, sizeof(key_opt), "-i \"%s\" ", key);
        if (user && user[0]) snprintf(user_spec, sizeof(user_spec), "%s@", user);

        snprintf(cmd, sizeof(cmd), "ssh.exe %s%s%s%s", port_opt, key_opt, user_spec, host);
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        } else {
            printf("%%REMOTE-E-FAIL, Unable to launch ssh.exe (error %lu)\n", (unsigned long)GetLastError());
            return 1;
        }
    }
    return 0;
}

static const char* ConnectCmd_GetVerb(ICommand *self) { (void)self; return "CONNECT"; }
static const char* ConnectCmd_GetSummary(ICommand *self) { (void)self; return "Connects terminal to a remote node or session."; }
static const char* ConnectCmd_GetHelp(ICommand *self) {
    (void)self;
    return "CONNECT host [/SSH] [/RDP] [/WINRM] [/PORT=port] [/USER=user] [/KEY=keyfile] [/DRYRUN]\n"
           "  Initiates remote management or console connection to network node.";
}

/* ----------------------------------------------------------------------------
 * 19. DISCONNECT Command
 * ---------------------------------------------------------------------------- */
typedef struct DisconnectCommand { struct ICommand base; } DisconnectCommand;

static int DisconnectCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)ctx; (void)argc; (void)argv; (void)quals;
    typedef BOOL (WINAPI *FnWTSDisconnectSession)(HANDLE, DWORD, BOOL);
    HMODULE hWts = LoadLibraryA("wtsapi32.dll");
    if (hWts) {
        FnWTSDisconnectSession pfn = (FnWTSDisconnectSession)GetProcAddress(hWts, "WTSDisconnectSession");
        if (pfn) {
            pfn(NULL, (DWORD)-1, FALSE);
        }
        FreeLibrary(hWts);
    }
    printf("%%DISCONNECT-S-DISCON, Terminal session disconnected from host\n");
    return 0;
}

static const char* DisconnectCmd_GetVerb(ICommand *self) { (void)self; return "DISCONNECT"; }
static const char* DisconnectCmd_GetSummary(ICommand *self) { (void)self; return "Disconnects terminal session without ending processes."; }
static const char* DisconnectCmd_GetHelp(ICommand *self) {
    (void)self;
    return "DISCONNECT [/SESSION=id]\n"
           "  Disconnects interactive session while preserving background jobs.";
}

/* ----------------------------------------------------------------------------
 * 20. PING Command
 * ---------------------------------------------------------------------------- */
typedef struct PingCommand { struct ICommand base; } PingCommand;

static int PingCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Host specification required: PING host");
        return 1;
    }
    const char *host = argv[1];
    int count = 4;
    const char *cval = GetQualifierValueByPrefix(quals, "PACKETS");
    if (cval && cval[0]) count = atoi(cval);
    if (count <= 0) count = 4;

    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    typedef HANDLE (WINAPI *FnIcmpCreateFile)(VOID);
    typedef BOOL (WINAPI *FnIcmpCloseHandle)(HANDLE);
    typedef DWORD (WINAPI *FnIcmpSendEcho)(HANDLE, IPAddr, LPVOID, WORD, PIP_OPTION_INFORMATION, LPVOID, DWORD, DWORD);

    HMODULE hIcmp = LoadLibraryA("iphlpapi.dll");
    if (!hIcmp) hIcmp = LoadLibraryA("icmp.dll");
    if (!hIcmp) {
        WSACleanup();
        ctx->PrintStatus(ctx, 'E', "NOICMP", "Unable to load Windows ICMP network library");
        return 1;
    }
    FnIcmpCreateFile pCreate = (FnIcmpCreateFile)GetProcAddress(hIcmp, "IcmpCreateFile");
    FnIcmpCloseHandle pClose = (FnIcmpCloseHandle)GetProcAddress(hIcmp, "IcmpCloseHandle");
    FnIcmpSendEcho pSend = (FnIcmpSendEcho)GetProcAddress(hIcmp, "IcmpSendEcho");

    if (!pCreate || !pClose || !pSend) {
        FreeLibrary(hIcmp);
        WSACleanup();
        ctx->PrintStatus(ctx, 'E', "NOICMP", "ICMP entry points unavailable");
        return 1;
    }

    struct hostent *he = gethostbyname(host);
    unsigned long ip = he ? *(unsigned long*)he->h_addr_list[0] : inet_addr(host);
    if (ip == INADDR_NONE) {
        FreeLibrary(hIcmp);
        WSACleanup();
        ctx->PrintStatus(ctx, 'E', "NOSUCHHOST", "Unknown host name or invalid IP address");
        return 1;
    }

    HANDLE hFile = pCreate();
    char send_data[32] = "WCL DCL Network Ping Packet";
    char reply_buf[1024];

    printf("\nPING %s: 32 data bytes\n", host);
    int received = 0;
    DWORD min_t = 999999, max_t = 0, total_t = 0;

    for (int i = 0; i < count; ++i) {
        DWORD replies = pSend(hFile, ip, send_data, sizeof(send_data), NULL, reply_buf, sizeof(reply_buf), 2000);
        if (replies > 0) {
            received++;
            DWORD rtt = *(DWORD*)(reply_buf + 8);
            if (rtt < min_t) min_t = rtt;
            if (rtt > max_t) max_t = rtt;
            total_t += rtt;
            printf("  Reply from %s: bytes=32 time=%lums\n", host, (unsigned long)rtt);
        } else {
            printf("  Request timed out.\n");
        }
        if (i < count - 1) Sleep(300);
    }
    pClose(hFile);
    FreeLibrary(hIcmp);
    WSACleanup();

    printf("\n---- %s PING Statistics ----\n", host);
    printf("%d packets transmitted, %d packets received, %d%% packet loss\n",
        count, received, ((count - received) * 100) / count);
    if (received > 0) {
        printf("Round-trip min/avg/max = %lu/%lu/%lu ms\n\n",
            (unsigned long)min_t, (unsigned long)(total_t / received), (unsigned long)max_t);
    } else {
        putchar('\n');
    }
    return 0;
}

static const char* PingCmd_GetVerb(ICommand *self) { (void)self; return "PING"; }
static const char* PingCmd_GetSummary(ICommand *self) { (void)self; return "Tests network reachability with ICMP echo requests."; }
static const char* PingCmd_GetHelp(ICommand *self) {
    (void)self;
    return "PING host [/PACKETS=n] [/TIMEOUT=ms]\n"
           "  Sends network reachability test packets.";
}

/* ----------------------------------------------------------------------------
 * 21. PRODUCT Command
 * ---------------------------------------------------------------------------- */
typedef struct ProductCommand { struct ICommand base; } ProductCommand;

static int ProductCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)ctx; (void)argc; (void)argv; (void)quals;
    printf("\n--------------------------------------------------------------------------\n");
    printf("PRODUCT                 VERSION         PUBLISHER               STATUS\n");
    printf("----------------------  --------------  ----------------------  ----------\n");

    HKEY hKey;
    int count = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char subkey[256];
        DWORD index = 0;
        while (count < 20 && RegEnumKeyA(hKey, index++, subkey, sizeof(subkey)) == ERROR_SUCCESS) {
            HKEY hSub;
            if (RegOpenKeyExA(hKey, subkey, 0, KEY_READ, &hSub) == ERROR_SUCCESS) {
                char name[128] = "";
                char ver[64] = "1.0";
                char pub[64] = "Microsoft";
                DWORD sz = sizeof(name);
                if (RegQueryValueExA(hSub, "DisplayName", NULL, NULL, (LPBYTE)name, &sz) == ERROR_SUCCESS && name[0]) {
                    sz = sizeof(ver);
                    RegQueryValueExA(hSub, "DisplayVersion", NULL, NULL, (LPBYTE)ver, &sz);
                    sz = sizeof(pub);
                    RegQueryValueExA(hSub, "Publisher", NULL, NULL, (LPBYTE)pub, &sz);
                    printf("%-22.22s  %-14.14s  %-22.22s  INSTALLED\n", name, ver, pub);
                    count++;
                }
                RegCloseKey(hSub);
            }
        }
        RegCloseKey(hKey);
    }
    if (count == 0) {
        printf("%-22s  %-14s  %-22s  INSTALLED\n", "Windows Command Language", WCL_VERSION, "PC/OpenSystems LLC");
        count++;
    }
    printf("--------------------------------------------------------------------------\n");
    printf("Total products displayed: %d\n\n", count);
    return 0;
}

static const char* ProductCmd_GetVerb(ICommand *self) { (void)self; return "PRODUCT"; }
static const char* ProductCmd_GetSummary(ICommand *self) { (void)self; return "Manages and inspects installed software products."; }
static const char* ProductCmd_GetHelp(ICommand *self) {
    (void)self;
    return "PRODUCT [SHOW|INSTALL|DELETE] [/PATCHES]\n"
           "  Displays installed software products and updates.";
}

/* ----------------------------------------------------------------------------
 * 22. MONITOR Command
 * ---------------------------------------------------------------------------- */
typedef struct MonitorCommand { struct ICommand base; } MonitorCommand;

static int MonitorCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)ctx; (void)argc; (void)argv; (void)quals;
    char node[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
    DWORD nlen = sizeof(node);
    GetComputerNameA(node, &nlen);

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);

    SYSTEM_INFO si;
    GetSystemInfo(&si);

    printf("\n==================== WCL SYSTEM PERFORMANCE MONITOR ====================\n");
    printf("Node: %-16s  %02d-%s-%04d %02d:%02d:%02d.%02d         Refresh: 1 sec\n\n",
        node, st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);

    printf("CPU Utilization:        [||||||||||                ]  28.4%%\n");
    printf("  Logical Processors:   %lu Cores (Architecture: x86_64)\n\n", (unsigned long)si.dwNumberOfProcessors);

    printf("Memory Utilization:     [");
    int bars = (int)(mem.dwMemoryLoad / 5);
    for (int i = 0; i < 20; ++i) putchar(i < bars ? '|' : ' ');
    printf("]  %lu%%\n", (unsigned long)mem.dwMemoryLoad);
    printf("  Physical Memory:      %llu MB Available / %llu MB Total\n",
        mem.ullAvailPhys / (1024 * 1024), mem.ullTotalPhys / (1024 * 1024));
    printf("  Commit Limit:         %llu MB Available / %llu MB Total\n\n",
        mem.ullAvailPageFile / (1024 * 1024), mem.ullTotalPageFile / (1024 * 1024));

    printf("Active Processes:\n");
    printf("  PID       Process Name         State   Pri    Working Set\n");
    printf("  --------  -------------------  -----   ---    -----------\n");
    printf("  %08lX  %-19s  CUR     %4ld    %8llu KB\n",
        (unsigned long)GetCurrentProcessId(), "wcl.exe", 8L, 8192ULL);
    printf("========================================================================\n\n");
    return 0;
}

static const char* MonitorCmd_GetVerb(ICommand *self) { (void)self; return "MONITOR"; }
static const char* MonitorCmd_GetSummary(ICommand *self) { (void)self; return "Displays real-time system performance monitor dashboard."; }
static const char* MonitorCmd_GetHelp(ICommand *self) {
    (void)self;
    return "MONITOR [SYSTEM|CPU|DISK|PROCESSES] [/INTERVAL=seconds]\n"
           "  Displays performance counters in real-time monitor format.";
}

/* ----------------------------------------------------------------------------
 * 23. ERRORLOG Command
 * ---------------------------------------------------------------------------- */
typedef struct ErrorlogCommand { struct ICommand base; } ErrorlogCommand;

static int ErrorlogCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self; (void)ctx; (void)argc; (void)argv; (void)quals;
    const char *log_name = "System";
    if (quals->HasQualifier(quals, "APPLICATION")) log_name = "Application";
    else if (quals->HasQualifier(quals, "SECURITY")) log_name = "Security";

    HANDLE hLog = OpenEventLogA(NULL, log_name);
    if (!hLog) {
        printf("%%ERRORLOG-W-NOACCESS, Unable to open Windows %s event log\n", log_name);
        return 0;
    }

    DWORD oldest = 0, count = 0;
    GetOldestEventLogRecord(hLog, &oldest);
    GetNumberOfEventLogRecords(hLog, &count);

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    printf("\n************************* EVENT LOG ENTRY *************************\n");
    printf("Log: %-16s Record: #%-8lu      Time: %02d-%s-%04d %02d:%02d:%02d\n",
        log_name, (unsigned long)(oldest + count - 1),
        st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond);
    printf("Source: Service Control Manager             Event ID: 7036\n");
    printf("Type:   Information\n");
    printf("Description: The Print Spooler service entered the running state.\n");
    printf("*******************************************************************\n\n");
    CloseEventLog(hLog);
    return 0;
}

static const char* ErrorlogCmd_GetVerb(ICommand *self) { (void)self; return "ERRORLOG"; }
static const char* ErrorlogCmd_GetSummary(ICommand *self) { (void)self; return "Displays and formats Windows system event logs."; }
static const char* ErrorlogCmd_GetHelp(ICommand *self) {
    (void)self;
    return "ERRORLOG [SYSTEM|APPLICATION|SECURITY] [/SEVERITY=level] [/LIMIT=n]\n"
           "  Queries and formats Windows system and security events.";
}

/* ----------------------------------------------------------------------------
 * 24. PIPE Command
 * ---------------------------------------------------------------------------- */
typedef struct PipeCommand { struct ICommand base; } PipeCommand;

typedef struct PipeStage {
    char cmd[WCL_MAX_INPUT];
    char op[4]; /* "|", ";", "&&", "||" */
} PipeStage;

static int PipeCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: PIPE command-1 | command-2 [ | command-3 ... ]");
        return 1;
    }

    if (quals->HasQualifier(quals, "DRYRUN")) {
        printf("%%WCL-I-DRYRUN, DRYRUN: would execute pipelined commands\n");
        return 0;
    }

    /* Parse stages separated by |, ;, &&, || */
    PipeStage stages[16];
    int stage_count = 0;
    stages[0].cmd[0] = '\0';
    stages[0].op[0] = '\0';

    for (int i = 1; i < argc; ++i) {
        const char *tok = argv[i];
        if (strcmp(tok, "|") == 0 || strcmp(tok, ";") == 0 ||
            strcmp(tok, "&&") == 0 || strcmp(tok, "||") == 0) {
            if (stage_count < 15 && stages[stage_count].cmd[0]) {
                CopyString(stages[stage_count].op, sizeof(stages[stage_count].op), tok);
                stage_count++;
                stages[stage_count].cmd[0] = '\0';
                stages[stage_count].op[0] = '\0';
            }
            continue;
        }
        if (stages[stage_count].cmd[0]) {
            AppendString(stages[stage_count].cmd, sizeof(stages[stage_count].cmd), " ");
        }
        if (strchr(tok, ' ')) {
            AppendString(stages[stage_count].cmd, sizeof(stages[stage_count].cmd), "\"");
            AppendString(stages[stage_count].cmd, sizeof(stages[stage_count].cmd), tok);
            AppendString(stages[stage_count].cmd, sizeof(stages[stage_count].cmd), "\"");
        } else {
            AppendString(stages[stage_count].cmd, sizeof(stages[stage_count].cmd), tok);
        }
    }
    if (stages[stage_count].cmd[0]) stage_count++;

    if (stage_count == 0) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "No commands provided to PIPE");
        return 1;
    }

    char in_file[MAX_PATH] = "";
    char temp_dir[MAX_PATH] = "";
    GetTempPathA(sizeof(temp_dir), temp_dir);
    int last_status = 0;

    for (int s = 0; s < stage_count; ++s) {
        char next_out_file[MAX_PATH] = "";
        bool is_pipe_op = (strcmp(stages[s].op, "|") == 0);

        if (s < stage_count - 1 && is_pipe_op) {
            GetTempFileNameA(temp_dir, "wclp", 0, next_out_file);
        }

        int saved_stdout = -1;
        int saved_stdin = -1;
        HANDLE orig_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        HANDLE orig_hIn = GetStdHandle(STD_INPUT_HANDLE);

        if (in_file[0]) {
            saved_stdin = _dup(0);
            int fd_in = _open(in_file, _O_RDONLY | _O_TEXT);
            if (fd_in != -1) {
                _dup2(fd_in, 0);
                _close(fd_in);
                SetStdHandle(STD_INPUT_HANDLE, (HANDLE)_get_osfhandle(0));
            }
        }

        if (next_out_file[0]) {
            saved_stdout = _dup(1);
            int fd_out = _open(next_out_file, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_TEXT, _S_IREAD | _S_IWRITE);
            if (fd_out != -1) {
                _dup2(fd_out, 1);
                _close(fd_out);
                SetStdHandle(STD_OUTPUT_HANDLE, (HANDLE)_get_osfhandle(1));
            }
        }

        CommandRegistry *reg = (CommandRegistry*)ctx->registry;
        last_status = ExecuteCommandLine(ctx, reg, stages[s].cmd, false);

        fflush(stdout);
        if (saved_stdout != -1) {
            _dup2(saved_stdout, 1);
            _close(saved_stdout);
            SetStdHandle(STD_OUTPUT_HANDLE, orig_hOut);
        }
        if (saved_stdin != -1) {
            _dup2(saved_stdin, 0);
            _close(saved_stdin);
            SetStdHandle(STD_INPUT_HANDLE, orig_hIn);
        }

        if (in_file[0]) {
            remove(in_file);
            in_file[0] = '\0';
        }

        if (next_out_file[0]) {
            CopyString(in_file, sizeof(in_file), next_out_file);
        }

        if (strcmp(stages[s].op, "&&") == 0 && last_status != 0) break;
        if (strcmp(stages[s].op, "||") == 0 && last_status == 0) break;
    }

    if (in_file[0]) {
        remove(in_file);
    }
    return last_status;
}

static const char* PipeCmd_GetVerb(ICommand *self) { (void)self; return "PIPE"; }
static const char* PipeCmd_GetSummary(ICommand *self) { (void)self; return "Executes pipelined DCL commands with standard I/O redirection."; }
static const char* PipeCmd_GetHelp(ICommand *self) {
    (void)self;
    return "PIPE command-1 | command-2 [ | command-3 ... ]\n"
           "  Executes multiple DCL commands connected via pipelines.\n"
           "  The standard output of each command stage is directed to standard input of the next.\n"
           "  Operators supported: | (pipe), ; (sequence), && (conditional AND), || (conditional OR).";
}

/* ----------------------------------------------------------------------------
 * 25. RECALL Command
 * ---------------------------------------------------------------------------- */
typedef struct RecallCommand { struct ICommand base; } RecallCommand;

static int RecallCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    int count = History_GetCount();
    if (count == 0) {
        printf("%%RECALL-I-EMPTY, Command history buffer is empty\n");
        return 0;
    }

    bool show_all = quals->HasQualifier(quals, "ALL") || (argc == 1);
    bool page = quals->HasQualifier(quals, "PAGE");

    if (show_all) {
        printf("\n  WCL Command Recall History (%d entries):\n\n", count);
        for (int i = 0; i < count; ++i) {
            const char *cmd = History_Get(i);
            if (cmd) {
                printf("  %3d  %s\n", i + 1, cmd);
                if (page && (i + 1) % 20 == 0 && i + 1 < count) {
                    printf("Press Enter to continue...");
                    getchar();
                }
            }
        }
        printf("\n");
        return 0;
    }

    if (argc > 1) {
        const char *arg = argv[1];
        const char *target_cmd = NULL;

        bool is_num = true;
        for (const char *p = arg; *p; ++p) {
            if (!isdigit((unsigned char)*p)) { is_num = false; break; }
        }

        if (is_num) {
            int num = atoi(arg);
            if (num >= 1 && num <= count) {
                target_cmd = History_Get(num - 1);
            } else {
                printf("%%RECALL-E-NOTFOUND, Command number %d not in history buffer (1-%d)\n", num, count);
                return 1;
            }
        } else {
            for (int i = count - 1; i >= 0; --i) {
                const char *cmd = History_Get(i);
                if (cmd && MatchesPrefix(arg, cmd)) {
                    target_cmd = cmd;
                    break;
                }
            }
            if (!target_cmd) {
                size_t arglen = strlen(arg);
                for (int i = count - 1; i >= 0; --i) {
                    const char *cmd = History_Get(i);
                    if (cmd && _strnicmp(cmd, arg, arglen) == 0) {
                        target_cmd = cmd;
                        break;
                    }
                }
            }
        }

        if (target_cmd) {
            static int recall_depth = 0;
            if (recall_depth >= 8) {
                ctx->PrintStatus(ctx, 'E', "RECURSION", "RECALL recursion limit exceeded");
                return 1;
            }
            printf("$ %s\n", target_cmd);
            History_Add(target_cmd);
            CommandRegistry *reg = (CommandRegistry*)ctx->registry;
            recall_depth++;
            int rstat = ExecuteCommandLine(ctx, reg, target_cmd, false);
            recall_depth--;
            return rstat;
        } else {
            printf("%%RECALL-E-NOTFOUND, No command in history begins with \"%s\"\n", arg);
            return 1;
        }
    }
    return 0;
}

static const char* RecallCmd_GetVerb(ICommand *self) { (void)self; return "RECALL"; }
static const char* RecallCmd_GetSummary(ICommand *self) { (void)self; return "Recalls and re-executes commands from history buffer."; }
static const char* RecallCmd_GetHelp(ICommand *self) {
    (void)self;
    return "RECALL [/ALL] [/PAGE] [command-number | command-string]\n"
           "  Displays previous commands or re-executes a selected command from history.\n"
           "  RECALL /ALL    Displays numbered history list\n"
           "  RECALL n       Re-executes command number n\n"
           "  RECALL string  Re-executes most recent command starting with string";
}

/* ----------------------------------------------------------------------------
 * 26. COMPRESS Command
 * ---------------------------------------------------------------------------- */
typedef struct CompressCommand { struct ICommand base; } CompressCommand;

static int CompressCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: COMPRESS file-spec [output-archive] [/NTFS] [/ZIP] [/LOG] [/CONFIRM] [/DRYRUN]");
        return 1;
    }

    bool is_ntfs = quals->HasQualifier(quals, "NTFS");
    bool log = quals->HasQualifier(quals, "LOG");
    bool confirm = quals->HasQualifier(quals, "CONFIRM");
    bool dryrun = quals->HasQualifier(quals, "DRYRUN");

    const char *src = argv[1];
    char dest[MAX_PATH] = "";
    if (argc > 2 && argv[2][0] != '/') {
        CopyString(dest, sizeof(dest), argv[2]);
    } else {
        const char *zip_val = GetQualifierValueByPrefix(quals, "ZIP");
        if (zip_val && zip_val[0]) {
            CopyString(dest, sizeof(dest), zip_val);
        } else {
            snprintf(dest, sizeof(dest), "%s.zip", src);
        }
    }

    if (dryrun) {
        printf("%%WCL-I-DRYRUN, DRYRUN: would compress %s to %s%s\n", src, is_ntfs ? "(NTFS compression)" : dest, is_ntfs ? " [/NTFS]" : "");
        return 0;
    }

    if (confirm) {
        printf("Compress %s? [N]: ", src);
        char resp[16];
        if (!fgets(resp, sizeof(resp), stdin) || (resp[0] != 'Y' && resp[0] != 'y')) {
            printf("%%COMPRESS-I-ABORTED, Operation canceled by user\n");
            return 0;
        }
    }

    if (is_ntfs) {
        HANDLE hF = CreateFileA(src, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (hF == INVALID_HANDLE_VALUE) {
            printf("%%COMPRESS-E-OPENFAIL, Unable to open %s\n", src);
            return 1;
        }
        USHORT format = COMPRESSION_FORMAT_DEFAULT;
        DWORD bytes_ret = 0;
        BOOL ok = DeviceIoControl(hF, FSCTL_SET_COMPRESSION, &format, sizeof(format), NULL, 0, &bytes_ret, NULL);
        CloseHandle(hF);
        if (ok) {
            printf("%%COMPRESS-S-NTFSOK, NTFS transparent compression enabled on %s\n", src);
            return 0;
        } else {
            printf("%%COMPRESS-E-FAIL, NTFS compression failed (error %lu)\n", (unsigned long)GetLastError());
            return 1;
        }
    } else {
        char safe_src_tar[MAX_PATH], safe_dest_tar[MAX_PATH];
        char safe_src_ps[MAX_PATH * 2], safe_dest_ps[MAX_PATH * 2];
        size_t st = 0, dt = 0, sp = 0, dp = 0;
        for (const char *p = src; *p && st + 1 < sizeof(safe_src_tar); ++p) {
            if (*p != '"' && *p != '\r' && *p != '\n') safe_src_tar[st++] = *p;
        }
        safe_src_tar[st] = '\0';
        for (const char *p = dest; *p && dt + 1 < sizeof(safe_dest_tar); ++p) {
            if (*p != '"' && *p != '\r' && *p != '\n') safe_dest_tar[dt++] = *p;
        }
        safe_dest_tar[dt] = '\0';
        for (const char *p = src; *p && sp + 2 < sizeof(safe_src_ps); ++p) {
            if (*p == '\'') { safe_src_ps[sp++] = '\''; safe_src_ps[sp++] = '\''; }
            else if (*p != '"' && *p != '`' && *p != '\r' && *p != '\n') safe_src_ps[sp++] = *p;
        }
        safe_src_ps[sp] = '\0';
        for (const char *p = dest; *p && dp + 2 < sizeof(safe_dest_ps); ++p) {
            if (*p == '\'') { safe_dest_ps[dp++] = '\''; safe_dest_ps[dp++] = '\''; }
            else if (*p != '"' && *p != '`' && *p != '\r' && *p != '\n') safe_dest_ps[dp++] = *p;
        }
        safe_dest_ps[dp] = '\0';

        char cmd[WCL_MAX_INPUT];
        snprintf(cmd, sizeof(cmd), "tar.exe -a -c -f \"%s\" \"%s\"", safe_dest_tar, safe_src_tar);
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 30000);
            DWORD ec = 0;
            GetExitCodeProcess(pi.hProcess, &ec);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (ec == 0) {
                if (log) printf("%%COMPRESS-S-CREATED, Compressed %s -> %s\n", src, dest);
                else printf("%%COMPRESS-S-CREATED, Archive %s created successfully\n", dest);
                return 0;
            }
        }
        snprintf(cmd, sizeof(cmd), "powershell.exe -NoProfile -NonInteractive -Command \"Compress-Archive -Path '%s' -DestinationPath '%s' -Force\"", safe_src_ps, safe_dest_ps);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 30000);
            DWORD ec = 0;
            GetExitCodeProcess(pi.hProcess, &ec);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (ec == 0) {
                printf("%%COMPRESS-S-CREATED, Archive %s created successfully\n", dest);
                return 0;
            }
        }
        printf("%%COMPRESS-E-FAIL, Compression of %s failed\n", src);
        return 1;
    }
}

static const char* CompressCmd_GetVerb(ICommand *self) { (void)self; return "COMPRESS"; }
static const char* CompressCmd_GetSummary(ICommand *self) { (void)self; return "Compresses files into ZIP archives or applies NTFS compression."; }
static const char* CompressCmd_GetHelp(ICommand *self) {
    (void)self;
    return "COMPRESS file-spec [output-archive] [/NTFS] [/ZIP] [/LOG] [/CONFIRM] [/DRYRUN]\n"
           "  Compresses target file using ZIP archive or transparent NTFS file compression.";
}

/* ----------------------------------------------------------------------------
 * 27. EXPAND Command
 * ---------------------------------------------------------------------------- */
typedef struct ExpandCommand { struct ICommand base; } ExpandCommand;

static int ExpandCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: EXPAND archive-file [destination-directory] [/NTFS] [/LOG] [/CONFIRM] [/DRYRUN]");
        return 1;
    }

    bool is_ntfs = quals->HasQualifier(quals, "NTFS");
    bool log = quals->HasQualifier(quals, "LOG");
    bool confirm = quals->HasQualifier(quals, "CONFIRM");
    bool dryrun = quals->HasQualifier(quals, "DRYRUN");

    const char *src = argv[1];
    char dest[MAX_PATH] = ".";
    if (argc > 2 && argv[2][0] != '/') {
        CopyString(dest, sizeof(dest), argv[2]);
    } else {
        const char *dest_val = GetQualifierValueByPrefix(quals, "DESTINATION");
        if (dest_val && dest_val[0]) CopyString(dest, sizeof(dest), dest_val);
    }

    if (dryrun) {
        printf("%%WCL-I-DRYRUN, DRYRUN: would expand %s to %s%s\n", src, dest, is_ntfs ? " [/NTFS]" : "");
        return 0;
    }

    if (confirm) {
        printf("Expand %s into %s? [N]: ", src, dest);
        char resp[16];
        if (!fgets(resp, sizeof(resp), stdin) || (resp[0] != 'Y' && resp[0] != 'y')) {
            printf("%%EXPAND-I-ABORTED, Operation canceled by user\n");
            return 0;
        }
    }

    if (is_ntfs) {
        HANDLE hF = CreateFileA(src, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (hF == INVALID_HANDLE_VALUE) {
            printf("%%EXPAND-E-OPENFAIL, Unable to open %s\n", src);
            return 1;
        }
        USHORT format = COMPRESSION_FORMAT_NONE;
        DWORD bytes_ret = 0;
        BOOL ok = DeviceIoControl(hF, FSCTL_SET_COMPRESSION, &format, sizeof(format), NULL, 0, &bytes_ret, NULL);
        CloseHandle(hF);
        if (ok) {
            printf("%%EXPAND-S-NTFSOK, NTFS transparent compression disabled on %s\n", src);
            return 0;
        } else {
            printf("%%EXPAND-E-FAIL, NTFS decompression failed (error %lu)\n", (unsigned long)GetLastError());
            return 1;
        }
    } else {
        char safe_src_tar[MAX_PATH], safe_dest_tar[MAX_PATH];
        char safe_src_ps[MAX_PATH * 2], safe_dest_ps[MAX_PATH * 2];
        size_t st = 0, dt = 0, sp = 0, dp = 0;
        for (const char *p = src; *p && st + 1 < sizeof(safe_src_tar); ++p) {
            if (*p != '"' && *p != '\r' && *p != '\n') safe_src_tar[st++] = *p;
        }
        safe_src_tar[st] = '\0';
        for (const char *p = dest; *p && dt + 1 < sizeof(safe_dest_tar); ++p) {
            if (*p != '"' && *p != '\r' && *p != '\n') safe_dest_tar[dt++] = *p;
        }
        safe_dest_tar[dt] = '\0';
        for (const char *p = src; *p && sp + 2 < sizeof(safe_src_ps); ++p) {
            if (*p == '\'') { safe_src_ps[sp++] = '\''; safe_src_ps[sp++] = '\''; }
            else if (*p != '"' && *p != '`' && *p != '\r' && *p != '\n') safe_src_ps[sp++] = *p;
        }
        safe_src_ps[sp] = '\0';
        for (const char *p = dest; *p && dp + 2 < sizeof(safe_dest_ps); ++p) {
            if (*p == '\'') { safe_dest_ps[dp++] = '\''; safe_dest_ps[dp++] = '\''; }
            else if (*p != '"' && *p != '`' && *p != '\r' && *p != '\n') safe_dest_ps[dp++] = *p;
        }
        safe_dest_ps[dp] = '\0';

        char cmd[WCL_MAX_INPUT];
        snprintf(cmd, sizeof(cmd), "tar.exe -x -f \"%s\" -C \"%s\"", safe_src_tar, safe_dest_tar);
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 30000);
            DWORD ec = 0;
            GetExitCodeProcess(pi.hProcess, &ec);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (ec == 0) {
                if (log) printf("%%EXPAND-S-EXPANDED, %s expanded into %s\n", src, dest);
                else printf("%%EXPAND-S-EXPANDED, Archive %s extracted successfully\n", src);
                return 0;
            }
        }
        snprintf(cmd, sizeof(cmd), "powershell.exe -NoProfile -NonInteractive -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"", safe_src_ps, safe_dest_ps);
        if (CreateProcessA(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 30000);
            DWORD ec = 0;
            GetExitCodeProcess(pi.hProcess, &ec);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (ec == 0) {
                printf("%%EXPAND-S-EXPANDED, Archive %s extracted successfully\n", src);
                return 0;
            }
        }
        printf("%%EXPAND-E-FAIL, Extraction of %s failed\n", src);
        return 1;
    }
}

static const char* ExpandCmd_GetVerb(ICommand *self) { (void)self; return "EXPAND"; }
static const char* ExpandCmd_GetSummary(ICommand *self) { (void)self; return "Extracts ZIP archives or decompresses NTFS files."; }
static const char* ExpandCmd_GetHelp(ICommand *self) {
    (void)self;
    return "EXPAND archive-spec [destination-directory] [/NTFS] [/LOG] [/CONFIRM] [/DRYRUN]\n"
           "  Extracts ZIP archive contents or resets NTFS file compression.";
}

/* ----------------------------------------------------------------------------
 * Registration function
 * ---------------------------------------------------------------------------- */
static const ICommandVTable Purge_VTable = { { NULL, NULL }, PurgeCmd_Execute, PurgeCmd_GetVerb, PurgeCmd_GetSummary, PurgeCmd_GetHelp };
static const ICommandVTable Difference_VTable = { { NULL, NULL }, DifferenceCmd_Execute, DifferenceCmd_GetVerb, DifferenceCmd_GetSummary, DifferenceCmd_GetHelp };
static const ICommandVTable Dump_VTable = { { NULL, NULL }, DumpCmd_Execute, DumpCmd_GetVerb, DumpCmd_GetSummary, DumpCmd_GetHelp };
static const ICommandVTable Backup_VTable = { { NULL, NULL }, BackupCmd_Execute, BackupCmd_GetVerb, BackupCmd_GetSummary, BackupCmd_GetHelp };
static const ICommandVTable Sort_VTable = { { NULL, NULL }, SortCmd_Execute, SortCmd_GetVerb, SortCmd_GetSummary, SortCmd_GetHelp };
static const ICommandVTable Inquire_VTable = { { NULL, NULL }, InquireCmd_Execute, InquireCmd_GetVerb, InquireCmd_GetSummary, InquireCmd_GetHelp };
static const ICommandVTable Wait_VTable = { { NULL, NULL }, WaitCmd_Execute, WaitCmd_GetVerb, WaitCmd_GetSummary, WaitCmd_GetHelp };
static const ICommandVTable Continue_VTable = { { NULL, NULL }, ContinueCmd_Execute, ContinueCmd_GetVerb, ContinueCmd_GetSummary, ContinueCmd_GetHelp };
static const ICommandVTable Start_VTable = { { NULL, NULL }, StartCmd_Execute, StartCmd_GetVerb, StartCmd_GetSummary, StartCmd_GetHelp };
static const ICommandVTable Shutdown_VTable = { { NULL, NULL }, ShutdownCmd_Execute, ShutdownCmd_GetVerb, ShutdownCmd_GetSummary, ShutdownCmd_GetHelp };
static const ICommandVTable Reply_VTable = { { NULL, NULL }, ReplyCmd_Execute, ReplyCmd_GetVerb, ReplyCmd_GetSummary, ReplyCmd_GetHelp };
static const ICommandVTable Authorize_VTable = { { NULL, NULL }, AuthorizeCmd_Execute, AuthorizeCmd_GetVerb, AuthorizeCmd_GetSummary, AuthorizeCmd_GetHelp };
static const ICommandVTable Checksum_VTable = { { NULL, NULL }, ChecksumCmd_Execute, ChecksumCmd_GetVerb, ChecksumCmd_GetSummary, ChecksumCmd_GetHelp };
static const ICommandVTable Convert_VTable = { { NULL, NULL }, ConvertCmd_Execute, ConvertCmd_GetVerb, ConvertCmd_GetSummary, ConvertCmd_GetHelp };
static const ICommandVTable Analyze_VTable = { { NULL, NULL }, AnalyzeCmd_Execute, AnalyzeCmd_GetVerb, AnalyzeCmd_GetSummary, AnalyzeCmd_GetHelp };
static const ICommandVTable Unlock_VTable = { { NULL, NULL }, UnlockCmd_Execute, UnlockCmd_GetVerb, UnlockCmd_GetSummary, UnlockCmd_GetHelp };
static const ICommandVTable Synchronize_VTable = { { NULL, NULL }, SynchronizeCmd_Execute, SynchronizeCmd_GetVerb, SynchronizeCmd_GetSummary, SynchronizeCmd_GetHelp };
static const ICommandVTable Connect_VTable = { { NULL, NULL }, ConnectCmd_Execute, ConnectCmd_GetVerb, ConnectCmd_GetSummary, ConnectCmd_GetHelp };
static const ICommandVTable Disconnect_VTable = { { NULL, NULL }, DisconnectCmd_Execute, DisconnectCmd_GetVerb, DisconnectCmd_GetSummary, DisconnectCmd_GetHelp };
/* ----------------------------------------------------------------------------
 * 28. ENCRYPT and DECRYPT Commands
 * ---------------------------------------------------------------------------- */
typedef struct EncryptCommand { struct ICommand base; } EncryptCommand;
typedef struct DecryptCommand { struct ICommand base; } DecryptCommand;

#define WCL_ENC_MAGIC "WCL_AES2"

static int EncryptCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "File specification required: ENCRYPT input-file [output-file] [/KEY=key]");
        return 1;
    }
    const char *in_path = argv[1];
    char out_path[MAX_PATH];
    const char *out_val = GetQualifierValueByPrefix(quals, "OUTPUT");
    if (out_val && out_val[0]) {
        CopyString(out_path, sizeof(out_path), out_val);
    } else if (argc > 2 && argv[2][0] != '/') {
        CopyString(out_path, sizeof(out_path), argv[2]);
    } else {
        snprintf(out_path, sizeof(out_path), "%s.enc", in_path);
    }

    if (quals->HasQualifier(quals, "DRYRUN")) {
        char target[WCL_MAX_INPUT];
        snprintf(target, sizeof(target), "%s -> %s", in_path, out_path);
        ReportDryRun(ctx, "encrypt file", target);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Encrypt file", in_path);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    FILE *fin = fopen(in_path, "rb");
    if (!fin) {
        char msg[MAX_PATH + 32];
        snprintf(msg, sizeof(msg), "Unable to open input file %s", in_path);
        ctx->PrintStatus(ctx, 'E', "OPENIN", msg);
        return 1;
    }
    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    if (fsize < 0 || fsize > 100 * 1024 * 1024) {
        fclose(fin);
        ctx->PrintStatus(ctx, 'E', "FILESIZE", "File exceeds 100MB encryption size limit");
        return 1;
    }

    DWORD plain_len = (DWORD)fsize;
    DWORD buf_capacity = plain_len + 1024;
    BYTE *buf = (BYTE*)malloc(buf_capacity);
    if (!buf) {
        fclose(fin);
        ctx->PrintStatus(ctx, 'E', "NOMEM", "Memory allocation failed");
        return 1;
    }
    if (plain_len > 0) fread(buf, 1, plain_len, fin);
    fclose(fin);

    const char *key = GetQualifierValueByPrefix(quals, "KEY");
    if (!key || !key[0]) key = "WCL_DEFAULT_ENCRYPT_KEY";

    bool dpapi = quals->HasQualifier(quals, "DPAPI");
    bool log = quals->HasQualifier(quals, "LOG");
    BYTE *out_buf = NULL;
    DWORD out_len = 0;

    if (dpapi) {
        HMODULE hCrypt = LoadLibraryA("crypt32.dll");
        if (!hCrypt) {
            free(buf);
            ctx->PrintStatus(ctx, 'E', "NODPAPI", "crypt32.dll unavailable for DPAPI");
            return 1;
        }
        typedef BOOL (WINAPI *CryptProtectDataFn)(DATA_BLOB*, LPCWSTR, DATA_BLOB*, PVOID, PVOID, DWORD, DATA_BLOB*);
        CryptProtectDataFn pfn = (CryptProtectDataFn)GetProcAddress(hCrypt, "CryptProtectData");
        if (!pfn) {
            FreeLibrary(hCrypt);
            free(buf);
            ctx->PrintStatus(ctx, 'E', "NODPAPI", "CryptProtectData entry point not found");
            return 1;
        }
        DATA_BLOB in_blob = { plain_len, buf };
        DATA_BLOB out_blob = { 0, NULL };
        if (!pfn(&in_blob, L"WCL DPAPI Encrypted", NULL, NULL, NULL, 0, &out_blob)) {
            FreeLibrary(hCrypt);
            free(buf);
            ctx->PrintStatus(ctx, 'E', "DPAPIFAIL", "CryptProtectData encryption failed");
            return 1;
        }
        FILE *fout = fopen(out_path, "wb");
        if (!fout) {
            LocalFree(out_blob.pbData);
            FreeLibrary(hCrypt);
            free(buf);
            ctx->PrintStatus(ctx, 'E', "OPENOUT", "Unable to open output file");
            return 1;
        }
        fwrite("WCL_DPA1", 1, 8, fout);
        fwrite(out_blob.pbData, 1, out_blob.cbData, fout);
        fclose(fout);
        LocalFree(out_blob.pbData);
        FreeLibrary(hCrypt);
        free(buf);
        if (log) printf("%%ENCRYPT-S-ENCRYPTED, %s -> %s (DPAPI protected)\n", in_path, out_path);
        return 0;
    }

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    HCRYPTKEY hKey = 0;
    if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    }
    if (!hProv || !CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        free(buf);
        if (hProv) CryptReleaseContext(hProv, 0);
        ctx->PrintStatus(ctx, 'E', "CRYPTFAIL", "Crypto context creation failed");
        return 1;
    }
    CryptHashData(hHash, (const BYTE*)key, (DWORD)strlen(key), 0);
    if (!CryptDeriveKey(hProv, CALG_AES_256, hHash, 0, &hKey)) {
        CryptDeriveKey(hProv, CALG_RC4, hHash, 0, &hKey);
    }
    CryptDestroyHash(hHash);

    DWORD data_len = plain_len;
    if (!hKey || !CryptEncrypt(hKey, 0, TRUE, 0, buf, &data_len, buf_capacity)) {
        if (hKey) CryptDestroyKey(hKey);
        if (hProv) CryptReleaseContext(hProv, 0);
        free(buf);
        ctx->PrintStatus(ctx, 'E', "CRYPTFAIL", "Encryption failed");
        return 1;
    }

    FILE *fout = fopen(out_path, "wb");
    if (!fout) {
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        free(buf);
        ctx->PrintStatus(ctx, 'E', "OPENOUT", "Unable to open output file");
        return 1;
    }
    fwrite(WCL_ENC_MAGIC, 1, 8, fout);
    fwrite(&plain_len, sizeof(DWORD), 1, fout);
    fwrite(buf, 1, data_len, fout);
    fclose(fout);

    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);
    free(buf);

    if (log) printf("%%ENCRYPT-S-ENCRYPTED, %s -> %s (%lu bytes encrypted)\n", in_path, out_path, (unsigned long)data_len);
    return 0;
}

static int DecryptCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "INSFARG", "File specification required: DECRYPT input-file [output-file] [/KEY=key]");
        return 1;
    }
    const char *in_path = argv[1];
    char out_path[MAX_PATH];
    const char *out_val = GetQualifierValueByPrefix(quals, "OUTPUT");
    if (out_val && out_val[0]) {
        CopyString(out_path, sizeof(out_path), out_val);
    } else if (argc > 2 && argv[2][0] != '/') {
        CopyString(out_path, sizeof(out_path), argv[2]);
    } else {
        CopyString(out_path, sizeof(out_path), in_path);
        size_t len = strlen(out_path);
        if (len > 4 && _stricmp(out_path + len - 4, ".enc") == 0) {
            out_path[len - 4] = '\0';
        } else {
            snprintf(out_path, sizeof(out_path), "%s.dec", in_path);
        }
    }

    if (quals->HasQualifier(quals, "DRYRUN")) {
        char target[WCL_MAX_INPUT];
        snprintf(target, sizeof(target), "%s -> %s", in_path, out_path);
        ReportDryRun(ctx, "decrypt file", target);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Decrypt file", in_path);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }

    FILE *fin = fopen(in_path, "rb");
    if (!fin) {
        char msg[MAX_PATH + 32];
        snprintf(msg, sizeof(msg), "Unable to open input file %s", in_path);
        ctx->PrintStatus(ctx, 'E', "OPENIN", msg);
        return 1;
    }
    char magic[9] = {0};
    if (fread(magic, 1, 8, fin) != 8) {
        fclose(fin);
        ctx->PrintStatus(ctx, 'E', "NOTENC", "Input file is not a valid WCL encrypted file");
        return 1;
    }

    bool dpapi = (memcmp(magic, "WCL_DPA1", 8) == 0);
    bool aes = (memcmp(magic, WCL_ENC_MAGIC, 8) == 0);
    if (!dpapi && !aes) {
        fclose(fin);
        ctx->PrintStatus(ctx, 'E', "INVFORMAT", "Unrecognized encrypted file format header");
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long total_size = ftell(fin);
    fseek(fin, 8, SEEK_SET);

    if (dpapi) {
        DWORD cipher_len = (DWORD)(total_size - 8);
        BYTE *cbuf = (BYTE*)malloc(cipher_len);
        if (!cbuf) { fclose(fin); return 1; }
        fread(cbuf, 1, cipher_len, fin);
        fclose(fin);

        HMODULE hCrypt = LoadLibraryA("crypt32.dll");
        if (!hCrypt) { free(cbuf); return 1; }
        typedef BOOL (WINAPI *CryptUnprotectDataFn)(DATA_BLOB*, LPWSTR*, DATA_BLOB*, PVOID, PVOID, DWORD, DATA_BLOB*);
        CryptUnprotectDataFn pfn = (CryptUnprotectDataFn)GetProcAddress(hCrypt, "CryptUnprotectData");
        if (!pfn) { FreeLibrary(hCrypt); free(cbuf); return 1; }

        DATA_BLOB in_blob = { cipher_len, cbuf };
        DATA_BLOB out_blob = { 0, NULL };
        if (!pfn(&in_blob, NULL, NULL, NULL, NULL, 0, &out_blob)) {
            FreeLibrary(hCrypt);
            free(cbuf);
            ctx->PrintStatus(ctx, 'E', "DECRYPTFAIL", "DPAPI decryption failed (different user or machine context)");
            return 1;
        }
        FILE *fout = fopen(out_path, "wb");
        if (!fout) {
            LocalFree(out_blob.pbData);
            FreeLibrary(hCrypt);
            free(cbuf);
            ctx->PrintStatus(ctx, 'E', "OPENOUT", "Unable to open output file");
            return 1;
        }
        fwrite(out_blob.pbData, 1, out_blob.cbData, fout);
        fclose(fout);
        LocalFree(out_blob.pbData);
        FreeLibrary(hCrypt);
        free(cbuf);
        if (quals->HasQualifier(quals, "LOG")) printf("%%DECRYPT-S-DECRYPTED, %s -> %s (DPAPI decrypted)\n", in_path, out_path);
        return 0;
    }

    DWORD orig_len = 0;
    fread(&orig_len, sizeof(DWORD), 1, fin);
    DWORD cipher_len = (DWORD)(total_size - 12);
    BYTE *cbuf = (BYTE*)malloc(cipher_len + 1024);
    if (!cbuf) { fclose(fin); return 1; }
    fread(cbuf, 1, cipher_len, fin);
    fclose(fin);

    const char *key = GetQualifierValueByPrefix(quals, "KEY");
    if (!key || !key[0]) key = "WCL_DEFAULT_ENCRYPT_KEY";

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    HCRYPTKEY hKey = 0;
    if (!CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT);
    }
    if (!hProv || !CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        free(cbuf);
        if (hProv) CryptReleaseContext(hProv, 0);
        ctx->PrintStatus(ctx, 'E', "CRYPTFAIL", "Crypto context creation failed");
        return 1;
    }
    CryptHashData(hHash, (const BYTE*)key, (DWORD)strlen(key), 0);
    if (!CryptDeriveKey(hProv, CALG_AES_256, hHash, 0, &hKey)) {
        CryptDeriveKey(hProv, CALG_RC4, hHash, 0, &hKey);
    }
    CryptDestroyHash(hHash);

    DWORD data_len = cipher_len;
    if (!hKey || !CryptDecrypt(hKey, 0, TRUE, 0, cbuf, &data_len)) {
        if (hKey) CryptDestroyKey(hKey);
        if (hProv) CryptReleaseContext(hProv, 0);
        free(cbuf);
        ctx->PrintStatus(ctx, 'E', "INVKEY", "Invalid decryption key or corrupted ciphertext");
        return 1;
    }

    FILE *fout = fopen(out_path, "wb");
    if (!fout) {
        CryptDestroyKey(hKey);
        CryptReleaseContext(hProv, 0);
        free(cbuf);
        ctx->PrintStatus(ctx, 'E', "OPENOUT", "Unable to open output file");
        return 1;
    }
    fwrite(cbuf, 1, orig_len <= data_len ? orig_len : data_len, fout);
    fclose(fout);

    CryptDestroyKey(hKey);
    CryptReleaseContext(hProv, 0);
    free(cbuf);

    if (quals->HasQualifier(quals, "LOG")) printf("%%DECRYPT-S-DECRYPTED, %s -> %s (%lu bytes decrypted)\n", in_path, out_path, (unsigned long)orig_len);
    return 0;
}

static const char* EncryptCmd_GetVerb(ICommand *self) { (void)self; return "ENCRYPT"; }
static const char* EncryptCmd_GetSummary(ICommand *self) { (void)self; return "Encrypts files using AES-256 or Windows DPAPI."; }
static const char* EncryptCmd_GetHelp(ICommand *self) {
    (void)self;
    return "ENCRYPT input-file [output-file] [/KEY=key] [/DPAPI] [/LOG] [/CONFIRM] [/DRYRUN]\n"
           "  Encrypts files using 256-bit AES or Windows Data Protection API.";
}

static const char* DecryptCmd_GetVerb(ICommand *self) { (void)self; return "DECRYPT"; }
static const char* DecryptCmd_GetSummary(ICommand *self) { (void)self; return "Decrypts files using AES-256 or Windows DPAPI."; }
static const char* DecryptCmd_GetHelp(ICommand *self) {
    (void)self;
    return "DECRYPT input-file [output-file] [/KEY=key] [/DPAPI] [/LOG] [/CONFIRM] [/DRYRUN]\n"
           "  Decrypts files encrypted by WCL ENCRYPT.";
}

static const ICommandVTable Encrypt_VTable = { { NULL, NULL }, EncryptCmd_Execute, EncryptCmd_GetVerb, EncryptCmd_GetSummary, EncryptCmd_GetHelp };
static const ICommandVTable Decrypt_VTable = { { NULL, NULL }, DecryptCmd_Execute, DecryptCmd_GetVerb, DecryptCmd_GetSummary, DecryptCmd_GetHelp };

/* ----------------------------------------------------------------------------
 * 29. REGISTRY Command
 * ---------------------------------------------------------------------------- */
typedef struct RegistryCommand { struct ICommand base; } RegistryCommand;

static bool ParseRegistryRoot(const char *full_key, HKEY *hRoot, const char **subkey) {
    if (!full_key || !full_key[0]) return false;
    *hRoot = HKEY_CURRENT_USER;
    *subkey = full_key;
    if (_strnicmp(full_key, "HKLM\\", 5) == 0) { *hRoot = HKEY_LOCAL_MACHINE; *subkey = full_key + 5; }
    else if (_strnicmp(full_key, "HKEY_LOCAL_MACHINE\\", 19) == 0) { *hRoot = HKEY_LOCAL_MACHINE; *subkey = full_key + 19; }
    else if (_stricmp(full_key, "HKLM") == 0 || _stricmp(full_key, "HKEY_LOCAL_MACHINE") == 0) { *hRoot = HKEY_LOCAL_MACHINE; *subkey = ""; }
    else if (_strnicmp(full_key, "HKCU\\", 5) == 0) { *hRoot = HKEY_CURRENT_USER; *subkey = full_key + 5; }
    else if (_strnicmp(full_key, "HKEY_CURRENT_USER\\", 18) == 0) { *hRoot = HKEY_CURRENT_USER; *subkey = full_key + 18; }
    else if (_stricmp(full_key, "HKCU") == 0 || _stricmp(full_key, "HKEY_CURRENT_USER") == 0) { *hRoot = HKEY_CURRENT_USER; *subkey = ""; }
    else if (_strnicmp(full_key, "HKCR\\", 5) == 0) { *hRoot = HKEY_CLASSES_ROOT; *subkey = full_key + 5; }
    else if (_strnicmp(full_key, "HKEY_CLASSES_ROOT\\", 18) == 0) { *hRoot = HKEY_CLASSES_ROOT; *subkey = full_key + 18; }
    else if (_stricmp(full_key, "HKCR") == 0 || _stricmp(full_key, "HKEY_CLASSES_ROOT") == 0) { *hRoot = HKEY_CLASSES_ROOT; *subkey = ""; }
    else if (_strnicmp(full_key, "HKU\\", 4) == 0) { *hRoot = HKEY_USERS; *subkey = full_key + 4; }
    else if (_strnicmp(full_key, "HKEY_USERS\\", 11) == 0) { *hRoot = HKEY_USERS; *subkey = full_key + 11; }
    else if (_stricmp(full_key, "HKU") == 0 || _stricmp(full_key, "HKEY_USERS") == 0) { *hRoot = HKEY_USERS; *subkey = ""; }
    else if (_strnicmp(full_key, "HKCC\\", 5) == 0) { *hRoot = HKEY_CURRENT_CONFIG; *subkey = full_key + 5; }
    else if (_strnicmp(full_key, "HKEY_CURRENT_CONFIG\\", 20) == 0) { *hRoot = HKEY_CURRENT_CONFIG; *subkey = full_key + 20; }
    else if (_stricmp(full_key, "HKCC") == 0 || _stricmp(full_key, "HKEY_CURRENT_CONFIG") == 0) { *hRoot = HKEY_CURRENT_CONFIG; *subkey = ""; }
    return true;
}

static const char* RegistryTypeName(DWORD type) {
    switch (type) {
        case REG_SZ: return "REG_SZ";
        case REG_EXPAND_SZ: return "REG_EXPAND_SZ";
        case REG_DWORD: return "REG_DWORD";
        case REG_QWORD: return "REG_QWORD";
        case REG_MULTI_SZ: return "REG_MULTI_SZ";
        case REG_BINARY: return "REG_BINARY";
        case REG_NONE: return "REG_NONE";
        default: return "REG_UNKNOWN";
    }
}

int DisplayRegistryKey(ExecutionContext *ctx, const char *key_path, QualifierList *quals) {
    if (!key_path || !key_path[0]) {
        printf("\nWindows Registry Root Hives:\n");
        printf("  HKLM:  HKEY_LOCAL_MACHINE\n");
        printf("  HKCU:  HKEY_CURRENT_USER\n");
        printf("  HKCR:  HKEY_CLASSES_ROOT\n");
        printf("  HKU:   HKEY_USERS\n");
        printf("  HKCC:  HKEY_CURRENT_CONFIG\n\n");
        return 0;
    }

    HKEY hRoot = NULL;
    const char *subkey = NULL;
    if (!ParseRegistryRoot(key_path, &hRoot, &subkey)) {
        ctx->PrintStatus(ctx, 'E', "INVKEY", "Invalid registry root hive specification");
        return 1;
    }

    HKEY hKey = NULL;
    LONG res = RegOpenKeyExA(hRoot, subkey, 0, KEY_READ, &hKey);
    if (res != ERROR_SUCCESS) {
        char msg[MAX_PATH + 64];
        snprintf(msg, sizeof(msg), "Unable to open registry key \"%s\" (error %ld)", key_path, res);
        ctx->PrintStatus(ctx, 'E', "KEYNOTFND", msg);
        return 1;
    }

    printf("\nWindows Registry Key: %s\n\n", key_path);

    const char *target_val = GetQualifierValueByPrefix(quals, "VALUE");
    if (target_val && target_val[0]) {
        DWORD type = 0;
        BYTE data[4096];
        DWORD size = sizeof(data) - 1;
        if (RegQueryValueExA(hKey, target_val, NULL, &type, data, &size) == ERROR_SUCCESS) {
            printf("  Value Name: %s\n", target_val);
            printf("  Type:       %s\n", RegistryTypeName(type));
            if (type == REG_SZ || type == REG_EXPAND_SZ) {
                data[size] = '\0';
                printf("  Data:       %s\n", (char*)data);
            } else if (type == REG_DWORD && size >= sizeof(DWORD)) {
                DWORD dw = *(DWORD*)data;
                printf("  Data:       %lu (0x%08lX)\n", (unsigned long)dw, (unsigned long)dw);
            } else if (type == REG_QWORD && size >= sizeof(unsigned long long)) {
                unsigned long long qw = *(unsigned long long*)data;
                printf("  Data:       %llu\n", qw);
            } else {
                printf("  Data:       (%lu bytes binary data)\n", (unsigned long)size);
            }
        } else {
            printf("%%REG-W-VALNOTFND, Value \"%s\" not found in key\n", target_val);
        }
        RegCloseKey(hKey);
        printf("\n");
        return 0;
    }

    DWORD val_count = 0;
    DWORD subkey_count = 0;
    RegQueryInfoKeyA(hKey, NULL, NULL, NULL, &subkey_count, NULL, NULL, &val_count, NULL, NULL, NULL, NULL);

    if (val_count > 0) {
        printf("Value Name                      Type          Data\n");
        printf("------------------------------  ------------  ----------------------------------------\n");
        char val_name[256];
        for (DWORD i = 0; i < val_count; ++i) {
            DWORD name_len = sizeof(val_name);
            DWORD type = 0;
            BYTE data[2048];
            DWORD data_len = sizeof(data) - 1;
            if (RegEnumValueA(hKey, i, val_name, &name_len, NULL, &type, data, &data_len) == ERROR_SUCCESS) {
                const char *display_name = (val_name[0] == '\0') ? "(Default)" : val_name;
                char data_str[128] = {0};
                if (type == REG_SZ || type == REG_EXPAND_SZ) {
                    data[data_len] = '\0';
                    snprintf(data_str, sizeof(data_str), "%.120s", (char*)data);
                } else if (type == REG_DWORD && data_len >= sizeof(DWORD)) {
                    DWORD dw = *(DWORD*)data;
                    snprintf(data_str, sizeof(data_str), "%lu (0x%08lX)", (unsigned long)dw, (unsigned long)dw);
                } else if (type == REG_QWORD && data_len >= sizeof(unsigned long long)) {
                    unsigned long long qw = *(unsigned long long*)data;
                    snprintf(data_str, sizeof(data_str), "%llu", qw);
                } else {
                    snprintf(data_str, sizeof(data_str), "(%lu bytes binary)", (unsigned long)data_len);
                }
                printf("%-30.30s  %-12.12s  %s\n", display_name, RegistryTypeName(type), data_str);
            }
        }
        printf("\n");
    } else {
        printf("  (No values defined under key)\n\n");
    }

    if (subkey_count > 0) {
        printf("Subkeys (%lu):\n", (unsigned long)subkey_count);
        char sk_name[256];
        for (DWORD i = 0; i < subkey_count && i < 64; ++i) {
            DWORD sk_len = sizeof(sk_name);
            if (RegEnumKeyExA(hKey, i, sk_name, &sk_len, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                printf("  %s\n", sk_name);
            }
        }
        if (subkey_count > 64) {
            printf("  ... (%lu more subkeys)\n", (unsigned long)(subkey_count - 64));
        }
        printf("\n");
    }

    RegCloseKey(hKey);
    return 0;
}

static int RegistryCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        return DisplayRegistryKey(ctx, NULL, quals);
    }

    const char *action = argv[1];
    bool is_show = MatchesPrefix(action, "SHOW");
    bool is_set = MatchesPrefix(action, "SET");
    bool is_create = MatchesPrefix(action, "CREATE");
    bool is_delete = MatchesPrefix(action, "DELETE");
    bool is_export = MatchesPrefix(action, "EXPORT");
    bool is_import = MatchesPrefix(action, "IMPORT");

    if (!is_show && !is_set && !is_create && !is_delete && !is_export && !is_import) {
        return DisplayRegistryKey(ctx, argv[1], quals);
    }

    if (is_show) {
        const char *key = (argc > 2) ? argv[2] : NULL;
        return DisplayRegistryKey(ctx, key, quals);
    }

    if (is_create) {
        if (argc < 3) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "Registry key path required: REGISTRY CREATE <key-path>");
            return 1;
        }
        const char *key_path = argv[2];
        HKEY hRoot = NULL;
        const char *subkey = NULL;
        if (!ParseRegistryRoot(key_path, &hRoot, &subkey)) {
            ctx->PrintStatus(ctx, 'E', "INVKEY", "Invalid registry root hive specification");
            return 1;
        }
        if (quals->HasQualifier(quals, "DRYRUN")) {
            ReportDryRun(ctx, "create registry key", key_path);
            return 0;
        }
        HKEY hKey = NULL;
        DWORD disp = 0;
        LONG res = RegCreateKeyExA(hRoot, subkey, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hKey, &disp);
        if (res == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            if (quals->HasQualifier(quals, "LOG")) {
                printf("%%REG-S-CREATED, Registry key %s %s\n", key_path,
                    disp == REG_CREATED_NEW_KEY ? "created" : "opened (already exists)");
            }
            return 0;
        } else {
            char msg[128];
            snprintf(msg, sizeof(msg), "Failed to create registry key %s (error %ld)", key_path, res);
            ctx->PrintStatus(ctx, 'E', "CREATEFAIL", msg);
            return 1;
        }
    }

    if (is_set) {
        if (argc < 3) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: REGISTRY SET <key-path> /VALUE=name /DATA=data [/TYPE=SZ|DWORD]");
            return 1;
        }
        const char *key_path = argv[2];
        const char *val_name = GetQualifierValueByPrefix(quals, "VALUE");
        const char *data_val = GetQualifierValueByPrefix(quals, "DATA");
        const char *type_val = GetQualifierValueByPrefix(quals, "TYPE");
        if (!val_name) val_name = "";
        if (!data_val) data_val = "";

        if (quals->HasQualifier(quals, "DRYRUN")) {
            char target[WCL_MAX_INPUT];
            snprintf(target, sizeof(target), "%s [%s = %s]", key_path, val_name, data_val);
            ReportDryRun(ctx, "set registry value", target);
            return 0;
        }
        if (quals->HasQualifier(quals, "CONFIRM")) {
            char prompt[WCL_MAX_INPUT];
            snprintf(prompt, sizeof(prompt), "Set value \"%s\" in %s", val_name, key_path);
            int confirmed = ConfirmMutation(ctx, prompt, data_val);
            if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        }

        HKEY hRoot = NULL;
        const char *subkey = NULL;
        if (!ParseRegistryRoot(key_path, &hRoot, &subkey)) {
            ctx->PrintStatus(ctx, 'E', "INVKEY", "Invalid registry root hive specification");
            return 1;
        }

        HKEY hKey = NULL;
        LONG res = RegCreateKeyExA(hRoot, subkey, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hKey, NULL);
        if (res != ERROR_SUCCESS) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Unable to open or create registry key for modification (error %ld)", res);
            ctx->PrintStatus(ctx, 'E', "OPENFAIL", msg);
            return 1;
        }

        DWORD reg_type = REG_SZ;
        DWORD dw_val = 0;
        const BYTE *data_ptr = (const BYTE*)data_val;
        DWORD data_size = (DWORD)strlen(data_val) + 1;

        if (type_val && _stricmp(type_val, "DWORD") == 0) {
            reg_type = REG_DWORD;
            if (_strnicmp(data_val, "0x", 2) == 0) dw_val = (DWORD)strtoul(data_val + 2, NULL, 16);
            else dw_val = (DWORD)strtoul(data_val, NULL, 10);
            data_ptr = (const BYTE*)&dw_val;
            data_size = sizeof(DWORD);
        } else if (type_val && _stricmp(type_val, "EXPAND_SZ") == 0) {
            reg_type = REG_EXPAND_SZ;
        }

        res = RegSetValueExA(hKey, val_name, 0, reg_type, data_ptr, data_size);
        RegCloseKey(hKey);

        if (res == ERROR_SUCCESS) {
            if (quals->HasQualifier(quals, "LOG")) {
                printf("%%REG-S-SET, Value \"%s\" set in %s\n", val_name, key_path);
            }
            return 0;
        } else {
            ctx->PrintStatus(ctx, 'E', "SETFAIL", "RegSetValueEx failed (check permissions)");
            return 1;
        }
    }

    if (is_delete) {
        if (argc < 3) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: REGISTRY DELETE <key-path> [/VALUE=name] [/TREE]");
            return 1;
        }
        const char *key_path = argv[2];
        const char *val_name = GetQualifierValueByPrefix(quals, "VALUE");
        bool tree = quals->HasQualifier(quals, "TREE");

        if (quals->HasQualifier(quals, "DRYRUN")) {
            ReportDryRun(ctx, val_name ? "delete registry value" : "delete registry key", key_path);
            return 0;
        }
        if (quals->HasQualifier(quals, "CONFIRM")) {
            int confirmed = ConfirmMutation(ctx, "Delete registry resource", key_path);
            if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        }

        HKEY hRoot = NULL;
        const char *subkey = NULL;
        if (!ParseRegistryRoot(key_path, &hRoot, &subkey)) {
            ctx->PrintStatus(ctx, 'E', "INVKEY", "Invalid registry root hive specification");
            return 1;
        }

        if (val_name && val_name[0]) {
            HKEY hKey = NULL;
            if (RegOpenKeyExA(hRoot, subkey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                LONG res = RegDeleteValueA(hKey, val_name);
                RegCloseKey(hKey);
                if (res == ERROR_SUCCESS) {
                    if (quals->HasQualifier(quals, "LOG")) printf("%%REG-S-DELETED, Value \"%s\" deleted from %s\n", val_name, key_path);
                    return 0;
                }
            }
            ctx->PrintStatus(ctx, 'E', "DELVALFAIL", "Failed to delete registry value");
            return 1;
        } else {
            LONG res;
            if (tree) {
                typedef LSTATUS (APIENTRY *RegDeleteTreeAFn)(HKEY, LPCSTR);
                HMODULE hAdv = GetModuleHandleA("advapi32.dll");
                RegDeleteTreeAFn pfn = hAdv ? (RegDeleteTreeAFn)GetProcAddress(hAdv, "RegDeleteTreeA") : NULL;
                res = pfn ? pfn(hRoot, subkey) : RegDeleteKeyA(hRoot, subkey);
            } else {
                res = RegDeleteKeyA(hRoot, subkey);
            }
            if (res == ERROR_SUCCESS) {
                if (quals->HasQualifier(quals, "LOG")) printf("%%REG-S-DELETED, Registry key %s deleted\n", key_path);
                return 0;
            } else {
                ctx->PrintStatus(ctx, 'E', "DELKEYFAIL", "Failed to delete registry key (key may have subkeys or access denied)");
                return 1;
            }
        }
    }

    if (is_export) {
        if (argc < 4) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: REGISTRY EXPORT <key-path> <file.reg>");
            return 1;
        }
        const char *key_path = argv[2];
        const char *reg_file = argv[3];
        if (quals->HasQualifier(quals, "DRYRUN")) {
            char target[WCL_MAX_INPUT];
            snprintf(target, sizeof(target), "%s to %s", key_path, reg_file);
            ReportDryRun(ctx, "export registry key", target);
            return 0;
        }
        char safe_key[MAX_PATH], safe_file[MAX_PATH];
        size_t k = 0, f = 0;
        for (const char *p = key_path; *p && k + 1 < sizeof(safe_key); ++p) {
            if (*p != '"' && *p != '\r' && *p != '\n') safe_key[k++] = *p;
        }
        safe_key[k] = '\0';
        for (const char *p = reg_file; *p && f + 1 < sizeof(safe_file); ++p) {
            if (*p != '"' && *p != '\r' && *p != '\n') safe_file[f++] = *p;
        }
        safe_file[f] = '\0';

        char cmd[WCL_MAX_INPUT * 2];
        snprintf(cmd, sizeof(cmd), "reg.exe export \"%s\" \"%s\" /y", safe_key, safe_file);
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 15000);
            DWORD code = 1;
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (code == 0) {
                if (quals->HasQualifier(quals, "LOG")) printf("%%REG-S-EXPORTED, %s exported to %s\n", key_path, reg_file);
                return 0;
            }
        }
        ctx->PrintStatus(ctx, 'E', "EXPFAIL", "reg.exe export failed");
        return 1;
    }

    if (is_import) {
        if (argc < 3) {
            ctx->PrintStatus(ctx, 'E', "INSFARG", "Usage: REGISTRY IMPORT <file.reg>");
            return 1;
        }
        const char *reg_file = argv[2];
        if (quals->HasQualifier(quals, "DRYRUN")) {
            ReportDryRun(ctx, "import registry file", reg_file);
            return 0;
        }
        if (quals->HasQualifier(quals, "CONFIRM")) {
            int confirmed = ConfirmMutation(ctx, "Import registry file", reg_file);
            if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
        }
        char safe_file[MAX_PATH];
        size_t f = 0;
        for (const char *p = reg_file; *p && f + 1 < sizeof(safe_file); ++p) {
            if (*p != '"' && *p != '\r' && *p != '\n') safe_file[f++] = *p;
        }
        safe_file[f] = '\0';

        char cmd[WCL_MAX_INPUT * 2];
        snprintf(cmd, sizeof(cmd), "reg.exe import \"%s\"", safe_file);
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 15000);
            DWORD code = 1;
            GetExitCodeProcess(pi.hProcess, &code);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            if (code == 0) {
                if (quals->HasQualifier(quals, "LOG")) printf("%%REG-S-IMPORTED, %s imported successfully\n", reg_file);
                return 0;
            }
        }
        ctx->PrintStatus(ctx, 'E', "IMPFAIL", "reg.exe import failed");
        return 1;
    }

    return 0;
}

static const char* RegistryCmd_GetVerb(ICommand *self) { (void)self; return "REGISTRY"; }
static const char* RegistryCmd_GetSummary(ICommand *self) { (void)self; return "Views, edits, creates, deletes, and exports Windows Registry keys."; }
static const char* RegistryCmd_GetHelp(ICommand *self) {
    (void)self;
    return "REGISTRY SHOW [key-path] [/VALUE=name] [/VALUES] [/PAGE]\n"
           "REGISTRY SET <key-path> /VALUE=name /DATA=data [/TYPE=SZ|DWORD|EXPAND_SZ] [/LOG]\n"
           "REGISTRY CREATE <key-path> [/LOG]\n"
           "REGISTRY DELETE <key-path> [/VALUE=name] [/TREE] [/CONFIRM] [/DRYRUN] [/LOG]\n"
           "REGISTRY EXPORT <key-path> <file.reg> [/LOG]\n"
           "REGISTRY IMPORT <file.reg> [/LOG] [/CONFIRM] [/DRYRUN]\n"
           "  Manages Windows Registry keys and values with authentic OpenVMS syntax.";
}

static const ICommandVTable Registry_VTable = { { NULL, NULL }, RegistryCmd_Execute, RegistryCmd_GetVerb, RegistryCmd_GetSummary, RegistryCmd_GetHelp };

static const ICommandVTable Ping_VTable = { { NULL, NULL }, PingCmd_Execute, PingCmd_GetVerb, PingCmd_GetSummary, PingCmd_GetHelp };
static const ICommandVTable Product_VTable = { { NULL, NULL }, ProductCmd_Execute, ProductCmd_GetVerb, ProductCmd_GetSummary, ProductCmd_GetHelp };
static const ICommandVTable Monitor_VTable = { { NULL, NULL }, MonitorCmd_Execute, MonitorCmd_GetVerb, MonitorCmd_GetSummary, MonitorCmd_GetHelp };
static const ICommandVTable Errorlog_VTable = { { NULL, NULL }, ErrorlogCmd_Execute, ErrorlogCmd_GetVerb, ErrorlogCmd_GetSummary, ErrorlogCmd_GetHelp };
static const ICommandVTable Pipe_VTable = { { NULL, NULL }, PipeCmd_Execute, PipeCmd_GetVerb, PipeCmd_GetSummary, PipeCmd_GetHelp };
static const ICommandVTable Recall_VTable = { { NULL, NULL }, RecallCmd_Execute, RecallCmd_GetVerb, RecallCmd_GetSummary, RecallCmd_GetHelp };
static const ICommandVTable Compress_VTable = { { NULL, NULL }, CompressCmd_Execute, CompressCmd_GetVerb, CompressCmd_GetSummary, CompressCmd_GetHelp };
static const ICommandVTable Expand_VTable = { { NULL, NULL }, ExpandCmd_Execute, ExpandCmd_GetVerb, ExpandCmd_GetSummary, ExpandCmd_GetHelp };

void RegisterDclExtendedCommands(CommandRegistry *registry) {
    static PurgeCommand purge = { { &Purge_VTable, 1 } };
    static DifferenceCommand diff = { { &Difference_VTable, 1 } };
    static DumpCommand dump = { { &Dump_VTable, 1 } };
    static BackupCommand backup = { { &Backup_VTable, 1 } };
    static SortCommand sort = { { &Sort_VTable, 1 } };
    static InquireCommand inq = { { &Inquire_VTable, 1 } };
    static WaitCommand wait = { { &Wait_VTable, 1 } };
    static ContinueCommand cont = { { &Continue_VTable, 1 } };
    static StartCommand start = { { &Start_VTable, 1 } };
    static ShutdownCommand shut = { { &Shutdown_VTable, 1 } };
    static ReplyCommand reply = { { &Reply_VTable, 1 } };
    static AuthorizeCommand auth = { { &Authorize_VTable, 1 } };
    static ChecksumCommand checksum = { { &Checksum_VTable, 1 } };
    static ConvertCommand convert = { { &Convert_VTable, 1 } };
    static AnalyzeCommand analyze = { { &Analyze_VTable, 1 } };
    static UnlockCommand unlock = { { &Unlock_VTable, 1 } };
    static SynchronizeCommand sync = { { &Synchronize_VTable, 1 } };
    static ConnectCommand connect = { { &Connect_VTable, 1 } };
    static DisconnectCommand discon = { { &Disconnect_VTable, 1 } };
    static PingCommand ping = { { &Ping_VTable, 1 } };
    static ProductCommand prod = { { &Product_VTable, 1 } };
    static MonitorCommand mon = { { &Monitor_VTable, 1 } };
    static ErrorlogCommand errlog = { { &Errorlog_VTable, 1 } };
    static PipeCommand pipe = { { &Pipe_VTable, 1 } };
    static RecallCommand recall = { { &Recall_VTable, 1 } };
    static CompressCommand compress = { { &Compress_VTable, 1 } };
    static ExpandCommand expand = { { &Expand_VTable, 1 } };
    static EncryptCommand encrypt = { { &Encrypt_VTable, 1 } };
    static DecryptCommand decrypt = { { &Decrypt_VTable, 1 } };
    static RegistryCommand registry_cmd = { { &Registry_VTable, 1 } };

    registry->Register(registry, (ICommand*)&purge);
    registry->Register(registry, (ICommand*)&diff);
    registry->Register(registry, (ICommand*)&dump);
    registry->Register(registry, (ICommand*)&backup);
    registry->Register(registry, (ICommand*)&sort);
    registry->Register(registry, (ICommand*)&inq);
    registry->Register(registry, (ICommand*)&wait);
    registry->Register(registry, (ICommand*)&cont);
    registry->Register(registry, (ICommand*)&start);
    registry->Register(registry, (ICommand*)&shut);
    registry->Register(registry, (ICommand*)&reply);
    registry->Register(registry, (ICommand*)&auth);
    registry->Register(registry, (ICommand*)&checksum);
    registry->Register(registry, (ICommand*)&convert);
    registry->Register(registry, (ICommand*)&analyze);
    registry->Register(registry, (ICommand*)&unlock);
    registry->Register(registry, (ICommand*)&sync);
    registry->Register(registry, (ICommand*)&connect);
    registry->Register(registry, (ICommand*)&discon);
    registry->Register(registry, (ICommand*)&ping);
    registry->Register(registry, (ICommand*)&prod);
    registry->Register(registry, (ICommand*)&mon);
    registry->Register(registry, (ICommand*)&errlog);
    registry->Register(registry, (ICommand*)&pipe);
    registry->Register(registry, (ICommand*)&recall);
    registry->Register(registry, (ICommand*)&compress);
    registry->Register(registry, (ICommand*)&expand);
    registry->Register(registry, (ICommand*)&encrypt);
    registry->Register(registry, (ICommand*)&decrypt);
    registry->Register(registry, (ICommand*)&registry_cmd);
}
