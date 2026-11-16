#pragma once
#include "afxwin.h"

class CMasteringPage3 : public CDialog
{
    DECLARE_DYNAMIC(CMasteringPage3)

public:
    CMasteringPage3(CWnd* pParent = nullptr);
    ~CMasteringPage3() override;

    enum { IDD = IDD_MASTERING_3 };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    BOOL m_AlwaysOnTop;
    CWnd* m_MainDialog;
    BOOL OnInitDialog() override;
    void SetLanguage(void);
    afx_msg void OnBnClickedAlwaysTop();
    BOOL m_NotifyTruncated;
    afx_msg void OnBnClickedNotifyTruncated();
    CComboBox m_Protection;
    afx_msg void OnCbnSelchangeProtectionFeatures();
};
