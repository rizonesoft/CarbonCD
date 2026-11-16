#include "stdafx.h"
#include "Resource.h"
#include "CDM.h"
#include "CDMDlg.h"
#include "MasteringFileDialog.h"

#include "ReadSettingDialog.h"
#include "ReadProgressDialog.h"
#include "ReadTrackDialog.h"
#include "SettingDialog.h"
#include "SubQProgressDialog.h"
#include "Setting.h"
#include "MsfDialog.h"
#include "WriteProgressDialog.h"
#include "WriteSettingDialog.h"
#include "EraseDialog.h"
#include "DriveCapabilityDialog.h"
#include "MMCWriter.h"
#include "LanguageDialog.h"
#include "Mmsystem.h"

#ifdef _DEBUG
    #define new DEBUG_NEW
#endif

class CAboutDlg : public CDialog
{
public:
    CAboutDlg();

    enum { IDD = IDD_ABOUTBOX };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

protected:
    DECLARE_MESSAGE_MAP()

public:
    CString m_VerString;
    BOOL OnInitDialog() override;
};

CAboutDlg::CAboutDlg() : CDialog(IDD)
                         , m_VerString(_T(""))
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, ID_VERSTRING, m_VerString);
}

BOOL CAboutDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_VerString.Format("Version %s", APP_VERSION);
    SetWindowText(theSetting.m_Lang.m_Str[LP_ABOUT + 0]);
    this->SetDlgItemText(IDC_SUBTITLE, theSetting.m_Lang.m_Str[LP_ABOUT + 1]);
    this->SetDlgItemText(IDC_AUTHOR, "Copyright © 2015 Rizonesoft");
    SetDlgItemText(IDOK, theSetting.m_Lang.m_Str[1]);
    UpdateData(FALSE);
    return TRUE; // return TRUE unless you set the focus to a control
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

CCDMDlg::CCDMDlg(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
      , m_Message(_T(""))
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_TocWnd = nullptr;
    m_LogWnd = nullptr;
}

void CCDMDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DRIVELIST, m_DriveList);
    DDX_Control(pDX, ID_TOOL_CREATEIMAGE, m_CreateImageButton);
    DDX_Control(pDX, ID_CD_WRITE_IMAGE, m_WriteImageButton);
    DDX_Control(pDX, ID_CD_DUPLICATE, m_DuplicateButton);
    DDX_Control(pDX, ID_TOOL_READTRACK, m_ReadTrackButton);
    DDX_Control(pDX, ID_TOOL_MASTERING, m_MasteringButton);
    DDX_Control(pDX, ID_TOOL_SUBQ, m_SubQButton);
    DDX_Control(pDX, ID_CD_RECOGNIZE, m_RecognizeButton);
    DDX_Control(pDX, ID_EXIT, m_ExitButton);
    DDX_Text(pDX, IDC_MESSAGE, m_Message);
}

BEGIN_MESSAGE_MAP(CCDMDlg, CDialog)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_BN_CLICKED(IDOK, OnBnClickedOk)
    ON_COMMAND(ID_EXIT, OnExit)
    ON_COMMAND(ID_VIEW_VERSION, OnViewVersion)
    ON_COMMAND(ID_RS_HOME, OnVisitRizonesoftHome)
    ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
    ON_WM_CLOSE()
    ON_COMMAND(ID_VIEW_TOCWINDOW, OnViewToc)
    //  ON_UPDATE_COMMAND_UI(ID_VIEWTOC, OnUpdateViewToc)
    ON_CBN_SELCHANGE(IDC_DRIVELIST, OnCbnSelchangeDrivelist)
    ON_COMMAND(ID_TOOL_TOCFROMSESSION, OnToolTocfromsession)
    ON_COMMAND(ID_TOOL_CREATEIMAGE, OnToolCreateimage)
    ON_COMMAND(ID_VIEW_LOG, OnViewLog)
    ON_COMMAND(ID_TOOL_READTRACK, OnToolReadtrack)
    ON_CBN_SELCHANGE(IDC_SESSIONLIST, OnCbnSelchangeSessionlist)
    ON_COMMAND(ID_READSETTING, OnGeneralSetting)
    ON_COMMAND(ID_TOOL_SUBQ, OnToolSubq)
    ON_COMMAND(ID_TOOL_MASTERING, OnToolMastering)
    ON_COMMAND(ID_TOOL_MSF, OnToolMsf)
    ON_COMMAND(ID_CD_WRITE_IMAGE, OnCdWriteImage)
    ON_COMMAND(ID_CD_ERASE, OnCdErase)
    ON_COMMAND(ID_CD_ERASE_FAST, OnCdEraseFast)
    ON_COMMAND(ID_CD_RECOGNIZE, OnCdRecognize)
    ON_COMMAND(ID_CD_DUPLICATE, OnCdDuplicate)
    ON_COMMAND(ID_DRIVE_CAPABILITY, OnDriveCapability)
    ON_COMMAND(ID_LANGUAGE, OnLanguage)
    //ON_WM_MOUSEMOVE()
    ON_WM_MOUSEMOVE()
    ON_WM_MOVE()
    //ON_BN_CLICKED(IDC_CHANGESIZE, OnBnClickedChangesize)
    ON_COMMAND(ID_HELP_HELP, OnHelpHelp)
    ON_BN_CLICKED(ID_CD_RECOGNIZE, &CCDMDlg::OnBnClickedCdRecognize)
