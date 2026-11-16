#include "stdafx.h"
#include "CDM.h"
#include "ReadSettingDialog.h"
#include "Setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_READS + i])

IMPLEMENT_DYNAMIC(CReadSettingDialog, CDialog)

CReadSettingDialog::CReadSettingDialog(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
{
    m_SettingAtCopy = false;
    m_CurrentEngine = 0;
}

CReadSettingDialog::~CReadSettingDialog()
{
}

void CReadSettingDialog::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_IMAGENAME, m_ImageName);
    DDX_Control(pDX, IDC_DRIVELIST, m_DriveList);
    DDX_Control(pDX, IDC_ENGINELIST, m_EngineList);
}

BEGIN_MESSAGE_MAP(CReadSettingDialog, CDialog)
    ON_BN_CLICKED(IDC_BROWSFILE, OnBnClickedBrowsfile)
    ON_BN_CLICKED(IDOK, OnBnClickedOk)
    ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
    ON_CBN_SELCHANGE(IDC_DRIVELIST, OnCbnSelchangeDrivelist)
    ON_CBN_SELCHANGE(IDC_ENGINELIST, OnCbnSelchangeEnginelist)
END_MESSAGE_MAP()

void CReadSettingDialog::OnBnClickedBrowsfile()
{
    if (m_EngineList.GetCurSel() == 3)
    {
        CFileDialog dlg(TRUE, nullptr, nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, MSG(22));
        UpdateData(TRUE);

        if (dlg.DoModal() == IDOK)
        {
            m_ImageName = dlg.GetPathName();
        }
    }

    else
    {
        CFileDialog dlg(FALSE, nullptr, nullptr, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, MSG(22));
        UpdateData(TRUE);

        if (dlg.DoModal() == IDOK)
        {
            m_ImageName = dlg.GetPathName();
        }
    }

    UpdateData(FALSE);
}

void CReadSettingDialog::OnBnClickedOk()
{
    OnCbnSelchangeEnginelist();
    theSetting.m_ReadEngine = m_EngineList.GetCurSel();

    if (theSetting.m_ReadEngine == 2)
    {
        theSetting.m_TestReadMode = false;
    }

    if (m_ImageName == "" && theSetting.m_TestReadMode == FALSE)
    {
        MessageBox(MSG(97), WARNING_MSG);
        return;
    }

    {
        char Buffer[1024], *p;
        lstrcpy(Buffer, m_ImageName);
        p = Buffer + lstrlen(Buffer) - 1;

        while (p >= Buffer)
        {
            if (*p == '.')
            {
                *p = '\0';
                break;
            }

            p--;
        }

        if (p < Buffer)
        {
            if (theSetting.m_ReadEngine == 1)
            {
                m_ImageName.Format("%s.cdm", Buffer);
            }

            else
            {
                m_ImageName.Format("%s.cue", Buffer);
            }
        }

        if ((theSetting.m_ReadEngine == 1 || theSetting.m_ReadEngine == 2) && !theSetting.m_TestReadMode && m_ImageName.
            Right(3).MakeLower() == "cue")
        {
            m_ImageName.Format("%s.cdm", Buffer);
        }

        if (!(theSetting.m_ReadEngine == 1 || theSetting.m_ReadEngine == 2) && !theSetting.m_TestReadMode && m_ImageName
            .Right(3).MakeLower() != "cue")
        {
            m_ImageName.Format("%s.cue", Buffer);
        }
    }

    if (m_SettingAtCopy)
    {
        theSetting.m_CopyTempFile = m_ImageName;
    }

    else
    {
        theSetting.m_LastAccessFile = m_ImageName;
    }

    UpdateData(FALSE);
    OnOK();
}

void CReadSettingDialog::OnBnClickedCancel()
{
    OnCbnSelchangeEnginelist();
    theSetting.m_ReadEngine = m_EngineList.GetCurSel();

    if (m_SettingAtCopy)
    {
        theSetting.m_CopyTempFile = m_ImageName;
    }

    else
    {
        theSetting.m_LastAccessFile = m_ImageName;
    }

    OnCancel();
}

