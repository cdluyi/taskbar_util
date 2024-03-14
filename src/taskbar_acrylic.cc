// taskbar_acrylic.cpp : 定义应用程序的入口点。
//

#include <Windows.h>

#include <sstream>
#include <string>

#include "ExplorerTAP/ITaskbarAppearanceService.h"
#include "main_window.h"
#include "util.h"

MainWindow* main_wnd_ = nullptr;

int APIENTRY Run(LPTSTR cmdline);
int APIENTRY wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR cmd_line,
                      _In_ int) {

  do {
    HRESULT com_init_res = CoInitialize(NULL);
    if (FAILED(com_init_res)) {
      break;
    }

    ::DefWindowProc(NULL, 0, 0, 0L);

    CreateMutex(NULL, FALSE, L"C025776D-8A8D-4B2E-88C8-DEA933EBF56D");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
      HWND wnd_existing = FindWindow(kWndName, kWndName);
      if (wnd_existing) {
        COPYDATASTRUCT cds;
        cds.dwData = kWmCustomCmdLine;
        cds.cbData =
            static_cast<DWORD>((wcslen(cmd_line) + 1) * sizeof(wchar_t));
        cds.lpData = cmd_line;
        SendMessage(wnd_existing, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&cds);
      }
      return 0;
    }

    if (Run(cmd_line)) {
      MSG msg;
      while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

  } while (0);

  return 0;
}

int APIENTRY Run(LPTSTR cmdline) {
  int result = 0;

  bool necessary_cmd = false;
  bool is_enable = true;
  bool is_quit = false;
  DWORD watch_pid = 0;
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

  for (int i = 1; i < argc; ++i) {
    std::wstring argw(argv[i]);
    std::wstring arg(argw.begin(), argw.end());

    if (arg == L"--disable") {
      is_enable = false;
      necessary_cmd = true;
    } else if (arg == L"--enable") {
      is_enable = true;
      necessary_cmd = true;
    } else if (arg.find(L"--pid=") == 0) {
      std::wstring pid_str = arg.substr(6);
      std::wistringstream(pid_str) >> watch_pid;
    } else if (arg.find(L"--quit") == 0) {
      necessary_cmd = true;
      is_enable = false;
      is_quit = true;
    }
  }

  if (!necessary_cmd) {
    return 1;
  }

  if (NULL == main_wnd_) {
    main_wnd_ = new MainWindow();

    if (watch_pid == 0) {
      watch_pid = ::GetCurrentProcessId();
    }
    main_wnd_->Create(NULL, NULL, kWndName);
    main_wnd_->SetWatchId(watch_pid);

  }

  if (argv) LocalFree(argv);

  if (is_enable) {
    result = main_wnd_->Set();
  } else {
    result = main_wnd_->Reset();
  }

  if (is_quit) {
    main_wnd_->DoClose();
  }

  return result;
}