END_MESSAGE_MAP()


BOOL CCDMDlg::OnInitDialog()
{
    CDialog::OnInitDialog();
    ASSERT(( IDM_ABOUTBOX & 0xFFF0 ) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);
    CMenu* pSysMenu = GetSystemMenu(FALSE);

    if (pSysMenu != nullptr)
    {
        CString strAboutMenu;
        strAboutMenu.LoadString(IDS_ABOUTBOX);

        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);
    InitializeControlls();
    InitializeButtons();
    OnCbnSelchangeDrivelist();
    m_TocWnd->m_CD = &m_CD;
    m_TocWnd->SetWindowPos(&wndTop,
                           theSetting.m_TocWnd.left, theSetting.m_TocWnd.top,
                           theSetting.m_TocWnd.right, theSetting.m_TocWnd.bottom,
                           SWP_NOZORDER);

    if (theSetting.m_ShowTocWnd)
    {
        m_TocWnd->ShowWindow(SW_SHOW);
    }

    m_LogWnd->SetWindowPos(&wndTop,
                           theSetting.m_LogWnd.left, theSetting.m_LogWnd.top,
                           theSetting.m_LogWnd.right, theSetting.m_LogWnd.bottom,
                           SWP_NOZORDER);

    if (theSetting.m_ShowLogWnd)
    {
        m_LogWnd->ShowWindow(SW_SHOW);
    }

    SetWindowPos(&wndTop, theSetting.m_MainDlgPos.x, theSetting.m_MainDlgPos.y, 0, 0, SWP_NOSIZE);
    NotifyButton();
    //   set language
    SetLanguage();

    if (theSetting.m_UseSPTI)
    {
        CString cs;

        if (m_CD.GetAspiCtrl()->IsActive())
        {
            cs.Format("SPTI - OK");
            m_LogWnd->AddMessage(LOG_NORMAL, cs);
        }

        else
        {
            cs.Format("SPTI - NG");
            m_LogWnd->AddMessage(LOG_ERROR, cs);
        }
    }

    else
    {
        CString cs;

        if (theSetting.m_AspiDLL == "")
        {
            cs = "ASPI - Windows Default";
        }

        else
        {
            cs.Format("ASPI - %s", theSetting.m_AspiDLL);
        }

        if (m_CD.GetAspiCtrl()->IsActive())
        {
            m_LogWnd->AddMessage(LOG_NORMAL, cs);
            DWORD Version;
            Version = m_CD.GetAspiCtrl()->GetVersion();
            cs.Format("ASPI version:%d.%d.%d.%d", Version & 0xff, (Version >> 8) & 0xff, (Version >> 16) & 0xff,
                      Version >> 24);
            m_LogWnd->AddMessage(LOG_INFO, cs);
        }

        else
        {
            m_LogWnd->AddMessage(LOG_ERROR, cs);
        }
    }

    UpdateData(FALSE);
    return TRUE;
}

