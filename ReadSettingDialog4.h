#pragma once
#include "afxwin.h"


class CReadSettingDialog4 : public CDialog {
        DECLARE_DYNAMIC ( CReadSettingDialog4 )

    public:
        CReadSettingDialog4 ( CWnd* pParent = NULL );
        virtual ~CReadSettingDialog4();

        enum { IDD = IDD_READSETTING_4 };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        void Load ( void );
        void Save ( void );
        CComboBox m_Speed;
        virtual BOOL OnInitDialog();
    protected:
        void SetLanguage ( void );
};
