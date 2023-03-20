#pragma once

#include "pluginmain.h"

// plugin data
#define PLUGIN_NAME "yummyPaste"
#define PLUGIN_VERSION 11

// functions
bool pluginInit(PLUG_INITSTRUCT *initStruct);
void pluginStop();
void pluginSetup();

LPSTR GetClipboardTextData(size_t *pLength);
BOOL CenterWindow(HWND hwndChild, HWND hwndCenter);