void CCDMDlg::InitializeControlls(void)
{
    //   Enum Drives
    m_DriveList.Initialize(m_CD.GetAspiCtrl());

    //   Create Log viewer
    if (m_LogWnd == nullptr)
    {
        CString cs;
        //      CTime tm = CTime::GetCurrentTime();
        m_LogWnd = new CLogWindow;
        m_LogWnd->LoadFrame(IDR_LOGFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, nullptr, nullptr);
        //   set start log
        cs.Format("Carbon CD Version %s", APP_VERSION);
        m_LogWnd->AddMessage(LOG_INFO, cs);
        m_LogWnd->DumpSystemVersion();
    }

    //   Create TOC viewer
    if (m_TocWnd == nullptr)
    {
        m_TocWnd = new CTocWindow;
        m_TocWnd->LoadFrame(IDR_TOCFRAME, WS_OVERLAPPEDWINDOW | FWS_ADDTOTITLE, nullptr, nullptr);
        //
    }

    else
    {
        m_TocWnd->ClearList();
    }

    //   set initial info
    m_DriveList.SetCurSel(theSetting.m_DriveNo);
}

void CCDMDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }

    else
    {
        CDialog::OnSysCommand(nID, lParam);
    }
}

void CCDMDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this);
        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;
        dc.DrawIcon(x, y, m_hIcon);
    }

    else
    {
        CDialog::OnPaint();
    }
}


HCURSOR CCDMDlg::OnQueryDragIcon()
{
    return m_hIcon;
}

void CCDMDlg::OnBnClickedOk()
{
    //  OnOK();
}

void CCDMDlg::OnExit()
{
    EndDialog(0);
}

void CCDMDlg::OnViewVersion()
{
    CAboutDlg dlgAbout;
    dlgAbout.DoModal();
}

void CCDMDlg::OnVisitRizonesoftHome()
{
    ShellExecute(nullptr, "open", "http://www.rizonesoft.com", nullptr, nullptr, SW_SHOWNORMAL);
}

void CCDMDlg::OnBnClickedCancel()
{
    //  OnCancel();
}

void CCDMDlg::OnClose()
{
    if (m_TocWnd != nullptr)
    {
        delete m_TocWnd;
        m_TocWnd = nullptr;
    }

    if (m_LogWnd != nullptr)
    {
        delete m_LogWnd;
        m_LogWnd = nullptr;
    }

    EndDialog(0);
    //  CDialog::OnClose();
}

void CCDMDlg::OnViewToc()
{
    if (m_TocWnd->GetStyle() & 0x10000000)
    {
        m_TocWnd->ShowWindow(SW_HIDE);
    }

    else
    {
        m_TocWnd->ShowWindow(SW_SHOW);
    }
}

void CCDMDlg::OnCbnSelchangeDrivelist()
{
    OnCdRecognize();
}

void CCDMDlg::OnToolTocfromsession()
{
    TableOfContents* Toc;

    if (m_CD.ReadTOC())
    {
        Toc = m_CD.GetTOC();
        m_TocWnd->SetTOC(Toc);
        UpdateData(FALSE);
    }

    else
    {
        m_TocWnd->ClearList();
    }
}

void CCDMDlg::OnToolCreateimage()
{
    CReadSettingDialog Dlg;
    Dlg.m_CD = &m_CD;

    if (Dlg.DoModal() == IDOK)
    {
        CReadProgressDialog ReadDlg;

        while (true)
        {
            if (theSetting.m_ReadEngine == 0 && m_CD.ReadTOC())
            {
                TableOfContents* Toc;
                Toc = m_CD.GetTOC();

                if (Toc->m_Track[Toc->m_LastTrack - 1].m_Session > 1)
                {
                    if (MessageBox(MSG(73), CONF_MSG, MB_YESNO) == IDNO)
                    {
                        break;
                    }
                }
            }

            if (m_CD.ReadTOC())
            {
                if (theSetting.m_ReadEngine == 0 && theSetting.m_AnalyzeSubQ)
                {
                    CSubQProgressDialog dlg;
                    OnCdRecognize();

                    if (!dlg.AnalyzeSubQ(&m_CD, m_LogWnd))
                    {
                        MessageBox(MSG(74));
                        m_CD.ReadTOC();
                    }
                }

                ReadDlg.ReadDisc(&m_CD, m_LogWnd, Dlg.m_ImageName);
                break;
            }
            if (MessageBox(MSG(75), CONF_MSG, MB_YESNO) == IDNO)
            {
                break;
            }
        }
    }

    OnCdRecognize();
}

void CCDMDlg::OnViewLog()
{
    if (m_LogWnd->GetStyle() & 0x10000000)
    {
        m_LogWnd->ShowWindow(SW_HIDE);
    }

    else
    {
        m_LogWnd->ShowWindow(SW_SHOW);
    }
}

