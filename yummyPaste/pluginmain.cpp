#include <crtdbg.h>

#include "pluginmain.h"
#include "plugin.h"

int g_hPlugin;
HWND g_hWndDlg;
int g_hMenu;
int g_hMenuDisasm;
int g_hMenuDump;
int g_hMenuStack;
HINSTANCE g_hInstPlugin;

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT *initStruct)
{
    initStruct->pluginVersion = PLUGIN_VERSION;
    initStruct->sdkVersion = PLUG_SDKVERSION;
    strncpy_s(initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);
    g_hPlugin = initStruct->pluginHandle;
    return pluginInit(initStruct);
}

PLUG_EXPORT bool plugstop()
{
    pluginStop();
    return true;
}

PLUG_EXPORT void plugsetup(PLUG_SETUPSTRUCT *setupStruct)
{
    g_hWndDlg = setupStruct->hwndDlg;
    g_hMenu = setupStruct->hMenu;
    g_hMenuDisasm = setupStruct->hMenuDisasm;
    g_hMenuDump = setupStruct->hMenuDump;
    g_hMenuStack = setupStruct->hMenuStack;
    pluginSetup();
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
            _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_CRT_DF);
            DisableThreadLibraryCalls(g_hInstPlugin = hinstDLL);
            break;

        case DLL_PROCESS_DETACH:
            _CrtCheckMemory();
            _CrtDumpMemoryLeaks();
            break;
    }

    return TRUE;
}
