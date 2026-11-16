#include "stdafx.h"
#include "CDM.h"
#include "WriteSettingDialog.h"
#include "Setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_WRITES + i])

static BYTE WriteSpeed[14] = {1, 2, 4, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 255};
static LPCSTR WriteSpeedText[14] =
{
    "x1", "x2", "x4", "x6", "x8", "x10", "x12", "x16", "x20",
    "x24", "x32", "x40", "x48", "MAX Speed",
};

IMPLEMENT_DYNAMIC(CWriteSettingDialog, CDialog)

CWriteSettingDialog::CWriteSettingDialog(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
      , m_CueSheetName(_T(""))
      , m_BurnProof(FALSE)
      , m_TestMode(FALSE)
      , m_Opc(FALSE)
      , m_EjectTray(FALSE)
      , m_WritingMode(0)
      , m_CheckDrive(FALSE)
      , m_DisableChangingFile(false)
      , m_AutoDetect(FALSE)
      , m_ExtMenu(FALSE)
{
}

CWriteSettingDialog::~CWriteSettingDialog()
{
}

void CWriteSettingDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_CUEFILE, m_CueSheetName);
    DDX_Check(pDX, IDC_BURNPROOF, m_BurnProof);
    DDX_Check(pDX, IDC_TESTMODE, m_TestMode);
    DDX_Check(pDX, IDC_OPC, m_Opc);
    DDX_Check(pDX, IDC_EJECT_TRAY, m_EjectTray);
    DDX_Radio(pDX, IDC_WRITEMODE_1, m_WritingMode);
    DDX_Check(pDX, IDC_CHECK_DRIVE, m_CheckDrive);
    DDX_Control(pDX, IDC_WRITESPEED, m_WriteSpeed);
    DDX_Check(pDX, IDC_AUTODETECT, m_AutoDetect);
    DDX_Control(pDX, IDC_DRIVELIST, m_DriveList);
    DDX_Check(pDX, IDC_EXTMENU, m_ExtMenu);
}


BEGIN_MESSAGE_MAP(CWriteSettingDialog, CDialog)
    ON_BN_CLICKED(IDC_BROWSE_CUESHEET, OnBnClickedBrowseCuesheet)
    ON_BN_CLICKED(IDOK, OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
    ON_BN_CLICKED(IDC_AUTODETECT, OnBnClickedAutodetect)
    ON_CBN_SELCHANGE(IDC_DRIVELIST, OnCbnSelchangeDrivelist)
    ON_BN_CLICKED(IDC_EXTMENU, OnBnClickedExtmenu)
END_MESSAGE_MAP()


BOOL CWriteSettingDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    WriteSpeedText[13] = MSG(20);
    m_DriveList.Initialize(m_CD->GetAspiCtrl());
    m_DriveList.SetCurSel(m_CD->GetAspiCtrl()->GetCurrentDevice());
    m_BurnProof = theSetting.m_Write_BurnProof;
    m_EjectTray = theSetting.m_Write_EjectTray;
    m_Opc = theSetting.m_Write_Opc;
    m_TestMode = theSetting.m_Write_TestMode;
    m_WritingMode = theSetting.m_Write_WritingMode;
    m_CheckDrive = theSetting.m_Write_CheckDrive;
    m_AutoDetect = theSetting.m_Write_AutoDetectMethod;
    m_ExtMenu = theSetting.m_ExtSetting_Write;
    int i, spd;
    spd = 13;

    for (i = 0; i < 14; i++)
    {
        if (theSetting.m_Write_Speed == WriteSpeed[i])
        {
            spd = i;
        }

        m_WriteSpeed.InsertString(i, WriteSpeedText[i]);
    }

    m_WriteSpeed.SetCurSel(spd);

    if (m_DisableChangingFile)
    {
        GetDlgItem(IDC_BROWSE_CUESHEET)->ShowWindow(SW_HIDE);
    }

    UpdateData(FALSE);
    SetLanguage();
    OnBnClickedExtmenu();
    return TRUE; // return TRUE unless you set the focus to a control
}

