#include "plugin.h"
#include "parser.h"
#include "resource.h"

enum
{
    MENU_DISASM_PASTE,
    MENU_DISASM_PATCH,
    MENU_DUMP_PASTE,
    MENU_DUMP_PATCH,
    MENU_DISASM_ABOUT,
    MENU_DUMP_ABOUT
};

constexpr LPCTSTR ABOUT_MSG = "yummyPaste by Oguz Kartal + HTC\n\n" \
"Paste your shellcode hex string into the x64dbg!\n\nhttps://oguzkartal.net\n" \
"https://github.com/HongThatCong/yummyPaste\n" \
"compiled in: "  __DATE__ " " __TIME__;

constexpr LPCTSTR H_EMPTY = "Hex string is empty";
constexpr LPCTSTR H_INVALID = "Hex string is invalid";
constexpr LPCTSTR H_NO_CHARS = "No hex characters entered";
constexpr LPCTSTR OUT_OFF_MEMORY = "Out off memory";

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

    if (0 == (dwStyle & WS_CHILD))
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

void UpdateHexStatus(HWND hDlg)
{
    char szStatus[_MAX_PATH * 2] = { 0 };

    HWND hEdit = GetDlgItem(hDlg, IDC_HEX_EDIT);
    _ASSERT(nullptr != hEdit);
    _ASSERT(IsWindow(hEdit));

    int nLen = GetWindowTextLength(hEdit);
    if (nLen > 0)
    {
        LPSTR pszText = (LPSTR) Malloc(static_cast<size_t>(nLen) + 1);
        if (pszText)
        {
            GetWindowText(hEdit, pszText, nLen + 1);
            size_t nHexChars = 0;
            bool valid = ValidateHexText(pszText, nHexChars);
            if (valid)
            {
                if (nHexChars)
                {
                    if (nHexChars % 2)
                        sprintf_s(szStatus, "Hex string can be invalid. Hex chars = %zu, odd number", nHexChars);
                    else
                        sprintf_s(szStatus, "Hex string valid. Hex chars = %zu", nHexChars);
                }
                else
                {
                    strcpy_s(szStatus, H_EMPTY);
                }
            }
            else
            {
                strcpy_s(szStatus, H_INVALID);
            }
            Free(pszText);
        }
        else
        {
            strcpy_s(szStatus, OUT_OFF_MEMORY);
        }
    }
    else
    {
        strcpy_s(szStatus, H_EMPTY);
    }

    SetDlgItemText(hDlg, IDC_HEX_STATUS, szStatus);
}

// Message handler for hex edit box.
INT_PTR CALLBACK HexEdit_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hHexEdit = nullptr;
    static LPSTR *ppszRet = nullptr;
    int nLen = 0;
    size_t nHexChars = 0;

    switch (uMsg)
    {
        case WM_INITDIALOG:
        {
            ppszRet = (LPSTR *) lParam;
            _ASSERT(nullptr != ppszRet);

            hHexEdit = GetDlgItem(hDlg, IDC_HEX_EDIT);

            LPSTR pszClipText = GetClipboardTextData(nullptr);
            if (pszClipText)
            {
                SetWindowText(hHexEdit, pszClipText);
            }
            Free(pszClipText);

            CenterWindow(hDlg, g_hWndDlg);
            UpdateHexStatus(hDlg);

            return (INT_PTR) TRUE;
        }

        case WM_COMMAND:
        {
            if (LOWORD(wParam) == IDOK)
            {
                nLen = GetWindowTextLength(hHexEdit);
                if (nLen > 0)
                {
                    LPSTR pszText = (LPSTR) Malloc(static_cast<size_t>(nLen) + 1);
                    if (pszText)
                    {
                        GetWindowText(hHexEdit, pszText, nLen + 1);
                        bool valid = ValidateHexText(pszText, nHexChars);
                        if (valid)
                        {
                            if (0 == nHexChars)
                            {
                                MessageBox(hDlg, H_NO_CHARS, PLUGIN_NAME, MB_ICONERROR);
                            }
                            else
                            {
                                // Ok, close dialog, return hex string to caller.
                                *ppszRet = pszText;
                                EndDialog(hDlg, IDOK);
                                return (INT_PTR) TRUE;
                            }
                        }
                        else
                        {
                            MessageBox(hDlg, H_INVALID, PLUGIN_NAME, MB_ICONERROR);
                        }

                        Free(pszText);
                    }
                    else
                    {
                        MessageBox(hDlg, OUT_OFF_MEMORY, PLUGIN_NAME, MB_ICONERROR);
                    }
                }
                else
                {
                    MessageBox(hDlg, H_EMPTY, PLUGIN_NAME, MB_ICONERROR);
                }

                return (INT_PTR) TRUE;
            }
            else if (LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, IDCANCEL);
                return (INT_PTR) TRUE;
            }
            else if ((HIWORD(wParam) == EN_UPDATE) && (LOWORD(wParam) == IDC_HEX_EDIT))
            {
                UpdateHexStatus(hDlg);
                return (INT_PTR) TRUE;
            }
            break;
        }
    }

    return (INT_PTR) FALSE;
}

