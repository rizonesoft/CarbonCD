#pragma once
#include "afxwin.h"


class CReadSettingDialog3 : public CDialog {
        DECLARE_DYNAMIC ( CReadSettingDialog3 )

    public:
        CReadSettingDialog3 ( CWnd* pParent = NULL );
        virtual ~CReadSettingDialog3();

        enum { IDD = IDD_READSETTING_3 };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        void Load ( void );
        void Save ( void );
        void SetLanguage ( void );
        virtual BOOL OnInitDialog();
        bool m_SettingAtCopy;
        BOOL m_FastErrorskip;
        BOOL m_AnalyzePregap;
        CComboBox m_AudioSpeed;
};
