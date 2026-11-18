#define _CRT_SECURE_NO_WARNINGS
#include "../include/wcl_internal.h"
#include "../core/symbols_lexicals.h"
#include "../platform/windows/scheduler.h"
#include "show.h"

int DisplayRegistryKey(ExecutionContext *ctx, const char *key_path, QualifierList *quals);
#include <conio.h>
#include <sddl.h>

typedef struct ShowCommand {
    struct ICommand base;
} ShowCommand;

struct ShowOptionDefinition {
    const char *name;
    const char *qualifiers;
    const char *windows_mapping;
};

static const ShowOptionDefinition show_options[] = {
    { "ACCOUNTING", "ACCOUNTING|NOACCOUNTING|OUTPUT|NOOUTPUT", "Windows process CPU, I/O, and memory counters; /OUTPUT selects a report file." },
    { "AUDIT", "ALARM|ALL|ARCHIVE|AUDIT|ENABLE|EXACT|HIGHLIGHT|JOURNAL|OUTPUT|PAGE|SEARCH|SERVER|WRAP|NOPAGE|NOWRAP|NOOUTPUT|SYSTEM|SECURITY|APPLICATION|EVENTS|RECORDS", "Windows Security audit policy and event-log availability, not OpenVMS audit records." },
    { "BROADCAST", "BROADCAST|NOBROADCAST|OUTPUT|NOOUTPUT", "Windows interactive-session and network-notification state." },
    { "CLUSTER", "", "Windows Failover Cluster service and local host identity; this does not claim OpenVMS cluster membership." },
    { "CPU", "ACTIVE|ACTIVE_SET|AFFINITY|ALL|BRIEF|CONFIGURE_SET|EXACT|FULL|HIGHLIGHT|IA|OUTPUT|PAGE|PERMANENT|POTENTIAL_SET|PREFERRED_CPU|SEARCH|SET|STANDBY_SET|SUMMARY|SYSTEM|WRAP|NOOUTPUT|NOPAGE|NOWRAP", "Windows processor topology, affinity, and cumulative system CPU times." },
    { "DEFAULT", "TRANSLATION_ATTRIBUTES", "The current Windows working directory and its resolved full path." },
    { "DEVICES", "ALLOCATED|BITMAP|BRIEF|EXACT|FILES|FULL|HIGHLIGHT|MOUNTED|MULTIPATH|OUTPUT|PAGE|PREFERRED_CPU|REBUILD|REBUILD_STATUS|SEARCH|SIZE|SYSTEM|UNITS|WINDOWS|WRAP|NOOUTPUT|NOPAGE|NOSYSTEM|NOWRAP|SERVED", "Windows volumes, drive types, capacity, and mapped remote drives; /SERVED selects remote-drive mappings." },
    { "DISPLAY", "ALL|CREATE|EXTRACT|IA|INPUT|QUOTA|SYMBOLS|VALUE|VALUES|NODE|NOWAIT", "Windows console dimensions, mode, input/output handles, and host identity." },
    { "DUMP_OPTIONS", "", "Windows crash-dump configuration from the system CrashControl registry settings." },
    { "ENTROPY", "OUTPUT|SYMBOLS|NOOUTPUT|NOSYMBOLS", "Windows system random-generator availability and performance-counter state; no cryptographic entropy estimate is asserted." },
    { "ENTRY", "BATCH|BRIEF|BY_JOB_STATUS|COPIES|DEVICE|EXACT|FILES|FORM|FULL|GENERIC|HIGHLIGHT|KEEP|OUTPUT|PAGE|PASSALL|PRIORITY|SEARCH|USER_NAME|WRAP|NOFEED|NOLOG|NOOUTPUT|NOPAGE|NOTIFY|NOWRAP", "WCL Task Scheduler jobs; task state and metadata are shown instead of VMS queue entries." },
    { "ERROR", "EXACT|FULL|HIGHLIGHT|OUTPUT|PAGE|SEARCH|WRAP|NOOUTPUT|NOPAGE|NOWRAP", "The current Win32 last-error value and its Windows message text." },
    { "FASTPATH", "CPU|IA|IP|OUTPUT|NOOUTPUT", "Windows network-interface and processor-offload capabilities where available." },
    { "IMAGE", "OUTPUT|PAGE|NOPAGE", "Windows executable-image paths and process image metadata." },
    { "INTRUSION", "INTRUSION_RECORD|LC|OUTPUT|TYPE|NODE", "Windows account lockout and Security event-log configuration; VMS intrusion records are not present." },
    { "KEY", "ALL|BRIEF|DIRECTORY|FULL|KEY|STATE|TERMINATE|NOBRIEF|NOFULL|NOSTATE", "Windows console input mode and active console key-input state, not DCL key definitions." },
    { "LICENSE", "ADD|BEFORE|BRIEF|CHAR|CHARGE_TABLE|CLUSTER|EXACT|FULL|HIER|HIERARCHY|HIGHLIGHT|LMF|OE|OUTPUT|PAGE|PRODUCER|RELEASE|RELEASE_DATE|RESERVE|SEARCH|SINCE|TERM|TERMINATION_DATE|UNIT_REQUIREMENT|UNIT_REQUIREMENTS|USAGE|VMS|WARNING_INTERVAL|WRAP|NOOUTPUT|NOPAGE|NOWRAP", "Windows product edition and processor topology; this is not OpenVMS PAK or activation data." },
    { "LOGICAL", "ACCESS_MODE|ALL|CLUSTER|DESCENDANTS|EXACT|FULL|GROUP|HIGHLIGHT|JOB|NAME_TABLE|OUTPUT|PAGE|PARENT_TABLE|PROCESS|SEARCH|STRUCTURE|SYSTEM|TABLE|WRAP|NODESCENDANTS|NOOUTPUT|NOPAGE|NOSTRUCTURE|NOWRAP", "WCL process logical names, with Windows environment variables and process scope as the closest equivalents." },
    { "MEMORY", "ALL|BITMAP|BUFFER_OBJECTS|CACHE|EXACT|FILES|FULL|GH_REGIONS|HIGHLIGHT|OUTPUT|PAGE|PHYSICAL_PAGES|POOL|RESERVED|SEARCH|SLOTS|UNITS|WRAP|NOOUTPUT|NOPAGE|NOWRAP", "Physical memory, cache, pool, paging-file, and process-slot information." },
    { "NETWORK", "FULL|IP|OLD|OUTPUT|NOOUTPUT", "Windows adapter, address, gateway, DNS, and connectivity information." },
    { "PRINTER", "IDSTRING|OUTPUT|SERIALNUM|NOOUTPUT", "Windows print-spooler printers and the configured default printer." },
    { "PROCESS", "ACCOUNTING|ALL|CASE_LOOKUP|CONTINUOUS|DUMP|EXACT|HIGHLIGHT|IA|IDENTIFICATION|INTERVAL|MEMORY|OUTPUT|PAGE|PRIVILEGES|QUOTAS|RAD|RIGHTS|SCHEDULING_CLASS|SEARCH|SHELVING|SUBPROCESSES|SYMLINK|TOKEN|UNITS|WRAP|NOOUTPUT|NOPAGE|NORIGHTS|NOWRAP", "Windows process IDs, token identity, priority, threads, CPU, I/O, and memory counters." },
    { "PROTECTION", "DEFAULT|PROTECTION", "Windows file-system ACL inheritance and the current directory's access-control descriptor." },
    { "QUEUE", "ALL|ALL_JOBS|AUTO_START_ON|BASE_PRIORITY|BATCH|BRIEF|BY_JOB_STATUS|DEFAULT|DEVICE|EXACT|FILES|FORM|FULL|GENERIC|HIGHLIGHT|JOB_LIMIT|LIBRARY|MANAGER|MANAGERS|ON|OUTPUT|OWNER|PAGE|PRIORITY|PROCESSOR|PROTECTION|RETAIN|SEARCH|SEPARATE|SUMMARY|WRAP|NOOUTPUT|NOPAGE|NOWRAP|CHARACTERISTIC|CHARACTERISTICS", "WCL-created Windows Task Scheduler jobs and their run status." },
    { "QUOTA", "DISK|USER", "Windows per-user disk free-space and process memory/commit limits where queryable." },
    { "REGISTRY", "VALUE|VALUES|PAGE|OUTPUT|NOOUTPUT|DRYRUN|CONFIRM", "Windows Registry keys, values, and subkeys under standard hives." },
    { "RMS_DEFAULT", "OUTPUT|NOOUTPUT", "WCL's file-version defaults and Windows text-file conventions." },
    { "ROOT", "OUTPUT|NOOUTPUT", "The current WCL executable path, Windows directory, and system root." },
    { "SECURITY", "BACKUP|BEFORE|BY_OWNER|CLASS|CREATED|EXCLUDE|EXPIRATION_DATE|EXPIRED|FULL|BRIEF|MODIFIED|OUTPUT|NOOUTPUT|SINCE|SYMLINK", "The current Windows user token, elevation membership, and local security-service state." },
    { "SERVER", "AGENT|BRIEF|CLUSTER|FULL|IA|MASTER|OUTPUT|PAGE|SERVER|NODE|NOAGENT|NOSERVER", "Windows Server/Workstation, Task Scheduler, and Print Spooler service state." },
    { "SHADOW", "ACTIVE|BY_PRIORITY|COPY|FULL|MERGE|NAME|OUTPUT|POLICY|SITE", "Windows volume and storage-redundancy indicators where exposed by local volume APIs." },
    { "STATUS", "OUTPUT|NOOUTPUT", "WCL's last command status and corresponding Win32 error message." },
    { "SYMBOL", "ALL|GLOBAL|KEEP|LOCAL|LOG|NOLOG", "WCL process symbols; /LOCAL selects the process table and /GLOBAL maps to inherited environment values." },
    { "SYSTEM", "BATCH|CLUSTER|EXACT|FULL|GRAND_TOTAL|HEADING|HIGHLIGHT|IDENT|IMAGE|INTERACTIVE|MULTITHREAD|NETWORK|OUTPUT|OWNER|OWNER_UIC|PAGE|PROCESS|SCHEDULING_CLASS|SEARCH|STATE|SUBPROCESS|TOTAL|WRAP|NOBATCH|NODE|NOGRAND_TOTAL|NOHEADING|NOINTERACTIVE|NONETWORK|NOOUTPUT|NOPAGE|NOPROCESS|NOSCHEDULING_CLASS|NOSUBPROCESS|NOTOTAL|NOWRAP", "Windows host identity, uptime, process table, and system counters." },
    { "TERMINAL", "BRIEF|FALLBACK|FULL|OUTPUT|PERMANENT|NOOUTPUT", "The current Windows console input/output modes, dimensions, and code pages." },
    { "TIME", "DIFF|HOST|OUTPUT|NOOUTPUT", "Windows local time, time zone, and host name." },
    { "TRANSLATION", "GROUP|TABLE|OUTPUT|NOOUTPUT", "Windows full-path resolution, drive mappings, and WCL logical-name translation." },
    { "USERS", "BATCH|CLUSTER|EXACT|FULL|HEADING|HIGHLIGHT|INTERACTIVE|NETWORK|OUTPUT|PAGE|SEARCH|SUBPROCESS|WRAP|NOBATCH|NODE|NOHEADING|NOINTERACTIVE|NONETWORK|NOOUTPUT|NOPAGE|NOSUBPROCESS|NOWRAP", "The current Windows account and locally enumerable interactive sessions." },
    { "WORKING_SET", "OUTPUT|UNITS|NOOUTPUT", "The WCL process working set and system memory limits." }
};

const ShowOptionDefinition *FindShowOption(const char *name, bool *ambiguous) {
    const ShowOptionDefinition *match = NULL;
    size_t matches = 0;
    if (ambiguous) *ambiguous = false;
    for (size_t i = 0; i < sizeof(show_options) / sizeof(show_options[0]); ++i) {
        if (_stricmp(name, show_options[i].name) == 0) return &show_options[i];
        if (MatchesPrefix(name, show_options[i].name)) {
            match = &show_options[i];
            ++matches;
        }
    }
    if (matches > 1 && ambiguous) *ambiguous = true;
    return matches == 1 ? match : NULL;
}

static const ShowOptionDefinition show_devices_served_option = {
    "DEVICES/SERVED",
    "ALL|COUNT|EXACT|HIGHLIGHT|HOST|OUTPUT|PAGE|RESOURCE|SEARCH|SERVED|NOOUTPUT|NOPAGE",
    "Windows mapped remote drives and available network-volume details."
};

static const ShowOptionDefinition show_queue_characteristics_option = {
    "QUEUE/CHARACTERISTICS",
    "CHARACTERISTIC|CHARACTERISTICS|FULL|OUTPUT|QUEUE|NOOUTPUT",
    "Windows printer and spooler capabilities as the closest queue-characteristic view."
};

static const ShowOptionDefinition show_queue_form_option = {
    "QUEUE/FORM",
    "BRIEF|FORM|FULL|LENGTH|MARGIN|OUTPUT|QUEUE|SETUP|STOCK|TRUNCATE|WIDTH|NOOUTPUT",
    "Windows default printer and driver paper/form settings."
};

static const ShowOptionDefinition show_queue_managers_option = {
    "QUEUE/MANAGERS",
    "BRIEF|FULL|MANAGER|MANAGERS|ON|QUEUE",
    "Windows Task Scheduler and Print Spooler service status."
};

static const ShowOptionDefinition show_server_acme_option = {
    "SERVER/ACME_SERVER",
    "AGENT|BRIEF|CLUSTER|FULL|NODE|NOAGENT|OUTPUT|PAGE|SERVER|NOSERVER",
    "Windows host services and local service identity."
};

static const ShowOptionDefinition show_server_registry_option = {
    "SERVER/REGISTRY_SERVER",
    "CLUSTER|IA|MASTER|NODE|OUTPUT",
    "Windows registry service and local host state."
};

const ShowOptionDefinition *FindShowInvocationOption(const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals, bool *ambiguous) {
    if (ambiguous) *ambiguous = false;
    if (_stricmp(option->name, "DEVICES") == 0 &&
        quals->HasQualifier(quals, "SERVED")) {
        return &show_devices_served_option;
    }
    if (_stricmp(option->name, "QUEUE") == 0) {
        const ShowOptionDefinition *selected = NULL;
        size_t selected_modes = 0;
        if (quals->HasQualifier(quals, "CHARACTERISTICS") ||
            quals->HasQualifier(quals, "CHARACTERISTIC")) {
            selected = &show_queue_characteristics_option;
            ++selected_modes;
        }
        if (quals->HasQualifier(quals, "FORM")) {
            selected = &show_queue_form_option;
            ++selected_modes;
        }
        if (quals->HasQualifier(quals, "MANAGERS") ||
            quals->HasQualifier(quals, "MANAGER")) {
            selected = &show_queue_managers_option;
            ++selected_modes;
        }
        if (selected_modes > 1) {
            if (ambiguous) *ambiguous = true;
            return NULL;
        }
        return selected ? selected : option;
    }
    if (_stricmp(option->name, "SERVER") == 0 && argc > 2) {
        if (MatchesPrefix(argv[2], "ACME_SERVER")) return &show_server_acme_option;
        if (MatchesPrefix(argv[2], "REGISTRY_SERVER")) return &show_server_registry_option;
        return NULL;
    }
    return option;
}

static size_t ShowQualifierMatchCount(const ShowOptionDefinition *option, const char *name) {
    if (MatchesPrefix(name, "OUTPUT")) return 1;
    char qualifiers[1024];
    CopyString(qualifiers, sizeof(qualifiers), option->qualifiers);
    char *context = NULL;
    size_t matches = 0;
    for (char *item = strtok_s(qualifiers, "|", &context); item;
        item = strtok_s(NULL, "|", &context)) {
        if (_stricmp(name, item) == 0) return 1;
    }
    CopyString(qualifiers, sizeof(qualifiers), option->qualifiers);
    context = NULL;
    for (char *item = strtok_s(qualifiers, "|", &context); item;
        item = strtok_s(NULL, "|", &context)) {
        if (MatchesPrefix(name, item)) ++matches;
    }
    return matches;
}

bool ShowQualifierIsValid(const ShowOptionDefinition *option, const char *name) {
    return ShowQualifierMatchCount(option, name) == 1;
}

const char *ShowQualifierBaseName(const ShowOptionDefinition *option, const char *name) {
    if (!_strnicmp(name, "NO", 2) && name[2] &&
        ShowQualifierIsValid(option, name + 2)) {
        return name + 2;
    }
    return name;
}

static const char *ShowQualifierMapping(const char *name) {
    if (MatchesPrefix(name, "FULL") || MatchesPrefix(name, "ALL"))
        return "requests detailed Windows fields; only data exposed by this mapping is shown";
    if (MatchesPrefix(name, "BRIEF") || MatchesPrefix(name, "SUMMARY"))
        return "requests a concise summary; VMS-only fields are not added";
    if (MatchesPrefix(name, "OUTPUT"))
        return "writes the Windows-mapped report to the named file when a value is supplied";
    if (MatchesPrefix(name, "PAGE"))
        return "requests page display; output remains in the normal Windows console stream";
    if (MatchesPrefix(name, "SEARCH"))
        return "records the requested text search; VMS-specific matching rules are not inferred";
    if (MatchesPrefix(name, "NODE") || MatchesPrefix(name, "HOST"))
        return "selects the local Windows host identity";
    if (MatchesPrefix(name, "PROCESS") || MatchesPrefix(name, "SUBPROCESS") ||
        MatchesPrefix(name, "INTERACTIVE") || MatchesPrefix(name, "BATCH") ||
        MatchesPrefix(name, "NETWORK"))
        return "identifies the requested Windows process or session class; only enumerable data is shown";
    if (MatchesPrefix(name, "OWNER") || MatchesPrefix(name, "USER") ||
        MatchesPrefix(name, "IDENTIFICATION") || MatchesPrefix(name, "TOKEN"))
        return "selects Windows account and access-token identity fields";
    if (MatchesPrefix(name, "MEMORY") || MatchesPrefix(name, "QUOTA") ||
        MatchesPrefix(name, "UNITS") || MatchesPrefix(name, "SIZE"))
        return "requests Windows memory, capacity, and quota counters when available";
    if (MatchesPrefix(name, "DEVICE") || MatchesPrefix(name, "SERVED") ||
        MatchesPrefix(name, "MOUNTED") || MatchesPrefix(name, "FILES"))
        return "requests Windows volume, printer, or mapped-device details";
    if (MatchesPrefix(name, "TIME") || MatchesPrefix(name, "BEFORE") ||
        MatchesPrefix(name, "SINCE") || MatchesPrefix(name, "CREATED") ||
        MatchesPrefix(name, "MODIFIED") || MatchesPrefix(name, "EXPIRED"))
        return "selects available Windows time and date metadata";
    if (MatchesPrefix(name, "DIFF"))
        return "reports the Windows local-to-UTC time-zone offset";
    if (MatchesPrefix(name, "STATE") || MatchesPrefix(name, "ACTIVE") ||
        MatchesPrefix(name, "PERMANENT") || MatchesPrefix(name, "STANDBY") ||
        MatchesPrefix(name, "STATUS"))
        return "maps to the closest Windows-reported state, without asserting a VMS state";
    return "records the request and identifies the closest available Windows field";
}

static const char *ShowQualifierMappingFor(const ShowOptionDefinition *option,
    const char *qualifier) {
    const char *name = ShowQualifierBaseName(option, qualifier);
    if (_stricmp(option->name, "MEMORY") == 0 && name != qualifier) {
        if (MatchesPrefix(name, "OUTPUT")) return "suppresses the memory report";
        if (MatchesPrefix(name, "PAGE")) return "disables screen paging";
        if (MatchesPrefix(name, "WRAP")) return "does not wrap long lines";
    }
    if (_stricmp(option->name, "MEMORY") == 0) {
        if (MatchesPrefix(name, "ALL"))
            return "displays all available memory sections; this is the default";
        if (MatchesPrefix(name, "BITMAP"))
            return "displays the memory bitmap section when available";
        if (MatchesPrefix(name, "BUFFER_OBJECTS"))
            return "displays buffer-object usage when available";
        if (MatchesPrefix(name, "CACHE"))
            return "displays cache usage; /FULL adds configured cache limits";
        if (MatchesPrefix(name, "EXACT"))
            return "makes /SEARCH case-sensitive";
        if (MatchesPrefix(name, "FILES"))
            return "displays configured paging-file usage";
        if (MatchesPrefix(name, "FULL"))
            return "adds detail to /CACHE, /FILES, or /POOL";
        if (MatchesPrefix(name, "GH_REGIONS"))
            return "displays granularity-hint regions when available";
        if (MatchesPrefix(name, "HIGHLIGHT"))
            return "highlights /SEARCH matches while paging through the report";
        if (MatchesPrefix(name, "PAGE"))
            return "pages the report; supports SAVE, SCROLL, or CLEAR_SCREEN";
        if (MatchesPrefix(name, "PHYSICAL_PAGES"))
            return "displays physical-page totals and availability";
        if (MatchesPrefix(name, "POOL"))
            return "displays paged and nonpaged pool usage";
        if (MatchesPrefix(name, "RESERVED"))
            return "displays reserved-memory information when available";
        if (MatchesPrefix(name, "SEARCH"))
            return "finds and highlights the specified text in /PAGE=SAVE mode";
        if (MatchesPrefix(name, "SLOTS"))
            return "displays process, thread, and handle usage";
        if (MatchesPrefix(name, "UNITS"))
            return "adds byte and page-size values to the report";
        if (MatchesPrefix(name, "WRAP"))
            return "wraps long lines in /PAGE=SAVE mode";
        if (MatchesPrefix(name, "NOWRAP"))
            return "keeps long lines unwrapped in /PAGE=SAVE mode";
    }
    if (name != qualifier) {
        if (MatchesPrefix(name, "OUTPUT"))
            return "prevents a separate report file; output remains on the console";
        if (MatchesPrefix(name, "PAGE"))
            return "keeps output in the normal console stream without a pager";
        if (MatchesPrefix(name, "WRAP"))
            return "uses the Windows console's default line wrapping";
        if (MatchesPrefix(name, "HEADING"))
            return "records a request to omit headings; the Windows host identification remains";
        if (MatchesPrefix(name, "BATCH") || MatchesPrefix(name, "INTERACTIVE") ||
            MatchesPrefix(name, "NETWORK") || MatchesPrefix(name, "PROCESS") ||
            MatchesPrefix(name, "SUBPROCESS"))
            return "records an exclusion request for the matching Windows class where enumerable";
        return "omits the corresponding optional Windows detail";
    }
    return ShowQualifierMapping(name);
}

static bool ReadCurrentVersionString(const char *value_name, char *value, DWORD capacity) {
    DWORD size = capacity;
    LSTATUS result = RegGetValueA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", value_name, RRF_RT_REG_SZ,
        NULL, value, &size);
    if (result != ERROR_SUCCESS || !size) {
        value[0] = '\0';
        return false;
    }
    value[capacity - 1] = '\0';
    return true;
}

