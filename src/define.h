#ifndef SRC_DEFINE_H_
#define SRC_DEFINE_H_


#include <windows.h>

const static wchar_t kWndName[] = L"TTB_WorkerWindow";

static const unsigned int kWmCustomCmdLine = WM_USER + 1;
static const unsigned int kWmAsyncCopyData = WM_USER + 2;
static const unsigned int kWmAsynTaskbarCreate = WM_USER + 3;


#endif
