#pragma once

class CSettingPage2 : public CDialog {
        DECLARE_DYNAMIC ( CSettingPage2 )

    public:
        CSettingPage2 ( CWnd* pParent = NULL );
        virtual ~CSettingPage2();

        enum { IDD = IDD_SETTING_2 };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        void Save ( void );
        BOOL m_UseSkin;
        CString m_SkinFile;
        virtual BOOL OnInitDialog();
        void SetLanguage ( void );
        afx_msg void OnBnClickedSetSkin();
        afx_msg void OnBnClickedBrowsSkin();
        bool m_ChangedSkin;
        BOOL m_UseWavOnSuccess;
        CString m_WavOnSuccessFile;
        afx_msg void OnBnClickedSetWavSuccess();
        afx_msg void OnBnClickedBrowsWavSuccess();
        BOOL m_UseWavOnFail;
        CString m_WavOnFailFile;
        afx_msg void OnBnClickedSetWavFail();
        afx_msg void OnBnClickedBrowsWavFail();
};