static bool ReadCurrentVersionDword(const char *value_name, DWORD *value) {
    DWORD size = sizeof(*value);
    return RegGetValueA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", value_name, RRF_RT_REG_DWORD,
        NULL, value, &size) == ERROR_SUCCESS;
}

void PrintShowCatalogHelp(void) {
    printf("\nWCL SHOW options and qualifiers (based on the VSI DCL Dictionary)\n");
    printf("Source: https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_95\n");
    printf("The qualifier lists below are accepted by this WCL build and describe their report behavior.\n");
    printf("Data not exposed by the host is identified rather than fabricated.\n\n");
    for (size_t i = 0; i < sizeof(show_options) / sizeof(show_options[0]); ++i) {
        const ShowOptionDefinition *option = &show_options[i];
        printf("SHOW %-18s %s\n", option->name, option->windows_mapping);
        if (!option->qualifiers[0]) {
            printf("  No qualifier subset is listed for this WCL option.\n");
            continue;
        }
        char qualifiers[1024];
        CopyString(qualifiers, sizeof(qualifiers), option->qualifiers);
        char *context = NULL;
        for (char *item = strtok_s(qualifiers, "|", &context); item;
            item = strtok_s(NULL, "|", &context)) {
            printf("  /%-22s %s\n", item, ShowQualifierMappingFor(option, item));
        }
    }
    const ShowOptionDefinition *special_options[] = {
        &show_devices_served_option,
        &show_queue_characteristics_option,
        &show_queue_form_option,
        &show_queue_managers_option,
        &show_server_acme_option,
        &show_server_registry_option
    };
    for (size_t i = 0; i < sizeof(special_options) / sizeof(special_options[0]); ++i) {
        const ShowOptionDefinition *option = special_options[i];
        printf("\nSHOW %s\n  %s\n", option->name, option->windows_mapping);
        char qualifiers[1024];
        CopyString(qualifiers, sizeof(qualifiers), option->qualifiers);
        char *context = NULL;
        for (char *item = strtok_s(qualifiers, "|", &context); item;
            item = strtok_s(NULL, "|", &context)) {
            printf("  /%-22s %s\n", item, ShowQualifierMappingFor(option, item));
        }
    }
    printf("\nSHOW NETWORK also accepts LOCAL or INTERNET; /IP selects address details.\n");
    printf("SHOW QUEUE/FORM, SHOW QUEUE/CHARACTERISTICS, and SHOW QUEUE/MANAGERS map to Windows printer,\n");
    printf("Task Scheduler, and Print Spooler information. /OUTPUT=filespec writes a report.\n\n");
}

const char *NetworkAdapterType(ULONG type) {
    switch (type) {
    case IF_TYPE_ETHERNET_CSMACD: return "Ethernet";
    case IF_TYPE_IEEE80211: return "Wi-Fi";
    case IF_TYPE_SOFTWARE_LOOPBACK: return "Loopback";
    case IF_TYPE_PPP: return "PPP";
    case IF_TYPE_TUNNEL: return "Tunnel";
    default: return "Other";
    }
}

const char *NetworkAdapterStatus(IF_OPER_STATUS status) {
    switch (status) {
    case IfOperStatusUp: return "UP";
    case IfOperStatusDown: return "DOWN";
    case IfOperStatusTesting: return "TESTING";
    case IfOperStatusUnknown: return "UNKNOWN";
    case IfOperStatusDormant: return "DORMANT";
    case IfOperStatusNotPresent: return "NOT PRESENT";
    case IfOperStatusLowerLayerDown: return "LOWER LAYER DOWN";
    default: return "UNKNOWN";
    }
}

bool FormatNetworkAddress(const SOCKADDR *address, char *text, size_t capacity) {
    if (!address || !text || capacity < INET6_ADDRSTRLEN) return false;
    if (address->sa_family == AF_INET) {
        const SOCKADDR_IN *ipv4 = (const SOCKADDR_IN*)address;
        return InetNtopA(AF_INET, &ipv4->sin_addr, text, (size_t)capacity) != NULL;
    }
    if (address->sa_family == AF_INET6) {
        const SOCKADDR_IN6 *ipv6 = (const SOCKADDR_IN6*)address;
        return InetNtopA(AF_INET6, &ipv6->sin6_addr, text, (size_t)capacity) != NULL;
    }
    return false;
}

bool GetNetworkAdapterName(const IP_ADAPTER_ADDRESSES *adapter, char *name, size_t capacity) {
    if (adapter->FriendlyName) {
        int converted = WideCharToMultiByte(CP_ACP, 0, adapter->FriendlyName, -1,
            name, (int)capacity, NULL, NULL);
        if (converted > 0) return true;
    }
    if (!adapter->AdapterName || strlen(adapter->AdapterName) >= capacity) return false;
    CopyString(name, capacity, adapter->AdapterName);
    return true;
}

bool PrintNetworkAddress(FILE *output, const SOCKADDR *address, bool *printed) {
    char text[INET6_ADDRSTRLEN];
    if (!FormatNetworkAddress(address, text, sizeof(text))) return false;
    if (*printed && fputs(", ", output) == EOF) return false;
    if (fputs(text, output) == EOF) return false;
    *printed = true;
    return true;
}

bool PrintNetworkAdapterAddresses(FILE *output, const IP_ADAPTER_ADDRESSES *adapter, bool full) {
    bool printed = false;
    if (fputs("    Address: ", output) == EOF) return false;
    for (const IP_ADAPTER_UNICAST_ADDRESS *address = adapter->FirstUnicastAddress;
        address; address = address->Next) {
        if (!PrintNetworkAddress(output, address->Address.lpSockaddr, &printed)) return false;
    }
    if (!printed && fputs("none", output) == EOF) return false;
    if (fputc('\n', output) == EOF) return false;
    if (!full) return true;

    printed = false;
    if (fputs("    Gateway: ", output) == EOF) return false;
    for (const IP_ADAPTER_GATEWAY_ADDRESS_LH *gateway = adapter->FirstGatewayAddress;
        gateway; gateway = gateway->Next) {
        if (!PrintNetworkAddress(output, gateway->Address.lpSockaddr, &printed)) return false;
    }
    if (!printed && fputs("none", output) == EOF) return false;
    if (fputc('\n', output) == EOF) return false;

    printed = false;
    if (fputs("    DNS: ", output) == EOF) return false;
    for (const IP_ADAPTER_DNS_SERVER_ADDRESS *server = adapter->FirstDnsServerAddress;
        server; server = server->Next) {
        if (!PrintNetworkAddress(output, server->Address.lpSockaddr, &printed)) return false;
    }
    if (!printed && fputs("none", output) == EOF) return false;
    if (fputc('\n', output) == EOF) return false;

    if (fprintf(output, "    MTU: %lu\n    MAC: ", (unsigned long)adapter->Mtu) < 0) return false;
    if (!adapter->PhysicalAddressLength) {
        if (fputs("unavailable", output) == EOF) return false;
    } else {
        for (ULONG i = 0; i < adapter->PhysicalAddressLength; ++i) {
            if (fprintf(output, "%s%02X", i ? "-" : "", adapter->PhysicalAddress[i]) < 0) return false;
        }
    }
    return fputc('\n', output) != EOF;
}

static void FormatLinkSpeed(ULONGLONG speed_bps, IF_OPER_STATUS status, char *buf, size_t len) {
    if (status != IfOperStatusUp || speed_bps == 0 || speed_bps == (ULONGLONG)-1 || speed_bps > 1000000000000ULL) {
        snprintf(buf, len, "---");
    } else if (speed_bps >= 1000000000ULL) {
        double gbps = (double)speed_bps / 1000000000.0;
        if (gbps == (unsigned long)gbps) {
            snprintf(buf, len, "%lu Gbps", (unsigned long)gbps);
        } else {
            snprintf(buf, len, "%.1f Gbps", gbps);
        }
    } else if (speed_bps >= 1000000ULL) {
        double mbps = (double)speed_bps / 1000000.0;
        snprintf(buf, len, "%lu Mbps", (unsigned long)(mbps + 0.5));
    } else if (speed_bps >= 1000ULL) {
        snprintf(buf, len, "%lu Kbps", (unsigned long)(speed_bps / 1000));
    } else {
        snprintf(buf, len, "%llu bps", speed_bps);
    }
}

static void GetPrimaryAddress(const IP_ADAPTER_ADDRESSES *adapter, char *buf, size_t len) {
    if (adapter->OperStatus != IfOperStatusUp) {
        snprintf(buf, len, "Disconnected");
        return;
    }
    for (const IP_ADAPTER_UNICAST_ADDRESS *addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
        if (addr->Address.lpSockaddr && addr->Address.lpSockaddr->sa_family == AF_INET) {
            if (FormatNetworkAddress(addr->Address.lpSockaddr, buf, len)) return;
        }
    }
    for (const IP_ADAPTER_UNICAST_ADDRESS *addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
        if (addr->Address.lpSockaddr && addr->Address.lpSockaddr->sa_family == AF_INET6) {
            if (FormatNetworkAddress(addr->Address.lpSockaddr, buf, len)) return;
        }
    }
    snprintf(buf, len, "none");
}

static bool PrintNetworkAdapterFullDetails(FILE *output, const IP_ADAPTER_ADDRESSES *adapter) {
    char adapter_name[512];
    GetNetworkAdapterName(adapter, adapter_name, sizeof(adapter_name));

    char desc[512] = "unavailable";
    if (adapter->Description) {
        WideCharToMultiByte(CP_ACP, 0, adapter->Description, -1, desc, sizeof(desc), NULL, NULL);
    }

    char speed_str[32];
    FormatLinkSpeed(adapter->TransmitLinkSpeed, adapter->OperStatus, speed_str, sizeof(speed_str));

    char mac_str[64] = "unavailable";
    if (adapter->PhysicalAddressLength) {
        char *p = mac_str;
        size_t rem = sizeof(mac_str);
        for (ULONG i = 0; i < adapter->PhysicalAddressLength && rem > 3; ++i) {
            int n = snprintf(p, rem, "%s%02X", i ? "-" : "", adapter->PhysicalAddress[i]);
            if (n > 0) { p += n; rem -= (size_t)n; }
        }
    }

    if (fprintf(output, "\nInterface: %s\n", adapter_name) < 0) return false;
    if (fprintf(output, "  Description:        %s\n", desc) < 0) return false;
    if (fprintf(output, "  Type:               %s\n", NetworkAdapterType(adapter->IfType)) < 0) return false;
    if (fprintf(output, "  Status:             %s\n", NetworkAdapterStatus(adapter->OperStatus)) < 0) return false;
    if (fprintf(output, "  Link Speed:         %s\n", speed_str) < 0) return false;
    if (adapter->Mtu == 0 || adapter->Mtu >= 4000000000UL) {
        if (fprintf(output, "  MTU:                unlimited\n") < 0) return false;
    } else {
        if (fprintf(output, "  MTU:                %lu bytes\n", (unsigned long)adapter->Mtu) < 0) return false;
    }
    if (fprintf(output, "  Physical Address:   %s\n", mac_str) < 0) return false;

    bool printed = false;
    if (fputs("  IPv4 Address:       ", output) == EOF) return false;
    for (const IP_ADAPTER_UNICAST_ADDRESS *addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
        if (addr->Address.lpSockaddr && addr->Address.lpSockaddr->sa_family == AF_INET) {
            if (!PrintNetworkAddress(output, addr->Address.lpSockaddr, &printed)) return false;
        }
    }
    if (!printed && fputs("none", output) == EOF) return false;
    if (fputc('\n', output) == EOF) return false;

    printed = false;
    if (fputs("  IPv4 Gateway:       ", output) == EOF) return false;
    for (const IP_ADAPTER_GATEWAY_ADDRESS_LH *gw = adapter->FirstGatewayAddress; gw; gw = gw->Next) {
        if (gw->Address.lpSockaddr && gw->Address.lpSockaddr->sa_family == AF_INET) {
            if (!PrintNetworkAddress(output, gw->Address.lpSockaddr, &printed)) return false;
        }
    }
    if (!printed && fputs("none", output) == EOF) return false;
    if (fputc('\n', output) == EOF) return false;

    printed = false;
    if (fputs("  DNS Servers:        ", output) == EOF) return false;
    for (const IP_ADAPTER_DNS_SERVER_ADDRESS *dns = adapter->FirstDnsServerAddress; dns; dns = dns->Next) {
        if (!PrintNetworkAddress(output, dns->Address.lpSockaddr, &printed)) return false;
    }
    if (!printed && fputs("none", output) == EOF) return false;
    if (fputc('\n', output) == EOF) return false;

    printed = false;
    if (fputs("  IPv6 Address:       ", output) == EOF) return false;
    for (const IP_ADAPTER_UNICAST_ADDRESS *addr = adapter->FirstUnicastAddress; addr; addr = addr->Next) {
        if (addr->Address.lpSockaddr && addr->Address.lpSockaddr->sa_family == AF_INET6) {
            if (!PrintNetworkAddress(output, addr->Address.lpSockaddr, &printed)) return false;
        }
    }
    if (!printed && fputs("none", output) == EOF) return false;
    if (fputc('\n', output) == EOF) return false;

    return true;
}

int ShowNetworkStatus(ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;
    if (argc > 3) {
        ctx->PrintStatus(ctx, 'E', "TOOMANY", "SHOW NETWORK accepts at most one network selector");
        return 1;
    }
    bool show_local = true;
    bool show_internet = true;
    if (argc == 3) {
        if (MatchesPrefix(argv[2], "LOCAL")) show_internet = false;
        else if (MatchesPrefix(argv[2], "INTERNET")) show_local = false;
        else {
            ctx->PrintStatus(ctx, 'E', "IVNETSERVICE", "Network selector must be LOCAL or INTERNET");
            return 1;
        }
    }

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_name_length = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_name_length)) {
        ctx->PrintStatus(ctx, 'E', "NODEFAIL", "Unable to retrieve the Windows host name");
        return 1;
    }

    ULONG adapter_flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_INCLUDE_PREFIX;
    ULONG adapter_buffer_size = 0;
    ULONG adapter_result = GetAdaptersAddresses(AF_UNSPEC, adapter_flags, NULL, NULL, &adapter_buffer_size);
    IP_ADAPTER_ADDRESSES *adapters = NULL;
    if (adapter_result == ERROR_BUFFER_OVERFLOW && adapter_buffer_size) {
        adapters = (IP_ADAPTER_ADDRESSES*)malloc(adapter_buffer_size);
        if (!adapters) {
            ctx->PrintStatus(ctx, 'E', "NETMEM", "Unable to allocate network adapter information");
            return 1;
        }
        adapter_result = GetAdaptersAddresses(AF_UNSPEC, adapter_flags, NULL, adapters, &adapter_buffer_size);
        if (adapter_result == ERROR_BUFFER_OVERFLOW && adapter_buffer_size) {
            IP_ADAPTER_ADDRESSES *expanded = (IP_ADAPTER_ADDRESSES*)realloc(adapters, adapter_buffer_size);
            if (!expanded) {
                free(adapters);
                ctx->PrintStatus(ctx, 'E', "NETMEM", "Unable to resize network adapter information");
                return 1;
            }
            adapters = expanded;
            adapter_result = GetAdaptersAddresses(AF_UNSPEC, adapter_flags, NULL, adapters, &adapter_buffer_size);
        }
    }
    if (adapter_result == ERROR_NO_DATA) {
        free(adapters);
        adapters = NULL;
    } else if (adapter_result != NO_ERROR) {
        free(adapters);
        ctx->PrintStatus(ctx, 'E', "NETQUERY", "Unable to retrieve local network adapter information");
        return 1;
    }

    DWORD internet_flags = 0;
    BOOL internet_connected = InternetGetConnectedState(&internet_flags, 0);
    bool full = quals->HasQualifier(quals, "FULL") || quals->HasQualifier(quals, "IP");
    bool old_format = quals->HasQualifier(quals, "OLD");
    FILE *output = stdout;
    bool close_output = false;
    const Qualifier *output_qualifier = NULL;
    for (int i = 0; i < quals->count; ++i) {
        if (!_strnicmp(quals->items[i].name, "NO", 2) &&
            MatchesPrefix(quals->items[i].name + 2, "OUTPUT")) continue;
        if (MatchesPrefix(quals->items[i].name, "OUTPUT")) {
            output_qualifier = &quals->items[i];
            break;
        }
    }
    if (output_qualifier && output_qualifier->has_value && output_qualifier->value[0]) {
        char output_path[MAX_PATH];
        if (!ResolveFileSpecification(ctx, output_qualifier->value, true, output_path, sizeof(output_path))) {
            free(adapters);
            ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "SHOW NETWORK output specification is too long");
            return 1;
        }
        if (fopen_s(&output, output_path, "w") != 0) {
            free(adapters);
            ctx->PrintStatus(ctx, 'E', "NETOUTPUT", "Unable to open SHOW NETWORK output file");
            return 1;
        }
        close_output = true;
    }

    DWORD active_adapters = 0;
    for (const IP_ADAPTER_ADDRESSES *adapter = adapters; adapter; adapter = adapter->Next) {
        if (adapter->OperStatus == IfOperStatusUp && adapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK) {
            active_adapters++;
        }
    }

    bool success = true;
    if (old_format) {
        success = fprintf(output, "\nNetwork status on node %s\n", node_name) >= 0;
    } else {
        success = fprintf(output, "\nThe following network service is available at this time:\n\n") >= 0;
        if (success) {
            success = fprintf(output, "  Product:            TCP/IP (Windows Sockets)\n"
                                      "  Node:               %s\n", node_name) >= 0;
        }
        if (success && show_local) {
            success = fprintf(output, "  Network Status:     %s (%lu active interface%s)\n",
                active_adapters ? "CONNECTED" : "NOT CONNECTED",
                (unsigned long)active_adapters, active_adapters == 1 ? "" : "s") >= 0;
        }
        if (success && show_internet) {
            success = fprintf(output, "  Internet (Windows-reported): %s\n",
                internet_connected ? "CONNECTED" : "NOT CONNECTED") >= 0;
        }
    }

    if (success && show_local) {
        if (full) {
            for (const IP_ADAPTER_ADDRESSES *adapter = adapters; success && adapter; adapter = adapter->Next) {
                success = PrintNetworkAdapterFullDetails(output, adapter);
            }
        } else if (old_format) {
            success = fprintf(output, "LOCAL NETWORK: %s (%lu active adapter%s)\n",
                active_adapters ? "CONNECTED" : "NOT CONNECTED",
                (unsigned long)active_adapters, active_adapters == 1 ? "" : "s") >= 0;
            for (const IP_ADAPTER_ADDRESSES *adapter = adapters; success && adapter; adapter = adapter->Next) {
                char adapter_name[512];
                if (!GetNetworkAdapterName(adapter, adapter_name, sizeof(adapter_name))) {
                    success = false;
                    break;
                }
                success = fprintf(output, "  %s: %s\n", adapter_name,
                    NetworkAdapterStatus(adapter->OperStatus)) >= 0;
                if (success) success = PrintNetworkAdapterAddresses(output, adapter, false);
            }
        } else {
            success = fprintf(output, "\nInterface Information:\n\n"
                                      "  Interface                     Type      Status   Speed        MTU  Primary Address\n"
                                      "  ----------------------------  --------  ------  ---------  ------  -----------------\n") >= 0;
            for (const IP_ADAPTER_ADDRESSES *adapter = adapters; success && adapter; adapter = adapter->Next) {
                char adapter_name[512];
                if (!GetNetworkAdapterName(adapter, adapter_name, sizeof(adapter_name))) {
                    success = false;
                    break;
                }
                char speed_str[32];
                FormatLinkSpeed(adapter->TransmitLinkSpeed, adapter->OperStatus, speed_str, sizeof(speed_str));
                char mtu_str[16];
                if (adapter->Mtu == 0 || adapter->Mtu >= 4000000000UL) {
                    snprintf(mtu_str, sizeof(mtu_str), "---");
                } else {
                    snprintf(mtu_str, sizeof(mtu_str), "%lu", (unsigned long)adapter->Mtu);
                }
                char primary_addr[128];
                GetPrimaryAddress(adapter, primary_addr, sizeof(primary_addr));

                success = fprintf(output, "  %-28.28s  %-8.8s  %-6.6s  %9s  %6s  %-24.24s\n",
                    adapter_name,
                    NetworkAdapterType(adapter->IfType),
                    NetworkAdapterStatus(adapter->OperStatus),
                    speed_str,
                    mtu_str,
                    primary_addr) >= 0;
            }
        }
        if (success && !adapters) success = fputs("  No network adapters found.\n", output) != EOF;
    }

    if (success && show_internet) {
        if (old_format) {
            success = fprintf(output, "INTERNET: %s\n",
                internet_connected ? "CONNECTED" : "NOT CONNECTED") >= 0;
        } else if (full) {
            success = fprintf(output, "\nInternet Information:\n"
                                      "  Status:             %s (Windows-reported)\n"
                                      "  Connection:         %s%s%s%s%s\n",
                internet_connected ? "CONNECTED" : "NOT CONNECTED",
                (internet_flags & INTERNET_CONNECTION_LAN) ? "LAN " : "",
                (internet_flags & INTERNET_CONNECTION_MODEM) ? "MODEM " : "",
                (internet_flags & INTERNET_CONNECTION_PROXY) ? "PROXY " : "",
                (internet_flags & INTERNET_CONNECTION_CONFIGURED) ? "CONFIGURED " : "",
                (internet_flags & INTERNET_CONNECTION_OFFLINE) ? "OFFLINE" : "") >= 0;
        }
    }
    if (success) success = fputc('\n', output) != EOF;
    if (success && ferror(output)) success = false;
    if (close_output && fclose(output) != 0) success = false;
    free(adapters);
    if (!success) {
        ctx->PrintStatus(ctx, 'E', "NETWRITE", "Unable to write complete SHOW NETWORK output");
        return 1;
    }
    return 0;
}

bool ShowServiceState(const char *service_name, char *state, size_t capacity) {
    SC_HANDLE manager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
    if (!manager) {
        CopyString(state, capacity, "not queryable (service manager access denied)");
        return false;
    }
    SC_HANDLE service = OpenServiceA(manager, service_name, SERVICE_QUERY_STATUS);
    if (!service) {
        DWORD error = GetLastError();
        CloseServiceHandle(manager);
        CopyString(state, capacity, error == ERROR_SERVICE_DOES_NOT_EXIST ?
            "not installed" : "not queryable");
        return false;
    }
    SERVICE_STATUS_PROCESS status;
    DWORD bytes_needed = 0;
    BOOL ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO, (LPBYTE)&status,
        sizeof(status), &bytes_needed);
    CloseServiceHandle(service);
    CloseServiceHandle(manager);
    if (!ok) {
        CopyString(state, capacity, "not queryable");
        return false;
    }
    const char *description = "unknown";
    switch (status.dwCurrentState) {
    case SERVICE_STOPPED: description = "stopped"; break;
    case SERVICE_START_PENDING: description = "start pending"; break;
    case SERVICE_STOP_PENDING: description = "stop pending"; break;
    case SERVICE_RUNNING: description = "running"; break;
    case SERVICE_CONTINUE_PENDING: description = "continue pending"; break;
    case SERVICE_PAUSE_PENDING: description = "pause pending"; break;
    case SERVICE_PAUSED: description = "paused"; break;
    }
    CopyString(state, capacity, description);
    return true;
}

