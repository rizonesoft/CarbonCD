#pragma once
#include "afxwin.h"
#include "cdcontroller.h"

class CEraseDialog : public CDialog
{
    DECLARE_DYNAMIC(CEraseDialog)

public:
    CEraseDialog(CWnd* pParent = nullptr);
    ~CEraseDialog() override;

    enum { IDD = IDD_ERASE };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedCancel();
    afx_msg void OnBnClickedOk();
    BOOL OnInitDialog() override;
    CButton m_OKButton;

protected:
    HANDLE m_hThread;
    DWORD m_ThreadID;
    bool m_FastErase;
    CCDController* m_CD;

public:
    void EraseFast(CCDController* cd);
    void EraseCompletely(CCDController* cd);
    bool Erase(void);
    void Complete(void);
    CString m_Message;
    void Incomplete(void);
    afx_msg void OnUpdateDialog();
};
