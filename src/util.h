#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include <windows.h>
#include <tlhelp32.h>
#include <wtsapi32.h>
#include <sddl.h>

#pragma comment(lib, "wtsapi32.lib")


namespace util {
DWORD GetSessionIdByProcessId(DWORD processId);
DWORD FindExplorerProcessId();
BOOL IsRunningAsExplorerToken();
BOOL RunAsExplorerContext(LPCWSTR lpApplicationName,
                          LPCWSTR lpCommandLineParams);
bool IsProcessRunningAsAdmin(DWORD processId);

};  // namespace util

#endif
