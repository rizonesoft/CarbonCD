#pragma once

class CMessageDialog : public CDialog {
        DECLARE_DYNAMIC ( CMessageDialog )

    public:
        CMessageDialog ( CWnd* pParent = NULL );
        virtual ~CMessageDialog();

        enum { IDD = IDD_MESSAGE };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        CString m_Message;
        void MessageBox ( LPCTSTR Title, LPCTSTR Message );
        virtual BOOL OnInitDialog();
    protected:
        CString m_Title;
};
