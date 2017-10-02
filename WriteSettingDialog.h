#pragma once
#include "afxwin.h"

#include "CDController.h"
#include "devicelist.h"


class CWriteSettingDialog : public CDialog {
        DECLARE_DYNAMIC ( CWriteSettingDialog )

    public:
        CWriteSettingDialog ( CWnd* pParent = NULL );
        virtual ~CWriteSettingDialog();

        enum { IDD = IDD_WRITESETTING };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
    public:
        afx_msg void OnBnClickedBrowseCuesheet();
        CString m_CueSheetName;
        afx_msg void OnBnClickedOk();
        BOOL m_BurnProof;
        BOOL m_TestMode;
        BOOL m_Opc;
        BOOL m_EjectTray;
        int m_WritingMode;
        virtual BOOL OnInitDialog();
        afx_msg void OnBnClickedCancel();
        BOOL m_CheckDrive;
        CComboBox m_WriteSpeed;
        //  CString m_DriveName;
        bool m_DisableChangingFile;
        CCDController *m_CD;
        BOOL m_AutoDetect;
        afx_msg void OnBnClickedAutodetect();
        void SetLanguage ( void );
        CDeviceList m_DriveList;
        afx_msg void OnCbnSelchangeDrivelist();
        BOOL m_ExtMenu;
        afx_msg void OnBnClickedExtmenu();
};
