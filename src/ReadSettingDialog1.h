#pragma once
#include "afxwin.h"


class CReadSettingDialog1 : public CDialog
{
    DECLARE_DYNAMIC(CReadSettingDialog1)

public:
    CReadSettingDialog1(CWnd* pParent = nullptr);
    ~CReadSettingDialog1() override;

    enum { IDD = IDD_READSETTING_1 };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    void Load(void);
    void Save(void);
    bool m_SettingAtCopy;
    BOOL m_TestRead;
    BOOL m_ExtMenu;
    BOOL m_AnalyzeSubQ;
    BOOL m_IgnoreReaderror;
    BOOL m_FastErrorskip;
    BOOL m_BurstErrorScan;
    BOOL m_SwapChannel;
    BOOL m_AutoDetectReadMethod;
    CComboBox m_DataSpeed;
    CComboBox m_AudioSpeed;
    CComboBox m_AudioMethod;
    BOOL OnInitDialog() override;
    afx_msg void OnBnClickedExtmenu();
    afx_msg void OnBnClickedAutodetectCommand();
    void SetLanguage(void);
};
