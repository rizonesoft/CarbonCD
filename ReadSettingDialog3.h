#pragma once
#include "afxwin.h"


class CReadSettingDialog3 : public CDialog
{
    DECLARE_DYNAMIC(CReadSettingDialog3)

public:
    CReadSettingDialog3(CWnd* pParent = nullptr);
    ~CReadSettingDialog3() override;

    enum { IDD = IDD_READSETTING_3 };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    void Load(void);
    void Save(void);
    void SetLanguage(void);
    BOOL OnInitDialog() override;
    bool m_SettingAtCopy;
    BOOL m_FastErrorskip;
    BOOL m_AnalyzePregap;
    CComboBox m_AudioSpeed;
};
