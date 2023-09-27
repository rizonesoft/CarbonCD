#include "stdafx.h"
#include "cdm.h"
#include "SettingPage2.h"
#include "Setting.h"


IMPLEMENT_DYNAMIC(CSettingPage2, CDialog)

CSettingPage2::CSettingPage2(CWnd* pParent /*=NULL*/)
    : CDialog(IDD, pParent)
      , m_UseSkin(FALSE)
      , m_SkinFile(_T(""))
      , m_ChangedSkin(false)
      , m_UseWavOnSuccess(FALSE)
      , m_WavOnSuccessFile(_T(""))
      , m_UseWavOnFail(FALSE)
      , m_WavOnFailFile(_T(""))
{
}

CSettingPage2::~CSettingPage2()
{
}

void CSettingPage2::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
    DDX_Check(pDX, IDC_SET_SKIN, m_UseSkin);
    DDX_Text(pDX, IDC_SKIN_FILE, m_SkinFile);
    DDX_Check(pDX, IDC_SET_WAV_SUCCESS, m_UseWavOnSuccess);
    DDX_Text(pDX, IDC_WAV_SUCCESS_FILE, m_WavOnSuccessFile);
    DDX_Check(pDX, IDC_SET_WAV_FAIL, m_UseWavOnFail);
    DDX_Text(pDX, IDC_WAV_FAIL_FILE, m_WavOnFailFile);
}


BEGIN_MESSAGE_MAP(CSettingPage2, CDialog)
    ON_BN_CLICKED(IDC_SET_SKIN, OnBnClickedSetSkin)
    ON_BN_CLICKED(IDC_BROWS_SKIN, OnBnClickedBrowsSkin)
    ON_BN_CLICKED(IDC_SET_WAV_SUCCESS, OnBnClickedSetWavSuccess)
    ON_BN_CLICKED(IDC_BROWS_WAV_SUCCESS, OnBnClickedBrowsWavSuccess)
    ON_BN_CLICKED(IDC_SET_WAV_FAIL, OnBnClickedSetWavFail)
    ON_BN_CLICKED(IDC_BROWS_WAV_FAIL, OnBnClickedBrowsWavFail)
END_MESSAGE_MAP()


void CSettingPage2::Save(void)
{
    m_ChangedSkin = false;

    if (m_UseSkin == FALSE)
    {
        m_SkinFile = "";
    }

    if (m_SkinFile != theSetting.m_SkinFile)
    {
        m_ChangedSkin = true;
        theSetting.m_SkinFile = m_SkinFile;
    }

    if (m_UseWavOnSuccess == FALSE)
    {
        theSetting.m_WavOnSuccess = "";
    }

    else
    {
        theSetting.m_WavOnSuccess = m_WavOnSuccessFile;
    }

    if (m_UseWavOnFail == FALSE)
    {
        theSetting.m_WavOnFail = "";
    }

    else
    {
        theSetting.m_WavOnFail = m_WavOnFailFile;
    }
}

BOOL CSettingPage2::OnInitDialog()
{
    CDialog::OnInitDialog();
    m_SkinFile = theSetting.m_SkinFile;

    if (m_SkinFile == "")
    {
        m_UseSkin = FALSE;
        GetDlgItem(IDC_SKIN_FILE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BROWS_SKIN)->ShowWindow(SW_HIDE);
    }

    else
    {
        m_UseSkin = TRUE;
    }

    m_WavOnSuccessFile = theSetting.m_WavOnSuccess;

    if (m_WavOnSuccessFile == "")
    {
        m_UseWavOnSuccess = FALSE;
        GetDlgItem(IDC_WAV_SUCCESS_FILE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BROWS_WAV_SUCCESS)->ShowWindow(SW_HIDE);
    }

    else
    {
        m_UseWavOnSuccess = TRUE;
    }

    m_WavOnFailFile = theSetting.m_WavOnFail;

    if (m_WavOnFailFile == "")
    {
        m_UseWavOnFail = FALSE;
        GetDlgItem(IDC_WAV_FAIL_FILE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BROWS_WAV_FAIL)->ShowWindow(SW_HIDE);
    }

    else
    {
        m_UseWavOnFail = TRUE;
    }

    UpdateData(FALSE);
    SetLanguage();
    return TRUE; // return TRUE unless you set the focus to a control
}

