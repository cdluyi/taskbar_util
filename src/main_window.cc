#include "main_window.h"

#include <sstream>

#include "define.h"

bool IsAtLeastBuild(DWORD buildNumber) noexcept {
  OSVERSIONINFOEX versionInfo = {sizeof(versionInfo), 10, 0, buildNumber};

  DWORDLONG mask = 0;
  VER_SET_CONDITION(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
  VER_SET_CONDITION(mask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

  return VerifyVersionInfo(
      &versionInfo, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER,
      mask);
}

bool MainWindow::Set() { return SetAtrribute(true); }

bool MainWindow::Reset() { return SetAtrribute(false); }

void MainWindow::DoClose() { PostMessage(WM_CLOSE); }

void MainWindow::OnDestroy() { PostQuitMessage(0); }

void MainWindow::OnClose() {
  if (watch_process_) {
    CloseHandle(watch_process_);
    watch_process_ = NULL;
  }
  if (wait_handle_) {
    UnregisterWait(wait_handle_);
    wait_handle_ = NULL;
  }

  DestroyWindow();
}

BOOL MainWindow::OnCopyData(CWindow wnd, PCOPYDATASTRUCT ptr_copy_data_struct) {
  ATLASSERT(ptr_copy_data_struct);
  if (ptr_copy_data_struct == NULL) return FALSE;

  PCOPYDATASTRUCT ptr_new_copy_data_struct = new (std::nothrow) COPYDATASTRUCT;
  ATLASSERT(ptr_new_copy_data_struct);
  if (ptr_new_copy_data_struct == NULL) return FALSE;

  ZeroMemory(ptr_new_copy_data_struct, sizeof(*ptr_new_copy_data_struct));
  ptr_new_copy_data_struct->dwData = ptr_copy_data_struct->dwData;
  ptr_new_copy_data_struct->cbData = ptr_copy_data_struct->cbData;
  if (ptr_new_copy_data_struct->cbData > 0) {
    ptr_new_copy_data_struct->lpData =
        new (std::nothrow) BYTE[ptr_new_copy_data_struct->cbData];
    if (ptr_new_copy_data_struct->lpData == NULL) {
      delete ptr_new_copy_data_struct;
      return FALSE;
    }

    memcpy(ptr_new_copy_data_struct->lpData, ptr_copy_data_struct->lpData,
           ptr_new_copy_data_struct->cbData);
  }

  PostMessage(kWmAsyncCopyData, (WPARAM)wnd.m_hWnd,
              (LPARAM)ptr_new_copy_data_struct);
  return TRUE;
}

LRESULT MainWindow::OnAsyncCopyData(UINT msg, WPARAM wParam, LPARAM lParam) {
  CWindow wnd = (HWND)wParam;
  PCOPYDATASTRUCT pcds = (PCOPYDATASTRUCT)lParam;

  if (pcds->dwData == kWmCustomCmdLine) {
    std::wstring cmd = (LPWSTR)pcds->lpData;

    bool necessary_cmd = false;
    bool is_enable = false;
    bool is_quit = false;

    DWORD watch_pid = 0;

    if (cmd.find(L"--enable") != std::wstring::npos) {
      is_enable = true;
      necessary_cmd = true;
    } else if (cmd.find(L"--disable") != std::wstring::npos) {
      is_enable = false;
      necessary_cmd = true;
    } else if (cmd.find(L"--quit") != std::wstring::npos) {
      necessary_cmd = true;
      is_enable = false;
      is_quit = true;
    }

    size_t pid_pos = cmd.find(L"--pid=");
    if (pid_pos != std::wstring::npos) {
      std::wstring pid_str = cmd.substr(pid_pos + 6);
      std::wistringstream(pid_str) >> watch_pid;
      SetWatchId(watch_pid);
    }

    if (!necessary_cmd) {
      return 0;
    }

    if (is_enable) {
      Set();
    } else {
      Reset();
    }

    if (is_quit) {
      DoClose();
    }
  }

  return 0;
}

int MainWindow::OnCreate(LPCREATESTRUCT create_struct) {
  request_attribute_msg_ =
      RegisterWindowMessage(L"TTBHook_RequestAttributeRefresh");
  taskbar_create_msg_ = RegisterWindowMessage(L"TaskbarCreated");
  is_new_explorer_ = IsAtLeastBuild(22621);

  return 0;
}

bool MainWindow::SetAtrribute(bool enable) {
  bool result = false;

  HWND main_taskbar = FindWindowExW(NULL, NULL, L"Shell_TrayWnd", NULL);
  DWORD pid = 0;
  GetWindowThreadProcessId(main_taskbar, &pid);

  if (last_explorer_pid_ == 0 || pid != last_explorer_pid_) {
    // explorer pid change, need re inject
    is_inject_hook_ = false;
    is_inject_tab_ = false;
  }

  last_explorer_pid_ = pid;
  if (last_explorer_pid_ == 0) {
    return result;
  }

  HWND secondary_explorer_wnd =
      FindWindowExW(NULL, NULL, L"Shell_SecondaryTrayWnd", NULL);

  InsertTaskbar(main_taskbar);
  InsertTaskbar(secondary_explorer_wnd);

  last_enable_ = enable;

  if (is_new_explorer_) {
    result = TransportOnNewWin(enable);
  } else {
    result = TransportOnWin(enable);
  }

  return result;
}

void MainWindow::InsertTaskbar(HWND window) {
  if (window != nullptr) {
    taskbars_.emplace(window);
  }
}

LRESULT MainWindow::RequestAttributeRefresh(UINT uMsg, WPARAM wParam,
                                            LPARAM lParam, BOOL& bHandled) {
  return last_enable_;
}

LRESULT MainWindow::TaskbarCreate(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                  BOOL& bHandled) {
  // Asynchronous processing
  PostMessage(kWmAsynTaskbarCreate, wParam, lParam);
  bHandled = true;
  return 0;
}

LRESULT MainWindow::AsynTaskbarCreate(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (last_enable_) {
    Set();
  } else {
    Reset();
  }

  return 0;
}

void NTAPI MainWindow::ProcessWaitCallback(void* parameter, BOOLEAN timed_out) {
  reinterpret_cast<MainWindow*>(parameter)->OnProcessDied();
}

void MainWindow::OnProcessDied() { DoClose(); }

void MainWindow::SetWatchId(DWORD pid) {
  if (watch_process_ != NULL) {
    // 仅监控第一次打开的进程
    return;
  }

  watch_pid_ = pid;
  if (pid == ::GetCurrentProcessId()) {
    return;
  }

  watch_process_ = OpenProcess(SYNCHRONIZE, false, pid);
  if (watch_process_ == NULL) {
    DoClose();
    return;
  }

  if (!RegisterWaitForSingleObject(&wait_handle_, watch_process_,
                                   ProcessWaitCallback, this, INFINITE,
                                   WT_EXECUTEONLYONCE)) {
    // HRESULT result = HRESULT_FROM_WIN32(GetLastError());
  }
}

bool MainWindow::TransportOnWin(bool enable) {
  if (enable) {
    SetWindowBlur();
  } else {
    for (const auto& task : taskbars_) {
      ::PostMessageW(task, WM_DWMCOMPOSITIONCHANGED, 1, 0);
    }
  }

  if (!is_inject_hook_ && enable) {
    HINSTANCE dll_instance = LoadLibrary(L"ExplorerHooks.dll");
    PFN_INJECT_EXPLORER_HOOK proc = (PFN_INJECT_EXPLORER_HOOK)GetProcAddress(
        dll_instance, "InjectExplorerHook");
    for (const auto& task : taskbars_) {
      if (proc != NULL) {
        proc(task);
      }
    }

    is_inject_hook_ = true;
  }

  return true;
}

bool MainWindow::TransportOnNewWin(bool enable) {
  if (!is_inject_tab_ || taskbar_service_ == nullptr) {
    HMODULE explorer_tab = LoadLibrary(L"ExplorerTAP.dll");

    PFN_INJECT_EXPLORER_TAP inject_explorer_tap =
        (PFN_INJECT_EXPLORER_TAP)GetProcAddress(explorer_tab,
                                                "InjectExplorerTAP");
    if (inject_explorer_tap == nullptr) {
      return false;
    }

    HRESULT hr = inject_explorer_tap(
        last_explorer_pid_, __uuidof(ITaskbarAppearanceService),
        reinterpret_cast<void**>(&taskbar_service_));

    if (hr == HRESULT_FROM_WIN32(ERROR_PRODUCT_VERSION)) {
      return false;
    }
    if (taskbar_service_ == NULL) {
      return false;
    }
  }

  TaskbarBrush brush = SolidColor;
  Color color = {0, 0, 0, 0};
  HRESULT result;
  if (taskbar_service_) {
    if (enable) {
      if (watch_pid_ != 0) {
        taskbar_service_->RestoreAllTaskbarsToDefaultWhenProcessDies(
            watch_pid_);
      }

      for (const auto& task : taskbars_) {
        result =
            taskbar_service_->SetTaskbarAppearance(task, brush, color.ToABGR());
        result = taskbar_service_->SetTaskbarBorderVisibility(task, false);
      }
    } else {
      result = taskbar_service_->RestoreAllTaskbarsToDefault();
    }

  } else {
  }

  return true;
}

void MainWindow::SetWindowBlur() {
  for (const auto& task : taskbars_) {
    typedef enum _WINDOWCOMPOSITIONATTRIB {
      WCA_UNDEFINED = 0,
      WCA_NCRENDERING_ENABLED = 1,
      WCA_NCRENDERING_POLICY = 2,
      WCA_TRANSITIONS_FORCEDISABLED = 3,
      WCA_ALLOW_NCPAINT = 4,
      WCA_CAPTION_BUTTON_BOUNDS = 5,
      WCA_NONCLIENT_RTL_LAYOUT = 6,
      WCA_FORCE_ICONIC_REPRESENTATION = 7,
      WCA_EXTENDED_FRAME_BOUNDS = 8,
      WCA_HAS_ICONIC_BITMAP = 9,
      WCA_THEME_ATTRIBUTES = 10,
      WCA_NCRENDERING_EXILED = 11,
      WCA_NCADORNMENTINFO = 12,
      WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
      WCA_VIDEO_OVERLAY_ACTIVE = 14,
      WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
      WCA_DISALLOW_PEEK = 16,
      WCA_CLOAK = 17,
      WCA_CLOAKED = 18,
      WCA_ACCENT_POLICY = 19,
      WCA_FREEZE_REPRESENTATION = 20,
      WCA_EVER_UNCLOAKED = 21,
      WCA_VISUAL_OWNER = 22,
      WCA_HOLOGRAPHIC = 23,
      WCA_EXCLUDED_FROM_DDA = 24,
      WCA_PASSIVEUPDATEMODE = 25,
      WCA_USEDARKMODECOLORS = 26,
      WCA_LAST = 27
    } WINDOWCOMPOSITIONATTRIB;

    typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
      WINDOWCOMPOSITIONATTRIB Attrib;
      PVOID pvData;
      SIZE_T cbData;
    } WINDOWCOMPOSITIONATTRIBDATA;

    typedef enum _ACCENT_STATE {
      ACCENT_DISABLED = 0,
      ACCENT_ENABLE_GRADIENT = 1,
      ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
      ACCENT_ENABLE_BLURBEHIND = 3,
      ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,  // RS4 1803
      ACCENT_ENABLE_HOSTBACKDROP = 5,       // RS5 1809
      ACCENT_INVALID_STATE = 6
    } ACCENT_STATE;

    typedef struct _ACCENT_POLICY {
      ACCENT_STATE AccentState;
      DWORD AccentFlags;
      DWORD GradientColor;
      DWORD AnimationId;
    } ACCENT_POLICY;

    const HINSTANCE hModule = LoadLibrary(TEXT("user32.dll"));
    if (hModule) {
      typedef BOOL(WINAPI * pSetWindowCompositionAttribute)(
          HWND, WINDOWCOMPOSITIONATTRIBDATA*);
      const pSetWindowCompositionAttribute SetWindowCompositionAttribute =
          (pSetWindowCompositionAttribute)GetProcAddress(
              hModule, "SetWindowCompositionAttribute");
      if (SetWindowCompositionAttribute) {
        ACCENT_POLICY policy = {ACCENT_ENABLE_TRANSPARENTGRADIENT, 2, 0,
                                0};  // ACCENT_ENABLE_BLURBEHIND=3...
        WINDOWCOMPOSITIONATTRIBDATA data = {
            WCA_ACCENT_POLICY, &policy,
            sizeof(WINDOWCOMPOSITIONATTRIBDATA)};  // WCA_ACCENT_POLICY=19
        SetWindowCompositionAttribute(task, &data);
      }
      // FreeLibrary(hModule);
    }
  }
}
