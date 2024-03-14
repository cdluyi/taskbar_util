#include "util.h"

#include <TlHelp32.h>

namespace util{

DWORD GetSessionIdByProcessId(DWORD processId) {
  DWORD sessionId = (DWORD)-1;
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId);
  if (hProcess != NULL) {
    if (!ProcessIdToSessionId(processId, &sessionId)) {
      sessionId = (DWORD)-1;  // 获取会话ID失败
    }
    CloseHandle(hProcess);
  }
  return sessionId;
}

DWORD FindExplorerProcessId() {
  DWORD explorerPid = 0;
  PROCESSENTRY32 pe32 = {0};
  pe32.dwSize = sizeof(PROCESSENTRY32);
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (Process32First(hSnapshot, &pe32)) {
    do {
      if (_wcsicmp(pe32.szExeFile, L"explorer.exe") == 0) {
        explorerPid = pe32.th32ProcessID;
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
  }
  CloseHandle(hSnapshot);
  return explorerPid;
}

BOOL RunAsExplorerContext(LPCWSTR lpApplicationName,
                          LPCWSTR lpCommandLineParams) {
  DWORD dwExplorerPID = 0;
  HANDLE hToken = NULL;
  HANDLE hExplorerProcess = NULL;
  BOOL bSuccess = FALSE;

  // 找到explorer.exe的PID
  PROCESSENTRY32 pe32 = {sizeof(PROCESSENTRY32)};
  HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (Process32First(hSnapshot, &pe32)) {
    do {
      if (_wcsicmp(pe32.szExeFile, L"explorer.exe") == 0) {
        dwExplorerPID = pe32.th32ProcessID;
        break;
      }
    } while (Process32Next(hSnapshot, &pe32));
  }
  CloseHandle(hSnapshot);

  if (dwExplorerPID == 0) {
    return FALSE;
  }

  // 打开explorer.exe的进程并获取令牌
  hExplorerProcess =
      OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwExplorerPID);
  if (hExplorerProcess != NULL &&
      OpenProcessToken(hExplorerProcess,
                       TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY,
                       &hToken)) {
    // 使用令牌创建新的进程
    STARTUPINFO si = {sizeof(si)};
    PROCESS_INFORMATION pi;

    // 组合命令行字符串
    WCHAR lpCommandLine[1024];  // 确保这个缓冲区足够大
    if (lpApplicationName != NULL) {
      // 如果提供了应用程序名称，将其包含在命令行中
      wsprintf(lpCommandLine, L"\"%s\" %s", lpApplicationName,
               lpCommandLineParams);
    } else {
      // 否则直接使用命令行参数
      wcsncpy_s(lpCommandLine, _countof(lpCommandLine), lpCommandLineParams,
                _TRUNCATE);

      lpCommandLine[1023] = L'\0';  // 确保字符串以NULL结尾
    }

    bSuccess = CreateProcessAsUser(hToken, NULL, lpCommandLine, NULL, NULL,
                                   FALSE, 0, NULL, NULL, &si, &pi);
    if (bSuccess) {
      CloseHandle(pi.hProcess);
      CloseHandle(pi.hThread);
    }
    CloseHandle(hToken);
  }

  if (hExplorerProcess != NULL) {
    CloseHandle(hExplorerProcess);
  }

  return bSuccess;
}

BOOL util::IsRunningAsExplorerToken() {
  DWORD currentSessionId = GetSessionIdByProcessId(GetCurrentProcessId());
  DWORD explorerSessionId = GetSessionIdByProcessId(FindExplorerProcessId());

  return currentSessionId == explorerSessionId && currentSessionId != (DWORD)-1;
}

BOOL IsUserAdmin() {
  BOOL bRet;
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  PSID AdministratorsGroup;
  bRet = AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &AdministratorsGroup);
  if (bRet) {
    if (!CheckTokenMembership(NULL, AdministratorsGroup, &bRet)) {
      bRet = FALSE;
    }
    FreeSid(AdministratorsGroup);
  }

  return bRet;
}

}