void MakeTomatoPaste(GUISELECTIONTYPE window, BOOL patched)
{
    bool bSuccess = false;
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

    // check again
    if ((nullptr == pPasteData) || (0 == (nLen = strlen(pPasteData))))
    {
        dputs("Nothing to paste/patch\n");
        return;
    }

    ResetBinaryObject();
    ParseBytes(pPasteData, nLen);

    binary = GetBinaryData();
    if (binary->invalid)
    {
        MessageBoxA(g_hWndDlg, H_INVALID, PLUGIN_NAME, MB_ICONWARNING);
        Free(pPasteData);
        return;
    }

    if (patched)
        bSuccess = DbgFunctions()->MemPatch(sel.start, binary->binary, binary->index);
    else
        bSuccess = DbgMemWrite(sel.start, binary->binary, binary->index);

    Free(pPasteData);

    if (bSuccess)
    {
        if (window == GUI_DISASSEMBLY)
            GuiUpdateDisassemblyView();
        else if (window == GUI_DUMP)
            GuiUpdateDumpView();
        sel.end = sel.start + binary->index;
        GuiSelectionSet(window, &sel);
        dprintf("Memory was %s at %p with %lu bytes.\n", patched ? "patched" : "written", sel.start, binary->index);
    }
    else
    {
        dprintf("Failed to paste/patch memory at %p with %lu bytes\n", sel.start, binary->index);
    }
}

PLUG_EXPORT void CBMENUENTRY(CBTYPE /*cbType*/, PLUG_CB_MENUENTRY *info)
{
    switch (info->hEntry)
    {
        case MENU_DISASM_PASTE:
            MakeTomatoPaste(GUI_DISASSEMBLY, FALSE);
            break;

        case MENU_DISASM_PATCH:
            MakeTomatoPaste(GUI_DISASSEMBLY, TRUE);
            break;

        case MENU_DUMP_PASTE:
            MakeTomatoPaste(GUI_DUMP, FALSE);
            break;

        case MENU_DUMP_PATCH:
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
        MessageBoxA(g_hWndDlg, OUT_OFF_MEMORY, PLUGIN_NAME, MB_ICONSTOP);
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
    constexpr LPCTSTR PASTE_MENU = "Paste (ignore selected size)";
    constexpr LPCTSTR PATCH_MENU = "Patch (ignore selected size)";
    constexpr LPCTSTR ABOUT_MENU = "About";

    _plugin_menuaddentry(g_hMenuDisasm, MENU_DISASM_PASTE, PASTE_MENU);
    _plugin_menuaddentry(g_hMenuDisasm, MENU_DISASM_PATCH, PATCH_MENU);
    _plugin_menuaddentry(g_hMenuDump, MENU_DUMP_PASTE, PASTE_MENU);
    _plugin_menuaddentry(g_hMenuDump, MENU_DUMP_PATCH, PATCH_MENU);
    _plugin_menuaddentry(g_hMenuDisasm, MENU_DISASM_ABOUT, ABOUT_MENU);
    _plugin_menuaddentry(g_hMenuDump, MENU_DUMP_ABOUT, ABOUT_MENU);
}