void CCDMDlg::OnToolReadtrack()
{
    CReadTrackDialog Dlg;

    if (!m_TocWnd->CheckSelect(m_CD.GetTOC()))
    {
        m_TocWnd->ShowWindow(SW_SHOW);
        MessageBox(MSG(76), CONF_MSG);
        return;
    }

    m_DriveList.GetWindowText(Dlg.m_DriveName);

    if (Dlg.DoModalOriginal(m_CD.GetTOC()) == IDOK)
    {
        CReadProgressDialog ReadDlg;
        ReadDlg.ReadTrack(&m_CD, m_LogWnd, Dlg.m_ImageName);
    }
}

void CCDMDlg::OnCbnSelchangeSessionlist()
{
    OnToolTocfromsession();
}

void CCDMDlg::OnGeneralSetting()
{
    CSettingDialog Dlg;
    Dlg.DoModal();

    if (Dlg.m_ChangedSkin)
    {
        InitializeButtons();
    }

    if (Dlg.m_ChangedASPI)
    {
        m_CD.InitializeAspi();
        InitializeControlls();
        OnCdRecognize();

        if (theSetting.m_UseSPTI)
        {
            CString cs;

            if (m_CD.GetAspiCtrl()->IsActive())
            {
                cs.Format("SPTI - OK");
                m_LogWnd->AddMessage(LOG_INFO, cs);
            }

            else
            {
                cs.Format("SPTI - NG");
                m_LogWnd->AddMessage(LOG_ERROR, cs);
            }
        }

        else
        {
            CString cs;

            if (theSetting.m_AspiDLL == "")
            {
                cs = "ASPI - Windows Default";
            }

            else
            {
                cs.Format("ASPI - %s", theSetting.m_AspiDLL);
            }

            if (m_CD.GetAspiCtrl()->IsActive())
            {
                m_LogWnd->AddMessage(LOG_NORMAL, cs);
                DWORD Version;
                Version = m_CD.GetAspiCtrl()->GetVersion();
                cs.Format("ASPI version:%d.%d.%d.%d", Version & 0xff, (Version >> 8) & 0xff, (Version >> 16) & 0xff,
                          Version >> 24);
                m_LogWnd->AddMessage(LOG_INFO, cs);
            }

            else
            {
                m_LogWnd->AddMessage(LOG_ERROR, cs);
            }
        }
    }
}

void CCDMDlg::OnToolSubq()
{
    TableOfContents* Toc;
    CSubQProgressDialog dlg;
    OnCdRecognize();

    if (dlg.AnalyzeSubQ(&m_CD, m_LogWnd))
    {
        Toc = m_CD.GetTOC();
        m_TocWnd->SetTOC(Toc);

        if (theSetting.m_WavOnSuccess != "")
        {
            PlaySound(theSetting.m_WavOnSuccess, nullptr, SND_ASYNC);
        }
    }

    else
    {
        MessageBox(MSG(74));

        if (theSetting.m_WavOnFail != "")
        {
            PlaySound(theSetting.m_WavOnFail, nullptr, SND_ASYNC);
        }
    }
}

void CCDMDlg::OnToolMastering()
{
    CMasteringFileDialog Dlg;
    Dlg.m_LogWnd = m_LogWnd;
    Dlg.m_CD = &m_CD;
    Dlg.DoModal();
    OnCdRecognize();
}

void CCDMDlg::OnToolMsf()
{
    CMsfDialog dlg;
    dlg.DoModal();
}

void CCDMDlg::OnCdWriteImage()
{
    CWriteSettingDialog Dlg;
    Dlg.m_CD = &m_CD;
    Dlg.m_CueSheetName = theSetting.m_LastAccessFile;

    if (Dlg.DoModal() == IDOK)
    {
        CWriteProgressDialog WriteDlg;
        WriteDlg.WriteDisc(Dlg.m_CueSheetName, &m_CD, m_LogWnd);
    }

    theSetting.m_LastAccessFile = Dlg.m_CueSheetName;
    OnCdRecognize();
}

void CCDMDlg::OnCdErase()
{
    int Index;
    OnCdRecognize();
    Index = m_DriveList.GetCurSel();

    if (Index == CB_ERR)
    {
        MessageBox(MSG(77));
        return;
    }

    if (MessageBox(MSG(78), CONF_MSG, MB_YESNO) == IDYES)
    {
        CEraseDialog dlg;
        dlg.EraseCompletely(&m_CD);
    }

    m_LogWnd->AutoSave();
}

