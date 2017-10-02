#pragma once
#include "afxcmn.h"
#include "dirstructure.h"
#include "LogWindow.h"
#include "CDController.h"
#include "masteringpage1.h"
#include "masteringpage2.h"
#include "masteringpage3.h"
#include "devicelist.h"

class CMasteringFileDialog : public CDialog {
        DECLARE_DYNAMIC ( CMasteringFileDialog )

    public:
        CMasteringFileDialog ( CWnd* pParent = NULL );
        virtual ~CMasteringFileDialog();

        enum { IDD = IDD_MASTERING_FILE };

    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );

        DECLARE_MESSAGE_MAP()
        CTreeCtrl *m_Tree;
        CListCtrl *m_List;
        CDirStructure m_Dir;
        DWORD m_LeadOutPos;
        DWORD m_ImageSize;
        CMasteringPage1 m_Page1;
        CMasteringPage2 m_Page2;
        CMasteringPage3 m_Page3;
        CTabCtrl m_Tab;
        CString m_Size;

        void UpdateDialog ( bool bSaveAndValidate );
        void SetLanguage ( void );
        void ChangeTab ( void );
        void GetLeadOutPos ( void );
    public:
        CCDController *m_CD;
        CLogWindow *m_LogWnd;
        CString *m_VolumeLabel;
        CListCtrl *m_TrackList;
        void CalcSize ( void );
        virtual BOOL OnInitDialog();
        afx_msg void OnEditAddfolder();
        afx_msg void OnEditLabel();
        afx_msg void OnEditDeletefolder();
        afx_msg void OnEditAddfile();
        afx_msg void OnBnClickedCreateIso();
        afx_msg void OnBnClickedOk();
        afx_msg void OnEditAddaudio();
        afx_msg void OnEditDeletetrack();
        afx_msg void OnEditAdddata();
        afx_msg void OnTcnSelchangeTab ( NMHDR *pNMHDR, LRESULT *pResult );
        afx_msg void OnBnClickedWriting();
        afx_msg void OnWindowClose();
        afx_msg void OnEditInsertfolder();
        afx_msg void OnSetFocus ( CWnd* pOldWnd );
        afx_msg void OnTrackIso();
        afx_msg void OnBnClickedExplorer();
        CDeviceList m_DriveList;
        afx_msg void OnCbnSelchangeDrivelist();
    protected:
        virtual void OnCancel();
        virtual void OnOK();
    public:
        afx_msg void OnCdErase();
        afx_msg void OnCdEraseFast();
};
