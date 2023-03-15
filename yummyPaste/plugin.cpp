#include "plugin.h"
#include "parser.h"
#include "resource.h"

enum
{
    MENU_DISASM_PASTE,
    MENU_DISASM_PASTE_PATCH,
    MENU_DUMP_PASTE,
    MENU_DUMP_PASTE_PATCH,
    MENU_DISASM_ABOUT,
    MENU_DUMP_ABOUT
};

#define ABOUT_MSG "yummyPaste by Oguz Kartal\r\n\r\n" \
                  "paste your shellcode string into the x64dbg!\r\n\r\nhttps://oguzkartal.net\r\n" \
                  "compiled in: "  __DATE__ " " __TIME__

void About()
{
    MessageBoxA(g_hWndDlg, ABOUT_MSG, PLUGIN_NAME, MB_ICONINFORMATION);
}

LPSTR GetClipboardTextData(size_t *pLength)
{
    LPSTR temp = nullptr, pastedContent = nullptr;
    size_t contentLength = 0;
    HANDLE cbHandle = nullptr;

    if (pLength)
        *pLength = 0;

    if (!OpenClipboard(g_hWndDlg))
    {
        MessageBoxA(g_hWndDlg, "The clipboard couldn't be opened", PLUGIN_NAME, MB_ICONWARNING);
        goto oneWayExit;
    }

    if (!IsClipboardFormatAvailable(CF_TEXT))
    {
        goto oneWayExit;
    }

    cbHandle = GetClipboardData(CF_TEXT);
    if (!cbHandle)
    {
        MessageBoxA(g_hWndDlg, "Clipboard data couldn't read", PLUGIN_NAME, MB_ICONWARNING);
        goto oneWayExit;
    }

    temp = (LPSTR) GlobalLock(cbHandle);
    if (!temp)
    {
        MessageBoxA(g_hWndDlg, "The data couldn't be extracted from the cb object", PLUGIN_NAME, MB_ICONSTOP);
        goto oneWayExit;
    }

    contentLength = strlen(temp);
    if (contentLength == 0)
        goto oneWayExit;

    pastedContent = (LPSTR) Malloc(contentLength + 1);
    if (!pastedContent)
    {
        goto oneWayExit;
    }

    strcpy_s(pastedContent, contentLength + 1, temp);

    if (pLength)
        *pLength = contentLength;

oneWayExit:
    if (temp)
    {
        GlobalUnlock(cbHandle);
        temp = nullptr;
    }

    CloseClipboard();

    return pastedContent;
}

// Copied in atlwin.h, fix multi monitors bug
BOOL CenterWindow(HWND hwndChild, HWND hwndCenter)
{
    _ASSERT(IsWindow(hwndChild));

    // determine owner window to center against
    DWORD dwStyle = GetWindowLong(hwndChild, GWL_STYLE);
    if (hwndCenter == nullptr)
    {
        if (dwStyle & WS_CHILD)
            hwndCenter = GetParent(hwndChild);
        else
            hwndCenter = GetWindow(hwndChild, GW_OWNER);
    }

    // get coordinates of the window relative to its parent
    RECT rcDlg = { 0 };
    GetWindowRect(hwndChild, &rcDlg);

    RECT rcArea = { 0 };
    RECT rcCenter = { 0 };

    if (!(dwStyle & WS_CHILD))
    {
        // don't center against invisible or minimized windows
        if (hwndCenter != nullptr)
        {
            dwStyle = GetWindowLong(hwndCenter, GWL_STYLE);
            if (!(dwStyle & WS_VISIBLE) || (dwStyle & WS_MINIMIZE))
                hwndCenter = nullptr;
        }

        // center within current monitor coordinates
        MONITORINFO mi = { 0 };
        mi.cbSize = sizeof(MONITORINFO);
        POINT pt = { rcDlg.left, rcDlg.top };
        if (GetMonitorInfo(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST), &mi))
            rcArea = mi.rcWork;
        else
            SystemParametersInfo(SPI_GETWORKAREA, 0, &rcArea, 0);

        if (hwndCenter == nullptr)
            rcCenter = rcArea;
        else
            GetWindowRect(hwndCenter, &rcCenter);
    }
    else
    {
        // center within parent client coordinates
        HWND hwndParent = GetParent(hwndChild);
        _ASSERT(IsWindow(hwndParent));
        GetClientRect(hwndParent, &rcArea);

        _ASSERT(IsWindow(hwndCenter));
        GetClientRect(hwndCenter, &rcCenter);

        MapWindowPoints(hwndCenter, hwndParent, (POINT *) &rcCenter, 2);
    }

    int DlgWidth = rcDlg.right - rcDlg.left;
    int DlgHeight = rcDlg.bottom - rcDlg.top;

    // find dialog's upper left based on rcCenter
    int xLeft = (rcCenter.left + rcCenter.right) / 2 - DlgWidth / 2;
    int yTop = (rcCenter.top + rcCenter.bottom) / 2 - DlgHeight / 2;

    // if the dialog is outside the screen, move it inside
    if (xLeft < rcArea.left)
        xLeft = rcArea.left;
    else if (xLeft + DlgWidth > rcArea.right)
        xLeft = rcArea.right - DlgWidth;

    if (yTop < rcArea.top)
        yTop = rcArea.top;
    else if (yTop + DlgHeight > rcArea.bottom)
        yTop = rcArea.bottom - DlgHeight;

    // map screen coordinates to child coordinates
    return SetWindowPos(hwndChild, nullptr, xLeft, yTop, -1, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

// Message handler for hex edit box.
INT_PTR CALLBACK HexEdit_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hHexEdit = nullptr;
    static LPSTR *ppszRet = nullptr;
    size_t nLen = 0;

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            CenterWindow(hDlg, g_hWndDlg);

            hHexEdit = GetDlgItem(hDlg, IDC_HEX_EDIT);
            ppszRet = (LPSTR *) lParam;

            LPSTR pszClipText = GetClipboardTextData(&nLen);
            if (pszClipText)
            {
                SetWindowText(hHexEdit, pszClipText);
            }

            return (INT_PTR) TRUE;
        }

        case WM_COMMAND:
        {
            if (LOWORD(wParam) == IDOK)
            {
                nLen = GetWindowTextLength(hHexEdit);
                if (nLen > 0)
                {
                    *ppszRet = (LPSTR) Malloc(nLen + 1);
                    GetWindowText(hHexEdit, *ppszRet, nLen + 1);
                }
                EndDialog(hDlg, IDOK);
                return (INT_PTR) TRUE;
            }
            else if (LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, IDCANCEL);
                return (INT_PTR) TRUE;
            }
            break;
        }
    }

    return (INT_PTR) FALSE;
}

