#ifndef SRC_MAIN_WINDOW_H_
#define SRC_MAIN_WINDOW_H_

#include <atlbase.h>

#include <atlapp.h>
#include <atlcrack.h>
#include <atlwin.h>

#include <chrono>
#include <set>

#include "ExplorerHooks/api.hpp"
#include "ExplorerTAP/ITaskbarAppearanceService.h"
#include "ExplorerTAP/api.hpp"
#include "define.h"

typedef ATL::CWinTraits<WS_POPUP | WS_DISABLED, 0> CAppWindowTraits;

class MainWindow
    : public ATL::CWindowImpl<MainWindow, ATL::CWindow, CAppWindowTraits> {
 public:
  DECLARE_WND_CLASS(kWndName)
  BEGIN_MSG_MAP_EX(MainWindow)
  MSG_WM_CREATE(OnCreate)
  MSG_WM_DESTROY(OnDestroy)
  MSG_WM_CLOSE(OnClose)
  MSG_WM_COPYDATA(OnCopyData)
  MESSAGE_HANDLER(request_attribute_msg_, RequestAttributeRefresh)
  MESSAGE_HANDLER(taskbar_create_msg_, TaskbarCreate)
  MESSAGE_HANDLER_EX(kWmAsyncCopyData, OnAsyncCopyData)
  MESSAGE_HANDLER_EX(kWmAsynTaskbarCreate, AsynTaskbarCreate)

  END_MSG_MAP()

 public:
  bool Set();
  bool Reset();
  void SetWatchId(DWORD pid);

  void DoClose();

 public:
 public:
  struct Color {
    uint8_t R, G, B, A;
    constexpr uint32_t ToABGR() const noexcept {
      return (static_cast<uint32_t>(A) << 24) |
             (static_cast<uint32_t>(B) << 16) |
             (static_cast<uint32_t>(G) << 8) | static_cast<uint32_t>(R);
    }
  };

 protected:
  int OnCreate(LPCREATESTRUCT create_struct);
  void OnDestroy();
  void OnClose();
  BOOL OnCopyData(CWindow wnd, PCOPYDATASTRUCT pCopyDataStruct);
  LRESULT OnAsyncCopyData(UINT msg, WPARAM wParam, LPARAM lParam);

 private:
  bool SetAtrribute(bool enable);

  void InsertTaskbar(HWND window);

  bool TransportOnWin(bool enable);
  bool TransportOnNewWin(bool enable);

  void SetWindowBlur();
  LRESULT RequestAttributeRefresh(UINT uMsg, WPARAM wParam, LPARAM lParam,
                                  BOOL& bHandled);
  LRESULT TaskbarCreate(UINT uMsg, WPARAM wParam, LPARAM lParam,
                        BOOL& bHandled);
  LRESULT AsynTaskbarCreate(UINT uMsg, WPARAM wParam, LPARAM lParam);

  static void NTAPI ProcessWaitCallback(void* parameter, BOOLEAN timed_out);
  void OnProcessDied();

 private:
  std::set<HWND> taskbars_;
  HANDLE watch_process_ = NULL;
  HANDLE wait_handle_ = NULL;

  ITaskbarAppearanceService* taskbar_service_ = nullptr;

  UINT request_attribute_msg_ = 0;
  UINT taskbar_create_msg_ = 0;
  DWORD watch_pid_ = 0;
  DWORD last_explorer_pid_ = 0;
  bool is_new_explorer_ = false;
  bool is_inject_hook_ = false;
  bool is_inject_tab_ = false;
  bool last_enable_ = false;
};

#endif