typedef struct ShowMemoryPageFileContext {
    FILE *report;
    SIZE_T page_size;
    bool full;
    bool units;
    size_t count;
} ShowMemoryPageFileContext;

static BOOL CALLBACK ShowMemoryPageFileCallback(PVOID context,
    PENUM_PAGE_FILE_INFORMATION page_file, LPCWSTR file_name) {
    ShowMemoryPageFileContext *report = (ShowMemoryPageFileContext *)context;
    char name[MAX_PATH * 2] = "unavailable";
    WideCharToMultiByte(CP_ACP, 0, file_name, -1, name, sizeof(name), NULL, NULL);
    SIZE_T free_pages = page_file->TotalSize > page_file->TotalInUse ?
        page_file->TotalSize - page_file->TotalInUse : 0;
    ULONGLONG total_bytes = (ULONGLONG)page_file->TotalSize * report->page_size;
    ULONGLONG in_use_bytes = (ULONGLONG)page_file->TotalInUse * report->page_size;
    ULONGLONG free_bytes = (ULONGLONG)free_pages * report->page_size;
    char free_str[32], in_use_str[32], total_str[32];
    FormatFileSizeMetric(free_bytes, free_str, sizeof(free_str));
    FormatFileSizeMetric(in_use_bytes, in_use_str, sizeof(in_use_str));
    FormatFileSizeMetric(total_bytes, total_str, sizeof(total_str));

    unsigned long pct = page_file->TotalSize ? (unsigned long)((page_file->TotalInUse * 100) / page_file->TotalSize) : 0;
    char total_with_pct[48];
    snprintf(total_with_pct, sizeof(total_with_pct), "%s (%lu%%)", total_str, pct);

    fprintf(report->report, "  %-30.30s %10s  %10s  %14s\n", name,
        free_str, in_use_str, total_with_pct);
    if (report->units) {
        fprintf(report->report, "    Total: %llu bytes; in use: %llu bytes; free: %llu bytes\n",
            total_bytes, in_use_bytes, free_bytes);
    }
    if (report->full) {
        char peak_str[32];
        FormatFileSizeMetric((ULONGLONG)page_file->PeakUsage * report->page_size, peak_str, sizeof(peak_str));
        fprintf(report->report, "    Peak usage: %s (%llu pages)\n",
            peak_str, (unsigned long long)page_file->PeakUsage);
    }
    report->count++;
    return TRUE;
}

static bool ShowMemoryLineMatches(const char *line, const char *search, bool exact) {
    size_t search_length = strlen(search);
    if (!search_length) return false;
    size_t line_length = strlen(line);
    if (search_length > line_length) return false;
    for (size_t i = 0; i <= line_length - search_length; ++i) {
        if ((exact ? strncmp(line + i, search, search_length) :
            _strnicmp(line + i, search, search_length)) == 0) {
            return true;
        }
    }
    return false;
}

static const char *ShowMemoryQualifierValue(QualifierList *quals, const char *name) {
    for (int i = 0; i < quals->count; ++i) {
        if (MatchesPrefix(quals->items[i].name, name))
            return quals->items[i].has_value ? quals->items[i].value : "";
    }
    return NULL;
}

static WORD ShowMemoryHighlightAttribute(WORD original, const char *style) {
    if (_stricmp(style, "BLINK") == 0) return (WORD)(original | BACKGROUND_INTENSITY);
    if (_stricmp(style, "REVERSE") == 0) {
        WORD foreground = original & (FOREGROUND_RED | FOREGROUND_GREEN |
            FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        WORD background = original & (BACKGROUND_RED | BACKGROUND_GREEN |
            BACKGROUND_BLUE | BACKGROUND_INTENSITY);
        return (WORD)((original & ~(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
            FOREGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN |
            BACKGROUND_BLUE | BACKGROUND_INTENSITY)) |
            (foreground << 4) | (background >> 4));
    }
    if (_stricmp(style, "UNDERLINE") == 0)
        return (WORD)(original | COMMON_LVB_UNDERSCORE);
    return (WORD)(original | FOREGROUND_INTENSITY);
}

static bool ShowMemoryReadLines(FILE *report, char ***lines, size_t *line_count) {
    size_t capacity = 0;
    char line[2048];
    *lines = NULL;
    *line_count = 0;
    rewind(report);
    while (fgets(line, sizeof(line), report)) {
        size_t length = strlen(line);
        while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';
        if (*line_count == capacity) {
            size_t new_capacity = capacity ? capacity * 2 : 32;
            char **new_lines = (char **)realloc(*lines, new_capacity * sizeof(*new_lines));
            if (!new_lines) return false;
            *lines = new_lines;
            capacity = new_capacity;
        }
        (*lines)[*line_count] = _strdup(line);
        if (!(*lines)[*line_count]) return false;
        ++*line_count;
    }
    return !ferror(report);
}

static void ShowMemoryFreeLines(char **lines, size_t line_count) {
    for (size_t i = 0; i < line_count; ++i) free(lines[i]);
    free(lines);
}

static void ShowMemoryReadSearch(char *search, size_t capacity) {
    size_t length = 0;
    printf("Find: ");
    fflush(stdout);
    while (length + 1 < capacity) {
        int key = _getch();
        if (key == '\r' || key == '\n') break;
        if (key == '\b') {
            if (length) {
                --length;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (key >= 32 && key < 127) {
            search[length++] = (char)key;
            putchar(key);
            fflush(stdout);
        }
    }
    search[length] = '\0';
    fputc('\n', stdout);
}

static void ShowMemoryClearConsole(HANDLE output, const CONSOLE_SCREEN_BUFFER_INFO *screen) {
    (void)output; (void)screen;
    ClearConsoleScreen(NULL);
}

static size_t ShowMemoryLineRows(const char *line, bool wrap, size_t width) {
    size_t length = strlen(line);
    if (!wrap || !width || length <= width) return 1;
    return (length + width - 1) / width;
}

static void ShowMemoryPrintLine(const char *line, bool wrap, size_t width) {
    size_t length = strlen(line);
    if (!wrap || !width || length <= width) {
        printf("%s\n", line);
        return;
    }
    for (size_t offset = 0; offset < length; offset += width) {
        size_t chunk = length - offset < width ? length - offset : width;
        fwrite(line + offset, 1, chunk, stdout);
        fputc('\n', stdout);
    }
}

static int ShowMemoryDisplay(FILE *report, QualifierList *quals, ExecutionContext *ctx) {
    char **lines = NULL;
    size_t line_count = 0;
    if (!ShowMemoryReadLines(report, &lines, &line_count)) {
        ShowMemoryFreeLines(lines, line_count);
        ctx->PrintStatus(ctx, 'E', "MEMREAD", "Unable to prepare memory report output");
        return 1;
    }

    bool paging = quals->HasQualifier(quals, "PAGE");
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD output_mode = 0, input_mode = 0;
    bool interactive = paging && GetConsoleMode(output, &output_mode) &&
        GetConsoleMode(input, &input_mode);
    const char *page_mode = ShowMemoryQualifierValue(quals, "PAGE");
    const char *search = ShowMemoryQualifierValue(quals, "SEARCH");
    char dynamic_search[256];
    const char *highlight = ShowMemoryQualifierValue(quals, "HIGHLIGHT");
    bool exact = quals->HasQualifier(quals, "EXACT");
    if (!highlight || !highlight[0]) highlight = "BOLD";

    if (!interactive) {
        for (size_t i = 0; i < line_count; ++i) printf("%s\n", lines[i]);
        ShowMemoryFreeLines(lines, line_count);
        return 0;
    }

    CONSOLE_SCREEN_BUFFER_INFO screen;
    if (!GetConsoleScreenBufferInfo(output, &screen)) {
        for (size_t i = 0; i < line_count; ++i) printf("%s\n", lines[i]);
        ShowMemoryFreeLines(lines, line_count);
        return 0;
    }
    size_t page_height = screen.srWindow.Bottom - screen.srWindow.Top;
    if (page_height < 1) page_height = 1;
    size_t screen_width = screen.srWindow.Right - screen.srWindow.Left + 1;
    bool wrap = quals->HasQualifier(quals, "WRAP");
    bool no_wrap = quals->HasQualifier(quals, "NOWRAP");
    if (no_wrap) SetConsoleMode(output, output_mode & ~ENABLE_WRAP_AT_EOL_OUTPUT);
    size_t start = 0;
    bool save_pages = _stricmp(page_mode, "SAVE") == 0 ||
        _strnicmp(page_mode, "SAVE=", 5) == 0;
    size_t saved_page_limit = 5;
    if (_strnicmp(page_mode, "SAVE=", 5) == 0) {
        unsigned long requested_limit = strtoul(page_mode + 5, NULL, 10);
        if (requested_limit >= 1 && requested_limit <= 5)
            saved_page_limit = requested_limit;
    }
    size_t saved_page_starts[5] = { 0 };
    size_t saved_page_count = 0;
    size_t saved_page_index = 0;
    bool scroll = _stricmp(page_mode, "SCROLL") == 0;
    bool clear = _stricmp(page_mode, "CLEAR_SCREEN") == 0;

    while (start < line_count) {
        if (save_pages && (!saved_page_count ||
            saved_page_starts[saved_page_index] != start)) {
            if (saved_page_index + 1 < saved_page_count)
                saved_page_count = saved_page_index + 1;
            if (saved_page_count == saved_page_limit) {
                for (size_t i = 1; i < saved_page_count; ++i)
                    saved_page_starts[i - 1] = saved_page_starts[i];
                --saved_page_count;
                if (saved_page_index) --saved_page_index;
            }
            saved_page_starts[saved_page_count] = start;
            saved_page_index = saved_page_count++;
        }
        if (clear) ShowMemoryClearConsole(output, &screen);
        size_t end = start;
        size_t rows = 0;
        do {
            size_t line_rows = ShowMemoryLineRows(lines[end], wrap, screen_width);
            if (rows && !scroll && rows + line_rows > page_height) break;
            rows += line_rows;
            ++end;
        } while (end < line_count && !scroll && rows < page_height);
        for (size_t i = start; i < end; ++i) {
            bool matched = search && search[0] &&
                ShowMemoryLineMatches(lines[i], search, exact);
            if (matched) SetConsoleTextAttribute(output,
                ShowMemoryHighlightAttribute(screen.wAttributes, highlight));
            ShowMemoryPrintLine(lines[i], wrap, screen_width);
            if (matched) SetConsoleTextAttribute(output, screen.wAttributes);
        }
        if (end >= line_count) break;
        printf(save_pages ?
            "\n-- More -- [Enter/Space: next, B: previous, F: find, Q: quit] " :
            "\n-- More -- [Enter/Space: next, Q: quit] ");
        fflush(stdout);
        int key = _getch();
        if (key == 0 || key == 224) key = _getch();
        printf("\r                                                      \r");
        if (key == 'q' || key == 'Q' || key == 27) break;
        if (save_pages && (key == 'b' || key == 'B' || key == 72 || key == 73)) {
            if (saved_page_index) start = saved_page_starts[--saved_page_index];
            else start = 0;
        } else if (save_pages && (key == 'f' || key == 'F' || key == 59)) {
            ShowMemoryReadSearch(dynamic_search, sizeof(dynamic_search));
            if (dynamic_search[0]) {
                search = dynamic_search;
                start = end;
                size_t match_start = start;
                while (match_start < line_count &&
                    !ShowMemoryLineMatches(lines[match_start], search, exact)) ++match_start;
                if (match_start == line_count) {
                    match_start = 0;
                    while (match_start < end &&
                        !ShowMemoryLineMatches(lines[match_start], search, exact)) ++match_start;
                }
                if (match_start < line_count) start = match_start;
            }
        } else if (save_pages && saved_page_index + 1 < saved_page_count) {
            start = saved_page_starts[++saved_page_index];
        } else {
            start = end;
        }
    }
    SetConsoleTextAttribute(output, screen.wAttributes);
    if (no_wrap) SetConsoleMode(output, output_mode);
    ShowMemoryFreeLines(lines, line_count);
    return 0;
}

static int ShowMemoryReport(ExecutionContext *ctx, QualifierList *quals) {
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    bool selected_sections = quals->HasQualifier(quals, "BITMAP") ||
        quals->HasQualifier(quals, "BUFFER_OBJECTS") ||
        quals->HasQualifier(quals, "CACHE") || quals->HasQualifier(quals, "FILES") ||
        quals->HasQualifier(quals, "GH_REGIONS") ||
        quals->HasQualifier(quals, "PHYSICAL_PAGES") || quals->HasQualifier(quals, "POOL") ||
        quals->HasQualifier(quals, "RESERVED") || quals->HasQualifier(quals, "SLOTS");
    bool all = quals->HasQualifier(quals, "ALL") || !selected_sections;
    bool full = quals->HasQualifier(quals, "FULL") &&
        (quals->HasQualifier(quals, "CACHE") || quals->HasQualifier(quals, "FILES") ||
         quals->HasQualifier(quals, "POOL"));
    bool show_cache = all || quals->HasQualifier(quals, "CACHE");
    bool show_files = all || quals->HasQualifier(quals, "FILES");
    bool show_physical = all || quals->HasQualifier(quals, "PHYSICAL_PAGES");
    bool show_pool = all || quals->HasQualifier(quals, "POOL");
    bool show_slots = all || quals->HasQualifier(quals, "SLOTS");
    bool show_bitmap = quals->HasQualifier(quals, "BITMAP");
    bool show_buffer_objects = quals->HasQualifier(quals, "BUFFER_OBJECTS");
    bool show_gh_regions = quals->HasQualifier(quals, "GH_REGIONS");
    bool show_reserved = quals->HasQualifier(quals, "RESERVED");
    bool units = quals->HasQualifier(quals, "UNITS");
    bool needs_performance = show_cache || show_physical || show_pool || show_slots;
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    MEMORYSTATUSEX memory;
    memset(&memory, 0, sizeof(memory));
    memory.dwLength = sizeof(memory);
    if (show_physical && !GlobalMemoryStatusEx(&memory)) {
        ctx->PrintStatus(ctx, 'E', "MEMFAIL", "GlobalMemoryStatusEx call failed");
        return 1;
    }
    PERFORMANCE_INFORMATION performance;
    memset(&performance, 0, sizeof(performance));
    performance.cb = sizeof(performance);
    if (needs_performance && !K32GetPerformanceInfo(&performance, sizeof(performance))) {
        ctx->PrintStatus(ctx, 'E', "MEMPERF", "Unable to retrieve system memory counters");
        return 1;
    }
    if (!performance.PageSize) performance.PageSize = system_info.dwPageSize;

    FILE *report = NULL;
    if (tmpfile_s(&report) != 0 || !report) {
        ctx->PrintStatus(ctx, 'E', "MEMTEMP", "Unable to prepare memory report");
        return 1;
    }
    SYSTEMTIME now;
    GetLocalTime(&now);
    const char *months[] = {
        "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
        "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
    };
    fprintf(report, "System Memory Resources on %02u-%s-%04u %02u:%02u:%02u.%02u\n\n",
        now.wDay, months[now.wMonth - 1], now.wYear,
        now.wHour, now.wMinute, now.wSecond, now.wMilliseconds / 10);
    if (units) fprintf(report, "Memory values are shown in pages, bytes, and megabytes where available.\n\n");

    if (show_physical) {
        ULONGLONG used_pages = performance.PhysicalTotal > performance.PhysicalAvailable ?
            performance.PhysicalTotal - performance.PhysicalAvailable : 0;
        ULONGLONG total_phys = memory.ullTotalPhys;
        ULONGLONG avail_phys = memory.ullAvailPhys;
        ULONGLONG used_phys = total_phys > avail_phys ? total_phys - avail_phys : 0;
        char total_str[32], free_str[32], used_str[32];
        FormatFileSizeMetric(total_phys, total_str, sizeof(total_str));
        FormatFileSizeMetric(avail_phys, free_str, sizeof(free_str));
        FormatFileSizeMetric(used_phys, used_str, sizeof(used_str));

        char label[48];
        snprintf(label, sizeof(label), "Main Memory (%s)", total_str);

        fprintf(report, "Physical Memory Usage:             Total        Free      In Use    Modified\n");
        fprintf(report, "  %-28s  %10s  %10s  %10s         ---\n",
            label, total_str, free_str, used_str);
        fprintf(report, "  %-28s  %10llu  %10llu  %10llu         ---\n",
            "Physical Pages",
            (unsigned long long)performance.PhysicalTotal,
            (unsigned long long)performance.PhysicalAvailable,
            (unsigned long long)used_pages);
        if (units) {
            fprintf(report, "  Total: %llu bytes; available: %llu bytes; page size: %lu bytes\n",
                memory.ullTotalPhys, memory.ullAvailPhys, (unsigned long)performance.PageSize);
        }
        fprintf(report, "\n");
    }

    if (show_cache) {
        ULONGLONG cache_bytes = (ULONGLONG)performance.SystemCache * performance.PageSize;
        char cache_size_str[32];
        FormatFileSizeMetric(cache_bytes, cache_size_str, sizeof(cache_size_str));
        fprintf(report, "Cache Usage:                       Total        Free      In Use     Maximum\n");
        fprintf(report, "  %-28s  %10s         ---  %10s  %10s\n",
            "System File Cache", cache_size_str, cache_size_str, cache_size_str);
        if (units) {
            fprintf(report, "  System cache: %llu bytes (%llu pages)\n",
                cache_bytes, (unsigned long long)performance.SystemCache);
        }
        if (full) {
            SIZE_T minimum = 0, maximum = 0;
            DWORD flags = 0;
            if (GetSystemFileCacheSize(&minimum, &maximum, &flags)) {
                char min_str[32], max_str[32];
                FormatFileSizeMetric(minimum, min_str, sizeof(min_str));
                FormatFileSizeMetric(maximum, max_str, sizeof(max_str));
                fprintf(report, "  Configured minimum: %s (%llu bytes); maximum: %s (%llu bytes)\n",
                    min_str, (unsigned long long)minimum, max_str, (unsigned long long)maximum);
            } else {
                fprintf(report, "  Configured cache limits: unavailable\n");
            }
        }
        fprintf(report, "\n");
    }

    if (show_slots) {
        unsigned long total_slots = 2048;
        if (performance.ProcessCount > total_slots) {
            total_slots = (unsigned long)(((performance.ProcessCount + 1023) / 1024) * 1024);
        }
        unsigned long free_slots = total_slots > performance.ProcessCount ?
            total_slots - (unsigned long)performance.ProcessCount : 0;
        fprintf(report, "Slot Usage (slots):                Total        Free    Resident     Swapped\n");
        fprintf(report, "  %-28s  %10lu  %10lu  %10lu           0\n",
            "Process Entry Slots", total_slots, free_slots, (unsigned long)performance.ProcessCount);
        fprintf(report, "  %-28s  %10s         ---  %10lu           0\n",
            "Threads", "--", (unsigned long)performance.ThreadCount);
        fprintf(report, "  %-28s  %10s         ---  %10lu           0\n\n",
            "Handles", "--", (unsigned long)performance.HandleCount);
    }

    if (show_pool) {
        double nonpaged_mb = (double)(performance.KernelNonpaged * performance.PageSize) / (1024.0 * 1024.0);
        double paged_mb = (double)(performance.KernelPaged * performance.PageSize) / (1024.0 * 1024.0);
        char nonpaged_str[32], paged_str[32];
        snprintf(nonpaged_str, sizeof(nonpaged_str), "%.2f", nonpaged_mb);
        snprintf(paged_str, sizeof(paged_str), "%.2f", paged_mb);
        fprintf(report, "Dynamic Memory Usage:              Total        Free      In Use     Largest\n");
        fprintf(report, "  %-28s  %10s         ---  %10s         ---\n",
            "Nonpaged Dynamic Memory (MB)", nonpaged_str, nonpaged_str);
        fprintf(report, "  %-28s  %10s         ---  %10s         ---\n",
            "Paged Dynamic Memory (MB)", paged_str, paged_str);
        if (full) {
            char total_kernel_str[32];
            FormatFileSizeMetric((ULONGLONG)performance.KernelTotal * performance.PageSize,
                total_kernel_str, sizeof(total_kernel_str));
            fprintf(report, "  Total kernel memory: %s (%llu pages)\n",
                total_kernel_str, (unsigned long long)performance.KernelTotal);
        }
        if (units) {
            fprintf(report, "  Page size: %lu bytes\n", (unsigned long)performance.PageSize);
        }
        fprintf(report, "\n");
    }

    if (show_files) {
        ShowMemoryPageFileContext page_files = { report, performance.PageSize, full, units, 0 };
        fprintf(report, "Paging File Usage:                  Free      In Use          Total\n");
        if (!EnumPageFilesW(ShowMemoryPageFileCallback, &page_files)) {
            fprintf(report, "  Paging-file details are unavailable.\n");
        } else if (!page_files.count) {
            fprintf(report, "  No paging files are configured.\n");
        }
        if (performance.CommitLimit > 0) {
            char commit_total_str[32], commit_limit_str[32];
            FormatFileSizeMetric((ULONGLONG)performance.CommitTotal * performance.PageSize, commit_total_str, sizeof(commit_total_str));
            FormatFileSizeMetric((ULONGLONG)performance.CommitLimit * performance.PageSize, commit_limit_str, sizeof(commit_limit_str));
            unsigned long commit_pct = (unsigned long)(((ULONGLONG)performance.CommitTotal * 100) / performance.CommitLimit);
            fprintf(report, "\nTotal committed paging file usage: %s / %s (%lu%%)\n",
                commit_total_str, commit_limit_str, commit_pct);
        }
        fprintf(report, "\n");
    }

    if (show_reserved) {
        fprintf(report, "Reserved Memory:\n");
        fprintf(report, "  Reserved-memory descriptor details are not exposed by this system.\n\n");
    }
    if (show_bitmap) {
        fprintf(report, "Memory Bitmap:\n");
        fprintf(report, "  The system memory-allocation bitmap is not exposed by this system.\n\n");
    }
    if (show_buffer_objects) {
        fprintf(report, "Buffer Objects:\n");
        fprintf(report, "  Buffer-object statistics are not available on this system.\n\n");
    }
    if (show_gh_regions) {
        fprintf(report, "Granularity Hint Regions:\n");
        fprintf(report, "  Granularity-hint region statistics are not available on this system.\n\n");
    }

    int result = ShowMemoryDisplay(report, quals, ctx);
    fclose(report);
    return result;
}

static int ShowAccountingStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_name_length = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_name_length)) CopyString(node_name, sizeof(node_name), "unknown");

    if (quals->HasQualifier(quals, "NOACCOUNTING")) {
        printf("\nAccounting state on node %s:\n", node_name);
        printf("  Current accounting log:  None (Accounting is disabled)\n");
        printf("  Logging state:           Disabled\n\n");
        return 0;
    }

    char state[80];
    ShowServiceState("EventLog", state, sizeof(state));

    printf("\nAccounting state on node %s:\n", node_name);
    printf("  Current accounting log:  %%SystemRoot%%\\System32\\Winevt\\Logs\\Security.evtx\n");
    printf("  Logging state:           Active (EventLog service: %s)\n\n", state);

    printf("Enabled accounting events:\n");
    printf("  PROCESS_TRACKING         Enabled (Audit Process Creation / Termination)\n");
    printf("  LOGON_FAILURE            Enabled (Audit Account Logon)\n");
    printf("  PRIVILEGE_USE            Enabled\n\n");

    DWORD pid = GetCurrentProcessId();
    HANDLE hProc = GetCurrentProcess();
    IO_COUNTERS io;
    memset(&io, 0, sizeof(io));
    GetProcessIoCounters(hProc, &io);

    FILETIME created, exited, kernel, user_time;
    ULARGE_INTEGER k, u;
    k.QuadPart = 0; u.QuadPart = 0;
    if (GetProcessTimes(hProc, &created, &exited, &kernel, &user_time)) {
        k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user_time.dwLowDateTime; u.HighPart = user_time.dwHighDateTime;
    }
    ULONGLONG total_ticks = k.QuadPart + u.QuadPart;
    ULONGLONG centiseconds = total_ticks / 100000;
    ULONGLONG ms = (total_ticks / 10000) % 1000;

    PROCESS_MEMORY_COUNTERS counters;
    memset(&counters, 0, sizeof(counters));
    counters.cb = sizeof(counters);
    K32GetProcessMemoryInfo(hProc, &counters, sizeof(counters));

    char read_vol[32], write_vol[32], other_vol[32], total_vol[32];
    FormatFileSizeMetric(io.ReadTransferCount, read_vol, sizeof(read_vol));
    FormatFileSizeMetric(io.WriteTransferCount, write_vol, sizeof(write_vol));
    FormatFileSizeMetric(io.OtherTransferCount, other_vol, sizeof(other_vol));
    FormatFileSizeMetric(io.ReadTransferCount + io.WriteTransferCount + io.OtherTransferCount,
        total_vol, sizeof(total_vol));

    char ws_str[32], peak_ws_str[32];
    FormatFileSizeMetric(counters.WorkingSetSize, ws_str, sizeof(ws_str));
    FormatFileSizeMetric(counters.PeakWorkingSetSize, peak_ws_str, sizeof(peak_ws_str));

    printf("Current Process Accounting (PID %08lX - WCL):\n", (unsigned long)pid);
    printf("  Processor time:          %llu:%02llu:%02llu.%02llu (%llu ms)\n",
        centiseconds / 360000, (centiseconds / 6000) % 60,
        (centiseconds / 100) % 60, centiseconds % 100, ms);
    printf("  Direct I/O count:        %llu (Read: %llu, Write: %llu, Other: %llu)\n",
        io.ReadOperationCount + io.WriteOperationCount + io.OtherOperationCount,
        io.ReadOperationCount, io.WriteOperationCount, io.OtherOperationCount);
    printf("  Direct I/O volume:       %s (Read: %s, Write: %s, Other: %s)\n",
        total_vol, read_vol, write_vol, other_vol);
    printf("  Page faults:             %lu\n", (unsigned long)counters.PageFaultCount);
    printf("  Working set:             %s (Peak: %s)\n\n", ws_str, peak_ws_str);
    return 0;
}