void CCDMDlg::OnCdEraseFast()
{
    int Index;
    OnCdRecognize();
    Index = m_DriveList.GetCurSel();

    if (Index == CB_ERR)
    {
        MessageBox(MSG(77));
        return;
    }

    if (MessageBox(MSG(79), CONF_MSG, MB_YESNO) == IDYES)
    {
        CEraseDialog dlg;
        dlg.EraseFast(&m_CD);
    }

    m_LogWnd->AutoSave();
}

void CCDMDlg::OnCdRecognize()
{
    int Index;
    TableOfContents* Toc;
    CString cs;
    Index = m_DriveList.GetCurSel();

    if (Index == CB_ERR)
    {
        return;
    }

    theSetting.m_DriveNo = Index;
    m_DriveList.GetLBText(Index, cs);
    m_TocWnd->ViewDriveName(cs);
    m_CD.GetAspiCtrl()->SetDevice(Index);

    if (m_CD.ReadTOC())
    {
        Toc = m_CD.GetTOC();
        m_TocWnd->SetTOC(Toc);
    }

    else
    {
        m_TocWnd->ClearList();
    }

    UpdateData(FALSE);
}

void CCDMDlg::OnCdDuplicate()
{
    CReadSettingDialog Dlg;
    bool Success;
    Dlg.m_CD = &m_CD;
    Success = false;
    Dlg.m_SettingAtCopy = true;

    if (Dlg.DoModal() == IDOK)
    {
        CReadProgressDialog ReadDlg;
        ReadDlg.m_NoConfirm = true;

        while (true)
        {
            if (theSetting.m_ReadEngine == 0 && m_CD.ReadTOC())
            {
                TableOfContents* Toc;
                Toc = m_CD.GetTOC();

                if (Toc->m_Track[Toc->m_LastTrack - 1].m_Session > 1)
                {
                    if (MessageBox(MSG(73), CONF_MSG, MB_YESNO) == IDNO)
                    {
                        OnCdRecognize();
                        return;
                    }
                }
            }

            if (m_CD.ReadTOC())
            {
                if (theSetting.m_ReadEngine == 0 && theSetting.m_AnalyzeSubQ)
                {
                    CSubQProgressDialog dlg;
                    OnCdRecognize();

                    if (!dlg.AnalyzeSubQ(&m_CD, m_LogWnd))
                    {
                        MessageBox(MSG(74));
                        m_CD.ReadTOC();
                    }
                }

                ReadDlg.ReadDisc(&m_CD, m_LogWnd, Dlg.m_ImageName);
                Success = ReadDlg.GetSuccessFlag();
                break;
            }
            if (MessageBox(MSG(75), CONF_MSG, MB_YESNO) == IDNO)
            {
                OnCdRecognize();
                return;
            }
        }
    }

    else
    {
        OnCdRecognize();
        return;
    }

    OnCdRecognize();

    if (Success)
    {
        while (true)
        {
            CWriteSettingDialog DlgW;
            DlgW.m_CD = &m_CD;
            DlgW.m_CueSheetName = Dlg.m_ImageName;
            DlgW.m_DisableChangingFile = true;

            if (DlgW.DoModal() == IDOK)
            {
                CWriteProgressDialog WriteDlg;
                WriteDlg.m_NoConfirm = true;
                WriteDlg.WriteDisc(Dlg.m_ImageName, &m_CD, m_LogWnd);
            }

            else
            {
                break;
            }

            if (MessageBox(MSG(80), CONF_MSG, MB_YESNO) == IDNO)
            {
                break;
            }
        }

        OnCdRecognize();
    }

    if (MessageBox(MSG(81), CONF_MSG, MB_YESNO) == IDYES)
    {
        char BinFile[512];
        char SubFile[512];
        char PreFile[512];
        char CueFile[512];
        //   create .bin file name
        lstrcpy(PreFile, Dlg.m_ImageName);
        lstrcpy(SubFile, Dlg.m_ImageName);
        lstrcpy(CueFile, Dlg.m_ImageName);
        lstrcpy(BinFile, Dlg.m_ImageName);
        lstrcpy(PreFile + lstrlen(BinFile) - 4, ".pre");
        lstrcpy(SubFile + lstrlen(BinFile) - 4, ".sub");
        lstrcpy(CueFile + lstrlen(BinFile) - 4, ".cue");
        lstrcpy(BinFile + lstrlen(BinFile) - 4, ".img");
        DeleteFile(Dlg.m_ImageName); //   delete .cdm file
        DeleteFile(BinFile); //   delete .img file
        DeleteFile(SubFile); //   delete .sub file
        DeleteFile(PreFile); //   delete .pre file
        DeleteFile(CueFile); //   delete .cue file
    }
}

