#include "stdafx.h"
#include "cdm.h"
#include "ReadSettingDialog3.h"
#include "setting.h"

#define STR(i) (theSetting.m_Lang.m_Str[LP_READS + i])

IMPLEMENT_DYNAMIC(CReadSettingDialog3, CDialog)

CReadSettingDialog3::CReadSettingDialog3(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
      , m_FastErrorskip(FALSE)
      , m_AnalyzePregap(FALSE)
{
}

CReadSettingDialog3::~CReadSettingDialog3()
{
}

void CReadSettingDialog3::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_FAST_ERRORSKIP, m_FastErrorskip);
    DDX_Check(pDX, IDC_ANALYZE_PREGAP, m_AnalyzePregap);
    DDX_Control(pDX, IDC_AUDIOSPEED, m_AudioSpeed);
}


BEGIN_MESSAGE_MAP(CReadSettingDialog3, CDialog)
END_MESSAGE_MAP()


void CReadSettingDialog3::Load(void)
{
    UpdateData(TRUE);
    m_FastErrorskip = theSetting.m_FastErrorSkip;
    m_AnalyzePregap = theSetting.m_AnalyzePregap;

    if (theSetting.m_Speed_Audio <= 1)
    {
        m_AudioSpeed.SetCurSel(4);
    }

    else if (theSetting.m_Speed_Audio <= 4)
    {
        m_AudioSpeed.SetCurSel(3);
    }

    else if (theSetting.m_Speed_Audio <= 8)
    {
        m_AudioSpeed.SetCurSel(2);
    }

    else if (theSetting.m_Speed_Audio <= 32)
    {
        m_AudioSpeed.SetCurSel(1);
    }

    else
    {
        m_AudioSpeed.SetCurSel(0);
    }

    UpdateData(FALSE);
}

void CReadSettingDialog3::Save(void)
{
    UpdateData(TRUE);
    int SpeedList[5] = {0xff, 32, 8, 4, 1};
    theSetting.m_Speed_Audio = SpeedList[m_AudioSpeed.GetCurSel()];
    theSetting.m_FastErrorSkip = (m_FastErrorskip == TRUE) ? true : false;
    theSetting.m_AnalyzePregap = m_AnalyzePregap;
    UpdateData(FALSE);
}

void CReadSettingDialog3::SetLanguage(void)
{
    DWORD CtrlStr[][2] =
    {
        {IDC_ANALYZE_PREGAP, 17},
        {IDC_FAST_ERRORSKIP, 12},
        {IDC_STATIC3, 16},
        {IDC_STATIC1, 19},
    };
    int i;

    for (i = 0; i < 4; i++)
    {
        this->SetDlgItemText(CtrlStr[i][0], STR(CtrlStr[i][1]));
    }
}

BOOL CReadSettingDialog3::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_AudioSpeed.AddString(MSG(20));
    m_AudioSpeed.AddString("x32");
    m_AudioSpeed.AddString("x8");
    m_AudioSpeed.AddString("x4");
    m_AudioSpeed.AddString("x1");
    UpdateData(FALSE);
    SetLanguage();
    return TRUE; // return TRUE unless you set the focus to a control
}
