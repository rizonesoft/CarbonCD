#pragma once
#include "afxcmn.h"


class CSettingPage3 : public CDialog
{
    DECLARE_DYNAMIC(CSettingPage3)

public:
    CSettingPage3(CWnd* pParent = nullptr);
    ~CSettingPage3() override;

    enum { IDD = IDD_SETTING_3 };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    void Save(void);
    bool m_ChangedASPI;
    CSliderCtrl m_BufferSize;
    CString m_BufferSizeText;
    BOOL m_UseSPTI;
    BOOL m_SetAspi;
    CString m_AspiDLL;
    BOOL OnInitDialog() override;
    void SetLanguage(void);
    afx_msg void OnNMCustomdrawBuffersize(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnBnClickedBrowsAspi();
    afx_msg void OnBnClickedSetAspi();
    afx_msg void OnBnClickedUseSpti();
};
