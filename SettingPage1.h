#pragma once
#include "afxwin.h"


class CSettingPage1 : public CDialog
{
    DECLARE_DYNAMIC(CSettingPage1)

public:
    CSettingPage1(CWnd* pParent = nullptr);
    ~CSettingPage1() override;

    enum { IDD = IDD_SETTING_1 };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    void Save(void);
    CComboBox m_Priority;
    CString m_LogFileName;
    CString m_CopyTempFile;
    BOOL OnInitDialog() override;
    void SetLanguage(void);
    BOOL m_AutosaveLog;
    afx_msg void OnBnClickedAutosaveLog();
    afx_msg void OnBnClickedBrowsLog();
    afx_msg void OnBnClickedBrowsTempfile();
};