void CWriteSettingDialog::OnBnClickedOk()
{
    UpdateData(TRUE);

    if (m_CueSheetName == "")
    {
        MessageBox(MSG(21));
        return;
    }

    theSetting.m_Write_BurnProof = m_BurnProof;
    theSetting.m_Write_EjectTray = m_EjectTray;
    theSetting.m_Write_Opc = m_Opc;
    theSetting.m_Write_TestMode = m_TestMode;
    theSetting.m_Write_WritingMode = m_WritingMode;
    theSetting.m_Write_CheckDrive = m_CheckDrive;
    theSetting.m_Write_Speed = WriteSpeed[m_WriteSpeed.GetCurSel()];
    theSetting.m_Write_AutoDetectMethod = m_AutoDetect;
    theSetting.m_ExtSetting_Write = m_ExtMenu;
    OnOK();
}

void CWriteSettingDialog::OnBnClickedCancel()
{
    UpdateData(TRUE);
    theSetting.m_Write_BurnProof = m_BurnProof;
    theSetting.m_Write_EjectTray = m_EjectTray;
    theSetting.m_Write_Opc = m_Opc;
    theSetting.m_Write_TestMode = m_TestMode;
    theSetting.m_Write_WritingMode = m_WritingMode;
    theSetting.m_Write_CheckDrive = m_CheckDrive;
    theSetting.m_Write_Speed = WriteSpeed[m_WriteSpeed.GetCurSel()];
    theSetting.m_Write_AutoDetectMethod = m_AutoDetect;
    theSetting.m_ExtSetting_Write = m_ExtMenu;
    OnCancel();
}

void CWriteSettingDialog::OnBnClickedBrowseCuesheet()
{
    CFileDialog dlg(TRUE, nullptr, nullptr, OFN_OVERWRITEPROMPT, MSG(22));
    UpdateData(TRUE);

    if (dlg.DoModal() == IDOK)
    {
        m_CueSheetName = dlg.GetPathName();
    }

    UpdateData(FALSE);
}

void CWriteSettingDialog::OnBnClickedAutodetect()
{
    UpdateData(TRUE);

    if (m_AutoDetect)
    {
        GetDlgItem(IDC_WRITEMODE_1)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_WRITEMODE_2)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_WRITEMODE_3)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_WRITEMODE_4)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_COMMAND_FRAME)->ShowWindow(SW_HIDE);
    }

    else
    {
        GetDlgItem(IDC_WRITEMODE_1)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_WRITEMODE_2)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_WRITEMODE_3)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_WRITEMODE_4)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_COMMAND_FRAME)->ShowWindow(SW_SHOW);
    }

    UpdateData(FALSE);
}

void CWriteSettingDialog::SetLanguage(void)
{
    int i;
    DWORD CtrlStr[][2] =
    {
        {IDC_STATIC1, 1},
        {IDC_STATIC2, 2},
        {IDC_BROWSE_CUESHEET, 3},
        {IDC_TESTMODE, 4},
        {IDC_OPC, 5},
        {IDC_BURNPROOF, 6},
        {IDC_EJECT_TRAY, 7},
        {IDC_STATIC3, 8},
        {IDC_CHECK_DRIVE, 9},
        {IDC_AUTODETECT, 10},
        {IDC_COMMAND_FRAME, 11},
        {IDC_EXTMENU, 12},
    };
    SetDlgItemText(IDOK, theSetting.m_Lang.m_Str[1]);
    SetDlgItemText(IDCANCEL, theSetting.m_Lang.m_Str[2]);
    SetWindowText(STR(0));

    for (i = 0; i < 12; i++)
    {
        this->SetDlgItemText(CtrlStr[i][0], STR(CtrlStr[i][1]));
    }
}

void CWriteSettingDialog::OnCbnSelchangeDrivelist()
{
    int Index;
    Index = m_DriveList.GetCurSel();

    if (Index == CB_ERR)
    {
        return;
    }

    m_CD->GetAspiCtrl()->SetDevice(Index);
}

void CWriteSettingDialog::OnBnClickedExtmenu()
{
    UpdateData(TRUE);

    if (m_ExtMenu)
    {
        GetDlgItem(IDC_OPC)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_BURNPROOF)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_EJECT_TRAY)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_CHECK_DRIVE)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_AUTODETECT)->ShowWindow(SW_SHOW);
        UpdateData(FALSE);
        OnBnClickedAutodetect();
    }

    else
    {
        GetDlgItem(IDC_OPC)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BURNPROOF)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_EJECT_TRAY)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_CHECK_DRIVE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_AUTODETECT)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_COMMAND_FRAME)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_WRITEMODE_1)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_WRITEMODE_2)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_WRITEMODE_3)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_WRITEMODE_4)->ShowWindow(SW_HIDE);
        UpdateData(FALSE);
    }
}
