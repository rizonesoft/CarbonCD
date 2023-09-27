#pragma once

class CMessageDialog : public CDialog
{
    DECLARE_DYNAMIC(CMessageDialog)

public:
    CMessageDialog(CWnd* pParent = nullptr);
    ~CMessageDialog() override;

    enum { IDD = IDD_MESSAGE };

protected:
    void DoDataExchange(CDataExchange* pDX) override;

    DECLARE_MESSAGE_MAP()

public:
    CString m_Message;
    void MessageBox(LPCTSTR Title, LPCTSTR Message);
    BOOL OnInitDialog() override;

protected:
    CString m_Title;
};