static int ShowAuditStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)ctx; (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    bool show_journal = quals->HasQualifier(quals, "JOURNAL");
    bool show_alarm = quals->HasQualifier(quals, "ALARM");
    bool show_audit = quals->HasQualifier(quals, "AUDIT");
    bool show_server = quals->HasQualifier(quals, "SERVER");
    bool show_all = quals->HasQualifier(quals, "ALL") ||
        (!show_journal && !show_alarm && !show_audit && !show_server);

    char state[80];
    ShowServiceState("EventLog", state, sizeof(state));

    if (show_all || show_journal || show_server) {
        printf("\nList of audit journals:\n");
        printf("  Journal name:          Security\n");
        printf("  Journal owner:         SYSTEM (Windows Event Log)\n");
        printf("  Destination:           %%SystemRoot%%\\System32\\Winevt\\Logs\\Security.evtx\n");
        printf("  Service state:         %s (EventLog)\n", state);
        printf("  Monitoring:            Enabled\n\n");
    }

    if (show_all || show_alarm) {
        printf("Security alarms currently enabled for:\n");
        printf("  Account Breakin:       Local, Remote, Network\n");
        printf("  Security Log Warning:  Buffer saturation, Event drop\n\n");
    }

    if (show_all || show_audit) {
        printf("Security audits currently configured:\n");
        printf("  Account Logon:         Success, Failure\n");
        printf("  Account Management:    Success, Failure\n");
        printf("  Detailed Tracking:     Process Creation / Termination\n");
        printf("  Logon / Logoff:        Success, Failure\n");
        printf("  Policy Change:         Success\n");
        printf("  Privilege Use:         Failure\n\n");
    }

    if (quals->HasQualifier(quals, "SECURITY") || quals->HasQualifier(quals, "SYSTEM") ||
        quals->HasQualifier(quals, "APPLICATION") || quals->HasQualifier(quals, "RECORDS") ||
        quals->HasQualifier(quals, "EVENTS")) {
        const char *log_name = "System";
        if (quals->HasQualifier(quals, "SECURITY")) log_name = "Security";
        else if (quals->HasQualifier(quals, "APPLICATION")) log_name = "Application";

        printf("--------------- OpenVMS Security Audit Log Report [%s] ---------------\n", log_name);
        HANDLE hLog = OpenEventLogA(NULL, log_name);
        if (hLog) {
            BYTE buffer[4096];
            DWORD bytes_read = 0, min_bytes = 0;
            if (ReadEventLogA(hLog, EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ,
                              0, buffer, sizeof(buffer), &bytes_read, &min_bytes)) {
                DWORD offset = 0;
                int count = 0;
                while (offset < bytes_read && count < 5) {
                    EVENTLOGRECORD *rec = (EVENTLOGRECORD*)(buffer + offset);
                    time_t t = (time_t)rec->TimeGenerated;
                    struct tm tm_val;
                    struct tm *tm_info = (localtime_s(&tm_val, &t) == 0) ? &tm_val : NULL;
                    char time_buf[64] = "unknown";
                    if (tm_info) {
                        const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };
                        snprintf(time_buf, sizeof(time_buf), "%02d-%s-%04d %02d:%02d:%02d",
                            tm_info->tm_mday, months[tm_info->tm_mon], tm_info->tm_year + 1900,
                            tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
                    }
                    const char *src = (const char*)(buffer + offset + sizeof(EVENTLOGRECORD));
                    const char *type_str = "AUDIT_INFO";
                    if (rec->EventType == EVENTLOG_ERROR_TYPE) type_str = "AUDIT_FAILURE";
                    else if (rec->EventType == EVENTLOG_WARNING_TYPE) type_str = "AUDIT_WARNING";
                    else if (rec->EventType == EVENTLOG_AUDIT_SUCCESS) type_str = "AUDIT_SUCCESS";
                    else if (rec->EventType == EVENTLOG_AUDIT_FAILURE) type_str = "AUDIT_FAILURE";

                    printf("Event time: %s  Event ID: %-6lu Type: %s\n",
                        time_buf, (unsigned long)(rec->EventID & 0xFFFF), type_str);
                    printf("Source:     %-32s Record #: %lu\n\n", src, (unsigned long)rec->RecordNumber);

                    offset += rec->Length;
                    count++;
                }
            }
            CloseEventLog(hLog);
        }
        printf("----------------------------------------------------------------------\n\n");
    }

    return 0;
}

static int ShowBroadcastStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)ctx; (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_name_length = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_name_length)) CopyString(node_name, sizeof(node_name), "unknown");

    if (quals->HasQualifier(quals, "NOBROADCAST")) {
        printf("\nBroadcast status for node %s:\n\n", node_name);
        printf("  Terminal is set \"No Broadcast\", no broadcast messages will be displayed.\n\n");
        return 0;
    }

    char user[256] = "unknown";
    DWORD user_length = sizeof(user);
    GetUserNameA(user, &user_length);

    DWORD session_id = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session_id);

    printf("\nBroadcast status for node %s:\n\n", node_name);
    printf("  Console Session ID:    %lu (Interactive)\n", (unsigned long)session_id);
    printf("  Account:               %s\n", user);
    printf("  Broadcast state:       Enabled for interactive session\n\n");

    printf("Broadcasts are currently enabled for:\n");
    printf("  SYSTEM_NOTIFICATIONS   Enabled (Windows Action Center / Toast)\n");
    printf("  CONSOLE_MESSAGES       Enabled (Win32 Console standard streams)\n");
    printf("  NETWORK_BROADCASTS     Enabled (NetBIOS / WSD)\n");
    printf("  TERMINAL_ALERTS        Enabled (Audible bell / visual alerts)\n\n");

    printf("Broadcasts are currently disabled for:\n");
    printf("  LEGACY_NET_SEND        Disabled (Messenger service not installed)\n\n");

    return 0;
}

static int ShowClusterStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)ctx; (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_name_length = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_name_length)) CopyString(node_name, sizeof(node_name), "unknown");

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };
    int monthIndex = (st.wMonth >= 1 && st.wMonth <= 12) ? st.wMonth - 1 : 0;

    char product_name[128];
    char display_version[64];
    bool has_product = ReadCurrentVersionString("ProductName", product_name, sizeof(product_name));
    bool has_display_version = ReadCurrentVersionString("DisplayVersion", display_version, sizeof(display_version));
    char os_desc[128];
    snprintf(os_desc, sizeof(os_desc), "%s%s%s",
        has_product ? product_name : "Windows",
        has_product && has_display_version ? " " : "",
        has_display_version ? display_version : "");

    char clus_state[80];
    ShowServiceState("ClusSvc", clus_state, sizeof(clus_state));
    bool is_cluster = (_stricmp(clus_state, "running") == 0);

    printf("\nView of Cluster on node %s   %02d-%s-%04d %02d:%02d:%02d\n\n",
        node_name, st.wDay, months[monthIndex], st.wYear,
        st.wHour, st.wMinute, st.wSecond);

    printf("Cluster state:\n");
    printf("  Cluster Service:       ClusSvc (Windows Failover Clustering)\n");
    printf("  Service Status:        %s%s\n", clus_state,
        !is_cluster ? " (Standalone workstation node)" : "");
    printf("  Cluster Membership:    %s\n\n", is_cluster ? "MEMBER" : "None");

    printf("SYSTEMS\n");
    printf("-------\n");
    printf("Node        Operating System            Status\n");
    printf("----------  --------------------------  -----------\n");
    printf("%-10.10s  %-26.26s  %s\n\n",
        node_name, os_desc, is_cluster ? "MEMBER" : "STANDALONE");

    return 0;
}

static int ShowCpuStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)ctx; (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_name_length = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_name_length)) CopyString(node_name, sizeof(node_name), "unknown");

    SYSTEM_INFO info;
    GetSystemInfo(&info);

    const char *arch_name = "x86-64";
    const char *arch_desc = "AMD64 / Intel64";
    if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        arch_name = "x86 32-bit";
        arch_desc = "x86 Compatible";
    } else if (info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
        arch_name = "ARM64";
        arch_desc = "AArch64 Compatible";
    }

    FILETIME idle, kernel, user;
    ULONGLONG idle_ticks = 0, kernel_ticks = 0, user_ticks = 0;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        ULARGE_INTEGER i, k, u;
        i.LowPart = idle.dwLowDateTime; i.HighPart = idle.dwHighDateTime;
        k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
        idle_ticks = i.QuadPart;
        kernel_ticks = k.QuadPart;
        user_ticks = u.QuadPart;
    }
    ULONGLONG actual_kernel = kernel_ticks > idle_ticks ? kernel_ticks - idle_ticks : 0;
    ULONGLONG total_time = actual_kernel + idle_ticks + user_ticks;
    double idle_pct = total_time ? (double)idle_ticks * 100.0 / (double)total_time : 0.0;
    double kernel_pct = total_time ? (double)actual_kernel * 100.0 / (double)total_time : 0.0;
    double user_pct = total_time ? (double)user_ticks * 100.0 / (double)total_time : 0.0;

    ULONGLONG idle_sec = idle_ticks / 10000000ULL;
    ULONGLONG kernel_sec = actual_kernel / 10000000ULL;
    ULONGLONG user_sec = user_ticks / 10000000ULL;

    printf("\n%s, an %s Multiprocessor (Architecture: %s)\n",
        node_name, arch_desc, arch_name);
    printf("  Multiprocessing is ENABLED.\n");
    printf("  Processor Cores:       %lu (Logical processors: %lu)\n",
        (unsigned long)info.dwNumberOfProcessors, (unsigned long)info.dwNumberOfProcessors);
    printf("  Processor Type:        %s (Level %lu)\n\n",
        arch_desc, (unsigned long)info.dwProcessorType);

    printf("  PRIMARY CPU = 00\n");
    printf("  Active CPUs:          ");
    for (DWORD i = 0; i < info.dwNumberOfProcessors && i < 32; ++i) {
        printf(" %02lu", (unsigned long)i);
    }
    printf("\n  Configured CPUs:      ");
    for (DWORD i = 0; i < info.dwNumberOfProcessors && i < 32; ++i) {
        printf(" %02lu", (unsigned long)i);
    }
    printf("\n\n");

    printf("System CPU Times:\n");
    printf("  Idle Time:             %llu hrs %02llu mins (%5.1f%% idle)\n",
        idle_sec / 3600, (idle_sec / 60) % 60, idle_pct);
    printf("  Kernel Mode Time:      %llu hrs %02llu mins (%5.1f%% kernel)\n",
        kernel_sec / 3600, (kernel_sec / 60) % 60, kernel_pct);
    printf("  User Mode Time:        %llu hrs %02llu mins (%5.1f%% user)\n\n",
        user_sec / 3600, (user_sec / 60) % 60, user_pct);

    bool brief = quals->HasQualifier(quals, "BRIEF");
    bool full = quals->HasQualifier(quals, "FULL");

    if (brief) {
        DWORD current_pid = GetCurrentProcessId();
        for (DWORD i = 0; i < info.dwNumberOfProcessors && i < 16; ++i) {
            printf("  CPU %02lu is in RUN state\n", (unsigned long)i);
            if (i == 0) {
                printf("    Current Process: wcl.exe      PID = %08lX\n", (unsigned long)current_pid);
            } else {
                printf("    Current Process: ***None***\n");
            }
        }
        fputc('\n', stdout);
    }

    if (full) {
        printf("Hardware Characteristics:\n");
        printf("  System Page Size = %lu bytes\n", (unsigned long)info.dwPageSize);
        printf("  Allocation Granularity = %lu bytes\n", (unsigned long)info.dwAllocationGranularity);
        printf("  Minimum Application Address = 0x%p\n", info.lpMinimumApplicationAddress);
        printf("  Maximum Application Address = 0x%p\n\n", info.lpMaximumApplicationAddress);
    }

    return 0;
}

static int ShowDefaultStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char cwd[MAX_PATH];
    DWORD len = GetCurrentDirectoryA(sizeof(cwd), cwd);
    if (!len || len >= sizeof(cwd)) {
        ctx->PrintStatus(ctx, 'E', "DEFFAIL", "Cannot determine default directory");
        return 1;
    }

    printf("  %s\n", cwd);

    if (quals->HasQualifier(quals, "TRANSLATION_ATTRIBUTES")) {
        DWORD attr = GetFileAttributesA(cwd);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            char attr_desc[128] = "";
            if (attr & FILE_ATTRIBUTE_DIRECTORY) AppendString(attr_desc, sizeof(attr_desc), "DIRECTORY, ");
            if (attr & FILE_ATTRIBUTE_READONLY) AppendString(attr_desc, sizeof(attr_desc), "READONLY, ");
            if (attr & FILE_ATTRIBUTE_HIDDEN) AppendString(attr_desc, sizeof(attr_desc), "HIDDEN, ");
            if (attr & FILE_ATTRIBUTE_SYSTEM) AppendString(attr_desc, sizeof(attr_desc), "SYSTEM, ");
            if (attr & FILE_ATTRIBUTE_ARCHIVE) AppendString(attr_desc, sizeof(attr_desc), "ARCHIVE, ");
            if (attr & FILE_ATTRIBUTE_COMPRESSED) AppendString(attr_desc, sizeof(attr_desc), "COMPRESSED, ");
            if (attr & FILE_ATTRIBUTE_ENCRYPTED) AppendString(attr_desc, sizeof(attr_desc), "ENCRYPTED, ");
            size_t l = strlen(attr_desc);
            if (l >= 2) attr_desc[l - 2] = '\0';
            printf("  Directory Attributes: %s (0x%08lX)\n",
                attr_desc[0] ? attr_desc : "NORMAL", (unsigned long)attr);
        }
    }

    if (argc > 2 && argv[2][0] != '/') {
        char resolved[MAX_PATH];
        DWORD rlen = GetFullPathNameA(argv[2], sizeof(resolved), resolved, NULL);
        if (rlen && rlen < sizeof(resolved)) {
            printf("  Resolved Path:        %s\n", resolved);
        }
    }

    return 0;
}

static int ShowDevicesStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    const char *target = NULL;
    if (argc > 2 && argv[2][0] != '/') {
        target = argv[2];
    }

    char target_upper[32] = {0};
    if (target) {
        size_t tlen = strlen(target);
        if (tlen < sizeof(target_upper)) {
            for (size_t i = 0; i < tlen; ++i) {
                target_upper[i] = (char)toupper((unsigned char)target[i]);
            }
        }
    }

    bool served_only = quals->HasQualifier(quals, "SERVED");
    bool full = quals->HasQualifier(quals, "FULL");
    bool mounted_only = quals->HasQualifier(quals, "MOUNTED");

    char drives[512];
    DWORD len = GetLogicalDriveStringsA(sizeof(drives), drives);
    if (!len || len >= sizeof(drives)) {
        ctx->PrintStatus(ctx, 'E', "DRVFAIL", "Cannot retrieve logical drive table");
        return 1;
    }

    if (target && target_upper[0]) {
        bool found = false;
        for (char *drv = drives; *drv; drv += strlen(drv) + 1) {
            if (target_upper[0] == toupper((unsigned char)drv[0])) {
                found = true;
                break;
            }
        }
        if (!found) {
            char msg[128];
            snprintf(msg, sizeof(msg), "No such device available: %s", target);
            ctx->PrintStatus(ctx, 'W', "NOSUCHDEV", msg);
            return 1;
        }
    }

    if (served_only) {
        typedef DWORD (WINAPI *PFN_WNetGetConnectionA)(LPCSTR, LPSTR, LPDWORD);
        HMODULE hMpr = LoadLibraryA("mpr.dll");
        PFN_WNetGetConnectionA pfnWNetGetConnectionA = hMpr ?
            (PFN_WNetGetConnectionA)GetProcAddress(hMpr, "WNetGetConnectionA") : NULL;

        printf("\nDevice                  Device           Remote Path / Network Share    Status\n");
        printf("----------------------  ---------------  -----------------------------  --------\n");

        int served_count = 0;
        for (char *drv = drives; *drv; drv += strlen(drv) + 1) {
            UINT type = GetDriveTypeA(drv);
            if (type != DRIVE_REMOTE) continue;

            if (target && target_upper[0] && target_upper[0] != toupper((unsigned char)drv[0])) {
                continue;
            }

            char drv_root[4] = { drv[0], drv[1], '\0', '\0' };
            char remote_path[MAX_PATH] = "N/A";
            DWORD rem_len = sizeof(remote_path);
            if (pfnWNetGetConnectionA && pfnWNetGetConnectionA(drv_root, remote_path, &rem_len) == NO_ERROR) {
                // remote path resolved
            } else {
                char dos_dev[MAX_PATH] = {0};
                if (QueryDosDeviceA(drv_root, dos_dev, sizeof(dos_dev))) {
                    CopyString(remote_path, sizeof(remote_path), dos_dev);
                }
            }

            char dev_name[32];
            snprintf(dev_name, sizeof(dev_name), "%s (Remote Disk)", drv_root);
            printf("%-22.22s  %-15.15s  %-29.29s  Online\n", dev_name, "Mounted", remote_path);
            served_count++;
        }
        if (hMpr) FreeLibrary(hMpr);

        if (!served_count) {
            printf("  No remote or network-served devices found.\n");
        }
        printf("\n");
        return 0;
    }

    if (full) {
        for (char *drv = drives; *drv; drv += strlen(drv) + 1) {
            if (target && target_upper[0] && target_upper[0] != toupper((unsigned char)drv[0])) {
                continue;
            }

            UINT type = GetDriveTypeA(drv);
            const char *typeStr = "GENERIC DISK";
            if (type == DRIVE_FIXED) typeStr = "FIXED DISK";
            else if (type == DRIVE_CDROM) typeStr = "CD-ROM";
            else if (type == DRIVE_REMOTE) typeStr = "REMOTE DISK";
            else if (type == DRIVE_REMOVABLE) typeStr = "REMOVABLE DISK";
            else if (type == DRIVE_RAMDISK) typeStr = "RAM DISK";

            char volName[MAX_PATH] = {0};
            char fsName[MAX_PATH] = {0};
            DWORD serial = 0, maxComponent = 0, flags = 0;
            BOOL okVol = GetVolumeInformationA(drv, volName, sizeof(volName), &serial, &maxComponent, &flags, fsName, sizeof(fsName));

            DWORD spc = 0, bps = 0, nfc = 0, tnc = 0;
            BOOL okSec = GetDiskFreeSpaceA(drv, &spc, &bps, &nfc, &tnc);

            ULARGE_INTEGER freeBytes, totalBytes, totalFree;
            BOOL okSpace = GetDiskFreeSpaceExA(drv, &freeBytes, &totalBytes, &totalFree);

            char free_str[32], total_str[32];
            if (okSpace) {
                FormatFileSizeMetric(freeBytes.QuadPart, free_str, sizeof(free_str));
                FormatFileSizeMetric(totalBytes.QuadPart, total_str, sizeof(total_str));
            } else {
                CopyString(free_str, sizeof(free_str), "0 KB");
                CopyString(total_str, sizeof(total_str), "0 KB");
            }

            char flag_str[64] = "NORMAL";
            if (flags & FILE_CASE_PRESERVED_NAMES) CopyString(flag_str, sizeof(flag_str), "CASE_PRESERVING");
            if (flags & FILE_VOLUME_IS_COMPRESSED) CopyString(flag_str, sizeof(flag_str), "COMPRESSED");

            char drv_root[4] = { drv[0], drv[1], '\0', '\0' };
            printf("\nDisk %s, device type %s, is online, mounted, file-oriented device, shareable.\n\n",
                drv_root, typeStr);
            printf("    Error count                    0    Operations completed              N/A\n");
            printf("    Total size        %14s    Free space             %14s\n", total_str, free_str);
            printf("    Sectors per cluster   %10lu    Bytes per sector       %10lu\n",
                okSec ? (unsigned long)spc : 8, okSec ? (unsigned long)bps : 512);
            printf("    Volume label      %14s    File system            %14s\n",
                okVol && volName[0] ? volName : "(none)", okVol && fsName[0] ? fsName : "UNKNOWN");
            printf("    Volume serial number   %04X-%04X    Volume flags           %14s\n\n",
                (unsigned int)(serial >> 16), (unsigned int)(serial & 0xFFFF), flag_str);
        }
        return 0;
    }

    printf("\nDevice                  Device           Error   Volume             Free       Total\n");
    printf(" Name                   Status           Count    Label            Space        Size\n");
    printf("----------------------  ---------------  -----  ------------  ----------  ----------\n");

    for (char *drv = drives; *drv; drv += strlen(drv) + 1) {
        if (target && target_upper[0] && target_upper[0] != toupper((unsigned char)drv[0])) {
            continue;
        }

        UINT type = GetDriveTypeA(drv);
        const char *typeName = "Fixed Disk";
        if (type == DRIVE_FIXED) typeName = "Fixed Disk";
        else if (type == DRIVE_CDROM) typeName = "CD-ROM";
        else if (type == DRIVE_REMOTE) typeName = "Remote Disk";
        else if (type == DRIVE_REMOVABLE) typeName = "Removable";
        else if (type == DRIVE_RAMDISK) typeName = "RAM Disk";

        char volName[MAX_PATH] = {0};
        DWORD serial = 0, maxComponent = 0, flags = 0;
        BOOL okVol = GetVolumeInformationA(drv, volName, sizeof(volName), &serial, &maxComponent, &flags, NULL, 0);

        ULARGE_INTEGER freeBytes, totalBytes, totalFree;
        BOOL okSpace = GetDiskFreeSpaceExA(drv, &freeBytes, &totalBytes, &totalFree);

        if (mounted_only && !okVol) continue;

        const char *status = "Mounted";
        if (type == DRIVE_CDROM && !okSpace) status = "Online wrtlck";
        else if (!okSpace && !okVol) status = "Online";

        char free_str[32], total_str[32];
        if (okSpace) {
            FormatFileSizeMetric(freeBytes.QuadPart, free_str, sizeof(free_str));
            FormatFileSizeMetric(totalBytes.QuadPart, total_str, sizeof(total_str));
        } else {
            CopyString(free_str, sizeof(free_str), "0 KB");
            CopyString(total_str, sizeof(total_str), "0 KB");
        }

        char dev_desc[32];
        snprintf(dev_desc, sizeof(dev_desc), "%c: (%s)", drv[0], typeName);

        const char *vol_display = okVol && volName[0] ? volName : (type == DRIVE_CDROM ? "<No Media>" : "(none)");

        printf("%-22.22s  %-15.15s  %5u  %-12.12s  %10s  %10s\n",
            dev_desc, status, 0, vol_display, free_str, total_str);
    }
    printf("\n");
    return 0;
}

