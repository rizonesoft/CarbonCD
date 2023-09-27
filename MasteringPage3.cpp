#include "stdafx.h"
#include "CDM.h"
#include "MasteringPage3.h"
#include "Setting.h"
#include "MasteringFileDialog.h"

#define STR(i)  (theSetting.m_Lang.m_Str[LP_MASTERING + i])


IMPLEMENT_DYNAMIC(CMasteringPage3, CDialog)

CMasteringPage3::CMasteringPage3(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
      , m_AlwaysOnTop(FALSE)
      , m_NotifyTruncated(FALSE)
{
}

CMasteringPage3::~CMasteringPage3()
{
}

void CMasteringPage3::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_ALWAYS_TOP, m_AlwaysOnTop);
    DDX_Check(pDX, IDC_NOTIFY_TRUNCATED, m_NotifyTruncated);
    DDX_Control(pDX, IDC_PROTECTION_FEATURES, m_Protection);
}


BEGIN_MESSAGE_MAP(CMasteringPage3, CDialog)
    ON_BN_CLICKED(IDC_ALWAYS_TOP, OnBnClickedAlwaysTop)
    ON_BN_CLICKED(IDC_NOTIFY_TRUNCATED, OnBnClickedNotifyTruncated)
    ON_CBN_SELCHANGE(IDC_PROTECTION_FEATURES, OnCbnSelchangeProtectionFeatures)
END_MESSAGE_MAP()


BOOL CMasteringPage3::OnInitDialog()
{
    CDialog::OnInitDialog();
#if COPY_PROTECTION
    int Protections[] = {0, 30, 60, 90, 300, 600, 1200, 1800};
    int i;
    m_Protection.InsertString(0, STR(26));
    m_Protection.InsertString(1, STR(27));
    m_Protection.InsertString(2, STR(28));
    m_Protection.InsertString(3, STR(29));
    m_Protection.InsertString(4, STR(30));
    m_Protection.InsertString(5, STR(31));
    m_Protection.InsertString(6, STR(32));
    m_Protection.InsertString(7, STR(33));
    m_Protection.SetCurSel(7);

    for (i = 0; i < 8; i++)
    {
        if ((theSetting.m_CopyProtectionSize / 75) <= Protections[i])
        {
            m_Protection.SetCurSel(i);
            break;
        }
    }

#else
    GetDlgItem ( IDC_STATIC1 )->ShowWindow ( SW_HIDE );
    GetDlgItem ( IDC_PROTECTION_FEATURES )->ShowWindow ( SW_HIDE );
#endif
    m_AlwaysOnTop = theSetting.m_Mastering_AlwaysOnTop;
    m_NotifyTruncated = theSetting.m_Mastering_NotifyTruncated;
    SetLanguage();
    return TRUE; // return TRUE unless you set the focus to a control
}

void CMasteringPage3::SetLanguage(void)
{
    int i;
    DWORD CtrlString[][2] =
    {
        {IDC_ALWAYS_TOP, 2},
        {IDC_NOTIFY_TRUNCATED, 21},
        {IDC_STATIC1, 25},
    };

    for (i = 0; i < 3; i++)
    {
        this->SetDlgItemText(CtrlString[i][0], STR(CtrlString[i][1]));
    }
}

void CMasteringPage3::OnBnClickedAlwaysTop()
{
    UpdateData(TRUE);
    theSetting.m_Mastering_AlwaysOnTop = m_AlwaysOnTop;
    UpdateData(FALSE);

    if (m_AlwaysOnTop)
    {
        m_MainDialog->SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    else
    {
        m_MainDialog->SetWindowPos(&wndNoTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
}

void CMasteringPage3::OnBnClickedNotifyTruncated()
{
    UpdateData(TRUE);
    theSetting.m_Mastering_NotifyTruncated = m_NotifyTruncated;
    UpdateData(FALSE);
}

void CMasteringPage3::OnCbnSelchangeProtectionFeatures()
{
#if COPY_PROTECTION
    int Protections[] = {0, 30, 60, 90, 300, 600, 1200, 1800};
    int i;
    UpdateData(TRUE);
    i = m_Protection.GetCurSel();

    if (i > 7)
    {
        i = 7;
    }

    theSetting.m_CopyProtectionSize = Protections[i] * 75;
    UpdateData(FALSE);
    static_cast<CMasteringFileDialog*>(m_MainDialog)->CalcSize();
#endif
}