BOOL CReadSettingDialog::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_DriveList.Initialize(m_CD->GetAspiCtrl());
    m_DriveList.SetCurSel(m_CD->GetAspiCtrl()->GetCurrentDevice());
    m_Page1.m_SettingAtCopy = m_SettingAtCopy;
    m_Page2.m_SettingAtCopy = m_SettingAtCopy;
    m_Page3.m_SettingAtCopy = m_SettingAtCopy;
    m_Page1.Create(IDD_READSETTING_1, this);
    m_Page2.Create(IDD_READSETTING_2, this);
    m_Page3.Create(IDD_READSETTING_3, this);
    m_Page4.Create(IDD_READSETTING_4, this);
    m_CurrentEngine = 0;
    m_Page1.Load();
    m_EngineList.InsertString(0, STR(5));
    m_EngineList.InsertString(1, STR(6));
    //#if ALPHA_MODE
    m_EngineList.InsertString(2, STR(19));

    //#else
    //m_EngineList.InsertString(2,"-");
    //#endif
    if (!m_SettingAtCopy)
    {
        m_EngineList.InsertString(3, STR(18));
    }

    else
    {
        if (theSetting.m_ReadEngine == 3)
        {
            theSetting.m_ReadEngine = 0;
        }
    }

    m_EngineList.SetCurSel(theSetting.m_ReadEngine);
    OnCbnSelchangeEnginelist();

    if (m_SettingAtCopy)
    {
        m_ImageName = theSetting.m_CopyTempFile;
    }

    else
    {
        m_ImageName = theSetting.m_LastAccessFile;
    }

    UpdateData(FALSE);
    SetLanguage();
    return TRUE; // return TRUE unless you set the focus to a control
}

void CReadSettingDialog::SetLanguage(void)
{
    DWORD CtrlStr[][2] =
    {
        {IDC_STATIC1, 1},
        {IDC_STATIC2, 2},
        {IDC_BROWSFILE, 3},
    };
    int i;
    SetWindowText(STR(0));

    for (i = 0; i < 3; i++)
    {
        this->SetDlgItemText(CtrlStr[i][0], STR(CtrlStr[i][1]));
    }

    SetDlgItemText(IDOK, theSetting.m_Lang.m_Str[1]);
    SetDlgItemText(IDCANCEL, theSetting.m_Lang.m_Str[2]);
}

void CReadSettingDialog::OnCbnSelchangeDrivelist()
{
    int Index;
    Index = m_DriveList.GetCurSel();

    if (Index == CB_ERR)
    {
        return;
    }

    m_CD->GetAspiCtrl()->SetDevice(Index);
}

void CReadSettingDialog::OnCbnSelchangeEnginelist()
{
    UpdateData(TRUE);

    switch (m_CurrentEngine)
    {
    case 0:
        m_Page1.Save();
        m_Page1.ShowWindow(SW_HIDE);
        break;

    case 1:
        m_Page2.Save();
        m_Page2.ShowWindow(SW_HIDE);
        break;

    case 2:
        m_Page3.Save();
        m_Page3.ShowWindow(SW_HIDE);
        break;

    case 3:
        m_Page4.Save();
        m_Page4.ShowWindow(SW_HIDE);
        break;
    }

    m_Page1.Load();
    m_Page2.Load();
    m_Page3.Load();
    m_Page4.Load();
    m_CurrentEngine = m_EngineList.GetCurSel();

    switch (m_CurrentEngine)
    {
    case 0:
        m_Page1.ShowWindow(SW_SHOW);
        break;

    case 1:
        m_Page2.ShowWindow(SW_SHOW);
        break;

    case 2:
        //#if ALPHA_MODE
        m_Page3.ShowWindow(SW_SHOW);
    //#endif
        break;

    case 3:
        m_Page4.ShowWindow(SW_SHOW);
        break;
    }
}
