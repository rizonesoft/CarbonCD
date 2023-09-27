#include "stdafx.h"
#include "CDM.h"
#include "TocWindow.h"
#include "Setting.h"
#include "CDMDlg.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_TOC + i])

static UINT indicators[] =
{
    ID_SEPARATOR,
    IDS_TOTALTIME,
    IDS_TOTALSIZE,
};

// CTocWindow

IMPLEMENT_DYNCREATE(CTocWindow, CFrameWnd)

CTocWindow::CTocWindow()
    : m_CD(nullptr)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_TOCFRAME);
}

CTocWindow::~CTocWindow()
{
}


BEGIN_MESSAGE_MAP(CTocWindow, CFrameWnd)
    ON_WM_CREATE()
    ON_COMMAND(ID_WINDOW_CLOSE, OnCloseWindow)
    ON_WM_CLOSE()
    ON_WM_SHOWWINDOW()
    ON_COMMAND(ID_FILE_SAVETXT, OnFileSavetxt)
    ON_COMMAND(ID_FILE_SAVEHTML, OnFileSavehtml)
    ON_COMMAND(ID_FILE_SAVECSV, OnFileSavecsv)
    ON_WM_MOVE()
    ON_WM_SIZE()
    ON_COMMAND(ID_FILE_SAVECUE, OnFileSavecue)
    //  ON_COMMAND(ID_TOOL_READTRACK, OnToolReadtrack)
END_MESSAGE_MAP()


int CTocWindow::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    {
        RECT rect;
        GetWindowRect(&rect);
        SetWindowPos(&wndTop, rect.left, rect.top, 630, 300, SWP_NOZORDER);
    }
    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    if (!m_View.Create(LVS_REPORT | LVS_SHOWSELALWAYS, CRect(0, 0, 0, 0), this, AFX_IDW_PANE_FIRST))
    {
        return -1;
    }

    m_View.SetExtendedStyle(LVS_EX_FULLROWSELECT);

    if (!m_StatusBar.Create(this) ||
        !m_StatusBar.SetIndicators(indicators, sizeof (indicators) / sizeof(UINT)))
    {
        return -1;
    }

    m_View.ShowWindow(SW_SHOW);
    m_View.InsertColumn(0, STR(1), LVCFMT_LEFT, 40, 0);
    m_View.InsertColumn(1, STR(2), LVCFMT_CENTER, 50, 1);
    m_View.InsertColumn(2, STR(3), LVCFMT_CENTER, 80, 2);
    m_View.InsertColumn(3, STR(4), LVCFMT_CENTER, 80, 3);
    m_View.InsertColumn(4, STR(5), LVCFMT_CENTER, 80, 4);
    m_View.InsertColumn(5, STR(6), LVCFMT_RIGHT, 100, 5);
    m_View.InsertColumn(6, STR(7), LVCFMT_CENTER, 80, 6);
    m_View.InsertColumn(7, STR(8), LVCFMT_CENTER, 80, 7);

    if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
    {
        return -1;
    }

    return 0;
}

BOOL CTocWindow::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CFrameWnd::PreCreateWindow(cs))
    {
        return FALSE;
    }

    cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
    cs.lpszClass = AfxRegisterWndClass(0);
    return TRUE;
}

void CTocWindow::OnCloseWindow()
{
    ShowWindow(SW_HIDE);
}

void CTocWindow::OnClose()
{
    ShowWindow(SW_HIDE);
}

