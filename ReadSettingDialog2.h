#pragma once
#include "afxwin.h"


class CReadSettingDialog2 : public CDialog
{
    DECLARE_DYNAMIC(CReadSettingDialog2)

public:
    CReadSettingDialog2(CWnd* pParent = nullptr);
    ~CReadSettingDialog2() override;

    enum { IDD = IDD_READSETTING_2 };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    void Load(void);
    void Save(void);
    bool m_SettingAtCopy;
    BOOL m_TestRead;
    BOOL m_ExtMenu;
    BOOL m_AnalyzePregap;
    BOOL m_AnalyzeSubQ;
    BOOL m_IgnoreReaderror;
    BOOL m_FastErrorskip;
    BOOL m_BurstErrorScan;
    CComboBox m_AudioSpeed;
    BOOL OnInitDialog() override;
    afx_msg void OnBnClickedExtmenu();
    void SetLanguage(void);
};