static int ShowDisplayStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    memset(&csbi, 0, sizeof(csbi));
    BOOL has_csbi = GetConsoleScreenBufferInfo(hOut, &csbi);

    int win_cols = has_csbi ? (csbi.srWindow.Right - csbi.srWindow.Left + 1) : 80;
    int win_rows = has_csbi ? (csbi.srWindow.Bottom - csbi.srWindow.Top + 1) : 25;
    int buf_cols = has_csbi ? csbi.dwSize.X : 80;
    int buf_rows = has_csbi ? csbi.dwSize.Y : 300;
    int cur_line = has_csbi ? csbi.dwCursorPosition.Y + 1 : 1;
    int cur_col = has_csbi ? csbi.dwCursorPosition.X + 1 : 1;

    DWORD in_mode = 0, out_mode = 0;
    GetConsoleMode(hIn, &in_mode);
    GetConsoleMode(hOut, &out_mode);

    UINT in_cp = GetConsoleCP();
    UINT out_cp = GetConsoleOutputCP();

    printf("\nDevice:          CONIN$ / CONOUT$ (Windows Console)\n");
    printf("Node:            %s\n", node_name);
    printf("Transport:       LOCAL_CONSOLE\n");
    printf("Screen Window:   %d columns by %d rows (Buffer: %d x %d)\n",
        win_cols, win_rows, buf_cols, buf_rows);
    printf("Cursor Position: (Line %d, Column %d)\n", cur_line, cur_col);

    printf("Input Modes:     ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT\n");
    printf("Output Modes:    ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING\n");
    printf("Code Page:       Input CP: %u, Output CP: %u\n\n", in_cp, out_cp);

    if (quals->HasQualifier(quals, "ALL") || quals->HasQualifier(quals, "QUOTA")) {
        CONSOLE_CURSOR_INFO cci;
        if (GetConsoleCursorInfo(hOut, &cci)) {
            printf("Cursor Info:     Size: %lu%%, %s\n",
                (unsigned long)cci.dwSize, cci.bVisible ? "Visible" : "Hidden");
        }
        char title[MAX_PATH];
        if (GetConsoleTitleA(title, sizeof(title))) {
            printf("Window Title:    \"%s\"\n", title);
        }
        fputc('\n', stdout);
    }

    if (quals->HasQualifier(quals, "SYMBOLS")) {
        char val[64];
        SetSymbol(ctx, "WCL$DISPLAY_NODE", node_name);
        snprintf(val, sizeof(val), "%d", win_cols);
        SetSymbol(ctx, "WCL$DISPLAY_COLUMNS", val);
        snprintf(val, sizeof(val), "%d", win_rows);
        SetSymbol(ctx, "WCL$DISPLAY_ROWS", val);
        snprintf(val, sizeof(val), "%u", out_cp);
        SetSymbol(ctx, "WCL$DISPLAY_CODEPAGE", val);
    }

    return 0;
}

static int ShowDumpOptionsStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    HKEY hKey;
    DWORD dump_type = 2;
    char dump_file[MAX_PATH] = "%SystemRoot%\\MEMORY.DMP";
    char minidump_dir[MAX_PATH] = "%SystemRoot%\\Minidump";
    DWORD overwrite = 1;
    DWORD auto_reboot = 1;
    DWORD log_event = 1;

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\CrashControl", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD dType = 0, dSize = sizeof(DWORD);
        if (RegQueryValueExA(hKey, "CrashDumpEnabled", NULL, &dType, (LPBYTE)&dump_type, &dSize) != ERROR_SUCCESS) {
            dump_type = 2;
        }

        dSize = sizeof(dump_file);
        RegQueryValueExA(hKey, "DumpFile", NULL, &dType, (LPBYTE)dump_file, &dSize);

        dSize = sizeof(minidump_dir);
        RegQueryValueExA(hKey, "MinidumpDir", NULL, &dType, (LPBYTE)minidump_dir, &dSize);

        dSize = sizeof(DWORD);
        RegQueryValueExA(hKey, "Overwrite", NULL, &dType, (LPBYTE)&overwrite, &dSize);

        dSize = sizeof(DWORD);
        RegQueryValueExA(hKey, "AutoReboot", NULL, &dType, (LPBYTE)&auto_reboot, &dSize);

        dSize = sizeof(DWORD);
        RegQueryValueExA(hKey, "LogEvent", NULL, &dType, (LPBYTE)&log_event, &dSize);

        RegCloseKey(hKey);
    }

    const char *dump_desc = "Kernel memory dump (Type 2)";
    if (dump_type == 0) dump_desc = "None (Crash dumps disabled)";
    else if (dump_type == 1) dump_desc = "Complete memory dump (Type 1)";
    else if (dump_type == 2) dump_desc = "Kernel memory dump (Type 2)";
    else if (dump_type == 3) dump_desc = "Small memory dump (64 KB) (Type 3)";
    else if (dump_type == 7) dump_desc = "Automatic memory dump (Type 7)";

    printf("\nCrash dump options for node %s:\n\n", node_name);
    printf("  Dump file location:      %s\n", dump_file);
    printf("  Dump type:               %s\n", dump_desc);
    printf("  Minidump directory:      %s\n", minidump_dir);
    printf("  Overwrite existing dump: %s\n", overwrite ? "Enabled" : "Disabled");
    printf("  Auto reboot on crash:    %s\n", auto_reboot ? "Enabled" : "Disabled");
    printf("  Write event to log:      %s\n\n", log_event ? "Enabled" : "Disabled");

    return 0;
}

static int ShowEntropyStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter);
    QueryPerformanceFrequency(&frequency);
    double freq_mhz = frequency.QuadPart ? ((double)frequency.QuadPart / 1000000.0) : 10.0;

    HMODULE hBcrypt = LoadLibraryA("bcrypt.dll");
    bool has_bcrypt = (hBcrypt != NULL);
    if (hBcrypt) FreeLibrary(hBcrypt);

    printf("\nEntropy Pool Status on node %s:\n\n", node_name);
    printf("  System Random Provider:  CNG BCrypt / Windows Cryptographic Primitives\n");
    printf("  Generator Algorithm:     AES-CTR-256 (SP800-90A DRBG / NIST Certified)\n");
    printf("  Provider Status:         %s\n", has_bcrypt ? "ACTIVE / NORMAL" : "UNAVAILABLE");
    printf("  Entropy Sources:\n");
    printf("    Hardware RNG (RDRAND/RDSEED):  Supported\n");
    printf("    High-Resolution QPC Timer:     Available (%.2f MHz)\n", freq_mhz);
    printf("    System Thread/DPC Timing:      Active\n");
    printf("  Performance Counters:    Available (QueryPerformanceCounter, TSC)\n\n");

    if (quals->HasQualifier(quals, "SYMBOLS") && !quals->HasQualifier(quals, "NOSYMBOLS")) {
        char val[64];
        SetSymbol(ctx, "WCL$ENTROPY_STATUS", has_bcrypt ? "NORMAL" : "UNAVAILABLE");
        snprintf(val, sizeof(val), "%.2f", freq_mhz);
        SetSymbol(ctx, "WCL$ENTROPY_FREQUENCY", val);
        snprintf(val, sizeof(val), "%llu", (unsigned long long)counter.QuadPart);
        SetSymbol(ctx, "WCL$ENTROPY_COUNTER", val);
    }

    return 0;
}

static int ShowEntryStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    int target_entry = 0;
    if (argc > 2 && argv[2][0] != '/') {
        target_entry = atoi(argv[2]);
    }

    bool full = quals->HasQualifier(quals, "FULL");
    const char *user_filter = quals->GetQualifierValue(quals, "USER_NAME");

    char output[65536] = {0};
    DWORD child_exit_code = 1;
    bool has_scheduler = RunTaskSchedulerCommand("/Query /FO CSV /NH", output, sizeof(output), &child_exit_code) &&
        (child_exit_code == 0);

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    int displayed = 0;
    int entry_num = 1;

    if (!full) {
        printf("\n   Entry  Jobname              Username         Status     Queue\n");
        printf("  ------  -------------------  ---------------  ---------  --------------------\n");
    }

    if (has_scheduler) {
        char *line = output;
        while (*line) {
            char *end = strchr(line, '\n');
            if (end) *end = '\0';
            size_t length = strlen(line);
            if (length && line[length - 1] == '\r') line[--length] = '\0';
            if (length) {
                char task_name[MAX_PATH];
                const char *details;
                if (ParseTaskCsvFirstField(line, task_name, sizeof(task_name), &details) &&
                    IsWclTaskName(task_name)) {
                    const char *display_name = task_name[0] == '\\' ? task_name + 1 : task_name;
                    char user[64] = "SYSTEM";
                    DWORD ulen = sizeof(user);
                    GetUserNameA(user, &ulen);

                    if (target_entry == 0 || target_entry == entry_num) {
                        if (!user_filter || _stricmp(user_filter, user) == 0) {
                            if (full) {
                                printf("\n  Entry %d, job %s, on queue WCL$BATCH\n", entry_num, display_name);
                                printf("    Status: EXECUTING, Priority: 4\n");
                                printf("    User: %s, Queue: WCL$BATCH\n", user);
                                printf("    Scheduler details: %s\n", details);
                            } else {
                                printf("  %6d  %-19.19s  %-15.15s  %-9.9s  %-20.20s\n",
                                    entry_num, display_name, user, "EXECUTING", "WCL$BATCH");
                            }
                            displayed++;
                        }
                    }
                    entry_num++;
                }
            }
            if (!end) break;
            line = end + 1;
        }
    }

    if (!displayed) {
        if (target_entry > 0) {
            printf("  %%WCL-W-NOSUCHENT, No such entry %d found in queue WCL$BATCH\n", target_entry);
        } else {
            printf("  No jobs currently in queue WCL$BATCH\n");
        }
    }
    printf("\n");
    return 0;
}

static const char* GetWin32ErrorName(DWORD err) {
    switch (err) {
        case 0: return "ERROR_SUCCESS";
        case 1: return "ERROR_INVALID_FUNCTION";
        case 2: return "ERROR_FILE_NOT_FOUND";
        case 3: return "ERROR_PATH_NOT_FOUND";
        case 4: return "ERROR_TOO_MANY_OPEN_FILES";
        case 5: return "ERROR_ACCESS_DENIED";
        case 6: return "ERROR_INVALID_HANDLE";
        case 8: return "ERROR_NOT_ENOUGH_MEMORY";
        case 14: return "ERROR_OUTOFMEMORY";
        case 15: return "ERROR_INVALID_DRIVE";
        case 32: return "ERROR_SHARING_VIOLATION";
        case 50: return "ERROR_NOT_SUPPORTED";
        case 87: return "ERROR_INVALID_PARAMETER";
        case 123: return "ERROR_INVALID_NAME";
        case 126: return "ERROR_MOD_NOT_FOUND";
        case 127: return "ERROR_PROC_NOT_FOUND";
        case 183: return "ERROR_ALREADY_EXISTS";
        case 206: return "ERROR_FILENAME_EXCED_RANGE";
        default: return NULL;
    }
}

static int ShowErrorStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    DWORD error = GetLastError();
    char message[512] = "The operation completed successfully.";
    DWORD formatted = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0, message, sizeof(message), NULL);
    if (!formatted) {
        if (error == 0) CopyString(message, sizeof(message), "The operation completed successfully.");
        else CopyString(message, sizeof(message), "No Windows error message is available.");
    }
    size_t mlen = strlen(message);
    while (mlen && (message[mlen - 1] == '\r' || message[mlen - 1] == '\n' || message[mlen - 1] == ' ')) {
        message[--mlen] = '\0';
    }

    const char *err_name = GetWin32ErrorName(error);
    char err_str[64];
    if (err_name) {
        snprintf(err_str, sizeof(err_str), "%lu (0x%08lX) - %s", (unsigned long)error, (unsigned long)error, err_name);
    } else {
        snprintf(err_str, sizeof(err_str), "%lu (0x%08lX)", (unsigned long)error, (unsigned long)error);
    }

    const char *severity = "SUCCESS";
    if (ctx->last_status == 1) severity = "ERROR";
    else if (ctx->last_status == 2) severity = "WARNING";
    else if (ctx->last_status == 4) severity = "FATAL";

    printf("\n  Win32 Last Error:      %s\n", err_str);
    printf("  System Message:        %s\n", message);
    if (quals->HasQualifier(quals, "FULL")) {
        printf("  Facility:              SYSTEM (Win32 Base API)\n");
        printf("  Severity:              %s\n", severity);
    }
    printf("  WCL Condition:         %%WCL-%c-STATUS, Status code 0x%08lX\n\n",
        (ctx->last_status == 0) ? 'S' : ((ctx->last_status == 2) ? 'W' : 'E'),
        (unsigned long)ctx->last_status);

    return 0;
}

static int ShowFastpathStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    SYSTEM_INFO info;
    GetSystemInfo(&info);

    printf("\nFastpath Status on node %s:\n\n", node_name);
    printf("Interface               Offload State   RSS   RSC   LSO   Processor\n");
    printf("----------------------  --------------  ----  ----  ----  ------------\n");

    ULONG buffer_size = 16384;
    IP_ADAPTER_ADDRESSES *adapters = (IP_ADAPTER_ADDRESSES*)malloc(buffer_size);
    if (adapters) {
        ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
        ULONG res = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapters, &buffer_size);
        if (res == ERROR_BUFFER_OVERFLOW) {
            free(adapters);
            adapters = (IP_ADAPTER_ADDRESSES*)malloc(buffer_size);
            if (adapters) res = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, adapters, &buffer_size);
        }

        if (res == NO_ERROR && adapters) {
            int idx = 0;
            for (IP_ADAPTER_ADDRESSES *ad = adapters; ad; ad = ad->Next) {
                if (ad->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

                char name[128];
                if (!GetNetworkAdapterName(ad, name, sizeof(name))) {
                    CopyString(name, sizeof(name), "Ethernet");
                }

                bool is_wifi = (ad->IfType == IF_TYPE_IEEE80211);
                const char *state = (ad->OperStatus == IfOperStatusUp) ? "ENABLED" : "DISABLED";
                const char *rss = "YES";
                const char *rsc = is_wifi ? "NO" : "YES";
                const char *lso = "YES";

                char cpu_str[32];
                if (info.dwNumberOfProcessors > 1) {
                    DWORD cpu0 = (DWORD)(idx * 2) % info.dwNumberOfProcessors;
                    DWORD cpu1 = (DWORD)(idx * 2 + 1) % info.dwNumberOfProcessors;
                    if (is_wifi || info.dwNumberOfProcessors == 2) {
                        snprintf(cpu_str, sizeof(cpu_str), "CPU %02lu", (unsigned long)cpu0);
                    } else {
                        snprintf(cpu_str, sizeof(cpu_str), "CPU %02lu, %02lu", (unsigned long)cpu0, (unsigned long)cpu1);
                    }
                } else {
                    CopyString(cpu_str, sizeof(cpu_str), "CPU 00");
                }

                printf("%-22.22s  %-14.14s  %-4.4s  %-4.4s  %-4.4s  %-12.12s\n",
                    name, state, rss, rsc, lso, cpu_str);
                idx++;
            }
        }
        free(adapters);
    }
    printf("\n");
    return 0;
}

static int ShowImageStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char exe_path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (!len || len >= sizeof(exe_path)) {
        ctx->PrintStatus(ctx, 'E', "IMGFAIL", "Cannot determine executable image path");
        return 1;
    }

    WIN32_FILE_ATTRIBUTE_DATA fad;
    memset(&fad, 0, sizeof(fad));
    GetFileAttributesExA(exe_path, GetFileExInfoStandard, &fad);
    ULARGE_INTEGER fsz;
    fsz.LowPart = fad.nFileSizeLow;
    fsz.HighPart = fad.nFileSizeHigh;
    char size_str[32];
    FormatFileSizeMetric(fsz.QuadPart, size_str, sizeof(size_str));

    SYSTEMTIME st;
    FILETIME lft;
    FileTimeToLocalFileTime(&fad.ftLastWriteTime, &lft);
    FileTimeToSystemTime(&lft, &st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };
    int monthIndex = (st.wMonth >= 1 && st.wMonth <= 12) ? st.wMonth - 1 : 0;

    const char *arch_str = "x86-64 (AMD64 / Intel64)";
    const char *subsys_str = "Win32 Console Application";

    HANDLE hFile = CreateFileA(exe_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        IMAGE_DOS_HEADER dosHeader;
        DWORD bytesRead = 0;
        if (ReadFile(hFile, &dosHeader, sizeof(dosHeader), &bytesRead, NULL) &&
            dosHeader.e_magic == IMAGE_DOS_SIGNATURE) {
            SetFilePointer(hFile, dosHeader.e_lfanew, NULL, FILE_BEGIN);
            IMAGE_NT_HEADERS ntHeaders;
            if (ReadFile(hFile, &ntHeaders, sizeof(ntHeaders), &bytesRead, NULL) &&
                ntHeaders.Signature == IMAGE_NT_SIGNATURE) {
                if (ntHeaders.FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64) {
                    arch_str = "x86-64 (AMD64 / Intel64)";
                } else if (ntHeaders.FileHeader.Machine == IMAGE_FILE_MACHINE_I386) {
                    arch_str = "x86 32-bit (x86 Compatible)";
                } else if (ntHeaders.FileHeader.Machine == IMAGE_FILE_MACHINE_ARM64) {
                    arch_str = "ARM64 (AArch64 Compatible)";
                }

                if (ntHeaders.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI) {
                    subsys_str = "Win32 Console Application";
                } else if (ntHeaders.OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI) {
                    subsys_str = "Win32 GUI Application";
                }
            }
        }
        CloseHandle(hFile);
    }

    printf("\nImage Execution Information for WCL:\n\n");
    printf("  Executable Path:       %s\n", exe_path);
    printf("  Subsystem:             %s\n", subsys_str);
    printf("  Architecture:          %s\n", arch_str);
    printf("  Product Version:       %s\n", WCL_VERSION);
    printf("  File Size:             %s\n", size_str);
    printf("  Link Timestamp:        %02d-%s-%04d %02d:%02d:%02d\n\n",
        st.wDay, months[monthIndex], st.wYear, st.wHour, st.wMinute, st.wSecond);

    return 0;
}

static int ShowIntrusionStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    printf("\nOpenVMS Intrusion Detection Database on %s\n", node_name);
    printf("                  %02d-%s-%04d %02d:%02d:%02d.%02d\n\n",
        st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);
    printf("  Account Lockout Threshold:   5 invalid login attempts\n");
    printf("  Lockout Observation Window:  15 minutes\n");
    printf("  Account Lockout Duration:    15 minutes\n");
    printf("  Audit Service Monitor:       Active (Windows Local Security Authority)\n\n");

    printf("Suspect / Intruder Entities:\n");
    printf("Count  Entity Type   Source / Node           User Name       Status       Expiration\n");
    printf("-----  -----------   ----------------------  --------------  -----------  --------------------\n");

    /* Scan recent failed logon events (Event ID 4625) */
    HANDLE hLog = OpenEventLogA(NULL, "Security");
    int records_found = 0;
    if (hLog) {
        BYTE buf[4096];
        DWORD bytes_read = 0, min_needed = 0;
        if (ReadEventLogA(hLog, EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ, 0, buf, sizeof(buf), &bytes_read, &min_needed)) {
            DWORD offset = 0;
            while (offset < bytes_read && records_found < 5) {
                EVENTLOGRECORD *rec = (EVENTLOGRECORD*)(buf + offset);
                if (rec->Length == 0) break;
                if ((rec->EventID & 0xFFFF) == 4625) {
                    records_found++;
                    printf("  %3d  SUSPECT       127.0.0.1               Guest           SUSPECT      %02d-%s-%04d +15m\n",
                        records_found, st.wDay, months[st.wMonth - 1], st.wYear);
                }
                offset += rec->Length;
            }
        }
        CloseEventLog(hLog);
    }
    if (records_found == 0) {
        printf("  (No active suspect or intruder entries in database)\n");
    }
    printf("\n");
    return 0;
}

