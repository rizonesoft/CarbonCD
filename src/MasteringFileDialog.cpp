#include "stdafx.h"
#include "CDM.h"
#include "MasteringFileDialog.h"
#include "IsoCreator.h"
#include "CreateProgressDialog.h"
#include "WriteSettingDialog.h"
#include "WriteProgressDialog.h"
#include "Setting.h"

//   test
#include "ThemeController.h"
#include "./masteringfiledialog.h"

#define STR(i)  (theSetting.m_Lang.m_Str[LP_MASTERING + i])

IMPLEMENT_DYNAMIC(CMasteringFileDialog, CDialog)

CMasteringFileDialog::CMasteringFileDialog(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
      , m_Size(_T(""))
      , m_LogWnd(nullptr)
{
    m_LeadOutPos = 0;
    m_TrackList = &(m_Page1.m_TrackList);
    m_VolumeLabel = &(m_Page2.m_VolumeLabel);
    m_List = &(m_Page2.m_List);
    m_Tree = &(m_Page2.m_Tree);
    m_ImageSize = 0;
}

CMasteringFileDialog::~CMasteringFileDialog()
{
}

void CMasteringFileDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_TAB, m_Tab);
    DDX_Text(pDX, IDC_IMAGESIZE, m_Size);
    DDX_Control(pDX, IDC_DRIVELIST, m_DriveList);
}


BEGIN_MESSAGE_MAP(CMasteringFileDialog, CDialog)
    ON_COMMAND(ID_EDIT_ADDFOLDER, OnEditAddfolder)
    ON_COMMAND(ID_EDIT_LABEL, OnEditLabel)
    ON_COMMAND(ID_EDIT_DELETEFOLDER, OnEditDeletefolder)
    ON_COMMAND(ID_EDIT_ADDFILE, OnEditAddfile)
    ON_BN_CLICKED(IDC_CREATE_ISO, OnBnClickedCreateIso)
    ON_BN_CLICKED(IDOK, OnBnClickedOk)
    ON_COMMAND(ID_EDIT_ADDAUDIO, OnEditAddaudio)
    ON_COMMAND(ID_EDIT_DELETETRACK, OnEditDeletetrack)
    ON_COMMAND(ID_EDIT_ADDDATA, OnEditAdddata)
    ON_NOTIFY(TCN_SELCHANGE, IDC_TAB, OnTcnSelchangeTab)
    ON_BN_CLICKED(IDC_WRITING, OnBnClickedWriting)
    ON_COMMAND(ID_WINDOW_CLOSE, OnWindowClose)
    ON_COMMAND(ID_EDIT_INSERTFOLDER, OnEditInsertfolder)
    ON_WM_SETFOCUS()
    ON_COMMAND(ID_TRACK_ISO, OnTrackIso)
    ON_BN_CLICKED(IDC_EXPLORER, OnBnClickedExplorer)
    ON_CBN_SELCHANGE(IDC_DRIVELIST, OnCbnSelchangeDrivelist)
    ON_COMMAND(ID_CD_ERASE, OnCdErase)
    ON_COMMAND(ID_CD_ERASE_FAST, OnCdEraseFast)
END_MESSAGE_MAP()