void MakeTomatoPaste(GUISELECTIONTYPE window, BOOL patched)
{
    size_t nLen = 0;
    LPSTR pPasteData = nullptr;
    SELECTIONDATA sel = { 0 };
    BINARY_DATA *binary = nullptr;

    if (!DbgIsDebugging())
    {
        MessageBoxA(g_hWndDlg, "Where is your tomato to be paste?", PLUGIN_NAME, MB_ICONWARNING);
        return;
    }

    GuiSelectionGet(window, &sel);

    if (DialogBoxParam(g_hInstPlugin, MAKEINTRESOURCE(IDD_PASTE_DIALOG), g_hWndDlg, HexEdit_DlgProc, (LPARAM) &pPasteData) != IDOK)
        return;

    if (!pPasteData || (0 == (nLen = strlen(pPasteData))))
        return;

    if (nLen % 2)
    {
        if (IDOK != MessageBox(g_hWndDlg, "Hex string len is an odd number. Do you want to auto append 0 to beginning of odd hex string ?",
            PLUGIN_NAME, MB_OKCANCEL))
        {
            Free(pPasteData);
            return;
        }
    }

    ResetBinaryObject();
    ParseBytes(pPasteData, nLen);

    binary = GetBinaryData();
    if (binary->invalid)
    {
        MessageBoxA(g_hWndDlg, "Invalid hex string", PLUGIN_NAME, MB_ICONWARNING);
        Free(pPasteData);
        return;
    }

    if (patched)
        DbgFunctions()->MemPatch(sel.start, binary->binary, binary->index);
    else
        DbgMemWrite(sel.start, binary->binary, binary->index);

    Free(pPasteData);

    if (window == GUI_DISASSEMBLY)
        GuiUpdateDisassemblyView();
    else if (window == GUI_DUMP)
        GuiUpdateDumpView();
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE /*cbType*/, PLUG_CB_MENUENTRY *info)
{
    switch (info->hEntry)
    {
        case MENU_DISASM_PASTE:
            MakeTomatoPaste(GUI_DISASSEMBLY, FALSE);
            break;

        case MENU_DISASM_PASTE_PATCH:
            MakeTomatoPaste(GUI_DISASSEMBLY, TRUE);
            break;

        case MENU_DUMP_PASTE:
            MakeTomatoPaste(GUI_DUMP, FALSE);
            break;

        case MENU_DUMP_PASTE_PATCH:
            MakeTomatoPaste(GUI_DUMP, TRUE);
            break;

        case MENU_DISASM_ABOUT:
        case MENU_DUMP_ABOUT:
            About();
            break;

        default:
            break;
    }
}

//Initialize your plugin data here.
bool pluginInit(PLUG_INITSTRUCT */*initStruct*/)
{
    if (!InitBinaryObject(0xFEED))
    {
        MessageBoxA(g_hWndDlg, "Ups. memory?", PLUGIN_NAME, MB_ICONSTOP);
        return false;
    }

    return true; //Return false to cancel loading the plugin.
}

void pluginStop()
{
    DestroyBinaryObject();
}

void pluginSetup()
{
    _plugin_menuaddentry(g_hMenuDisasm, MENU_DISASM_PASTE, "&Paste it!");
    _plugin_menuaddentry(g_hMenuDisasm, MENU_DISASM_PASTE_PATCH, "Paste and Patch");
    _plugin_menuaddentry(g_hMenuDump, MENU_DUMP_PASTE, "&Paste it!");
    _plugin_menuaddentry(g_hMenuDump, MENU_DUMP_PASTE_PATCH, "Paste and Patch");
    _plugin_menuaddentry(g_hMenuDisasm, MENU_DISASM_ABOUT, "A&bout");
    _plugin_menuaddentry(g_hMenuDump, MENU_DUMP_ABOUT, "A&bout");
}
