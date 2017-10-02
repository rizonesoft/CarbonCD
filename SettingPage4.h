#pragma once
#include "afxwin.h"

class CSettingPage4 : public CDialog {
        DECLARE_DYNAMIC ( CSettingPage4 )

    public:
        CSettingPage4 ( CWnd* pParent = NULL );
        virtual ~CSettingPage4();

        enum { IDD = IDD_SETTING_4 };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        void Save ( void );
        virtual BOOL OnInitDialog();
        CComboBox m_SubQSpeed;
        CComboBox m_SubQMethod;
        void SetLanguage ( void );
};
