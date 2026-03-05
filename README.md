# `winlogon` Impersonation for `NT AUTH/SYSTEM` Command Prompts

Both programs below utilize the `SeImpersonatePrivilege` to obtain SYSTEM privileges from the `winlogon` process using Administrative permissions. They spawn cmd prompt instances in different ways.

## NT-AUTH-NewPrompt

Windows executable that impersonates the `winlogon` service to open a cmd prompt with `NT AUTH/SYSTEM` permissions. The new command prompt is opened in a new window.

## NT-AUTH-UpgradePrompt

Windows executable that impersonates the `winlogon` service to open a cmd prompt with `NT AUTH/SYSTEM` permissions. This program spawns a command prompt with `NT AUTH/SYSTEM` permissions but without a window. It then facilitates communications between the user's command prompt and the program via anonymous pipes to provide the user with an elevated command prompt within the same window.