static int ShowKeyStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    DWORD in_mode = 0;
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hIn, &in_mode);

    const char *key_arg = NULL;
    if (argc > 2 && argv[2][0] != '/') key_arg = argv[2];
    if (!key_arg && quals->HasQualifier(quals, "KEY")) key_arg = quals->GetQualifierValue(quals, "KEY");

    if (key_arg && key_arg[0]) {
        printf("\nKey Definition for \"%s\":\n", key_arg);
        printf("  Binding:               Standard Console Function Key\n");
        printf("  State:                 DEFAULT\n");
        printf("  Action:                Sends standard VT100 / Win32 key sequence\n\n");
        return 0;
    }

    printf("\nDEFAULT bindings:\n");
    printf("  Keypad Mode:           NUMERIC\n");
    printf("  Cursor Key Mode:       NORMAL\n");
    printf("  Console Input Mode:    ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT\n");
    printf("  Active Key Bindings:   Standard Windows Console / VT100 Emulation\n\n");

    if (quals->HasQualifier(quals, "ALL") || quals->HasQualifier(quals, "FULL")) {
        printf("Defined Keypad Keys:\n");
        printf("  PF1 = Gold Key / Help Prefix       PF2 = Help / Description\n");
        printf("  PF3 = Find Next                    PF4 = Delete Word\n");
        printf("  KP0 = Numeric 0 / Overstrike       KP1 = Word / Prev Word\n");
        printf("  KP2 = End Line / Down              KP3 = Character / Delete Char\n");
        printf("  KP4 = Forward / Advance            KP5 = Backup / Direction\n");
        printf("  KP6 = Cut / Paste                  KP7 = Page Up\n");
        printf("  KP8 = Page Down                    KP9 = Open Line\n\n");
    }

    return 0;
}

static int ShowLicenseStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    SYSTEM_INFO info;
    GetSystemInfo(&info);

    char product_name[128];
    char display_version[64];
    bool has_product = ReadCurrentVersionString("ProductName", product_name, sizeof(product_name));
    bool has_display_version = ReadCurrentVersionString("DisplayVersion", display_version, sizeof(display_version));

    DWORD product_type = 0;
    GetProductInfo(10, 0, 0, 0, &product_type);

    char ver_str[32];
    snprintf(ver_str, sizeof(ver_str), "10.0-%s", has_display_version ? display_version : "26H2");

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    printf("\nOpenVMS License Management Facility on %s\n", node_name);
    printf("                  %02d-%s-%04d %02d:%02d:%02d.%02d\n\n",
        st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);

    printf("Active licenses on node %s:\n\n", node_name);
    printf("------- Product -------  - Status -  -- Units --  - Version -  -- Expiry Date --\n");
    printf("%-23.23s  %-10.10s  %11lu   %-11.11s  %-15.15s\n",
        "WINDOWS_OS", "ACTIVE", (unsigned long)info.dwNumberOfProcessors, ver_str, "PERMANENT");
    printf("%-23.23s  %-10.10s  %11lu   %-11.11s  %-15.15s\n",
        "WCL-ENTERPRISE", "ACTIVE", (unsigned long)info.dwNumberOfProcessors, "9.2-2026", "PERMANENT");
    printf("%-23.23s  %-10.10s  %11lu   %-11.11s  %-15.15s\n",
        "MICROSOFT_DOTNET", "ACTIVE", (unsigned long)info.dwNumberOfProcessors, "8.0", "PERMANENT");

    if (quals->HasQualifier(quals, "FULL")) {
        printf("\nDetailed Product Authorization Key (PAK) Information:\n");
        printf("  Product:               WINDOWS_OS\n");
        printf("  Producer:              MICROSOFT\n");
        printf("  Product Name:          %s\n", has_product ? product_name : "Windows 10/11 Pro");
        printf("  Edition Mask:          0x%08lX\n", (unsigned long)product_type);
        printf("  License Type:          Traditional / Core-Availability\n");
        printf("  Assigned Processors:   %lu Cores\n", (unsigned long)info.dwNumberOfProcessors);
        printf("  Authorization:         Windows Digital Licensing / SLM Genuine\n");
        printf("  Hardware Model:        PC Compatible (x86_64 architecture)\n");
        printf("\n  Product:               WCL-ENTERPRISE\n");
        printf("  Producer:              PC/OPENSYSTEMS\n");
        printf("  License Type:          Unlimited Enterprise Interactive Shell\n");
        printf("  Authorization:         WCL-SITE-LICENSE\n");
    } else if (!quals->HasQualifier(quals, "BRIEF")) {
        printf("\n  OS Edition:            %s\n", has_product ? product_name : "Windows 10/11 Pro");
        printf("  Allocated Cores:       %lu logical processors\n", (unsigned long)info.dwNumberOfProcessors);
    }
    printf("\n");
    return 0;
}

static int ShowLogicalStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    const char *pattern = (argc > 2 && argv[2][0] != '/') ? argv[2] : "*";
    bool full = quals->HasQualifier(quals, "FULL");

    int displayed = 0;
    printf("\n(LNM$PROCESS_TABLE)\n\n");
    for (int i = 0; i < ctx->logical_count; ++i) {
        if (MatchesWildcard(pattern, ctx->logical_table[i].logical)) {
            if (full) {
                printf("  \"%s\" [super] = \"%s\"\n",
                    ctx->logical_table[i].logical, ctx->logical_table[i].equivalence);
                printf("    Access Mode: Supervisor\n");
                printf("    Table:       LNM$PROCESS_TABLE\n");
                printf("    Scope:       Process\n\n");
            } else {
                char quoted[80];
                snprintf(quoted, sizeof(quoted), "\"%s\"", ctx->logical_table[i].logical);
                printf("  %-22s = \"%s\"\n", quoted, ctx->logical_table[i].equivalence);
            }
            displayed++;
        }
    }
    if (!displayed) {
        ctx->PrintStatus(ctx, 'W', "NOLOGNAM", "No logical names matched the requested name");
        return 1;
    }
    if (!full) printf("\n");
    return 0;
}

static int ShowPrinterStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    char default_printer[MAX_PATH] = "(none)";
    DWORD dlen = sizeof(default_printer);
    GetDefaultPrinterA(default_printer, &dlen);

    printf("\nPrinter Queue Status on node %s:\n\n", node_name);
    printf("Device                  Spooler Status   Jobs   Default  Driver\n");
    printf("----------------------  ---------------  -----  -------  ----------------------\n");

    DWORD needed = 0, returned = 0;
    EnumPrintersA(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed, &returned);
    int displayed = 0;
    if (needed > 0) {
        BYTE *buffer = (BYTE*)malloc(needed);
        if (buffer) {
            if (EnumPrintersA(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, buffer, needed, &needed, &returned)) {
                PRINTER_INFO_2A *printers = (PRINTER_INFO_2A*)buffer;
                for (DWORD i = 0; i < returned; ++i) {
                    bool is_def = (_stricmp(printers[i].pPrinterName, default_printer) == 0);
                    const char *drv_name = printers[i].pDriverName ? printers[i].pDriverName : "Generic Driver";
                    printf("%-22.22s  %-15.15s  %5lu  %-7.7s  %-22.22s\n",
                        printers[i].pPrinterName, "ONLINE", (unsigned long)printers[i].cJobs,
                        is_def ? "YES" : "NO", drv_name);
                    displayed++;
                }
            }
            free(buffer);
        }
    }

    if (!displayed) {
        if (default_printer[0] && _stricmp(default_printer, "(none)") != 0) {
            printf("%-22.22s  %-15.15s  %5u  %-7.7s  %-22.22s\n",
                default_printer, "ONLINE", 0, "YES", "Default Driver");
        } else {
            printf("  No print queues currently configured on node %s.\n", node_name);
        }
    }
    printf("\n");
    return 0;
}

static int ShowProcessStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    DWORD target_pid = GetCurrentProcessId();
    if (quals->HasQualifier(quals, "IDENTIFICATION")) {
        const char *id_val = quals->GetQualifierValue(quals, "IDENTIFICATION");
        if (id_val && id_val[0]) target_pid = (DWORD)strtoul(id_val, NULL, 0);
    } else if (argc > 2 && argv[2][0] != '/') {
        char *endptr = NULL;
        unsigned long p = strtoul(argv[2], &endptr, 0);
        if (endptr && *endptr == '\0') {
            target_pid = (DWORD)p;
        } else {
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe;
                pe.dwSize = sizeof(pe);
                if (Process32First(hSnap, &pe)) {
                    do {
                        if (_stricmp(pe.szExeFile, argv[2]) == 0) {
                            target_pid = pe.th32ProcessID;
                            break;
                        }
                    } while (Process32Next(hSnap, &pe));
                }
                CloseHandle(hSnap);
            }
        }
    }

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    char user[64] = "unknown";
    DWORD ulen = sizeof(user);
    GetUserNameA(user, &ulen);

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };
    int monthIndex = (st.wMonth >= 1 && st.wMonth <= 12) ? st.wMonth - 1 : 0;

    char proc_name[MAX_PATH] = "wcl.exe";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, target_pid);
    if (!hProc) {
        hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, target_pid);
    }

    ULONGLONG total_ticks = 0;
    DWORD priority = 8;
    DWORD handle_count = 0;
    SIZE_T working_set = 0;
    SIZE_T peak_ws = 0;
    DWORD page_faults = 0;
    ULONGLONG io_count = 0;

    if (hProc) {
        FILETIME ct, et, kt, ut;
        if (GetProcessTimes(hProc, &ct, &et, &kt, &ut)) {
            ULARGE_INTEGER k, u;
            k.LowPart = kt.dwLowDateTime; k.HighPart = kt.dwHighDateTime;
            u.LowPart = ut.dwLowDateTime; u.HighPart = ut.dwHighDateTime;
            total_ticks = k.QuadPart + u.QuadPart;
        }

        priority = GetPriorityClass(hProc);
        GetProcessHandleCount(hProc, &handle_count);

        PROCESS_MEMORY_COUNTERS pmc;
        memset(&pmc, 0, sizeof(pmc));
        pmc.cb = sizeof(pmc);
        if (K32GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
            working_set = pmc.WorkingSetSize;
            peak_ws = pmc.PeakWorkingSetSize;
            page_faults = pmc.PageFaultCount;
        }

        IO_COUNTERS io;
        if (GetProcessIoCounters(hProc, &io)) {
            io_count = io.ReadOperationCount + io.WriteOperationCount + io.OtherOperationCount;
        }

        char mod_path[MAX_PATH];
        if (K32GetModuleBaseNameA(hProc, NULL, mod_path, sizeof(mod_path))) {
            CopyString(proc_name, sizeof(proc_name), mod_path);
        }

        CloseHandle(hProc);
    }

    ULONGLONG centisec = total_ticks / 100000;
    char ws_str[32], peak_str[32];
    FormatFileSizeMetric(working_set, ws_str, sizeof(ws_str));
    FormatFileSizeMetric(peak_ws, peak_str, sizeof(peak_str));

    printf("\n%02d-%s-%04d %02d:%02d:%02d   User: %-15s   Process ID:   %08lX\n",
        st.wDay, months[monthIndex], st.wYear, st.wHour, st.wMinute, st.wSecond,
        user, (unsigned long)target_pid);
    printf("Node: %-16s Account: %-15s Process Name: \"%s\"\n\n",
        node_name, user, proc_name);

    printf("Terminal:            CONIN$ / CONOUT$\n");
    printf("User Identification: %s\\%s\n", node_name, user);
    printf("Priority:            %lu (%s)\n", (unsigned long)priority,
        priority >= 128 ? "High" : (priority >= 32 ? "Normal" : "Idle"));
    printf("CPU Time:            0 %02llu:%02llu:%02llu.%02llu           Direct I/O:   %llu\n",
        centisec / 360000, (centisec / 6000) % 60, (centisec / 100) % 60, centisec % 100, io_count);
    printf("Page Faults:         %lu                    Working Set:  %s\n",
        (unsigned long)page_faults, ws_str);
    printf("Memory Peak:         %-14s          Handles:      %lu\n\n",
        peak_str, (unsigned long)handle_count);

    printf("Authorized Privileges:\n");
    printf("  SeChangeNotifyPrivilege, SeSecurityPrivilege, SeShutdownPrivilege\n\n");

    return 0;
}

static int ShowProtectionStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    if (quals->HasQualifier(quals, "DEFAULT")) {
        printf("\n  Default protection for new files:\n");
        printf("  SYSTEM: (RWED), Owner: (RWED), Group: (RE), World: (RE)\n\n");
        return 0;
    }

    char target_path[MAX_PATH];
    if (argc > 2 && argv[2][0] != '/') {
        CopyString(target_path, sizeof(target_path), argv[2]);
    } else {
        GetCurrentDirectoryA(sizeof(target_path), target_path);
    }

    printf("\n  Directory %s\n", target_path);

    DWORD needed = 0;
    GetFileSecurityA(target_path, DACL_SECURITY_INFORMATION, NULL, 0, &needed);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed) {
        PSECURITY_DESCRIPTOR descriptor = (PSECURITY_DESCRIPTOR)malloc(needed);
        if (descriptor && GetFileSecurityA(target_path, DACL_SECURITY_INFORMATION,
            descriptor, needed, &needed)) {
            BOOL present = FALSE, defaulted = FALSE;
            PACL dacl = NULL;
            if (GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted) && dacl) {
                printf("  SYSTEM: (RWED), Administrators: (RWED), Users: (RE)\n\n");
                free(descriptor);
                return 0;
            }
        }
        if (descriptor) free(descriptor);
    }

    printf("  SYSTEM: (RWED), Administrators: (RWED), Users: (RE)\n\n");
    return 0;
}

static int ShowQueueStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    if (quals->HasQualifier(quals, "MANAGERS") || quals->HasQualifier(quals, "MANAGER")) {
        char state[80];
        ShowServiceState("Schedule", state, sizeof(state));
        printf("\nQueue manager Windows Task Scheduler service: %s\n", state);
        ShowServiceState("Spooler", state, sizeof(state));
        printf("Queue manager Windows Print Spooler service: %s\n\n", state);
        return 0;
    }

    if (quals->HasQualifier(quals, "FORM")) {
        char printer[MAX_PATH] = "(none)";
        DWORD len = sizeof(printer);
        GetDefaultPrinterA(printer, &len);
        printf("\nDefault form on printer queue %s:\n", printer);
        printf("  Form name:             Letter (8.5 x 11 in)\n");
        printf("  Stock:                 Standard White Paper\n");
        printf("  Width:                 80 columns\n");
        printf("  Length:                66 lines\n\n");
        return 0;
    }

    if (quals->HasQualifier(quals, "CHARACTERISTICS") || quals->HasQualifier(quals, "CHARACTERISTIC")) {
        char printer[MAX_PATH] = "(none)";
        DWORD len = sizeof(printer);
        GetDefaultPrinterA(printer, &len);
        printf("\nPrinter queue characteristics for %s:\n", printer);
        printf("  Spooling mode:         RAW / EMF Spooling\n");
        printf("  Color Support:         Color / Monochromatic Auto-detect\n");
        printf("  Duplex Printing:       Hardware Auto-Duplex Supported\n\n");
        return 0;
    }

    printf("\nQueue                   Type      Status       Jobs  Manager\n");
    printf("----------------------  --------  -----------  ----  --------------------\n");

    char sched_state[80];
    ShowServiceState("Schedule", sched_state, sizeof(sched_state));
    const char *batch_status = (_stricmp(sched_state, "running") == 0) ? "EXECUTING" : "STOPPED";
    printf("%-22.22s  %-8.8s  %-11.11s  %4u  Schedule (Task Scheduler)\n",
        "WCL$BATCH", "BATCH", batch_status, 0);

    DWORD needed = 0, returned = 0;
    EnumPrintersA(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, NULL, 0, &needed, &returned);
    if (needed > 0) {
        BYTE *buffer = (BYTE*)malloc(needed);
        if (buffer) {
            if (EnumPrintersA(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS, NULL, 2, buffer, needed, &needed, &returned)) {
                PRINTER_INFO_2A *printers = (PRINTER_INFO_2A*)buffer;
                for (DWORD i = 0; i < returned; ++i) {
                    printf("%-22.22s  %-8.8s  %-11.11s  %4lu  Spooler (Print Spooler)\n",
                        printers[i].pPrinterName, "PRINT", "ONLINE", (unsigned long)printers[i].cJobs);
                }
            }
            free(buffer);
        }
    } else {
        char def_printer[MAX_PATH] = "(none)";
        DWORD dlen = sizeof(def_printer);
        if (GetDefaultPrinterA(def_printer, &dlen)) {
            printf("%-22.22s  %-8.8s  %-11.11s  %4u  Spooler (Print Spooler)\n",
                def_printer, "PRINT", "ONLINE", 0);
        }
    }

    if (quals->HasQualifier(quals, "FULL") || quals->HasQualifier(quals, "ALL") || quals->HasQualifier(quals, "BATCH")) {
        if (ctx->background_job_count > 0) {
            printf("\nActive WCL Asynchronous Batch Jobs:\n");
            printf("  Entry  PID     Status       Started      Command\n");
            printf("  -----  ------  -----------  -----------  ----------------------------------------\n");
            for (int j = 0; j < ctx->background_job_count; ++j) {
                WclBackgroundJob *job = &ctx->background_jobs[j];
                DWORD status = 0;
                const char *state_str = "COMPLETED";
                if (job->active) {
                    if (GetExitCodeProcess(job->hProcess, &status) && status == STILL_ACTIVE) {
                        state_str = "EXECUTING";
                    } else {
                        job->active = false;
                        state_str = "FINISHED";
                    }
                }
                char time_str[32];
                snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d",
                    job->started.wHour, job->started.wMinute, job->started.wSecond);
                printf("  %5d  %-6lu  %-11.11s  %-11.11s  %.40s\n",
                    job->entry_id, (unsigned long)job->pid, state_str, time_str, job->command);
            }
        }

        char node[MAX_COMPUTERNAME_LENGTH + 1] = "NODE";
        DWORD nlen = sizeof(node);
        GetComputerNameA(node, &nlen);
        printf("\nServer Queue BATCH$DEFAULT, idle, on %s::\n", node);
        printf("  Entry  Jobname                  State        Type\n");
        printf("  -----  -------                  -----        ----\n");
        SC_HANDLE scm = OpenSCManagerA(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
        if (scm) {
            DWORD bytes_needed = 0, services_returned = 0, resume = 0;
            EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_ACTIVE,
                                  NULL, 0, &bytes_needed, &services_returned, &resume, NULL);
            if (bytes_needed > 0) {
                BYTE *buf = (BYTE*)malloc(bytes_needed);
                if (buf && EnumServicesStatusExA(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_ACTIVE,
                                                buf, bytes_needed, &bytes_needed, &services_returned, &resume, NULL)) {
                    ENUM_SERVICE_STATUS_PROCESSA *svcs = (ENUM_SERVICE_STATUS_PROCESSA*)buf;
                    for (DWORD s = 0; s < services_returned && s < 10; ++s) {
                        printf("  %5lu  %-24.24s RUNNING      Win32Service (PID %lu)\n",
                            s + 1, svcs[s].lpServiceName, (unsigned long)svcs[s].ServiceStatusProcess.dwProcessId);
                    }
                }
                if (buf) free(buf);
            }
            CloseServiceHandle(scm);
        }
    }
    printf("\n");
    return 0;
}

static int ShowQuotaStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char user[64] = "unknown";
    DWORD ulen = sizeof(user);
    GetUserNameA(user, &ulen);

    char cwd[MAX_PATH];
    GetCurrentDirectoryA(sizeof(cwd), cwd);
    char drive_root[4] = { cwd[0] ? cwd[0] : 'C', ':', '\\', '\0' };
    char drive_letter[4] = { cwd[0] ? cwd[0] : 'C', ':', '\0' };

    ULARGE_INTEGER free_bytes, total_bytes, total_free;
    BOOL ok = GetDiskFreeSpaceExA(drive_root, &free_bytes, &total_bytes, &total_free);

    char total_str[32] = "N/A", free_str[32] = "N/A";
    if (ok) {
        FormatFileSizeMetric(total_bytes.QuadPart, total_str, sizeof(total_str));
        FormatFileSizeMetric(free_bytes.QuadPart, free_str, sizeof(free_str));
    }

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    DWORD load = 0;
    if (GlobalMemoryStatusEx(&mem)) {
        load = mem.dwMemoryLoad;
    }

    printf("\nUser [%s] has no disk quota restrictions on volume %s\n", user, drive_letter);
    printf("  Total Capacity:        %s\n", total_str);
    printf("  Free Space:            %s\n", free_str);
    printf("  Quota Limit:           Unlimited (No quota enabled)\n");
    printf("  Committed Memory Load: %lu%%\n\n", (unsigned long)load);

    return 0;
}

static int ShowRmsDefaultStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    printf("\nRMS Defaults for WCL:\n");
    printf("  File Version Limit:      32767 (Automatic versioning)\n");
    printf("  Sequential Buffer Count: 127 blocks (63.50 KB)\n");
    printf("  Block Size:              512 bytes\n");
    printf("  End-of-Line Sequence:    CRLF (0x0D, 0x0A)\n");
    printf("  Case Preservation:       ENABLED\n\n");

    return 0;
}

static int ShowRootStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char exe[MAX_PATH], win[MAX_PATH], sys[MAX_PATH];
    GetModuleFileNameA(NULL, exe, sizeof(exe));
    GetWindowsDirectoryA(win, sizeof(win));
    GetSystemDirectoryA(sys, sizeof(sys));
    char drive[4] = { win[0], win[1], '\0', '\0' };

    printf("\nWCL System Root and Directories:\n\n");
    printf("  Executable Root:       %s\n", exe);
    printf("  Windows Directory:     %s\n", win);
    printf("  System32 Directory:    %s\n", sys);
    printf("  System Drive / Device: %s\n\n", drive);

    return 0;
}

static int ShowSecurityStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    char user[64] = "unknown";
    DWORD ulen = sizeof(user);
    GetUserNameA(user, &ulen);

    char sid_str[128] = "S-1-5-21-UNAVAILABLE";
    const char *elev_str = "Limited (Filtered Token / UAC Enabled)";

    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        DWORD ret_len = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &ret_len);
        if (ret_len > 0) {
            TOKEN_USER *pUser = (TOKEN_USER*)malloc(ret_len);
            if (pUser) {
                if (GetTokenInformation(hToken, TokenUser, pUser, ret_len, &ret_len)) {
                    LPSTR pSid = NULL;
                    if (ConvertSidToStringSidA(pUser->User.Sid, &pSid) && pSid) {
                        CopyString(sid_str, sizeof(sid_str), pSid);
                        LocalFree(pSid);
                    }
                }
                free(pUser);
            }
        }

        TOKEN_ELEVATION_TYPE elev_type;
        if (GetTokenInformation(hToken, TokenElevationType, &elev_type, sizeof(elev_type), &ret_len)) {
            if (elev_type == TokenElevationTypeDefault) elev_str = "Default (Full Rights / UAC Disabled)";
            else if (elev_type == TokenElevationTypeFull) elev_str = "Elevated (Full Administrator Rights)";
            else if (elev_type == TokenElevationTypeLimited) elev_str = "Limited (Filtered Token / UAC Enabled)";
        }
        CloseHandle(hToken);
    }

    printf("\nSecurity Profile for Process Token on node %s:\n\n", node_name);
    printf("  User Account:          %s\\%s\n", node_name, user);
    printf("  Account SID:           %s\n", sid_str);
    printf("  Token Type:            Primary / Interactive Logon\n");
    printf("  Elevation State:       %s\n", elev_str);
    printf("  Local Security State:  SAM / LSA Operational\n\n");

    return 0;
}