void CTocWindow::SetTOC(TableOfContents* Toc)
{
    int Track, Id;
    CString cs;
    MSFAddress msf, prev;
    m_View.DeleteAllItems();
    //  Toc = m_CD->GetTOC();
    prev = Toc->m_Track[0].m_MSF;

    for (Track = 0; Track < Toc->m_LastTrack; Track++)
    {
        //   set track no.
        if (Toc->m_Track[Track].m_Session == 0)
        {
            cs.Format("%d", Toc->m_Track[Track].m_TrackNo);
        }

        else
        {
            cs.Format("%d/%d", Toc->m_Track[Track].m_Session, Toc->m_Track[Track].m_TrackNo);
        }

        Id = m_View.InsertItem(m_View.GetItemCount(), cs);

        //   set track type
        if (Toc->m_Track[Track].m_TrackType == TRACKTYPE_AUDIO)
        {
            m_View.SetItemText(Id, 1, "Audio");
        }

        else
        {
            m_View.SetItemText(Id, 1, "Data");
        }

        //   set start position
        //      msf = Toc->m_Track[Track].m_MSF.GetByLBA();
        cs.Format("%02d:%02d.%02d", Toc->m_Track[Track].m_MSF.Minute, Toc->m_Track[Track].m_MSF.Second,
                  Toc->m_Track[Track].m_MSF.Frame);
        m_View.SetItemText(Id, 2, cs);
        //   set Track Length
        msf = Toc->m_Track[Track].m_EndMSF.GetByLBA() - Toc->m_Track[Track].m_MSF.GetByLBA();
        cs.Format("%02d:%02d.%02d", msf.Minute, msf.Second, msf.Frame);
        m_View.SetItemText(Id, 3, cs);

        //   set gap
        if (prev == Toc->m_Track[Track].m_MSF)
        {
            m_View.SetItemText(Id, 4, "--:--.--");
        }

        else
        {
            cs.Format("%02d:%02d.%02d", prev.Minute, prev.Second, prev.Frame);
            m_View.SetItemText(Id, 4, cs);
        }

        prev = Toc->m_Track[Track].m_EndMSF;
        //   set file size
        cs.Format("%10.2f MB",
                  (msf.GetByLBA() * 2352) / 1048576.0);
        m_View.SetItemText(Id, 5, cs);

        //   set digital copy flag
        if (Toc->m_Track[Track].m_DigitalCopy == TRACKFLAG_YES)
        {
            m_View.SetItemText(Id, 6, STR(9));
        }

        else if (Toc->m_Track[Track].m_DigitalCopy == TRACKFLAG_NO)
        {
            m_View.SetItemText(Id, 6, STR(10));
        }

        else
        {
            m_View.SetItemText(Id, 6, "----");
        }

        //   set pre-emphasis flag
        if (Toc->m_Track[Track].m_Emphasis == TRACKFLAG_YES)
        {
            m_View.SetItemText(Id, 7, "50/15");
        }

        else if (Toc->m_Track[Track].m_Emphasis == TRACKFLAG_NO)
        {
            m_View.SetItemText(Id, 7, STR(11));
        }

        else
        {
            m_View.SetItemText(Id, 7, "----");
        }
    }

    msf = Toc->m_Track[Toc->m_LastTrack].m_MSF - Toc->m_Track[0].m_MSF;
    cs.Format("%s %02d:%02d.%02d", STR(12), msf.Minute, msf.Second, msf.Frame);
    m_StatusBar.SetPaneText(1, cs, TRUE);
    cs.Format("%10.2f MB", (msf.GetByLBA() * 2352) / 1048576.0);
    m_StatusBar.SetPaneText(2, cs, TRUE);
}

void CTocWindow::ClearList(void)
{
    m_View.DeleteAllItems();
    m_StatusBar.SetPaneText(1, "00:00.00", TRUE);
    m_StatusBar.SetPaneText(2, "0.00 MB", TRUE);
}

void CTocWindow::ViewDriveName(LPCSTR DriveName)
{
    m_StatusBar.GetStatusBarCtrl().SetText(DriveName, 0, SBT_NOBORDERS);
}

bool CTocWindow::CheckSelect(TableOfContents* Toc)
{
    POSITION pos;
    bool RetValue;
    int i;

    for (i = 0; i < 99; i++)
    {
        Toc->m_Track[i].m_SelectFlag = 0;
    }

    RetValue = false;
    pos = m_View.GetFirstSelectedItemPosition();

    while (pos)
    {
        RetValue = true;
        i = m_View.GetNextSelectedItem(pos);
        Toc->m_Track[i].m_SelectFlag = 1;
    }

    return RetValue;
}

int CTocWindow::GetItemCount(void)
{
    return m_View.GetItemCount();
}

void CTocWindow::OnShowWindow(BOOL bShow, UINT nStatus)
{
    CFrameWnd::OnShowWindow(bShow, nStatus);
    theSetting.m_ShowTocWnd = bShow;
}

void CTocWindow::OnFileSavetxt()
{
    CFileDialog dlg(FALSE, ".txt", nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, STR(13));

    if (dlg.DoModal() == IDOK)
    {
        CString TrackType, Start, Length, Gap, Size, DigitalCopy, Emphasis, cs;
        int i;
        FILE* fp;
        fp = fopen(dlg.GetPathName(), "w");

        if (fp == nullptr)
        {
            MessageBox(MSG(133), ERROR_MSG, MB_OK);
            return;
        }

        cs = m_StatusBar.GetStatusBarCtrl().GetText(0);
        fprintf(fp, "Drive :%s\n", cs);
        cs = m_StatusBar.GetStatusBarCtrl().GetText(1);
        fprintf(fp, "%s", cs);
        cs = m_StatusBar.GetStatusBarCtrl().GetText(2);
        fprintf(fp, " %s\n", cs);
        fprintf(fp, "Tr Type  Start    Length   PreGap   Size          COPY Emphasis\n");

        for (i = 0; i < m_View.GetItemCount(); i++)
        {
            TrackType = m_View.GetItemText(i, 1);
            Start = m_View.GetItemText(i, 2);
            Length = m_View.GetItemText(i, 3);
            Gap = m_View.GetItemText(i, 4);
            Size = m_View.GetItemText(i, 5);
            DigitalCopy = m_View.GetItemText(i, 6);
            Emphasis = m_View.GetItemText(i, 7);
            fprintf(fp, "%02d %5s %8s %8s %8s %13s %4s %s\n", i, TrackType, Start, Length, Gap, Size, DigitalCopy,
                    Emphasis);
        }

        fclose(fp);
    }
}