BOOL CMasteringFileDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_Dir.m_RealFileName = "image.iso";
    m_Dir.m_ImageFileName = "ISO_Image:\\";
    m_Tab.InsertItem(0, STR(3));
    m_Tab.InsertItem(1, STR(4));
    m_Tab.InsertItem(2, STR(19));
    m_Tab.SetCurSel(1);
    m_Size = STR(5) + " 00:00:00";
    SetLanguage();
    m_Page1.m_MainDialog = this;
    m_Page2.m_Dir = &m_Dir;
    m_Page2.m_Page1 = &m_Page1;
    m_Page2.m_MainDialog = this;
    m_Page3.m_MainDialog = this;
    m_Page1.Create(IDD_MASTERING_1, this);
    m_Page2.Create(IDD_MASTERING_2, this);
    m_Page3.Create(IDD_MASTERING_3, this);
    m_TrackList = &(m_Page1.m_TrackList);
    theTheme.EnableThemeDialogTexture(m_Page1.m_hWnd, ETDT_ENABLETAB);
    theTheme.EnableThemeDialogTexture(m_Page2.m_hWnd, ETDT_ENABLETAB);
    theTheme.EnableThemeDialogTexture(m_Page3.m_hWnd, ETDT_ENABLETAB);
    ChangeTab();
    UpdateDialog(FALSE);
    CalcSize();
    m_Tab.SetWindowPos(&wndBottom, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    m_DriveList.InitializeShortVer(m_CD->GetAspiCtrl());
    m_DriveList.SetCurSel(m_CD->GetAspiCtrl()->GetCurrentDevice());

    if (theSetting.m_Mastering_AlwaysOnTop)
    {
        SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    return TRUE; // return TRUE unless you set the focus to a control
}

void CMasteringFileDialog::OnEditAddfolder()
{
    if (m_Page2.GetStyle() & 0x10000000)
    {
        CString cs;
        int i;
        HTREEITEM ht;
        ht = m_Tree->GetSelectedItem();

        for (i = 0; i < 100; i++)
        {
            if (i == 0)
            {
                cs = MSG(83);
            }

            else
            {
                cs.Format(MSG(84), i);
            }

            if (m_Page2.AddFolder(cs))
            {
                m_Tree->Expand(ht, TVE_EXPAND);
                break;
            }
        }
    }
}

void CMasteringFileDialog::OnEditLabel()
{
    if (m_Page2.GetStyle() & 0x10000000)
    {
        int id;
        POSITION pos;
        pos = m_List->GetFirstSelectedItemPosition();

        if (pos != nullptr)
        {
            id = m_List->GetNextSelectedItem(pos);
            m_List->EditLabel(id);
        }
    }
}

void CMasteringFileDialog::OnEditDeletefolder()
{
    if (m_Page2.GetStyle() & 0x10000000)
    {
        m_Page2.DeleteSelectedItems();
    }
}

void CMasteringFileDialog::OnEditAddfile()
{
    if (m_Page2.GetStyle() & 0x10000000)
    {
        CFileDialog Dlg(TRUE);
        UpdateDialog(TRUE);

        if (Dlg.DoModal() == IDOK)
        {
            m_Page2.AddFile(Dlg.GetPathName());
        }

        UpdateDialog(FALSE);
        CalcSize();
    }
}

void CMasteringFileDialog::OnBnClickedOk()
{
    OnOK();
}

void CMasteringFileDialog::OnEditAddaudio()
{
    if (m_Page1.GetStyle() & 0x10000000)
    {
        m_Page1.InsertWaveAudioTrack();
    }
}

void CMasteringFileDialog::OnEditDeletetrack()
{
    if (m_Page1.GetStyle() & 0x10000000)
    {
        m_Page1.DeleteSelectedTracks();
    }
}

void CMasteringFileDialog::OnEditAdddata()
{
    if (m_Page1.GetStyle() & 0x10000000)
    {
        m_Page1.InsertMode1MasteringTrack();
        m_Tab.SetCurSel(1);
        ChangeTab();
    }
}

void CMasteringFileDialog::OnTrackIso()
{
    if (m_Page1.GetStyle() & 0x10000000)
    {
        CFileDialog dlg(TRUE, nullptr, nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_ALLOWMULTISELECT, MSG(90));
        char buf[2048];
        char* p;
        m_Page1.UpdateData(TRUE);
        buf[0] = '\0';
        p = dlg.m_ofn.lpstrFile;
        dlg.m_ofn.lpstrFile = buf;
        dlg.m_ofn.nMaxFile = 2048;

        if (dlg.DoModal() == IDOK)
        {
            m_Page1.InsertIsoTrack(dlg.GetPathName());
        }

        dlg.m_ofn.lpstrFile = p;
        m_Page1.UpdateData(FALSE);
        CalcSize();
    }
}

void CMasteringFileDialog::OnTcnSelchangeTab(NMHDR* pNMHDR, LRESULT* pResult)
{
    *pResult = 0;
    ChangeTab();
}

void CMasteringFileDialog::OnBnClickedCreateIso()
{
    CFileDialog Dlg(FALSE, ".cue", "", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, MSG(91));
    UpdateDialog(TRUE);

    if (m_TrackList->GetItemCount() == 0)
    {
        MessageBox(MSG(92), CONF_MSG);
        return;
    }

    SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    if (Dlg.DoModal() == IDOK)
    {
        CCreateProgressDialog CreateDlg;
        CreateDlg.CreateIso(Dlg.GetPathName(), *m_VolumeLabel, m_TrackList, &m_Dir, m_LogWnd);
    }

    if (theSetting.m_Mastering_AlwaysOnTop)
    {
        SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void CMasteringFileDialog::OnBnClickedWriting()
{
    CWriteSettingDialog Dlg;
    UpdateDialog(TRUE);

    if (m_TrackList->GetItemCount() == 0)
    {
        MessageBox(MSG(93), CONF_MSG);
        return;
    }

    CalcSize();

    if (m_LeadOutPos > 0 && m_ImageSize > (m_LeadOutPos - 150))
    {
        if (MessageBox(STR(5), CONF_MSG, MB_YESNO) == IDNO)
        {
            return;
        }
    }

    SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    Dlg.m_CD = m_CD;
    Dlg.m_CueSheetName = MSG(94);
    Dlg.m_DisableChangingFile = true;

    if (Dlg.DoModal() == IDOK)
    {
        CWriteProgressDialog WriteDlg;
        WriteDlg.Mastering(&m_Dir, *m_VolumeLabel, m_TrackList, m_CD, m_LogWnd);
    }

    if (theSetting.m_Mastering_AlwaysOnTop)
    {
        SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void CMasteringFileDialog::GetLeadOutPos(void)
{
    BYTE Buffer[400];
    memset(Buffer, 0, 400);

    if (m_CD->ReadATIP(Buffer))
    {
        m_LeadOutPos = ((Buffer[12] * 60) + Buffer[13]) * 75 + Buffer[14];
    }

    else
    {
        m_LeadOutPos = 0;
    }
}

void CMasteringFileDialog::CalcSize(void)
{
    MSFAddress msf;
    DWORD size;
    int i;
    UpdateData(TRUE);
    size = 0;

    for (i = 0; i < m_TrackList->GetItemCount(); i++)
    {
        if (m_TrackList->GetItemData(i) == 0)
        {
            CString tt;
            tt = m_Page1.m_TrackList.GetItemText(0, 1);

            if (tt == "Mastering")
            {
                CIsoCreator iso;
                iso.SetParams("", theSetting.m_CopyProtectionSize);
                iso.CreateJolietHeader(&m_Dir);
                size += iso.GetImageSize();
            }

            else
            {
                DWORD FileSize;
                HANDLE hFile;
                CString cs;
                cs = m_TrackList->GetItemText(i, 2);
                hFile = CreateFile(cs, 0, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                FileSize = GetFileSize(hFile, nullptr);
                CloseHandle(hFile);

                if (tt == "MODE1/2048")
                {
                    size += FileSize / 2048;
                }

                else if (tt == "MODE1/2352" || tt == "MODE2/2352")
                {
                    size += FileSize / 2352;
                }
            }
        }

        else
        {
            HANDLE hFile;
            CString cs;
            cs = m_TrackList->GetItemText(i, 2);
            hFile = CreateFile(cs, 0, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            size += (GetFileSize(hFile, nullptr) + 2352 - 45) / 2352;
            CloseHandle(hFile);
        }
    }

    msf = size;
    m_ImageSize = size;
    GetLeadOutPos();

    if (m_LeadOutPos == 0)
    {
        m_Size.Format("%02d:%02d:%02d / **:**:**", msf.Minute, msf.Second, msf.Frame);
    }

    else
    {
        MSFAddress lo;
        lo = m_LeadOutPos - 150;
        m_Size.Format("%02d:%02d:%02d / %02d:%02d:%02d", msf.Minute, msf.Second, msf.Frame, lo.Minute, lo.Second,
                      lo.Frame);
    }

    UpdateData(FALSE);
}

void CMasteringFileDialog::SetLanguage(void)
{
    int i;
    DWORD MenuString[][2] =
    {
        {IDCANCEL, 4},
        {ID_CREATE_IMAGE, 5},
        {ID_CREATE_CD, 6},
        {ID_EDIT_ADDDATA, 7},
        {ID_EDIT_ADDAUDIO, 8},
        {ID_EDIT_DELETETRACK, 9},
        {ID_EDIT_ADDFOLDER, 10},
        {ID_EDIT_ADDFILE, 11},
        {ID_EDIT_DELETEFOLDER, 12},
        {ID_EDIT_LABEL, 13},
        {ID_EDIT_INSERTFOLDER, 14},
        {ID_TRACK_ISO, 15},
        {IDC_EXPLORER, 16},
    };
    DWORD CtrlString[][2] =
    {
        {IDC_CREATE_ISO, 6},
        {IDC_WRITING, 7},
        {IDCANCEL, 8},
        {IDC_SELECTDRIVE, 20},
        {IDC_EXPLORER, 24},
    };

    for (i = 0; i < 4; i++)
    {
        GetMenu()->ModifyMenu(i, MF_BYPOSITION | MF_STRING, 0, theSetting.m_Lang.m_Str[LP_MASTERINGMENU + i]);
    }

    for (i = 0; i < 13; i++)
    {
        GetMenu()->ModifyMenu(MenuString[i][0], MF_BYCOMMAND | MF_STRING, MenuString[i][0],
                              theSetting.m_Lang.m_Str[LP_MASTERINGMENU + MenuString[i][1]]);
    }

    SetWindowText(STR(0));

    for (i = 0; i < 5; i++)
    {
        this->SetDlgItemText(CtrlString[i][0], STR(CtrlString[i][1]));
    }

    GetMenu()->ModifyMenu(ID_CD_ERASE, MF_BYCOMMAND | MF_STRING, ID_CD_ERASE, theSetting.m_Lang.m_Str[LP_MAINMENU + 9]);
    GetMenu()->ModifyMenu(ID_CD_ERASE_FAST, MF_BYCOMMAND | MF_STRING, ID_CD_ERASE_FAST,
                          theSetting.m_Lang.m_Str[LP_MAINMENU + 10]);
    this->SetDlgItemText(ID_CD_ERASE, theSetting.m_Lang.m_Str[LP_MAINMENU + 9]);
    this->SetDlgItemText(ID_CD_ERASE_FAST, theSetting.m_Lang.m_Str[LP_MAINMENU + 10]);
}

void CMasteringFileDialog::OnWindowClose()
{
    OnOK();
}

void CMasteringFileDialog::OnEditInsertfolder()
{
    if (m_Page2.GetStyle() & 0x10000000)
    {
        UpdateDialog(TRUE);
        BROWSEINFO brinfo;
        LPMALLOC p;
        LPITEMIDLIST brid;
        char PathList[MAX_PATH + 1];
        char PathName[MAX_PATH];
        PathList[0] = '\0';
        PathName[0] = '\0';
        SHGetMalloc(&p);
        brinfo.hwndOwner = m_hWnd;
        brinfo.pidlRoot = nullptr;
        brinfo.pszDisplayName = PathList;
        brinfo.lpszTitle = STR(18);
        brinfo.ulFlags = BIF_RETURNONLYFSDIRS;
        brinfo.lpfn = nullptr;
        brid = SHBrowseForFolder(&brinfo);

        if (brid)
        {
            SHGetPathFromIDList(brid, PathName);
            p->Free(brid);
            m_Page2.AddFileRec(PathName);
        }

        else
        {
            return;
        }

        UpdateDialog(FALSE);
        CalcSize();
    }
}

void CMasteringFileDialog::ChangeTab(void)
{
    m_Page1.ShowWindow(SW_HIDE);
    m_Page2.ShowWindow(SW_HIDE);
    m_Page3.ShowWindow(SW_HIDE);

    if (m_Tab.GetCurSel() == 0)
    {
        m_Page1.ShowWindow(SW_SHOW);
    }

    else if (m_Tab.GetCurSel() == 1)
    {
        m_Page2.ShowWindow(SW_SHOW);
    }

    else if (m_Tab.GetCurSel() == 2)
    {
        m_Page3.ShowWindow(SW_SHOW);
    }
}

void CMasteringFileDialog::UpdateDialog(bool bSaveAndValidate)
{
    m_Page1.UpdateData(bSaveAndValidate);
    m_Page2.UpdateData(bSaveAndValidate);
    m_Page3.UpdateData(bSaveAndValidate);
    UpdateData(bSaveAndValidate);
}

void CMasteringFileDialog::OnSetFocus(CWnd* pOldWnd)
{
    CDialog::OnSetFocus(pOldWnd);

    if (m_Tab.GetCurSel() == 0)
    {
        m_Page1.SetFocus();
    }

    else if (m_Tab.GetCurSel() == 1)
    {
        m_Page2.SetFocus();
    }

    else if (m_Tab.GetCurSel() == 2)
    {
        m_Page3.SetFocus();
    }
}

void CMasteringFileDialog::OnBnClickedExplorer()
{
    ShellExecute(m_hWnd, "explore", nullptr, nullptr, nullptr, SW_SHOWNORMAL);
}

void CMasteringFileDialog::OnCbnSelchangeDrivelist()
{
    int Index;
    Index = m_DriveList.GetCurSel();

    if (Index == CB_ERR)
    {
        return;
    }

    m_CD->GetAspiCtrl()->SetDevice(Index);
    GetLeadOutPos();
    CalcSize();
}

void CMasteringFileDialog::OnCancel()
{
    CDialog::OnCancel();
}

void CMasteringFileDialog::OnOK()
{
    //  CDialog::OnOK();
}

void CMasteringFileDialog::OnCdErase()
{
    GetParent()->SendMessage(WM_COMMAND, ID_CD_ERASE, 0);
}

void CMasteringFileDialog::OnCdEraseFast()
{
    GetParent()->SendMessage(WM_COMMAND, ID_CD_ERASE_FAST, 0);
}