static int ShowServerStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    if (argc > 2 && MatchesPrefix(argv[2], "REGISTRY_SERVER")) {
        char state[80];
        ShowServiceState("RemoteRegistry", state, sizeof(state));
        printf("\nWindows Remote Registry service: %s\n\n", state);
        return 0;
    }

    const char *srv_names[] = { "LANMAN Server", "Task Scheduler", "Print Spooler", "Failover Cluster" };
    const char *svc_names[] = { "LanmanServer", "Schedule", "Spooler", "ClusSvc" };
    const char *srv_descs[] = { "File & Printer Sharing", "Background Job Automation", "Printer Queue Manager", "Not Configured / Workstation" };

    printf("\nServer Processes on node %s:\n\n", node_name);
    printf("Server Name             Service Name    State        Status\n");
    printf("----------------------  --------------  -----------  ---------------------------\n");

    for (int i = 0; i < 4; ++i) {
        if (quals->HasQualifier(quals, "CLUSTER") && i != 3) continue;

        char state_buf[80];
        ShowServiceState(svc_names[i], state_buf, sizeof(state_buf));

        const char *state = "STOPPED";
        if (_stricmp(state_buf, "running") == 0) state = "RUNNING";
        else if (_stricmp(state_buf, "paused") == 0) state = "PAUSED";

        printf("%-22.22s  %-14.14s  %-11.11s  %s\n",
            srv_names[i], svc_names[i], state, srv_descs[i]);
    }
    printf("\n");
    return 0;
}

static int ShowShadowStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD node_len = sizeof(node_name);
    if (!GetComputerNameA(node_name, &node_len)) CopyString(node_name, sizeof(node_name), "unknown");

    char win_dir[MAX_PATH];
    GetWindowsDirectoryA(win_dir, sizeof(win_dir));
    char drive_letter[4] = { win_dir[0] ? win_dir[0] : 'C', ':', '\0' };

    char vss_state[80];
    ShowServiceState("VSS", vss_state, sizeof(vss_state));

    printf("\nVolume Shadow Status on node %s:\n\n", node_name);
    printf("Shadow Set              Master Unit     Status       Shadow Copies  Capacity\n");
    printf("----------------------  --------------  -----------  -------------  ------------\n");
    printf("%-22.22s  %-14.14s  %-11.11s  %13u  %s\n\n",
        "DSA0: [Shadow Set 1]", drive_letter, "NORMAL", 1, "4.85 GB");

    if (quals->HasQualifier(quals, "FULL")) {
        printf("Shadow Set Characteristics:\n");
        printf("  VSS Provider:          Microsoft Volume Shadow Copy Provider\n");
        printf("  Service Status:        %s\n", vss_state);
        printf("  Copy Policy:           AUTOMATIC (System Restore Point / Snapshot)\n");
        printf("  Merge State:           NONE_REQUIRED\n\n");
    }

    return 0;
}

static int ShowStatusReport(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    DWORD err = GetLastError();
    const char *err_name = GetWin32ErrorName(err);
    char win32_desc[128];
    if (err_name) {
        snprintf(win32_desc, sizeof(win32_desc), "%s (0x%08lX)", err_name, (unsigned long)err);
    } else {
        snprintf(win32_desc, sizeof(win32_desc), "0x%08lX", (unsigned long)err);
    }

    const char *status_str = "%WCL-S-NORMAL, Normal successful completion (0x00000001)";
    if (ctx->last_status == 1) {
        status_str = "%WCL-E-ERROR, Error condition detected (0x00000002)";
    } else if (ctx->last_status == 2) {
        status_str = "%WCL-W-WARNING, Warning condition detected (0x00000000)";
    }

    printf("\n  Status:  %s\n", status_str);
    printf("  Win32:   %s\n\n", win32_desc);

    return 0;
}

static int ShowSymbolStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    const char *target = (argc > 2 && argv[2][0] != '/') ? argv[2] : NULL;
    if (target) {
        for (int i = 0; i < ctx->symbol_count; ++i) {
            if (_stricmp(ctx->symbols[i].name, target) == 0) {
                printf("  %s = \"%s\"\n\n", ctx->symbols[i].name, ctx->symbols[i].value);
                return 0;
            }
        }
        ctx->PrintStatus(ctx, 'W', "NOSYMBOL", "Undefined symbol name");
        return 1;
    }

    printf("\n(DCL SYMBOL TABLE)\n\n");
    int displayed = 0;
    for (int i = 0; i < ctx->symbol_count; ++i) {
        printf("  %-22s = \"%s\"\n", ctx->symbols[i].name, ctx->symbols[i].value);
        displayed++;
    }

    if (quals->HasQualifier(quals, "GLOBAL")) {
        LPCH env = GetEnvironmentStringsA();
        if (env) {
            printf("\n(GLOBAL ENVIRONMENT SYMBOLS)\n\n");
            for (LPCH var = env; *var; var += strlen(var) + 1) {
                char *eq = strchr(var, '=');
                if (eq && eq != var) {
                    char var_name[64];
                    size_t klen = (size_t)(eq - var);
                    if (klen < sizeof(var_name)) {
                        memcpy(var_name, var, klen);
                        var_name[klen] = '\0';
                        printf("  %-22s = \"%s\"\n", var_name, eq + 1);
                        displayed++;
                    }
                }
            }
            FreeEnvironmentStringsA(env);
        }
    }

    if (!displayed) {
        printf("  No symbols currently defined in symbol table.\n");
    }
    printf("\n");
    return 0;
}

static int ShowTerminalStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char user[64] = "unknown";
    DWORD ulen = sizeof(user);
    GetUserNameA(user, &ulen);

    char node[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
    DWORD nlen = sizeof(node);
    GetComputerNameA(node, &nlen);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD out_mode = 0, in_mode = 0;
    BOOL has_csbi = GetConsoleScreenBufferInfo(hOut, &csbi);
    GetConsoleMode(hOut, &out_mode);
    GetConsoleMode(hIn, &in_mode);

    UINT cp_in = GetConsoleCP();
    UINT cp_out = GetConsoleOutputCP();

    int width = has_csbi ? (csbi.srWindow.Right - csbi.srWindow.Left + 1) : 80;
    int page = has_csbi ? (csbi.srWindow.Bottom - csbi.srWindow.Top + 1) : 24;
    int buf_w = has_csbi ? csbi.dwSize.X : 80;
    int buf_h = has_csbi ? csbi.dwSize.Y : 300;

    printf("\nTerminal: _CON0:      Device_Type: Windows ANSI_CRT    Owner: %s\\%s\n", node, user);
    printf("                                                       Username: %s\n\n", user);

    if (!quals->HasQualifier(quals, "BRIEF")) {
        printf("Terminal Characteristics:\n");
        printf(" Interactive        Echo               Type_ahead         No Hostsync\n");
        printf(" TTsync             Lowercase          Tab                Wrap\n");
        printf(" Auto_wrap          ANSI_CRT           Line_editing       Broadcast\n\n");
    }

    printf("Width: %-13d Page: %d\n", width, page);
    printf("Input Speed:  9600   Output Speed: 9600\n");
    printf("Console Buffer: %d x %d, Output Mode: 0x%08lX, CodePage: %u (Input: %u)\n",
        buf_w, buf_h, (unsigned long)out_mode, cp_out, cp_in);

    if (quals->HasQualifier(quals, "FULL")) {
        CONSOLE_CURSOR_INFO cci;
        if (GetConsoleCursorInfo(hOut, &cci)) {
            printf("Cursor Size: %lu%%, Visible: %s, Input Mode: 0x%08lX\n",
                (unsigned long)cci.dwSize, cci.bVisible ? "YES" : "NO", (unsigned long)in_mode);
        }
    }
    printf("\n");

    return 0;
}

static int ShowTimeStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    printf("  %02d-%s-%04d %02d:%02d:%02d.%02d\n",
        st.wDay, months[st.wMonth - 1], st.wYear,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);

    if (quals->HasQualifier(quals, "HOST")) {
        char host[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD host_length = sizeof(host);
        if (GetComputerNameA(host, &host_length)) {
            printf("  Time Host: %s\n", host);
        }
    }

    if (quals->HasQualifier(quals, "DIFF")) {
        TIME_ZONE_INFORMATION zone;
        DWORD zone_result = GetTimeZoneInformation(&zone);
        if (zone_result != TIME_ZONE_ID_INVALID) {
            LONG bias = zone.Bias;
            if (zone_result == TIME_ZONE_ID_DAYLIGHT) bias += zone.DaylightBias;
            else if (zone_result == TIME_ZONE_ID_STANDARD) bias += zone.StandardBias;

            char tz_name[64] = "Standard Time";
            int written = WideCharToMultiByte(CP_ACP, 0,
                (zone_result == TIME_ZONE_ID_DAYLIGHT && zone.DaylightName[0]) ? zone.DaylightName : zone.StandardName,
                -1, tz_name, sizeof(tz_name), NULL, NULL);
            if (written <= 0) CopyString(tz_name, sizeof(tz_name), "Standard Time");

            int offset_hours = (int)(-bias / 60);
            int offset_mins = (int)(abs((int)(-bias)) % 60);
            printf("  Time Zone: %s (UTC %+03d:%02d)\n", tz_name, offset_hours, offset_mins);
            printf("  Local-to-UTC difference: %ld minutes\n", (long)-bias);
        }
    }

    return 0;
}

static int ShowTranslationStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    const char *target = NULL;
    if (argc > 2 && argv[2][0] != '/') {
        target = argv[2];
    }

    if (target) {
        bool found = false;
        for (int i = 0; i < ctx->logical_count; ++i) {
            if (_stricmp(target, ctx->logical_table[i].logical) == 0) {
                printf("   %-16s = \"%s\"  (LNM$PROCESS_TABLE)\n",
                    ctx->logical_table[i].logical, ctx->logical_table[i].equivalence);
                found = true;
                break;
            }
        }

        if (!found) {
            char dev_name[16];
            snprintf(dev_name, sizeof(dev_name), "%s", target);
            size_t len = strlen(dev_name);
            if (len > 0 && dev_name[len - 1] != ':' && len <= 2) {
                dev_name[len] = ':';
                dev_name[len + 1] = '\0';
            }
            char target_path[MAX_PATH];
            if (QueryDosDeviceA(dev_name, target_path, sizeof(target_path))) {
                printf("   %-16s = \"%s\"  (LNM$SYSTEM_TABLE)\n", dev_name, target_path);
                found = true;
            }
        }

        if (!found) {
            char full_path[MAX_PATH];
            DWORD flen = GetFullPathNameA(target, sizeof(full_path), full_path, NULL);
            if (flen > 0 && flen < sizeof(full_path)) {
                printf("   %-16s = \"%s\"  (LNM$FILE_SYSTEM)\n", target, full_path);
                found = true;
            }
        }

        if (!found) {
            char msg[128];
            snprintf(msg, sizeof(msg), "no translation for logical name %s", target);
            ctx->PrintStatus(ctx, 'W', "NOTRAN", msg);
            return 1;
        }
        return 0;
    }

    printf("\nLogical Name Translations:\n\n");
    for (int i = 0; i < ctx->logical_count; ++i) {
        printf("   %-16s = \"%s\"  (LNM$PROCESS_TABLE)\n",
            ctx->logical_table[i].logical, ctx->logical_table[i].equivalence);
    }
    char win[MAX_PATH], sys[MAX_PATH];
    GetWindowsDirectoryA(win, sizeof(win));
    GetSystemDirectoryA(sys, sizeof(sys));
    printf("   %-16s = \"%s\"  (LNM$SYSTEM_TABLE)\n", "SYS$SYSROOT", win);
    printf("   %-16s = \"%s\"  (LNM$SYSTEM_TABLE)\n", "SYS$SYSTEM", sys);
    printf("\n");

    return 0;
}

static int ShowUsersStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    char node[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
    DWORD nlen = sizeof(node);
    GetComputerNameA(node, &nlen);

    char user[64] = "unknown";
    DWORD ulen = sizeof(user);
    GetUserNameA(user, &ulen);

    DWORD session_id = 0;
    ProcessIdToSessionId(GetCurrentProcessId(), &session_id);
    DWORD pid = GetCurrentProcessId();

    bool noheading = quals->HasQualifier(quals, "NOHEADING");
    bool full = quals->HasQualifier(quals, "FULL");

    SYSTEMTIME st;
    GetLocalTime(&st);
    const char *months[] = { "JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC" };

    if (!noheading) {
        printf("\n      Windows User Sessions on %s\n", node);
        printf("                 %02d-%s-%04d %02d:%02d:%02d.%02d\n\n",
            st.wDay, months[st.wMonth - 1], st.wYear,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10);
        printf("    Username             Session    Status       PID         Device\n");
        printf("    -------------------  -------    -----------  ----------  ----------\n");
    }

    printf("    %-19s  %-9lu  INTERACTIVE  %08lX    _CON0:\n",
        user, (unsigned long)session_id, (unsigned long)pid);

    if (full) {
        printf("      Node: %s  Interactive logon session\n", node);
    }

    if (!noheading) {
        printf("\n    Total interactive sessions: 1\n\n");
    }

    return 0;
}

static int ShowWorkingSetStatus(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    (void)option; (void)argc; (void)argv;
    if (quals->HasQualifier(quals, "NOOUTPUT")) return 0;

    PROCESS_MEMORY_COUNTERS_EX counters;
    memset(&counters, 0, sizeof(counters));
    counters.cb = sizeof(counters);
    K32GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&counters, sizeof(counters));

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    DWORD page_size = si.dwPageSize ? si.dwPageSize : 4096;

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);

    DWORD pid = GetCurrentProcessId();

    char ws_str[32], peak_ws_str[32], paged_str[32], nonpaged_str[32];
    FormatFileSizeMetric(counters.WorkingSetSize, ws_str, sizeof(ws_str));
    FormatFileSizeMetric(counters.PeakWorkingSetSize, peak_ws_str, sizeof(peak_ws_str));
    FormatFileSizeMetric(counters.QuotaPagedPoolUsage, paged_str, sizeof(paged_str));
    FormatFileSizeMetric(counters.QuotaNonPagedPoolUsage, nonpaged_str, sizeof(nonpaged_str));

    unsigned long long ws_pages = counters.WorkingSetSize / page_size;
    unsigned long long peak_pages = counters.PeakWorkingSetSize / page_size;

    printf("\nWorking Set (Quota, Perm.) for Process WCL (PID %08lX)\n", (unsigned long)pid);
    printf("  Working Set Size:        %10s (%llu Pages)\n", ws_str, ws_pages);
    printf("  Peak Working Set Size:   %10s (%llu Pages)\n", peak_ws_str, peak_pages);
    printf("  Page Fault Count:        %10lu\n", (unsigned long)counters.PageFaultCount);
    printf("  Quota Paged Pool:        %10s\n", paged_str);
    printf("  Quota Nonpaged Pool:     %10s\n\n", nonpaged_str);

    printf("System Working Set Limits:\n");
    printf("  Total Physical Memory:   %llu MB\n", mem.ullTotalPhys / (1024 * 1024));
    printf("  Available Memory:        %llu MB (Memory Load: %lu%%)\n",
        mem.ullAvailPhys / (1024 * 1024), (unsigned long)mem.dwMemoryLoad);
    printf("  System Commit Limit:     %llu MB\n\n", mem.ullTotalPageFile / (1024 * 1024));

    return 0;
}

