#include "StdAfx.h"
#include "cdcontroller.h"

#include "Setting.h"

#include "AspiDriver.h"
#include "SptiDriver.h"

CCDController::CCDController ( void )
{
    m_Aspi = NULL;
    InitializeAspi();
}

CCDController::~CCDController ( void )
{
    if ( m_Aspi != NULL )
        {
            delete m_Aspi;
        }
}

void CCDController::InitializeAspi ( void )
{
    if ( m_Aspi != NULL )
        {
            delete m_Aspi;
            m_Aspi = NULL;
        }

    if ( theSetting.m_UseSPTI )
        {
            m_Aspi = new CSptiDriver;
        }

    else
        {
            m_Aspi = new CAspiDriver;
        }

    m_Reader.Initialize ( m_Aspi );
    m_SubReader.Initialize ( m_Aspi );
    m_Writer.Initialize ( m_Aspi );
}

CAspi * CCDController::GetAspiCtrl ( void )
{
    return m_Aspi;
}

bool CCDController::ReadTOC ( void )
{
    return m_Reader.ReadTOCFromSession ( m_Toc );
}

TableOfContents *CCDController::GetTOC ( void )
{
    return &m_Toc;
}

bool CCDController::GetDriveName ( CString & String )
{
    CString Vendor, Product, Revision, Address;
    GetAspiCtrl()->GetDeviceString ( Vendor, Product, Revision, Address );
    String.Format ( "(%s) %s %s %s", Address, Vendor, Product, Revision );
    return true;
}

bool CCDController::ReadCDRaw ( MSFAddress MSF, BYTE * Buffer )
{
    if ( !m_Reader.ReadCDRaw ( MSF, ( LPSTR ) Buffer ) )
        {
            return false;
        }

    return true;
}

bool CCDController::ReadCDAudio ( MSFAddress MSF, BYTE * Buffer )
{
    if ( theSetting.m_ReadAudioMethod == 0 )
        {
            if ( m_Reader.ReadCD_D8 ( MSF, ( LPSTR ) Buffer ) )
                {
                    return true;
                }
        }

    else
        if ( theSetting.m_ReadAudioMethod == 1 )
            {
                if ( m_Reader.ReadCDDA_LBA ( MSF, ( LPSTR ) Buffer ) )
                    {
                        return true;
                    }
            }

        else
            if ( theSetting.m_ReadAudioMethod == 2 )
                {
                    if ( m_Reader.ReadCDDA ( MSF, ( LPSTR ) Buffer ) )
                        {
                            return true;
                        }
                }

            else
                if ( theSetting.m_ReadAudioMethod == 3 )
                    {
                        if ( m_Reader.ReadCD_LBA ( MSF, ( LPSTR ) Buffer ) )
                            {
                                return true;
                            }
                    }

                else
                    if ( theSetting.m_ReadAudioMethod == 4 )
                        {
                            if ( m_Reader.ReadCD ( MSF, ( LPSTR ) Buffer ) )
                                {
                                    return true;
                                }
                        }

                    else
                        if ( theSetting.m_ReadAudioMethod == 5 )
                            {
                                if ( m_Reader.ReadCDRaw_LBA ( MSF, ( LPSTR ) Buffer ) )
                                    {
                                        return true;
                                    }
                            }

                        else
                            if ( theSetting.m_ReadAudioMethod == 6 )
                                {
                                    if ( m_Reader.ReadCDRaw ( MSF, ( LPSTR ) Buffer ) )
                                        {
                                            return true;
                                        }
                                }

                            else
                                if ( theSetting.m_ReadAudioMethod == 7 )
                                    {
                                        if ( m_Reader.ReadCD_Read10 ( MSF, ( LPSTR ) Buffer ) )
                                            {
                                                return true;
                                            }
                                    }

                                else
                                    if ( theSetting.m_ReadAudioMethod == 8 )
                                        {
                                            if ( m_Reader.ReadCD_D4 ( MSF, ( LPSTR ) Buffer ) )
                                                {
                                                    return true;
                                                }
                                        }

                                    else
                                        if ( theSetting.m_ReadAudioMethod == 9 )
                                            {
                                                if ( m_Reader.ReadCD_D4_2 ( MSF, ( LPSTR ) Buffer ) )
                                                    {
                                                        return true;
                                                    }
                                            }

                                        else
                                            if ( theSetting.m_ReadAudioMethod == 10 )
                                                {
                                                    if ( m_Reader.ReadCD_D5 ( MSF, ( LPSTR ) Buffer ) )
                                                        {
                                                            return true;
                                                        }
                                                }

    return false;
}

void CCDController::SetSpeed ( BYTE ReadSpeed, BYTE WriteSpeed )
{
    m_Reader.SetCDSpeed ( ReadSpeed, WriteSpeed );
}

