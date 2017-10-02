#pragma once
#include "afxcmn.h"
#include "dirstructure.h"

class CMasteringPage2 : public CDialog {
        DECLARE_DYNAMIC ( CMasteringPage2 )
    public:
        CMasteringPage2 ( CWnd* pParent = NULL );
        virtual ~CMasteringPage2();
        enum { IDD = IDD_MASTERING_2 };
    protected:
        virtual void DoDataExchange ( CDataExchange* pDX );
        DECLARE_MESSAGE_MAP()
    public:
        CWnd *m_MainDialog;
        CWnd *m_Page1;
        CListCtrl m_List;
        CTreeCtrl m_Tree;
        CString m_VolumeLabel;
        void SetLanguage ( void );
        virtual BOOL OnInitDialog();
        CDirStructure *m_Dir;
    protected:
        CMenu m_Menu;
        CImageList m_TreeImageNormal;
        CImageList m_ListImage;
    public:
        afx_msg void OnLvnKeydownList ( NMHDR *pNMHDR, LRESULT *pResult );
        afx_msg void OnNMRclickList ( NMHDR *pNMHDR, LRESULT *pResult );
        afx_msg void OnNMDblclkList ( NMHDR *pNMHDR, LRESULT *pResult );
        afx_msg void OnTvnSelchangedTree ( NMHDR *pNMHDR, LRESULT *pResult );
        afx_msg void OnLvnEndlabeleditList ( NMHDR *pNMHDR, LRESULT *pResult );
        bool AddFile ( LPCSTR FileName );
        CDirStructure * AddFolder ( LPCSTR FolderName );
        void AddFileRec ( LPCSTR PathName );
        afx_msg void OnDropFiles ( HDROP hDropInfo );
        void DeleteSelectedItems ( void );
};