void CCDMDlg::OnDriveCapability()
{
    CDriveCapabilityDialog Dlg;
    Dlg.m_CD = &m_CD;
    Dlg.DoModal();
}

void CCDMDlg::OnLanguage()
{
    CLanguageDialog Dlg;

    if (Dlg.DoModal() == IDOK)
    {
        theSetting.m_Lang.read_language(theSetting.m_LangFile);
        SetLanguage();
        m_TocWnd->SetLanguage();
        m_LogWnd->SetLanguage();

        if (m_TocWnd->GetStyle() & 0x10000000)
        {
            m_TocWnd->ShowWindow(SW_HIDE);
            m_TocWnd->ShowWindow(SW_SHOW);
        }

        if (m_LogWnd->GetStyle() & 0x10000000)
        {
            m_LogWnd->ShowWindow(SW_HIDE);
            m_LogWnd->ShowWindow(SW_SHOW);
        }

        ShowWindow(SW_HIDE);
        ShowWindow(SW_SHOW);
    }
}

void CCDMDlg::InitializeButtons(void)
{
    bool ExternalSkin;
    ExternalSkin = false;

    if (theSetting.m_SkinFile != "")
    {
        CBitmap bmp;

        if (bmp.m_hObject != nullptr)
        {
            DeleteObject(bmp.m_hObject);
        }

        bmp.m_hObject = LoadImage(nullptr, theSetting.m_SkinFile, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

        if (bmp.m_hObject != nullptr)
        {
            CDC *pDC, SkinDC, BmpDC;
            CBitmap *Tmp1, *Tmp2;
            pDC = GetDC();
            SkinDC.CreateCompatibleDC(pDC);
            BmpDC.CreateCompatibleDC(pDC);
            Tmp1 = SkinDC.GetCurrentBitmap();
            Tmp2 = BmpDC.GetCurrentBitmap();
            SkinDC.SelectObject(&bmp);
            m_CreateImageBmp.CreateCompatibleBitmap(pDC, 128, 128);
            m_WriteImageBmp.CreateCompatibleBitmap(pDC, 128, 128);
            m_DuplicateBmp.CreateCompatibleBitmap(pDC, 128, 128);
            m_ReadTrackBmp.CreateCompatibleBitmap(pDC, 128, 128);
            m_MasteringBmp.CreateCompatibleBitmap(pDC, 128, 128);
            m_SubQBmp.CreateCompatibleBitmap(pDC, 128, 128);
            m_RecognizeBmp.CreateCompatibleBitmap(pDC, 128, 128);
            m_ExitBmp.CreateCompatibleBitmap(pDC, 128, 128);
            BmpDC.SelectObject(&m_CreateImageBmp);
            BmpDC.BitBlt(0, 0, 128, 128, &SkinDC, 0, 0, SRCCOPY);
            BmpDC.SelectObject(&m_WriteImageBmp);
            BmpDC.BitBlt(0, 0, 128, 128, &SkinDC, 129, 0, SRCCOPY);
            BmpDC.SelectObject(&m_DuplicateBmp);
            BmpDC.BitBlt(0, 0, 128, 128, &SkinDC, 257, 0, SRCCOPY);
            BmpDC.SelectObject(&m_RecognizeBmp);
            BmpDC.BitBlt(0, 0, 128, 128, &SkinDC, 385, 0, SRCCOPY);
            BmpDC.SelectObject(&m_ReadTrackBmp);
            BmpDC.BitBlt(0, 0, 128, 128, &SkinDC, 641, 0, SRCCOPY);
            BmpDC.SelectObject(&m_MasteringBmp);
            BmpDC.BitBlt(0, 0, 128, 128, &SkinDC, 513, 0, SRCCOPY);
            BmpDC.SelectObject(&m_SubQBmp);
            BmpDC.BitBlt(0, 0, 128, 128, &SkinDC, 769, 0, SRCCOPY);
            BmpDC.SelectObject(&m_ExitBmp);
            BmpDC.BitBlt(0, 0, 128, 128, &SkinDC, 897, 0, SRCCOPY);
            SkinDC.SelectObject(Tmp1);
            BmpDC.SelectObject(Tmp2);
            ReleaseDC(pDC);
            ExternalSkin = true;
        }
    }

    if (!ExternalSkin)
    {
        m_CreateImageBmp.LoadBitmap(IDB_B_CREATE_IMAGE);
        m_WriteImageBmp.LoadBitmap(IDB_B_WRITE_IMAGE);
        m_DuplicateBmp.LoadBitmap(IDB_B_DUPLICATE);
        m_ReadTrackBmp.LoadBitmap(IDB_B_READ_TRACK);
        m_MasteringBmp.LoadBitmap(IDB_B_MASTERING);
        m_SubQBmp.LoadBitmap(IDB_B_SUBQ);
        m_RecognizeBmp.LoadBitmap(IDB_B_RECOGNIZE);
        m_ExitBmp.LoadBitmap(IDB_B_EXIT);
    }

    m_CreateImageButton.SetBitmap(static_cast<HBITMAP>(m_CreateImageBmp.m_hObject));
    m_WriteImageButton.SetBitmap(static_cast<HBITMAP>(m_WriteImageBmp.m_hObject));
    m_DuplicateButton.SetBitmap(static_cast<HBITMAP>(m_DuplicateBmp.m_hObject));
    m_ReadTrackButton.SetBitmap(static_cast<HBITMAP>(m_ReadTrackBmp.m_hObject));
    m_MasteringButton.SetBitmap(static_cast<HBITMAP>(m_MasteringBmp.m_hObject));
    m_SubQButton.SetBitmap(static_cast<HBITMAP>(m_SubQBmp.m_hObject));
    m_RecognizeButton.SetBitmap(static_cast<HBITMAP>(m_RecognizeBmp.m_hObject));
    m_ExitButton.SetBitmap(static_cast<HBITMAP>(m_ExitBmp.m_hObject));
}

void CCDMDlg::ViewMessage(LPCSTR Message)
{
    if (m_Message != Message)
    {
        m_Message = Message;
        UpdateData(FALSE);
    }
}

void CCDMDlg::NotifyButton(void)
{
    RECT r;
    POINT pos;
    GetCursorPos(&pos);
    GetDlgItem(ID_TOOL_CREATEIMAGE)->GetWindowRect(&r);

    if (pos.x >= r.left && pos.x <= r.right && pos.y >= r.top && pos.y <= r.bottom)
    {
        ViewMessage(theSetting.m_Lang.m_Str[LP_MAIN + 1]);
        return;
    }

    GetDlgItem(ID_CD_WRITE_IMAGE)->GetWindowRect(&r);

    if (pos.x >= r.left && pos.x <= r.right && pos.y >= r.top && pos.y <= r.bottom)
    {
        ViewMessage(theSetting.m_Lang.m_Str[LP_MAIN + 2]);
        return;
    }

    GetDlgItem(ID_CD_DUPLICATE)->GetWindowRect(&r);

    if (pos.x >= r.left && pos.x <= r.right && pos.y >= r.top && pos.y <= r.bottom)
    {
        ViewMessage(theSetting.m_Lang.m_Str[LP_MAIN + 3]);
        return;
    }

    GetDlgItem(ID_CD_RECOGNIZE)->GetWindowRect(&r);

    if (pos.x >= r.left && pos.x <= r.right && pos.y >= r.top && pos.y <= r.bottom)
    {
        ViewMessage(theSetting.m_Lang.m_Str[LP_MAIN + 5]);
        return;
    }

    GetDlgItem(ID_TOOL_READTRACK)->GetWindowRect(&r);

    if (pos.x >= r.left && pos.x <= r.right && pos.y >= r.top && pos.y <= r.bottom)
    {
        ViewMessage(theSetting.m_Lang.m_Str[LP_MAIN + 6]);
        return;
    }

    GetDlgItem(ID_TOOL_MASTERING)->GetWindowRect(&r);

    if (pos.x >= r.left && pos.x <= r.right && pos.y >= r.top && pos.y <= r.bottom)
    {
        ViewMessage(theSetting.m_Lang.m_Str[LP_MAIN + 7]);
        return;
    }

    GetDlgItem(ID_TOOL_SUBQ)->GetWindowRect(&r);

    if (pos.x >= r.left && pos.x <= r.right && pos.y >= r.top && pos.y <= r.bottom)
    {
        ViewMessage(theSetting.m_Lang.m_Str[LP_MAIN + 4]);
        return;
    }

    GetDlgItem(ID_EXIT)->GetWindowRect(&r);

    if (pos.x >= r.left && pos.x <= r.right && pos.y >= r.top && pos.y <= r.bottom)
    {
        ViewMessage(theSetting.m_Lang.m_Str[LP_MAIN + 8]);
        return;
    }

    CString cs;
    cs.Format(" Carbon CD Version %s", APP_VERSION);
    ViewMessage(cs);
}

void CCDMDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    CDialog::OnMouseMove(nFlags, point);
    NotifyButton();
}