DWORD CCDController::GetErrorStatus ( void )
{
    return m_Reader.m_SRB_Status | ( m_Reader.m_SK << 24 ) | ( m_Reader.m_ASC << 16 ) | ( m_Reader.m_ASCQ << 8 );
}

bool CCDController::ReadSubQ ( MSFAddress msf, BYTE *Buffer )
{
    return m_Reader.ReadSubQ ( msf, Buffer );
}

bool CCDController::SetErrorCorrectMode ( bool CorrectFlag )
{
    return m_Reader.SetErrorCorrectMode ( CorrectFlag );;
}

bool CCDController::ReadRawSub ( MSFAddress & MSF, BYTE * Buffer, int Method )
{
    if ( Method == 0 )
        {
            return m_SubReader.ReadRaw96 ( MSF, Buffer );
        }

    else
        if ( Method == 1 )
            {
                return m_SubReader.ReadCD96 ( MSF, Buffer );
            }

        else
            if ( Method == 2 )
                {
                    return m_SubReader.ReadCDDA96 ( MSF, Buffer );
                }

            else
                if ( Method == 3 )
                    {
                        return m_SubReader.ReadRaw16 ( MSF, Buffer );
                    }

                else
                    if ( Method == 4 )
                        {
                            return m_SubReader.ReadCD16 ( MSF, Buffer );
                        }

                    else
                        if ( Method == 5 )
                            {
                                return m_SubReader.ReadCDDA16 ( MSF, Buffer );
                            }

    return false;
}

bool CCDController::ReadATIP ( BYTE * Buffer )
{
    return m_Reader.ReadATIP ( Buffer );
}

int CCDController::ReadCDText ( BYTE * Buffer )
{
    return m_Reader.ReadCDText ( Buffer );
}

//   writer's staff

LPCSTR CCDController::GetWriteError ( void )
{
    return m_Writer.GetErrorMessage();
}

bool CCDController::ParseCueSheetFile ( LPCSTR FileName )
{
    return m_Writer.ParseCueSheetFile ( FileName );
}

bool CCDController::ParseCueSheet ( LPCSTR cue, DWORD ImageSize )
{
    return m_Writer.ParseCueSheet ( cue, ImageSize );
}

void CCDController::FinishWriting ( void )
{
    m_Writer.FinishWriting();
}

DWORD CCDController::GetLeadInSize ( void )
{
    return m_Writer.GetLeadInSize();
}

bool CCDController::WriteRaw ( BYTE * Buffer )
{
    return m_Writer.WriteRaw ( Buffer );
}

bool CCDController::WriteRawLeadIn ( void )
{
    return m_Writer.WriteRawLeadIn();
}

bool CCDController::WriteRawGap ( void )
{
    return m_Writer.WriteRawGap();
}

bool CCDController::StartWriting ( bool ModeMS )
{
    return m_Writer.StartWriting ( ModeMS );
}

bool CCDController::OPC ( void )
{
    return m_Writer.OPC();
}

bool CCDController::SetWritingParams ( int WritingMode, bool BurnProof, bool TestMode, int BufferingFrames )
{
    return m_Writer.SetWritingParams ( WritingMode, BurnProof, TestMode, BufferingFrames );
}

bool CCDController::LoadTray ( bool LoadingMode )
{
    return m_Writer.LoadTray ( LoadingMode );
}

LPCSTR CCDController::GetImageFileName ( void )
{
    return m_Writer.GetImageFileName();
}

DWORD CCDController::GetImageFrames ( void )
{
    return m_Writer.GetImageFrames();
}

void CCDController::GetWriteErrorParams ( BYTE & SK, BYTE & ASC, BYTE & ASCQ )
{
    m_Writer.GetErrorParams ( SK, ASC, ASCQ );
}

void CCDController::ForceScramble ( BYTE *Buffer )
{
    m_Writer.ForceScramble ( Buffer );
}

bool CCDController::IsCDR ( void )
{
    return m_Writer.IsCDR();
}

void CCDController::AbortWriting ( void )
{
    m_Writer.AbortWriting();
}

bool CCDController::EraseMedia ( bool FastErase )
{
    return m_Writer.EraseMedia ( FastErase );
}

int CCDController::GetWritingMode ( void )
{
    return m_Writer.GetWritingMode();
}

int CCDController::GetBufferSize ( void )
{
    return m_Writer.GetBufferSize();
}

bool CCDController::CheckDisc ( void )
{
    return m_Writer.CheckDisc();
}

bool CCDController::WriteRaw96 ( BYTE * Buffer, DWORD lba )
{
    return m_Writer.WriteRaw96 ( Buffer, lba );
}

void CCDController::SetReadingBCDMode ( bool TransBCD )
{
    m_SubReader.SetBCDMode ( TransBCD );
}
