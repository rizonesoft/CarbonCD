#pragma once
#include "afxwin.h"
#include "afxcmn.h"
#include "settingpage1.h"
#include "settingpage2.h"
#include "settingpage3.h"
#include "settingpage4.h"


class CSettingDialog : public CDialog {
        DECLARE_DYNAMIC ( CSettingDialog )

    public:
        CSettingDialog ( CWnd* pParent = NULL );
        virtual ~CSettingDialog();

        enum { IDD = IDD_SETTING };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        afx_msg void OnBnClickedOk();
        virtual BOOL OnInitDialog();
        void SetLanguage ( void );
        bool m_ChangedSkin;
        bool m_ChangedASPI;
        CTabCtrl m_Tab;
    protected:
        CSettingPage1 m_Page1;
        CSettingPage2 m_Page2;
        CSettingPage3 m_Page3;
        CSettingPage4 m_Page4;
    public:
        afx_msg void OnTcnSelchangeStab ( NMHDR *pNMHDR, LRESULT *pResult );
        void ChangeTab ( void );
};
