#include "stdafx.h"
#include "CDM.h"
#include "LogWindow.h"
#include "Setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_LOG + i])

// CLogWindow

IMPLEMENT_DYNCREATE(CLogWindow, CFrameWnd)

CLogWindow::CLogWindow()
{
    m_Flag = 0;
    m_hIcon = AfxGetApp()->LoadIcon(IDR_TOCFRAME);
}

CLogWindow::~CLogWindow()
{
}


BEGIN_MESSAGE_MAP(CLogWindow, CFrameWnd)
    ON_WM_CREATE()
    ON_WM_CLOSE()
    ON_COMMAND(ID_WINDOW_CLOSE, OnWindowClose)
    ON_WM_SIZE()
    ON_COMMAND(ID_SAVELOG, OnSavelog)
    ON_WM_SHOWWINDOW()
    ON_WM_MOVE()
    ON_COMMAND(ID_CLEARLOG, OnClearlog)
END_MESSAGE_MAP()


int CLogWindow::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    //   set window size
    SetWindowPos(&wndTop, 0, 0, 400, 200, SWP_NOZORDER | SWP_NOMOVE);
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    //   create list controller
    if (!m_View.Create(LVS_REPORT | LVS_SINGLESEL | LVS_NOCOLUMNHEADER, CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST))
    {
        return -1;
    }

    m_View.SetExtendedStyle(LVS_EX_FULLROWSELECT);
    m_View.ShowWindow(SW_SHOW);
    m_View.InsertColumn(0, "Message", LVCFMT_LEFT, 100, 0);
    //   set image to view
    m_ImageList.Create(IDB_LOGTYPE, 12, 4, 0xffffff);
    m_View.SetImageList(&m_ImageList, LVSIL_SMALL);
    m_Flag |= 1;
    return 0;
}

void CLogWindow::OnClose()
{
    ShowWindow(SW_HIDE);
    //  CFrameWnd::OnClose();
}

void CLogWindow::OnWindowClose()
{
    ShowWindow(SW_HIDE);
}

BOOL CLogWindow::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs))
    {
        return FALSE;
    }

    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.lpszClass = AfxRegisterWndClass(0);
    return TRUE;
}

void CLogWindow::AddMessage(int Type, LPCSTR Message)
{
    int id;
    CSize size;
    CString RegistMsg;
    CTime tm = CTime::GetCurrentTime();
    size.cy = 40;

    if ((m_Flag & 1) == 0)
    {
        return;
    }

    RegistMsg.Format("%d/%d/%d %02d:%02d:%02d %s",
                     tm.GetYear(), tm.GetMonth(), tm.GetDay(),
                     tm.GetHour(), tm.GetMinute(), tm.GetSecond(),
                     Message);
    id = m_View.InsertItem(m_View.GetItemCount(), RegistMsg, Type);
    m_View.SetItemData(id, static_cast<DWORD>(Type));
    m_View.Scroll(size);
}

void CLogWindow::OnSavelog()
{
    CFileDialog dlg(FALSE, ".txt", nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, STR(1));

    if (dlg.DoModal() == IDOK)
    {
        char* TypeList[4] = {"O ", "? ", "X ", "! "};
        FILE* fp;
        CString cs;
        int i, Type;
        fp = fopen(dlg.GetPathName(), "w");

        if (fp == nullptr)
        {
            MessageBox(MSG(82), ERROR_MSG, MB_OK);
            return;
        }

        for (i = 0; i < m_View.GetItemCount(); i++)
        {
            cs = m_View.GetItemText(i, 0);
            Type = static_cast<int>(m_View.GetItemData(i));
            fprintf(fp, "%s %s\n", TypeList[Type], cs);
        }

        fclose(fp);
    }
}

void CLogWindow::AutoSave(void)
{
    if (theSetting.m_AutoLogFile == "")
    {
        return;
    }

    char* TypeList[4] = {"O ", "? ", "X ", "! "};
    FILE* fp;
    CString cs;
    int i, Type;
    fp = fopen(theSetting.m_AutoLogFile, "w");

    if (fp == nullptr)
    {
        MessageBox(MSG(82), ERROR_MSG, MB_OK);
        return;
    }

    for (i = 0; i < m_View.GetItemCount(); i++)
    {
        cs = m_View.GetItemText(i, 0);
        Type = static_cast<int>(m_View.GetItemData(i));
        fprintf(fp, "%s %s\n", TypeList[Type], cs);
    }

    fclose(fp);
}

