#pragma once
#include "afxcmn.h"


class CSettingPage3 : public CDialog {
        DECLARE_DYNAMIC ( CSettingPage3 )

    public:
        CSettingPage3 ( CWnd* pParent = NULL );
        virtual ~CSettingPage3();

        enum { IDD = IDD_SETTING_3 };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        void Save ( void );
        bool m_ChangedASPI;
        CSliderCtrl m_BufferSize;
        CString m_BufferSizeText;
        BOOL m_UseSPTI;
        BOOL m_SetAspi;
        CString m_AspiDLL;
        virtual BOOL OnInitDialog();
        void SetLanguage ( void );
        afx_msg void OnNMCustomdrawBuffersize ( NMHDR *pNMHDR, LRESULT *pResult );
        afx_msg void OnBnClickedBrowsAspi();
        afx_msg void OnBnClickedSetAspi();
        afx_msg void OnBnClickedUseSpti();
};