#define STR(i) (theSetting.m_Lang.m_Str[LP_SET + i])

void CSettingPage2::SetLanguage(void)
{
    DWORD CtrlStr[][2] =
    {
        {IDC_SET_SKIN, 6},
        {IDC_BROWS_SKIN, 7},
        {IDC_SET_WAV_SUCCESS, 17},
        {IDC_BROWS_WAV_SUCCESS, 7},
        {IDC_SET_WAV_FAIL, 18},
        {IDC_BROWS_WAV_FAIL, 7},
    };
    int i;

    for (i = 0; i < 6; i++)
    {
        SetDlgItemText(CtrlStr[i][0], STR(CtrlStr[i][1]));
    }
}

void CSettingPage2::OnBnClickedSetSkin()
{
    UpdateData(TRUE);

    if (m_UseSkin == TRUE)
    {
        GetDlgItem(IDC_SKIN_FILE)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_BROWS_SKIN)->ShowWindow(SW_SHOW);
    }

    else
    {
        GetDlgItem(IDC_SKIN_FILE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BROWS_SKIN)->ShowWindow(SW_HIDE);
    }

    UpdateData(FALSE);
}

void CSettingPage2::OnBnClickedBrowsSkin()
{
    CFileDialog dlg(FALSE, nullptr, nullptr, OFN_HIDEREADONLY, "Skin files(*.bmp)|*.bmp|All type files|*.*||");
    UpdateData(TRUE);

    if (dlg.DoModal() == IDOK)
    {
        m_SkinFile = dlg.GetPathName();
    }

    UpdateData(FALSE);
}

void CSettingPage2::OnBnClickedSetWavSuccess()
{
    UpdateData(TRUE);

    if (m_UseWavOnSuccess == TRUE)
    {
        GetDlgItem(IDC_WAV_SUCCESS_FILE)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_BROWS_WAV_SUCCESS)->ShowWindow(SW_SHOW);
    }

    else
    {
        GetDlgItem(IDC_WAV_SUCCESS_FILE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BROWS_WAV_SUCCESS)->ShowWindow(SW_HIDE);
    }

    UpdateData(FALSE);
}

void CSettingPage2::OnBnClickedBrowsWavSuccess()
{
    CFileDialog dlg(FALSE, nullptr, nullptr, OFN_HIDEREADONLY, "Wav files(*.wav)|*.wav|All type files|*.*||");
    UpdateData(TRUE);

    if (dlg.DoModal() == IDOK)
    {
        m_WavOnSuccessFile = dlg.GetPathName();
    }

    UpdateData(FALSE);
}

void CSettingPage2::OnBnClickedSetWavFail()
{
    UpdateData(TRUE);

    if (m_UseWavOnFail == TRUE)
    {
        GetDlgItem(IDC_WAV_FAIL_FILE)->ShowWindow(SW_SHOW);
        GetDlgItem(IDC_BROWS_WAV_FAIL)->ShowWindow(SW_SHOW);
    }

    else
    {
        GetDlgItem(IDC_WAV_FAIL_FILE)->ShowWindow(SW_HIDE);
        GetDlgItem(IDC_BROWS_WAV_FAIL)->ShowWindow(SW_HIDE);
    }

    UpdateData(FALSE);
}

void CSettingPage2::OnBnClickedBrowsWavFail()
{
    CFileDialog dlg(FALSE, nullptr, nullptr, OFN_HIDEREADONLY, "Wav files(*.wav)|*.wav|All type files|*.*||");
    UpdateData(TRUE);

    if (dlg.DoModal() == IDOK)
    {
        m_WavOnFailFile = dlg.GetPathName();
    }

    UpdateData(FALSE);
}