void CTocWindow::OnFileSavehtml()
{
    CFileDialog dlg(FALSE, ".html", nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, STR(14));

    if (dlg.DoModal() == IDOK)
    {
        CString TrackType, Start, Length, Gap, Size, DigitalCopy, Emphasis, cs;
        int i;
        FILE* fp;
        fp = fopen(dlg.GetPathName(), "w");

        if (fp == nullptr)
        {
            MessageBox(MSG(133), ERROR_MSG, MB_OK);
            return;
        }

        cs = m_StatusBar.GetStatusBarCtrl().GetText(0);
        fprintf(fp, "<html>\n<head>\n");
        fprintf(fp, "<title>%s</title>\n", cs);
        fprintf(fp, "</head>\n<body>\n");
        fprintf(fp, "<table border=\"1\" align=\"center\">\n");
        fprintf(fp, "<tr><td colspan=\"8\" align=\"center\">%s</td></tr>\n", cs);
        cs = m_StatusBar.GetStatusBarCtrl().GetText(1);
        fprintf(fp, "<tr><td colspan=\"4\" align=\"center\">%s</td>", cs);
        cs = m_StatusBar.GetStatusBarCtrl().GetText(2);
        fprintf(fp, " <td colspan=\"4\" align=\"center\">%s</td></tr>\n", cs);
        fprintf(
            fp,
            "<tr><td>ƒgƒ‰ƒbƒN</td><td>Ží—Þ</td><td>ŠJŽnˆÊ’u</td><td>ƒgƒ‰ƒbƒN’·</td><td>ƒMƒƒƒbƒv</td><td>ƒTƒCƒY</td><td>ƒfƒWƒ^ƒ‹ƒRƒs[</td><td>ƒGƒ“ƒtƒ@ƒVƒX</td></tr>\n");

        for (i = 0; i < m_View.GetItemCount(); i++)
        {
            TrackType = m_View.GetItemText(i, 1);
            Start = m_View.GetItemText(i, 2);
            Length = m_View.GetItemText(i, 3);
            Gap = m_View.GetItemText(i, 4);
            Size = m_View.GetItemText(i, 5);
            DigitalCopy = m_View.GetItemText(i, 6);
            Emphasis = m_View.GetItemText(i, 7);
            fprintf(
                fp,
                "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
                i, TrackType, Start, Length, Gap, Size, DigitalCopy, Emphasis);
        }

        fprintf(fp, "</body>\n</html>\n");
        fclose(fp);
    }
}

void CTocWindow::OnFileSavecsv()
{
    CFileDialog dlg(FALSE, ".csv", nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, STR(15));

    if (dlg.DoModal() == IDOK)
    {
        CString TrackType, Start, Length, Gap, Size, DigitalCopy, Emphasis, cs;
        int i;
        FILE* fp;
        fp = fopen(dlg.GetPathName(), "w");

        if (fp == nullptr)
        {
            MessageBox(MSG(133), ERROR_MSG, MB_OK);
            return;
        }

        cs = m_StatusBar.GetStatusBarCtrl().GetText(0);
        fprintf(fp, "\"Drive :%s\"\n", cs);
        cs = m_StatusBar.GetStatusBarCtrl().GetText(1);
        fprintf(fp, "\"%s\",", cs);
        cs = m_StatusBar.GetStatusBarCtrl().GetText(2);
        fprintf(fp, "\"%s\"\n", cs);
        fprintf(fp, "\"Tr\",\"Type\",\"Start\",\"Length\",\"PreGap\",\"Size\",\"COPY\",\"Emphasis\"\n");

        for (i = 0; i < m_View.GetItemCount(); i++)
        {
            TrackType = m_View.GetItemText(i, 1);
            Start = m_View.GetItemText(i, 2);
            Length = m_View.GetItemText(i, 3);
            Gap = m_View.GetItemText(i, 4);
            Size = m_View.GetItemText(i, 5);
            DigitalCopy = m_View.GetItemText(i, 6);
            Emphasis = m_View.GetItemText(i, 7);
            fprintf(fp, "\"%d\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n", i, TrackType, Start, Length, Gap,
                    Size, DigitalCopy, Emphasis);
        }

        fclose(fp);
    }
}

BOOL CTocWindow::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
    SetLanguage();
    return CFrameWnd::OnCreateClient(lpcs, pContext);
}

