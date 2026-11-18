# WCL DCL compatibility target

## Target

WCL targets the latest documented OpenVMS DCL behavior that has a meaningful
Windows-hosted equivalent. Compatibility is behavioral, not an attempt to
emulate OpenVMS devices, RMS, cluster services, or privileged system state.
Windows-specific operations are documented as mappings, not as identical DCL
operations. OpenVMS documentation is used as a behavioral reference; no VSI
implementation code is included.

## Reference baseline

The compatibility baseline is the VSI OpenVMS x86-64 9.2-2 DCL Dictionary.
The current official [A-M volume](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/)
and [N-Z volume](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/)
both state applicability to x86-64 9.2-2 or higher. WCL uses 9.2-2 as the
minimum reference point; later OpenVMS releases are not separately certified.
Command links below point to the corresponding entries in those volumes.

## Status definitions

- **Supported**: implemented and covered by WCL regression tests.
- **Mapped**: implemented using a Windows service or API; behavior and limits
  differ from OpenVMS.
- **Partial**: common behavior exists, but DCL edge cases or qualifiers are
  not fully implemented.
- **Unsupported**: rejected or explicitly documented as unavailable.

## Command procedures

| Area | Status | Current behavior and boundary |
|---|---|---|
| Parameters and symbols | Partial | `P1`-`P8`, process symbols, `&name`, and apostrophe substitution. Symbol and line sizes are bounded. |
| Control flow | Partial | `IF/ELSE/ENDIF`, `GOTO`, `GOSUB/RETURN`, and one-shot `ON ERROR` handlers. Nesting and procedure length are bounded. |
| Conditions | Partial | Numeric and string comparisons, logical operators, parentheses, and DCL odd-integer / Y-or-T truth values. |
| Continuation | Partial | A trailing hyphen joins physical lines; a dangling continuation is a read error. |
| Verification | Partial | Procedure command verification is supported. Image/data-line verification has no current Windows input-stream mapping. |
| Lexical functions | Partial | `F$LENGTH`, `F$EXTRACT`, `F$LOCATE`, `F$ELEMENT`, `F$EDIT`, `F$SEARCH`, `F$ENVIRONMENT`, `F$INTEGER`, `F$STRING`, and `F$VERIFY`. Other DCL lexical functions are unsupported. |
| File channels | Mapped | `OPEN`, `READ`, `WRITE`, and `CLOSE` use Windows files and streams. RMS record attributes and sharing semantics are not emulated. |

Procedure limits currently include 8 nested procedures, 8 parameters, 4096
logical lines per procedure, and 2047 characters per command line.

## Command and qualifier compatibility matrix

The accepted WCL qualifier subset is intentionally narrower than the DCL
Dictionary. Unlisted DCL qualifiers must be rejected, not silently ignored.
WCL-only safety qualifiers are marked **WCL**; accepted qualifiers that only
report a Windows mapping do not imply identical DCL behavior.

