#pragma once
#include "afxcmn.h"

class CLogWindow : public CFrameWnd {
        DECLARE_DYNCREATE ( CLogWindow )
    public:
        CLogWindow();
        virtual ~CLogWindow();

    protected:
        DECLARE_MESSAGE_MAP()
        CListCtrl m_View;
    public:
        afx_msg int OnCreate ( LPCREATESTRUCT lpCreateStruct );
        afx_msg void OnClose();
        afx_msg void OnWindowClose();
    protected:
        virtual BOOL PreCreateWindow ( CREATESTRUCT& cs );
        CImageList m_ImageList;
    public:
        afx_msg void OnSize ( UINT nType, int cx, int cy );
    protected:
        HICON m_hIcon;
        int m_Flag;
    public:
        void AddMessage ( int Type, LPCSTR Message );
        afx_msg void OnSavelog();
        void AutoSave ( void );
        afx_msg void OnShowWindow ( BOOL bShow, UINT nStatus );
    protected:
        virtual BOOL OnCreateClient ( LPCREATESTRUCT lpcs, CCreateContext* pContext );
    public:
        afx_msg void OnMove ( int x, int y );
        void SetLanguage ( void );
        afx_msg void OnClearlog();
        void DumpSystemVersion ( void );
};

#define LOG_NORMAL  0
#define LOG_WARNING 1
#define LOG_ERROR   2
#define LOG_INFO	3