void CTocWindow::OnMove(int x, int y)
{
    CFrameWnd::OnMove(x, y);

    if (GetStyle() & 0x10000000)
    {
        RECT r;
        GetWindowRect(&r);
        theSetting.m_TocWnd.left = r.left;
        theSetting.m_TocWnd.top = r.top;
        theSetting.m_TocWnd.right = r.right - r.left;
        theSetting.m_TocWnd.bottom = r.bottom - r.top;
    }
}

void CTocWindow::OnSize(UINT nType, int cx, int cy)
{
    CFrameWnd::OnSize(nType, cx, cy);

    if (GetStyle() & 0x10000000)
    {
        RECT r;
        GetWindowRect(&r);
        theSetting.m_TocWnd.left = r.left;
        theSetting.m_TocWnd.top = r.top;
        theSetting.m_TocWnd.right = r.right - r.left;
        theSetting.m_TocWnd.bottom = r.bottom - r.top;
    }
}

void CTocWindow::OnFileSavecue()
{
    CFileDialog dlg(FALSE, ".cue", nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, MSG(22));

    if (dlg.DoModal() == IDOK)
    {
        FILE* fp;
        TableOfContents* Toc;
        MSFAddress PrevAddr, Tmp;
        DWORD HeadLBA;
        int track;
        char fn[256];
        fp = fopen(dlg.GetPathName(), "w");

        if (fp == nullptr)
        {
            CString cs;
            cs.Format(MSG(106), dlg.GetPathName());
            MessageBox(cs);
            return;
        }

        lstrcpy(fn, dlg.GetFileName());
        fn[lstrlen(fn) - dlg.GetFileExt().GetLength()] = '\0';
        Toc = m_CD->GetTOC();
        PrevAddr = 150;
        HeadLBA = 150;
        fprintf(fp, "FILE \"%sbin\" BINARY\n", fn);

        for (track = 0; track < Toc->m_LastTrack; track++)
        {
            if (Toc->m_Track[track].m_TrackType == TRACKTYPE_AUDIO)
            {
                fprintf(fp, "  TRACK %02d AUDIO\n", track + 1);
            }

            else if (Toc->m_Track[track].m_TrackType == TRACKTYPE_DATA)
            {
                fprintf(fp, "  TRACK %02d MODE1/2352\n", track + 1);
            }

            else if (Toc->m_Track[track].m_TrackType == TRACKTYPE_DATA_2)
            {
                fprintf(fp, "  TRACK %02d MODE2/2352\n", track + 1);
            }

            if (!(PrevAddr.GetByLBA() == Toc->m_Track[track].m_MSF.GetByLBA()))
            {
                Tmp = PrevAddr - 150;
                fprintf(fp, "    INDEX 00 %02d:%02d:%02d\n", Tmp.Minute, Tmp.Second, Tmp.Frame);
            }

            Tmp = Toc->m_Track[track].m_MSF.GetByLBA() - HeadLBA;
            fprintf(fp, "    INDEX 01 %02d:%02d:%02d\n", Tmp.Minute, Tmp.Second, Tmp.Frame);
            PrevAddr = Toc->m_Track[track].m_EndMSF;
        }

        fclose(fp);
    }
}

void CTocWindow::SetLanguage(void)
{
    int i;
    DWORD MenuString[6][2] =
    {
        {ID_FILE_SAVETXT, 2},
        {ID_FILE_SAVEHTML, 3},
        {ID_FILE_SAVECSV, 4},
        {ID_FILE_SAVECUE, 5},
        {ID_WINDOW_CLOSE, 6},
    };

    for (i = 0; i < 2; i++)
    {
        GetMenu()->ModifyMenu(i, MF_BYPOSITION | MF_STRING, 0, theSetting.m_Lang.m_Str[LP_TOCMENU + i]);
    }

    for (i = 0; i < 5; i++)
    {
        GetMenu()->ModifyMenu(MenuString[i][0], MF_BYCOMMAND | MF_STRING, MenuString[i][0],
                              theSetting.m_Lang.m_Str[LP_TOCMENU + MenuString[i][1]]);
    }

    SetWindowText(STR(0));

    if (m_View.GetHeaderCtrl() != nullptr)
    {
        HDITEM hi;
        int i;
        char Buffer[100];

        for (i = 0; i < 8; i++)
        {
            hi.mask = HDI_TEXT;
            lstrcpy(Buffer, STR(i + 1));
            hi.pszText = Buffer;
            m_View.GetHeaderCtrl()->SetItem(i, &hi);
        }
    }
}

//void CTocWindow::OnToolReadtrack()
//{
//  CCDMDlg *Parent = (CCDMDlg*)GetParent();
//
//  Parent->OnToolReadtrack();
////    GetParent()->PostMessage(WM_COMMAND,ID_TOOL_READTRACK,0);
//}