void CCDMDlg::OnMove(int x, int y)
{
    CDialog::OnMove(x, y);

    if (GetStyle() & 0x10000000)
    {
        RECT r;
        GetWindowRect(&r);
        theSetting.m_MainDlgPos.x = r.left;
        theSetting.m_MainDlgPos.y = r.top;
    }
}

void CCDMDlg::SetLanguage(void)
{
    {
        int i;
        DWORD MenuString[][2] =
        {
            {ID_READSETTING, 5},
            {ID_EXIT, 6},
            {ID_DRIVE_CAPABILITY, 7},
            {ID_TOOL_MSF, 8},
            {ID_CD_ERASE, 9},
            {ID_CD_ERASE_FAST, 10},
            {ID_CD_RECOGNIZE, 11},
            {ID_TOOL_SUBQ, 12},
            {ID_CD_DUPLICATE, 13},
            {ID_TOOL_CREATEIMAGE, 14},
            {ID_CD_WRITE_IMAGE, 15},
            {ID_TOOL_MASTERING, 16},
            {ID_TOOL_READTRACK, 17},
            {ID_VIEW_TOCWINDOW, 18},
            {ID_VIEW_LOG, 19},
            {ID_HELP_HELP, 20},
            {ID_VIEW_VERSION, 21},
        };
        DWORD CtrlString[8][2] =
        {
            {ID_TOOL_CREATEIMAGE, 1},
            {ID_CD_WRITE_IMAGE, 2},
            {ID_CD_DUPLICATE, 3},
            {ID_TOOL_SUBQ, 4},
            {ID_CD_RECOGNIZE, 5},
            {ID_TOOL_READTRACK, 6},
            {ID_TOOL_MASTERING, 7},
            {ID_EXIT, 8},
        };

        for (i = 0; i < 5; i++)
        {
            GetMenu()->ModifyMenu(i, MF_BYPOSITION | MF_STRING, 0, theSetting.m_Lang.m_Str[LP_MAINMENU + i]);
        }

        if (theSetting.m_Lang.m_Str[LP_MAINMENU + i][0] == '\0')
        {
            GetMenu()->ModifyMenu(0, MF_BYPOSITION | MF_STRING, 0, "Carbon CD");
        }

        for (i = 0; i < 17; i++)
        {
            GetMenu()->ModifyMenu(MenuString[i][0], MF_BYCOMMAND | MF_STRING, MenuString[i][0],
                                  theSetting.m_Lang.m_Str[LP_MAINMENU + MenuString[i][1]]);
        }

        SetWindowText(theSetting.m_Lang.m_Str[LP_MAIN + 0]);

        for (i = 0; i < 8; i++)
        {
            SetDlgItemText(CtrlString[i][0], theSetting.m_Lang.m_Str[LP_MAIN + CtrlString[i][1]]);
        }
    }
}

//void CCDMDlg::OnBnClickedChangesize()
//{
//  //   big   445x228
//  //   small 445x60
//  this->SetWindowPos(&wndTop,0,0,445,90,SWP_NOMOVE | SWP_NOZORDER);
//}

void CCDMDlg::OnHelpHelp()
{
    CString cs;
    cs.Format("%sCarbonCD.chm", theSetting.m_Dir);
    ::HtmlHelp(m_hWnd, cs, HH_DISPLAY_TOPIC, 0);
}

void CCDMDlg::WinHelp(DWORD dwData, UINT nCmd)
{
    OnHelpHelp();
    CDialog::WinHelp(dwData, nCmd);
}


void CCDMDlg::OnBnClickedCdRecognize()
{
    // TODO: Add your control notification handler code here
    OnCdRecognize();
}