void CLogWindow::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CFrameWnd::OnShowWindow(bShow, nStatus);
    theSetting.m_ShowLogWnd = bShow;
}

BOOL CLogWindow::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
    SetLanguage();
    return CFrameWnd::OnCreateClient(lpcs, pContext);
}

void CLogWindow::OnMove(int x, int y)
{
    CFrameWnd::OnMove(x, y);

    if (GetStyle() & 0x10000000)
    {
        RECT r;
        GetWindowRect(&r);
        theSetting.m_LogWnd.left = r.left;
        theSetting.m_LogWnd.top = r.top;
        theSetting.m_LogWnd.right = r.right - r.left;
        theSetting.m_LogWnd.bottom = r.bottom - r.top;
    }
}

void CLogWindow::OnSize(UINT nType, int cx, int cy)
{
    CFrameWnd::OnSize(nType, cx, cy);

    if ((m_Flag & 1) == 1)
    {
        RECT r;
        HDITEM hi;
        m_View.GetClientRect(&r);
        hi.mask = HDI_WIDTH;
        hi.cxy = r.right - r.left - 20;
        m_View.GetHeaderCtrl()->SetItem(0, &hi);
    }

    if (GetStyle() & 0x10000000)
    {
        RECT r;
        GetWindowRect(&r);
        theSetting.m_LogWnd.left = r.left;
        theSetting.m_LogWnd.top = r.top;
        theSetting.m_LogWnd.right = r.right - r.left;
        theSetting.m_LogWnd.bottom = r.bottom - r.top;
    }
}

void CLogWindow::SetLanguage(void)
{
    int i;
    DWORD MenuString[3][2] =
    {
        {ID_SAVELOG, 2},
        {ID_WINDOW_CLOSE, 3},
        {ID_CLEARLOG, 4},
    };

    for (i = 0; i < 2; i++)
    {
        GetMenu()->ModifyMenu(i, MF_BYPOSITION | MF_STRING, 0, theSetting.m_Lang.m_Str[LP_LOGMENU + i]);
    }

    for (i = 0; i < 3; i++)
    {
        GetMenu()->ModifyMenu(MenuString[i][0], MF_BYCOMMAND | MF_STRING, MenuString[i][0],
                              theSetting.m_Lang.m_Str[LP_LOGMENU + MenuString[i][1]]);
    }

    SetWindowText(STR(0));
}

void CLogWindow::OnClearlog()
{
    if (MessageBox(MSG(121), CONF_MSG, MB_YESNO) == IDYES)
    {
        m_View.DeleteAllItems();
    }
}

void CLogWindow::DumpSystemVersion(void)
{
    DWORD CheckTable[7][2] = {{4, 0}, {4, 10}, {4, 90}, {103, 51}, {104, 0}, {105, 0}, {105, 1}};
    LPCSTR VersionTable[8] =
    {
        "Windows 95",
        "Windows 98",
        "Windows Me",
        "Windows NT 3.5",
        "Windows NT 4.0",
        "Windows 2000",
        "Windows XP/.NET Server",
        "Unknown Windows ver."
    };
    OSVERSIONINFO osver;
    int i;
    CString cs;
    osver.dwOSVersionInfoSize = sizeof (osver);

    if (GetVersionEx(&osver))
    {
        if (osver.dwPlatformId == VER_PLATFORM_WIN32_NT)
        {
            osver.dwMajorVersion += 100;
        }

        for (i = 0; i < 7; i++)
        {
            if (osver.dwMajorVersion == CheckTable[i][0] && osver.dwMinorVersion == CheckTable[i][1])
            {
                break;
            }
        }
    }

    else
    {
        i = 7;
    }

    cs.Format("%s", VersionTable[i]);
    AddMessage(LOG_INFO, cs);
}