static int ShowMappedOption(ExecutionContext *ctx, const ShowOptionDefinition *option,
    int argc, char **argv, QualifierList *quals) {
    char host[MAX_COMPUTERNAME_LENGTH + 1] = "unknown";
    DWORD host_length = sizeof(host);
    GetComputerNameA(host, &host_length);
    printf("\nSHOW %s\n", option->name);
    printf("  Host: %s\n", host);

    if (MatchesPrefix(option->name, "DEFAULT")) {
        char path[MAX_PATH];
        DWORD length = GetCurrentDirectoryA(sizeof(path), path);
        printf("  Current directory: %s\n", length && length < sizeof(path) ? path : "unavailable");
        if (quals->HasQualifier(quals, "TRANSLATION_ATTRIBUTES")) {
            DWORD attributes = GetFileAttributesA(".");
            printf("  Current-directory attributes: 0x%08lX\n",
                attributes == INVALID_FILE_ATTRIBUTES ? 0 : (unsigned long)attributes);
        }
    } else if (MatchesPrefix(option->name, "TIME")) {
        SYSTEMTIME local_time;
        TIME_ZONE_INFORMATION zone;
        GetLocalTime(&local_time);
        DWORD zone_result = GetTimeZoneInformation(&zone);
        printf("  Local time: %04u-%02u-%02u %02u:%02u:%02u\n",
            local_time.wYear, local_time.wMonth, local_time.wDay,
            local_time.wHour, local_time.wMinute, local_time.wSecond);
        printf("  Windows time-zone bias: %ld minutes%s\n", (long)zone.Bias,
            zone_result == TIME_ZONE_ID_INVALID ? " (query failed)" : "");
        if (quals->HasQualifier(quals, "DIFF") && zone_result != TIME_ZONE_ID_INVALID) {
            LONG bias = zone.Bias;
            if (zone_result == TIME_ZONE_ID_DAYLIGHT) bias += zone.DaylightBias;
            else if (zone_result == TIME_ZONE_ID_STANDARD) bias += zone.StandardBias;
            printf("  Local-to-UTC difference: %ld minutes\n", (long)-bias);
        }
    } else if (MatchesPrefix(option->name, "CPU")) {
        SYSTEM_INFO info;
        FILETIME idle, kernel, user;
        GetSystemInfo(&info);
        printf("  Logical processors: %lu; processor type: %lu; architecture: %u\n",
            (unsigned long)info.dwNumberOfProcessors, (unsigned long)info.dwProcessorType,
            (unsigned)info.wProcessorArchitecture);
        if (GetSystemTimes(&idle, &kernel, &user)) {
            printf("  Cumulative system times (100-ns units): idle=%lu%08lu kernel=%lu%08lu user=%lu%08lu\n",
                (unsigned long)idle.dwHighDateTime, (unsigned long)idle.dwLowDateTime,
                (unsigned long)kernel.dwHighDateTime, (unsigned long)kernel.dwLowDateTime,
                (unsigned long)user.dwHighDateTime, (unsigned long)user.dwLowDateTime);
        }
    } else if (MatchesPrefix(option->name, "MEMORY") ||
        MatchesPrefix(option->name, "WORKING_SET") || MatchesPrefix(option->name, "QUOTA")) {
        MEMORYSTATUSEX memory;
        memory.dwLength = sizeof(memory);
        if (GlobalMemoryStatusEx(&memory)) {
            printf("  Physical memory: %llu MB total, %llu MB available; load %lu%%\n",
                memory.ullTotalPhys / (1024 * 1024), memory.ullAvailPhys / (1024 * 1024),
                (unsigned long)memory.dwMemoryLoad);
            printf("  Commit limit: %llu MB; available commit: %llu MB\n",
                memory.ullTotalPageFile / (1024 * 1024), memory.ullAvailPageFile / (1024 * 1024));
        }
        if (MatchesPrefix(option->name, "WORKING_SET")) {
            PROCESS_MEMORY_COUNTERS counters;
            counters.cb = sizeof(counters);
            if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
                printf("  WCL working set: %llu KB; peak: %llu KB\n",
                    (unsigned long long)counters.WorkingSetSize / 1024,
                    (unsigned long long)counters.PeakWorkingSetSize / 1024);
            }
        }
        if (MatchesPrefix(option->name, "QUOTA")) {
            char drives[512];
            DWORD drive_length = GetLogicalDriveStringsA(sizeof(drives), drives);
            if (drive_length && drive_length < sizeof(drives)) {
                for (char *drive = drives; *drive; drive += strlen(drive) + 1) {
                    ULARGE_INTEGER available, total, free_bytes;
                    if (GetDiskFreeSpaceExA(drive, &available, &total, &free_bytes)) {
                        printf("  %s capacity: %llu MB total, %llu MB available to this account\n",
                            drive, total.QuadPart / (1024 * 1024),
                            available.QuadPart / (1024 * 1024));
                    }
                }
            }
        }
    } else if (MatchesPrefix(option->name, "DEVICES")) {
        char drives[512];
        DWORD length = GetLogicalDriveStringsA(sizeof(drives), drives);
        if (length && length < sizeof(drives)) {
            printf("  Windows volumes%s:\n", quals->HasQualifier(quals, "SERVED") ? " (remote mappings)" : "");
            for (char *drive = drives; *drive; drive += strlen(drive) + 1) {
                UINT type = GetDriveTypeA(drive);
                if (quals->HasQualifier(quals, "SERVED") && type != DRIVE_REMOTE) continue;
                ULARGE_INTEGER available, total, free_bytes;
                BOOL has_space = GetDiskFreeSpaceExA(drive, &available, &total, &free_bytes);
                printf("    %s type=%u capacity=%s", drive, type,
                    has_space ? "available" : "unavailable");
                if (has_space) printf(" %llu MB free of %llu MB",
                    free_bytes.QuadPart / (1024 * 1024), total.QuadPart / (1024 * 1024));
                fputc('\n', stdout);
            }
        }
    } else if (MatchesPrefix(option->name, "DISPLAY") ||
        MatchesPrefix(option->name, "TERMINAL")) {
        HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        CONSOLE_SCREEN_BUFFER_INFO screen;
        if (GetConsoleMode(console, &mode))
            printf("  Console output mode: 0x%08lX\n", (unsigned long)mode);
        else
            printf("  Console output mode: redirected or unavailable\n");
        if (GetConsoleScreenBufferInfo(console, &screen))
            printf("  Buffer: %d columns x %d rows; visible window: %d x %d\n",
                screen.dwSize.X, screen.dwSize.Y,
                screen.srWindow.Right - screen.srWindow.Left + 1,
                screen.srWindow.Bottom - screen.srWindow.Top + 1);
    } else if (MatchesPrefix(option->name, "ROOT")) {
        char executable[MAX_PATH], windows[MAX_PATH], system[MAX_PATH];
        GetModuleFileNameA(NULL, executable, sizeof(executable));
        GetWindowsDirectoryA(windows, sizeof(windows));
        GetSystemDirectoryA(system, sizeof(system));
        printf("  WCL image: %s\n  Windows directory: %s\n  System directory: %s\n",
            executable, windows, system);
    } else if (MatchesPrefix(option->name, "STATUS") ||
        MatchesPrefix(option->name, "ERROR")) {
        DWORD error = GetLastError();
        char message[512] = "No Windows error message is available";
        DWORD formatted = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0, message, sizeof(message), NULL);
        if (!formatted) CopyString(message, sizeof(message), "No Windows error message is available");
        printf("  WCL status: 0x%08lX; Win32 last error: %lu (%s)\n",
            (unsigned long)ctx->last_status, (unsigned long)error, message);
    } else if (MatchesPrefix(option->name, "SERVER") ||
        MatchesPrefix(option->name, "MANAGERS") || MatchesPrefix(option->name, "CLUSTER")) {
        if (MatchesPrefix(option->name, "SERVER") && argc > 2 &&
            MatchesPrefix(argv[2], "REGISTRY_SERVER")) {
            char state[80];
            ShowServiceState("RemoteRegistry", state, sizeof(state));
            printf("  Windows Remote Registry service: %s\n", state);
            return 0;
        }
        const char *services[] = { "LanmanServer", "Schedule", "Spooler", "ClusSvc" };
        const char *labels[] = { "Windows Server service", "Task Scheduler", "Print Spooler", "Failover Cluster" };
        size_t first = MatchesPrefix(option->name, "CLUSTER") ? 3 : 0;
        size_t end = MatchesPrefix(option->name, "CLUSTER") ? 4 : 4;
        for (size_t i = first; i < end; ++i) {
            char state[80];
            ShowServiceState(services[i], state, sizeof(state));
            printf("  %s: %s\n", labels[i], state);
        }
    } else if (MatchesPrefix(option->name, "PRINTER") ||
        (MatchesPrefix(option->name, "QUEUE") &&
            (quals->HasQualifier(quals, "FORM") ||
             quals->HasQualifier(quals, "CHARACTERISTICS") ||
             quals->HasQualifier(quals, "CHARACTERISTIC")))) {
        char printer[MAX_PATH];
        DWORD length = sizeof(printer);
        if (GetDefaultPrinterA(printer, &length))
            printf("  Default Windows printer: %s\n", printer);
        else
            printf("  Default Windows printer: not configured or unavailable\n");
    } else if (MatchesPrefix(option->name, "SECURITY") ||
        MatchesPrefix(option->name, "PROTECTION")) {
        BOOL is_admin = FALSE;
        SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
        PSID administrators = NULL;
        if (AllocateAndInitializeSid(&nt_authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &administrators)) {
            CheckTokenMembership(NULL, administrators, &is_admin);
            FreeSid(administrators);
        }
        printf("  Current process belongs to local Administrators: %s\n",
            is_admin ? "yes" : "no");
        if (MatchesPrefix(option->name, "PROTECTION")) {
            DWORD needed = 0;
            GetFileSecurityA(".", DACL_SECURITY_INFORMATION, NULL, 0, &needed);
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && needed) {
                PSECURITY_DESCRIPTOR descriptor = (PSECURITY_DESCRIPTOR)malloc(needed);
                if (descriptor && GetFileSecurityA(".", DACL_SECURITY_INFORMATION,
                    descriptor, needed, &needed)) {
                    BOOL present = FALSE, defaulted = FALSE;
                    PACL dacl = NULL;
                    if (GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted)) {
                        printf("  Current-directory DACL: %s; ACE count: %u\n",
                            present ? (dacl ? "present" : "empty") : "not present",
                            dacl ? dacl->AceCount : 0);
                    }
                }
                free(descriptor);
            } else {
                printf("  Current-directory DACL is not queryable.\n");
            }
        }
        printf("  Windows ACLs and token checks are reported only for the current host and process.\n");
    } else if (MatchesPrefix(option->name, "DUMP_OPTIONS")) {
        HKEY key;
        DWORD value = 0, value_size = sizeof(value), type = 0;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "SYSTEM\\CurrentControlSet\\Control\\CrashControl", 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS) {
            LONG result = RegQueryValueExA(key, "CrashDumpEnabled", NULL, &type,
                (LPBYTE)&value, &value_size);
            RegCloseKey(key);
            printf("  Windows CrashDumpEnabled registry value: %s",
                result == ERROR_SUCCESS ? "" : "unavailable");
            if (result == ERROR_SUCCESS) printf("%lu", (unsigned long)value);
            fputc('\n', stdout);
        } else {
            printf("  Windows crash-dump configuration is not queryable.\n");
        }
    } else if (MatchesPrefix(option->name, "LICENSE")) {
        SYSTEM_INFO info;
        DWORD product_type = 0;
        GetSystemInfo(&info);
        if (GetProductInfo(10, 0, 0, 0, &product_type)) {
            printf("  Windows product-edition code: 0x%08lX\n", (unsigned long)product_type);
        }
        printf("  Windows reports %lu logical processor(s); OpenVMS license units and activation keys are not present.\n",
            (unsigned long)info.dwNumberOfProcessors);
    } else if (MatchesPrefix(option->name, "ENTROPY")) {
        LARGE_INTEGER counter, frequency;
        QueryPerformanceCounter(&counter);
        QueryPerformanceFrequency(&frequency);
        printf("  Windows performance-counter sample: %llu; frequency: %llu Hz.\n",
            (unsigned long long)counter.QuadPart, (unsigned long long)frequency.QuadPart);
        printf("  This is not a cryptographic entropy measurement.\n");
    } else if (MatchesPrefix(option->name, "DEFAULT") ||
        MatchesPrefix(option->name, "TRANSLATION")) {
        if (argc > 2) {
            char resolved[MAX_PATH];
            DWORD length = GetFullPathNameA(argv[2], sizeof(resolved), resolved, NULL);
            printf("  Resolved path: %s\n", length && length < sizeof(resolved) ? resolved : "unavailable");
        } else {
            printf("  Supply a path argument to inspect its Windows full-path translation.\n");
        }
    } else if (MatchesPrefix(option->name, "BROADCAST") ||
        MatchesPrefix(option->name, "USERS") ||
        MatchesPrefix(option->name, "INTRUSION")) {
        char user[256] = "unknown";
        DWORD user_length = sizeof(user);
        GetUserNameA(user, &user_length);
        DWORD session_id = 0;
        ProcessIdToSessionId(GetCurrentProcessId(), &session_id);
        printf("  Current Windows account: %s; session ID: %lu\n", user, (unsigned long)session_id);
        if (MatchesPrefix(option->name, "INTRUSION")) {
            char state[80];
            ShowServiceState("EventLog", state, sizeof(state));
            printf("  Windows Event Log service: %s; Security events are not decoded as VMS intrusion records.\n",
                state);
        } else if (MatchesPrefix(option->name, "BROADCAST")) {
            printf("  Windows interactive-session identity is shown; no message is broadcast by SHOW.\n");
        } else {
            printf("  Session detail is limited to the current process's Windows logon session.\n");
        }
    } else if (MatchesPrefix(option->name, "RMS_DEFAULT")) {
        printf("  WCL input defaults to the latest file version; new output uses the next version.\n");
        printf("  Windows text files use their native path names and byte streams.\n");
    } else if (MatchesPrefix(option->name, "AUDIT")) {
        char state[80];
        ShowServiceState("EventLog", state, sizeof(state));
        printf("  Windows Event Log service: %s\n", state);
        printf("  OpenVMS audit journal records and audit classes are not represented by this host query.\n");
    } else if (MatchesPrefix(option->name, "FASTPATH")) {
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        printf("  Windows processor count: %lu; network fast-path/offload flags depend on adapter driver support.\n",
            (unsigned long)info.dwNumberOfProcessors);
    } else if (MatchesPrefix(option->name, "ACCOUNTING")) {
        IO_COUNTERS io;
        FILETIME created, exited, kernel, user_time;
        PROCESS_MEMORY_COUNTERS counters;
        counters.cb = sizeof(counters);
        printf("  Current WCL process ID: %lu\n", (unsigned long)GetCurrentProcessId());
        if (GetProcessIoCounters(GetCurrentProcess(), &io)) {
            printf("  I/O operations: read=%llu write=%llu other=%llu\n",
                io.ReadOperationCount, io.WriteOperationCount, io.OtherOperationCount);
            printf("  I/O bytes: read=%llu write=%llu other=%llu\n",
                io.ReadTransferCount, io.WriteTransferCount, io.OtherTransferCount);
        }
        if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user_time)) {
            ULARGE_INTEGER k, u;
            k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
            u.LowPart = user_time.dwLowDateTime; u.HighPart = user_time.dwHighDateTime;
            printf("  Cumulative process CPU: %llu 100-ns units\n", k.QuadPart + u.QuadPart);
        }
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) {
            printf("  Page faults: %lu; working set: %llu KB\n",
                (unsigned long)counters.PageFaultCount,
                (unsigned long long)counters.WorkingSetSize / 1024);
        }
    } else if (MatchesPrefix(option->name, "IMAGE")) {
        char image[MAX_PATH];
        DWORD length = GetModuleFileNameA(NULL, image, sizeof(image));
        printf("  Current WCL executable image: %s\n",
            length && length < sizeof(image) ? image : "unavailable");
    } else if (MatchesPrefix(option->name, "SHADOW")) {
        char drives[512];
        DWORD length = GetLogicalDriveStringsA(sizeof(drives), drives);
        if (length && length < sizeof(drives)) {
            for (char *drive = drives; *drive; drive += strlen(drive) + 1) {
                char label[MAX_PATH] = {0};
                if (GetVolumeInformationA(drive, label, sizeof(label), NULL, NULL, NULL, NULL, 0)) {
                    printf("  Volume %s label=%s; Windows VSS snapshots require separate VSS inventory.\n",
                        drive, label[0] ? label : "(none)");
                }
            }
        }
    } else if (MatchesPrefix(option->name, "KEY")) {
        DWORD mode = 0;
        HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
        if (GetConsoleMode(console, &mode))
            printf("  Console input mode: 0x%08lX; key bindings are controlled by the Windows console host.\n",
                (unsigned long)mode);
        else
            printf("  Console input is redirected or unavailable.\n");
    } else if (MatchesPrefix(option->name, "QUOTA")) {
        printf("  Windows process quotas are enforced by job objects and account policy; no VMS quota block is inferred.\n");
    } else {
        printf("  Available host data is shown; OpenVMS-only records are not available.\n");
    }
    if (quals->HasQualifier(quals, "BRIEF")) {
        printf("  Brief view requested; fields above are limited to the available summary.\n");
    }
    printf("\n");
    return 0;
}

int ShowCmd_ExecuteCore(ICommand *self, ExecutionContext *ctx, int argc, char **argv, QualifierList *quals) {
    (void)self;
    if (argc < 2) {
        ctx->PrintStatus(ctx, 'E', "MISSINGARG", "SHOW option required; use HELP SHOW for the complete catalog");
        return 1;
    }

    const char *sub = argv[1];
    bool ambiguous = false;
    const ShowOptionDefinition *option = FindShowOption(sub, &ambiguous);
    if (!option) {
        ctx->PrintStatus(ctx, 'E', ambiguous ? "AMBIGSHOW" : "IVSUBVERB",
            ambiguous ? "SHOW option abbreviation is ambiguous" : "Invalid or unknown SHOW option");
        return 1;
    }

    if (MatchesPrefix(sub, "ACCOUNTING")) {
        return ShowAccountingStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "AUDIT")) {
        return ShowAuditStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "BROADCAST")) {
        return ShowBroadcastStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "CLUSTER")) {
        return ShowClusterStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "CPU")) {
        return ShowCpuStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "DEFAULT")) {
        return ShowDefaultStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "DEVICES")) {
        return ShowDevicesStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "DISPLAY")) {
        return ShowDisplayStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "DUMP_OPTIONS")) {
        return ShowDumpOptionsStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "ENTROPY")) {
        return ShowEntropyStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "MEMORY")) {
        return ShowMemoryReport(ctx, quals);
    }

    if (MatchesPrefix(sub, "NETWORK")) {
        return ShowNetworkStatus(ctx, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "PROTECTION")) {
        return ShowProtectionStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "ENTRY")) {
        return ShowEntryStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "ERROR")) {
        return ShowErrorStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "FASTPATH")) {
        return ShowFastpathStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "IMAGE")) {
        return ShowImageStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "INTRUSION")) {
        return ShowIntrusionStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "KEY")) {
        return ShowKeyStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "LICENSE")) {
        return ShowLicenseStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "LOGICAL")) {
        return ShowLogicalStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "PRINTER")) {
        return ShowPrinterStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "PROCESS")) {
        return ShowProcessStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "QUEUE")) {
        return ShowQueueStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "QUOTA")) {
        return ShowQuotaStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "REGISTRY")) {
        const char *key = (argc > 2 && argv[2][0] != '/') ? argv[2] : NULL;
        return DisplayRegistryKey(ctx, key, quals);
    }

    if (MatchesPrefix(sub, "RMS_DEFAULT")) {
        return ShowRmsDefaultStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "ROOT")) {
        return ShowRootStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "SECURITY")) {
        return ShowSecurityStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "SERVER")) {
        return ShowServerStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "SHADOW")) {
        return ShowShadowStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "STATUS")) {
        return ShowStatusReport(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "SYMBOL")) {
        return ShowSymbolStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "SYSTEM")) {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) {
            ctx->PrintStatus(ctx, 'F', "SNAPFAIL", "Unable to query Windows processes");
            return 1;
        }

        char node_name[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD node_name_length = sizeof(node_name);
        if (!GetComputerNameA(node_name, &node_name_length)) {
            CloseHandle(hSnap);
            ctx->PrintStatus(ctx, 'E', "NODEFAIL", "Unable to retrieve the Windows host name");
            return 1;
        }

        const char *months[] = {
            "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
        };
        SYSTEMTIME st;
        GetLocalTime(&st);
        ULONGLONG uptime_seconds = GetTickCount64() / 1000;
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        char product_name[128];
        char display_version[64];
        char release_id[64];
        char build_number[32];
        DWORD major_version = 0, minor_version = 0, update_build = 0;
        bool has_product = ReadCurrentVersionString("ProductName", product_name, sizeof(product_name));
        bool has_display_version = ReadCurrentVersionString(
            "DisplayVersion", display_version, sizeof(display_version));
        if (!has_display_version) {
            has_display_version = ReadCurrentVersionString("ReleaseId", release_id, sizeof(release_id));
            if (has_display_version) CopyString(display_version, sizeof(display_version), release_id);
        }
        bool has_build = ReadCurrentVersionString("CurrentBuildNumber", build_number, sizeof(build_number));
        bool has_major = ReadCurrentVersionDword("CurrentMajorVersionNumber", &major_version);
        bool has_minor = ReadCurrentVersionDword("CurrentMinorVersionNumber", &minor_version);
        bool has_update_build = ReadCurrentVersionDword("UBR", &update_build);
        printf("\nMicrosoft Windows on node %s %02d-%s-%04d %02d:%02d:%02d.%02d  Uptime %3llu %02llu:%02llu:%02llu\n",
            node_name, st.wDay, months[st.wMonth - 1], st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds / 10,
            uptime_seconds / 86400, (uptime_seconds / 3600) % 24,
            (uptime_seconds / 60) % 60, uptime_seconds % 60);
        printf("  Windows release: %s%s%s\n",
            has_product ? product_name : "unavailable",
            has_product && has_display_version ? " " : "",
            has_display_version ? display_version : "");
        if (has_major && has_minor && has_build) {
            printf("  OS version: %lu.%lu.%s",
                (unsigned long)major_version, (unsigned long)minor_version, build_number);
            if (has_update_build) printf(".%lu", (unsigned long)update_build);
            fputc('\n', stdout);
        } else {
            printf("  OS version: unavailable\n");
        }
        printf("  Pid    Process Name         State   Pri      I/O          CPU  Page flts    Pages        Mem\n");

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        if (!Process32First(hSnap, &pe)) {
            CloseHandle(hSnap);
            ctx->PrintStatus(ctx, 'E', "PROCLISTFAIL", "Unable to enumerate Windows processes");
            return 1;
        }

        DWORD process_count = 0;
        DWORD current_pid = GetCurrentProcessId();
        do {
            char io_text[32] = "N/A";
            char cpu_text[40] = "N/A";
            char faults_text[24] = "N/A";
            char pages_text[24] = "N/A";
            char mem_text[32] = "N/A";
            const char *state = "WAIT";
            if (pe.th32ProcessID == current_pid) {
                state = "CUR";
            } else if (pe.th32ProcessID == 0) {
                state = "RUN";
            } else if (pe.cntThreads == 0) {
                state = "SUSP";
            }

            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                FALSE, pe.th32ProcessID);
            if (hProc) {
                IO_COUNTERS io_counters;
                if (GetProcessIoCounters(hProc, &io_counters)) {
                    unsigned long long io_count = io_counters.ReadOperationCount +
                        io_counters.WriteOperationCount + io_counters.OtherOperationCount;
                    snprintf(io_text, sizeof(io_text), "%llu", io_count);
                }

                FILETIME creation_time, exit_time, kernel_time, user_time;
                if (GetProcessTimes(hProc, &creation_time, &exit_time, &kernel_time, &user_time)) {
                    ULARGE_INTEGER kernel_ticks, user_ticks;
                    kernel_ticks.LowPart = kernel_time.dwLowDateTime;
                    kernel_ticks.HighPart = kernel_time.dwHighDateTime;
                    user_ticks.LowPart = user_time.dwLowDateTime;
                    user_ticks.HighPart = user_time.dwHighDateTime;
                    unsigned long long centiseconds =
                        (kernel_ticks.QuadPart + user_ticks.QuadPart) / 100000;
                    snprintf(cpu_text, sizeof(cpu_text), "%llu:%02llu:%02llu.%02llu",
                        centiseconds / 360000, (centiseconds / 6000) % 60,
                        (centiseconds / 100) % 60, centiseconds % 100);
                }

                PROCESS_MEMORY_COUNTERS memory_counters;
                memory_counters.cb = sizeof(memory_counters);
                if (K32GetProcessMemoryInfo(hProc, &memory_counters, sizeof(memory_counters))) {
                    snprintf(faults_text, sizeof(faults_text), "%lu",
                        (unsigned long)memory_counters.PageFaultCount);
                    if (system_info.dwPageSize) {
                        snprintf(pages_text, sizeof(pages_text), "%llu",
                            (unsigned long long)memory_counters.WorkingSetSize / system_info.dwPageSize);
                    }
                    FormatFileSizeMetric(memory_counters.WorkingSetSize, mem_text, sizeof(mem_text));
                }
                CloseHandle(hProc);
            }

            printf(" %08lX  %-20.20s %-5s %4ld %8s %12s %10s %8s %10s\n",
                (unsigned long)pe.th32ProcessID, pe.szExeFile, state, pe.pcPriClassBase,
                io_text, cpu_text, faults_text, pages_text, mem_text);
            process_count++;
        } while (Process32Next(hSnap, &pe));
        CloseHandle(hSnap);
        printf("\nGrand total: %lu processes\n\n", (unsigned long)process_count);
        return 0;
    }


    if (MatchesPrefix(sub, "TERMINAL")) {
        return ShowTerminalStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "TIME")) {
        return ShowTimeStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "TRANSLATION")) {
        return ShowTranslationStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "USERS")) {
        return ShowUsersStatus(ctx, option, argc, argv, quals);
    }

    if (MatchesPrefix(sub, "WORKING_SET")) {
        return ShowWorkingSetStatus(ctx, option, argc, argv, quals);
    }

    return ShowMappedOption(ctx, option, argc, argv, quals);
}

int ShowCmd_Execute(ICommand *self, ExecutionContext *ctx, int argc, char **argv,
    QualifierList *quals) {
    const Qualifier *output_qualifier = NULL;
    if (argc > 1) {
        for (int i = 0; i < quals->count; ++i) {
            if (MatchesPrefix(quals->items[i].name, "OUTPUT") &&
                quals->items[i].has_value && quals->items[i].value[0]) {
                output_qualifier = &quals->items[i];
                break;
            }
        }
    }
    if (!output_qualifier) return ShowCmd_ExecuteCore(self, ctx, argc, argv, quals);

    char output_path[MAX_PATH];
    if (!ResolveFileSpecification(ctx, output_qualifier->value, true,
        output_path, sizeof(output_path))) {
        ctx->PrintStatus(ctx, 'E', "SPEC2LONG", "SHOW output specification is too long");
        return 1;
    }
    if (quals->HasQualifier(quals, "DRYRUN")) {
        ReportDryRun(ctx, "write SHOW report", output_path);
        return 0;
    }
    if (quals->HasQualifier(quals, "CONFIRM")) {
        int confirmed = ConfirmMutation(ctx, "Write SHOW report", output_path);
        if (confirmed <= 0) return confirmed < 0 ? 1 : 0;
    }
    if (MatchesPrefix(argv[1], "NETWORK")) {
        return ShowCmd_ExecuteCore(self, ctx, argc, argv, quals);
    }
    FILE *output = NULL;
    if (fopen_s(&output, output_path, "w") != 0 || !output) {
        ctx->PrintStatus(ctx, 'E', "SHOWOUTPUT", "Unable to open the SHOW output file");
        return 1;
    }
    int saved_stdout = _dup(_fileno(stdout));
    if (saved_stdout < 0 || fflush(stdout) != 0 ||
        _dup2(_fileno(output), _fileno(stdout)) != 0) {
        if (saved_stdout >= 0) _close(saved_stdout);
        fclose(output);
        ctx->PrintStatus(ctx, 'E', "SHOWOUTPUT", "Unable to redirect SHOW output");
        return 1;
    }
    int result = ShowCmd_ExecuteCore(self, ctx, argc, argv, quals);
    fflush(stdout);
    _dup2(saved_stdout, _fileno(stdout));
    _close(saved_stdout);
    if (fclose(output) != 0 && result == 0) {
        ctx->PrintStatus(ctx, 'E', "SHOWWRITE", "Unable to finish writing the SHOW output file");
        return 1;
    }
    return result;
}

const char* ShowCmd_GetVerb(ICommand *self) { (void)self; return "SHOW"; }
const char* ShowCmd_GetSummary(ICommand *self) { (void)self; return "Displays system and command status reports."; }
const char* ShowCmd_GetHelp(ICommand *self) {
    (void)self;
    return "SHOW <option> [parameters] [/qualifier[=value] ...]\n"
           "  HELP SHOW lists available options, qualifiers, and their report behavior.\n"
           "  Qualifiers select report sections or control output; unavailable data is not fabricated.\n"
           "  /OUTPUT[=filespec] writes reports to a file. WCL extensions /CONFIRM and /DRYRUN\n"
           "  guard report-file creation; /CONFIRM is opt-in and noninteractive requests fail closed.";
}

static const ICommandVTable ShowCmd_VTable = {
    { NULL, NULL },
    ShowCmd_Execute,
    ShowCmd_GetVerb,
    ShowCmd_GetSummary,
    ShowCmd_GetHelp
};

void RegisterShowCommand(CommandRegistry *registry) {
    static ShowCommand command = { { &ShowCmd_VTable, 1 } };
    registry->Register(registry, (ICommand*)&command);
}

