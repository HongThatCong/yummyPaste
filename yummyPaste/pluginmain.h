#pragma once

#define _CRTDBG_MAP_ALLOC

#include <windows.h>
#include <crtdbg.h>
#include <bridgemain.h>
#include <_plugins.h>

#include <_scriptapi_argument.h>
#include <_scriptapi_assembler.h>
#include <_scriptapi_bookmark.h>
#include <_scriptapi_comment.h>
#include <_scriptapi_debug.h>
#include <_scriptapi_flag.h>
#include <_scriptapi_function.h>
#include <_scriptapi_gui.h>
#include <_scriptapi_label.h>
#include <_scriptapi_memory.h>
#include <_scriptapi_misc.h>
#include <_scriptapi_module.h>
#include <_scriptapi_pattern.h>
#include <_scriptapi_register.h>
#include <_scriptapi_stack.h>
#include <_scriptapi_symbol.h>

#include <DeviceNameResolver/DeviceNameResolver.h>
#include <jansson/jansson.h>
#include <lz4/lz4file.h>
#include <TitanEngine/TitanEngine.h>
#include <XEDParse/XEDParse.h>

#ifdef _WIN64
#pragma comment(lib, "x64dbg.lib")
#pragma comment(lib, "x64bridge.lib")
#pragma comment(lib, "DeviceNameResolver/DeviceNameResolver_x64.lib")
#pragma comment(lib, "jansson/jansson_x64.lib")
#pragma comment(lib, "lz4/lz4_x64.lib")
#pragma comment(lib, "TitanEngine/TitanEngine_x64.lib")
#pragma comment(lib, "XEDParse/XEDParse_x64.lib")
#else
#pragma comment(lib, "x32dbg.lib")
#pragma comment(lib, "x32bridge.lib")
#pragma comment(lib, "DeviceNameResolver/DeviceNameResolver_x86.lib")
#pragma comment(lib, "jansson/jansson_x86.lib")
#pragma comment(lib, "lz4/lz4_x86.lib")
#pragma comment(lib, "TitanEngine/TitanEngine_x86.lib")
#pragma comment(lib, "XEDParse/XEDParse_x86.lib")
#endif //_WIN64

#define Cmd(x) DbgCmdExecDirect(x)
#define Eval(x) DbgValFromString(x)
#define dprintf(x, ...) _plugin_logprintf("[" PLUGIN_NAME "] " x, __VA_ARGS__)
#define dputs(x) _plugin_logputs("[" PLUGIN_NAME "] " x)
#define PLUG_EXPORT extern "C" __declspec(dllexport)

//super global variables
extern int g_hPlugin;
extern HWND g_hWndDlg;
extern int g_hMenu;
extern int g_hMenuDisasm;
extern int g_hMenuDump;
extern int g_hMenuStack;
extern HINSTANCE g_hInstPlugin;
