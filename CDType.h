#pragma once

// structures of MSF Address
class MSFAddress {
    public:
        MSFAddress ( void );
        virtual ~MSFAddress ( void );
        BYTE Minute;
        BYTE Second;
        BYTE Frame;
        void operator = ( DWORD LBA );
        MSFAddress operator + ( MSFAddress MSF );
        MSFAddress operator - ( MSFAddress MSF );
        MSFAddress operator - ( DWORD LBA );
        void operator = ( MSFAddress MSF );
        DWORD GetByLBA ( void );
        bool operator == ( MSFAddress comp );
};

// structures and definitions of TOC
struct tTrackData
{
    BYTE m_TrackNo;
    BYTE m_TrackType;
    BYTE m_DigitalCopy;
    BYTE m_Emphasis;
    BYTE m_Session;
    MSFAddress m_MSF;
    MSFAddress m_EndMSF;

    BYTE m_SelectFlag;
};

typedef struct tTableOfContents{
	BYTE m_LastTrack;
	struct tTrackData m_Track[99];
	BYTE m_RawTOC[4400];
} TableOfContents;

#define TRACKTYPE_AUDIO		0
#define TRACKTYPE_DATA		1
#define TRACKTYPE_DATA_2	2
#define TRACKFLAG_YES		1
#define TRACKFLAG_NO		0
#define TRACKFLAG_UNKNOWN	2