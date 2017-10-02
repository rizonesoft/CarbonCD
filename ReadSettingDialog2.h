#pragma once
#include "afxwin.h"


class CReadSettingDialog2 : public CDialog {
        DECLARE_DYNAMIC ( CReadSettingDialog2 )

    public:
        CReadSettingDialog2 ( CWnd* pParent = NULL );
        virtual ~CReadSettingDialog2();

        enum { IDD = IDD_READSETTING_2 };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        void Load ( void );
        void Save ( void );
        bool m_SettingAtCopy;
        BOOL m_TestRead;
        BOOL m_ExtMenu;
        BOOL m_AnalyzePregap;
        BOOL m_AnalyzeSubQ;
        BOOL m_IgnoreReaderror;
        BOOL m_FastErrorskip;
        BOOL m_BurstErrorScan;
        CComboBox m_AudioSpeed;
        virtual BOOL OnInitDialog();
        afx_msg void OnBnClickedExtmenu();
        void SetLanguage ( void );
};
