#pragma once
#include "aspi.h"
#include "afxwin.h"
#include "tocwindow.h"
#include "cdcontroller.h"
#include "logwindow.h"
#include "afxcmn.h"
#include "ButtonEx.h"
#include "DeviceList.h"


class CCDMDlg : public CDialog {

    public:
        CCDMDlg ( CWnd* pParent = NULL );

        enum { IDD = IDD_MAIN_DIALOG };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

    protected:
        HICON m_hIcon;

        virtual BOOL OnInitDialog();
        afx_msg void OnSysCommand ( UINT nID, LPARAM lParam );
        afx_msg void OnPaint();
        afx_msg HCURSOR OnQueryDragIcon();
        DECLARE_MESSAGE_MAP()
    public:
        afx_msg void OnBnClickedOk();
        afx_msg void OnExit();
        afx_msg void OnViewVersion();
		afx_msg void OnVisitRizonesoftHome();
        afx_msg void OnBnClickedCancel();
        afx_msg void OnClose();
        //  CComboBox m_DriveList;
        CDeviceList m_DriveList;
        void InitializeControlls ( void );
    protected:
        CTocWindow *m_TocWnd;
        CLogWindow *m_LogWnd;
    public:
        afx_msg void OnViewToc();
        //  afx_msg void OnUpdateViewToc(CCmdUI *pCmdUI);
        afx_msg void OnCbnSelchangeDrivelist();
    protected:
        CCDController m_CD;
        CBitmap m_CreateImageBmp;
        CBitmap m_WriteImageBmp;
        CBitmap m_DuplicateBmp;
        CBitmap m_ReadTrackBmp;
        CBitmap m_MasteringBmp;
        CBitmap m_SubQBmp;
        CBitmap m_RecognizeBmp;
        CBitmap m_ExitBmp;
    public:
        afx_msg void OnToolTocfromsession();
        afx_msg void OnToolCreateimage();
        afx_msg void OnViewLog();
        afx_msg void OnToolReadtrack();
        afx_msg void OnCbnSelchangeSessionlist();
        afx_msg void OnGeneralSetting();
        afx_msg void OnToolSubq();
        afx_msg void OnToolMastering();
        afx_msg void OnToolMsf();
        afx_msg void OnCdWriteImage();
        afx_msg void OnCdErase();
        afx_msg void OnCdEraseFast();
        afx_msg void OnCdRecognize();
        afx_msg void OnCdDuplicate();
        afx_msg void OnDriveCapability();
        afx_msg void OnLanguage();
        CButtonEx m_CreateImageButton;
        CButtonEx m_WriteImageButton;
        CButtonEx m_DuplicateButton;
        CButtonEx m_ReadTrackButton;
        CButtonEx m_MasteringButton;
        CButtonEx m_SubQButton;
        CButtonEx m_RecognizeButton;
        CButtonEx m_ExitButton;
        void InitializeButtons ( void );
        CString m_Message;
        void ViewMessage ( LPCSTR Message );
    protected:
        void NotifyButton ( void );
    public:
        afx_msg void OnMouseMove ( UINT nFlags, CPoint point );
        afx_msg void OnMove ( int x, int y );
        void SetLanguage ( void );
        //  afx_msg void OnBnClickedChangesize();
        afx_msg void OnHelpHelp();
        virtual void WinHelp ( DWORD dwData, UINT nCmd = HELP_CONTEXT );
		afx_msg void OnBnClickedCdRecognize();
};