| DCL command | WCL qualifier subset | Windows behavior / compatibility boundary |
|---|---|---|
| [DIRECTORY](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_73) | `/FULL`, `/NOFULL`, `/ALL`, `/VERSIONS=n` | `/ALL` includes hidden Windows entries; `/VERSIONS=n` limits version-suffixed names. VMS date, owner, ACL, RMS and output-format qualifiers are rejected. |
| [TYPE](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_157) | `/OUTPUT[=file]`, `/NOOUTPUT`, `/CONFIRM`, `/NOCONFIRM`, `/TAIL[=n]`, `/DRYRUN` (**WCL**) | `/TAIL` is a WCL extension. `/OUTPUT` supports one exact input file; paging and RMS-record qualifiers are unsupported. |
| [COPY](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_43), [APPEND](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_16), [RENAME](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_14) | `/CONFIRM`, `/NOCONFIRM`, `/LOG`, `/NOLOG`, `/DRYRUN` (**WCL**) | Windows file operations; APPEND/RENAME support one exact source. RMS, protection, ownership, date-selection and version-policy qualifiers are rejected. |
| [DELETE](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_61) | `/CONFIRM`, `/NOCONFIRM`, `/LOG`, `/NOLOG`, `/SYMBOL`, `/ENTRY`, `/DRYRUN` (**WCL**) | `/SYMBOL` deletes WCL symbols; `/ENTRY` deletes WCL-created scheduled tasks only. Neither substitutes for VMS queue/file semantics. |
| [CREATE](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_47) | `/DIRECTORY`, `/NODIRECTORY`, `/LOG`, `/NOLOG`, `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | Creates a Windows file or directory. RMS FDL, protection, ownership and ACL qualifiers are rejected. |
| [SEARCH](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_35) | `/EXACT`, `/NOEXACT`, `/NUMBERS`, `/NONUMBERS`, `/HEADING`, `/NOHEADING`, `/SKIP=n`, `/LIMIT=n`, `/OUTPUT[=file]`, `/NOOUTPUT`, `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | Searches Windows text records. `/SKIP` and `/LIMIT` are WCL record controls; wildcard output, indexed/RMS search and paging qualifiers are unsupported. |
| [PRINT](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_9000) | `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | Invokes the file's registered Windows print action; it does not select a VMS queue or implement VMS job qualifiers. |
| [DEFINE](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#PURPLE_1), [ASSIGN](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_17) | `/LOG`, `/NOLOG`, `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | Process-local WCL logical names. Non-process DCL tables and translation attributes are not emulated. |
| [DEASSIGN](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_54) | `/ALL`, `/CONFIRM`, `/NOCONFIRM`, `/LOG`, `/NOLOG`, `/DRYRUN` (**WCL**) | `/ALL` removes WCL process logicals only. Job, group, system and cluster tables are unsupported. |
| [SET](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_36) | `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**); subcommands `ON`, `NOON`, `VERIFY`, `NOVERIFY`, `PROMPT`, `DEFAULT` | Only these subcommands are implemented. Verification flags are tracked; image/data-line verification has no Windows input-stream mapping. Other DCL `SET` commands are rejected. |
| [SPAWN](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_134), [RUN](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_23) | `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | Uses Windows process creation and returns the child exit status. DCL quotas, UIC, privileges, debug and process attributes are unsupported. |
| [STOP](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_141) | `STOP process-image-name`, `/IDENTIFICATION=pid` (`/ID` abbreviation), `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | Terminates one local Windows process. Names must match one executable image; ambiguous names require a PID. Targetless STOP and self-termination are rejected; Windows access controls apply. |
| [SUBMIT](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_153) | `/AFTER=time`, `/AT=time` (**WCL synonym**), `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | `/AFTER` accepts a future local `YYYY-MM-DDTHH:MM` time and creates a one-time current-user Task Scheduler task. VMS queue controls and recurring/elevated execution are unsupported. |
| [OPEN](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_5000) | `/READ`, `/WRITE`, `/APPEND`, `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | Defaults to `/READ`; `/READ/WRITE` updates an existing file. `/WRITE` creates/truncates; `/APPEND` creates or appends. RMS sharing and record attributes are unsupported. |
| [READ](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_12) | None | Reads one text line from a WCL `/READ` channel into a symbol. DCL record-format and EOF/error-handler qualifiers are unsupported. |
| [CLOSE](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_24) | `/DISPOSITION=KEEP\|DELETE`, `/CONFIRM`, `/NOCONFIRM`, `/LOG`, `/NOLOG`, `/DRYRUN` (**WCL**) | `DELETE` removes the Windows file after closing. VMS `PRINT`/`SUBMIT` dispositions and RMS channel options are unsupported. |
| [WRITE](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_222) | `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` (**WCL**) | Writes a line to a WCL channel, `SYS$OUTPUT`, or `SYS$ERROR`; DCL record-control qualifiers are unsupported. |
| [LOGOUT](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#CHROME_6), [EXIT](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_110) | `LOGOUT /BRIEF`, `/FULL`; `EXIT [status]` | `/FULL` reports Windows account/host information. VMS terminal hangup control is unsupported; `EXIT` status behavior is WCL procedure/session behavior. |
| [SHOW](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_95) | Option-specific subsets shown by `HELP SHOW`; global WCL `/OUTPUT[=file]`, `/CONFIRM`, `/NOCONFIRM`, `/DRYRUN` | The runtime catalog lists every option and qualifier accepted by this build. Qualifiers are mapped to available Windows data; a qualifier that is only reported as a mapping does not change the report or assert VMS semantics. |
| [SHOW MEMORY](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-n-z/#BRASS_113) | `/ALL`, `/BITMAP`, `/BUFFER_OBJECTS`, `/CACHE`, `/EXACT`, `/FILES`, `/FULL`, `/GH_REGIONS`, `/HIGHLIGHT`, `/OUTPUT`, `/NOOUTPUT`, `/PAGE`, `/NOPAGE`, `/PHYSICAL_PAGES`, `/POOL`, `/RESERVED`, `/SEARCH`, `/SLOTS`, `/UNITS`, `/WRAP`, `/NOWRAP` | `/ALL` is the default and reports all available sections. Section qualifiers select only their corresponding report; `/FULL` adds detail to `/CACHE`, `/FILES`, and `/POOL`. `/PAGE` controls interactive paging (`SAVE`, `SCROLL`, or `CLEAR_SCREEN`) and cannot be combined with `/OUTPUT`. |
| [ALLOCATE](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_5), [MOUNT](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#MOUNT_COMM), [DISMOUNT](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#d0e24472), [INITIALIZE](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_117) | `ALLOCATE /DRYRUN`; `MOUNT` and `DISMOUNT` require `/CONFIRM` (or `/DRYRUN`); `INITIALIZE` requires `/CONFIRM` and accepts `/DRYRUN`, `/FS=type`, `/LABEL=name` | Guarded Windows drive/volume operations; they are not equivalents of VMS device allocation, volume mounting or initialization. Unsupported VMS qualifiers are rejected. |
| [HELP](https://docs.vmssoftware.com/vsi-openvms-dcl-dictionary-a-m/#BLUE_114) | None | WCL help topics are Windows-specific. Unknown commands are launched through Windows process search. |

### SHOW qualifier inventory and limits

`HELP SHOW` is the canonical option-by-option inventory and mapping. It is
generated from the same option definitions used by qualifier validation.
`SHOW MEMORY` follows the DCL section-selection model: no section qualifier or
`/ALL` displays all available sections; `/CACHE`, `/FILES`, `/PHYSICAL_PAGES`,
`/POOL`, and `/SLOTS` select only their respective reports. `/FULL` adds
detail to explicitly selected cache, paging-file, or pool reports. `/UNITS`
adds byte and page-size values. `/PAGE` supports screen-by-screen paging,
line-by-line scrolling, and screen clearing; `/PAGE=SAVE` supports search,
case-sensitive search, highlighting, and line wrapping. `/NOOUTPUT` suppresses
the report. `/BITMAP`, `/BUFFER_OBJECTS`, `/GH_REGIONS`, and `/RESERVED` produce
their own section and state when the corresponding host data is unavailable.
Windows does not expose every OpenVMS memory counter, so unavailable values
are identified rather than replaced with unrelated statistics.

Options with no qualifier subset are `CLUSTER`, `DUMP_OPTIONS`, `ROOT`, and
`STATUS`; the other option names are `ACCOUNTING`, `AUDIT`, `BROADCAST`, `CPU`,
`DEFAULT`, `DEVICES`, `DISPLAY`, `ENTROPY`, `ENTRY`, `ERROR`, `FASTPATH`,
`IMAGE`, `INTRUSION`, `KEY`, `LICENSE`, `LOGICAL`, `MEMORY`, `NETWORK`,
`PRINTER`, `PROCESS`, `PROTECTION`, `QUEUE`, `QUOTA`, `RMS_DEFAULT`,
`SECURITY`, `SERVER`, `SHADOW`, `SYMBOL`, `SYSTEM`, `TERMINAL`, `TIME`,
`TRANSLATION`, `USERS`, and `WORKING_SET`. Special option forms include
`DEVICES/SERVED`, `QUEUE/CHARACTERISTICS`, `QUEUE/FORM`, `QUEUE/MANAGERS`,
`SERVER/ACME_SERVER`, and `SERVER/REGISTRY_SERVER`.

The catalog distinguishes report selectors from presentation qualifiers.
`SHOW MEMORY` selectors change which report sections are generated. Other
SHOW options may have narrower host-specific behavior, documented in their
individual qualifier descriptions.

## Qualifier regression coverage

[`regression.ps1`](../src/tests/regression.ps1) exercises qualifier parsing and effects for the
file, procedure, process, SHOW, and Windows-mapped command families. The tests
cover representative positive and negative forms, value validation, dry-run
non-mutation, confirmation failure without an interactive console, output
redirection, and unsupported-qualifier rejection. They are Windows behavior
regressions, not a claim that every accepted SHOW annotation reproduces a VMS
effect.

## Windows mappings and unsupported OpenVMS facilities

- `SHOW` reports Windows-derived process, network, service, printer, and system
  information; the result is not identical to OpenVMS accounting or device
  data.
- Process logical names map to WCL process state. Job, group, system, and
  cluster logical-name tables are unsupported.
- File versions and wildcards are mapped onto Windows filesystem names.
  Windows paths, metadata, sharing, protection, and RMS semantics differ.
- `SUBMIT` maps to a one-time current-user Task Scheduler task. OpenVMS queue
  controls, privileges, and execution environment are not reproduced.
- Device allocation, volume mounting, and formatting have guarded Windows
  mappings. They are not equivalent to OpenVMS device/volume operations.

## Regression policy

Every new compatibility claim should have a focused Windows regression case
for success, invalid input, and relevant boundary behavior. Tests should not
assert parity unless the expected behavior is grounded in the selected DCL
reference. Windows-only mappings must assert their documented limits as well
as their successful path.
