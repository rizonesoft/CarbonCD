#pragma once
#include "afxwin.h"

class CMasteringPage3 : public CDialog {
        DECLARE_DYNAMIC ( CMasteringPage3 )

    public:
        CMasteringPage3 ( CWnd* pParent = NULL );
        virtual ~CMasteringPage3();

        enum { IDD = IDD_MASTERING_3 };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        BOOL m_AlwaysOnTop;
        CWnd *m_MainDialog;
        virtual BOOL OnInitDialog();
        void SetLanguage ( void );
        afx_msg void OnBnClickedAlwaysTop();
        BOOL m_NotifyTruncated;
        afx_msg void OnBnClickedNotifyTruncated();
        CComboBox m_Protection;
        afx_msg void OnCbnSelchangeProtectionFeatures();
};
