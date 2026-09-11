/*
	CMP3Info.cpp
	Info sul file MP3.
	Codice modificato e riadattato a partire dall'originale di Gustav Munkby.
	Luca Piergentili, 07/08/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <string.h>
#include "strings.h"
#include "CBinFile.h"
#include "CMP3Info.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	Load()
*/
BOOL CMP3Info::Load(LPCSTR lpcszFilename)
{
	CBinFileEx file;
	BOOL bLoaded = FALSE;

	if(file.OpenExistingReadOnly(lpcszFilename))
	{
		int nPos = 0;
		m_qwFileSize = file.GetFileSizeEx();

		// search and load the first frame-header in the file
		char szHeaderChars[4];
		file.Seek(0L,FILE_BEGIN);
		file.Read(szHeaderChars,4);

		nPos += 4;
		BOOL bHeaderFound = FALSE;
		BOOL bEof = FALSE;
		for(; !bHeaderFound && nPos < (1024 * 200) && !bEof; nPos++)
		{
			// convert four chars to CFrameHeader structure
			m_FrameHeader.loadHeader(szHeaderChars);
			
			if(!m_FrameHeader.isValidHeader())
			{
				// read one more byte and try again
				szHeaderChars[0] = szHeaderChars[1];
				szHeaderChars[1] = szHeaderChars[2];
				szHeaderChars[2] = szHeaderChars[3];
				bEof = file.Read(&szHeaderChars[3],1)==FILE_EOF;
			}
			else
				bHeaderFound = TRUE;
		}

		// if no header has been found after 200kB
		// or the end of the file has been reached
		// then there's probably no mp3-file
		if(bHeaderFound)
		{
			/* check for a vbr-header, to ensure the info from a  */
			/* vbr-mp3 is correct                                 */
			char szVbrChars[12];

			// determine offset from first frame-header
			// it depends on two things, the mpeg-version
			// and the mode(stereo/mono)
			int nSkip = 0;
			if(m_FrameHeader.getVersionIndex()==3)// mpeg version 1
			{
				if(m_FrameHeader.getModeIndex()==3)
					nSkip = 17; // Single Channel
				else
					nSkip = 32;
			}
			else // mpeg version 2 or 2.5
			{
				if(m_FrameHeader.getModeIndex()==3)
					nSkip =  9; // Single Channel
				else
					nSkip = 17;
			}

			char skipChars[64];
			file.Read(skipChars,nSkip);
			nPos += nSkip;

			// read next twelve bits in
			file.Read(szVbrChars,12);

			// turn 12 chars into a CVBitRate class structure
			m_bVBitRate = m_VBitRate.loadHeader(szVbrChars);        
		}
			
		bLoaded = bHeaderFound;
		
		file.Close();
	}

	return(bLoaded);
}

/*
	GetLayer()
*/
LPCSTR CMP3Info::GetLayer(void)
{
	switch(GetLayerNumber())
	{
		case 3:
			strcpyn(m_szLayer,"III",sizeof(m_szLayer));
			break;
		case 2:
			strcpyn(m_szLayer,"II",sizeof(m_szLayer));
			break;
		case 1:
			strcpyn(m_szLayer,"I",sizeof(m_szLayer));
			break;
		default:
			strcpyn(m_szLayer,"unknown",sizeof(m_szLayer));
			break;
	}

	return(m_szLayer);
}

/*
	GetBitRate()
*/
int CMP3Info::GetBitRate(void)
{
	if(m_bVBitRate)
	{
		// get average frame size by deviding m_qwFileSize by the number of frames
		float nAvgFrameSize = FDIV((float)m_qwFileSize,(float)GetFrameCount());

		/* Now using the formula for FrameSizes which looks different,
		depending on which mpeg version we're using, for mpeg v1:

		FrameSize = 12 * BitRate / SampleRate + Padding (if there is padding)

		for mpeg v2 the same thing is:

		FrameSize = 144 * BitRate / SampleRate + Padding (if there is padding)

		remember that bitrate is in kbps and sample rate in Hz, so we need to
		multiply our BitRate with 1000.

		For our purpose, just getting the average frame size, will make the
		padding obsolete, so our formula looks like:

		FrameSize = (mpeg1?12:144) * 1000 * BitRate / SampleRate;
		*/
		return((int)(FDIV((nAvgFrameSize * (float)m_FrameHeader.getFrequency()),(1000.0 * (m_FrameHeader.getLayerIndex()==3 ? 12.0 : 144.0)))));

	}
	else
		return(m_FrameHeader.getBitrate());
}

/*
	GetLength()
*/
long CMP3Info::GetLength(void)
{
	// kiloBitFileSize to match kiloBitPerSecond in bitrate...
	long kiloBitFileSize = (long)DIV((8L * m_qwFileSize),1000L);

	return(DIV(kiloBitFileSize,GetBitRate()));
}

/*
	GetLength()
*/
QWORD CMP3Info::GetLength(long& lMinutes,long& lSeconds)
{
	long lLength = GetLength();
	lMinutes = lSeconds = 0L;

	if(lLength > 0L)
	{
		lMinutes = lLength / 60L;
		lSeconds = lLength % 60L;
	}

	return(m_qwFileSize);
}

/*
	GetFrameCount()
*/
int CMP3Info::GetFrameCount(void)
{
	if(!m_bVBitRate)
	{
		/* Now using the formula for FrameSizes which looks different,
		depending on which mpeg version we're using, for layer 1:

		FrameSize = 12 * BitRate / SampleRate + Padding (if there is padding)

		for layer 2 & 3 the same thing is:

		FrameSize = 144 * BitRate / SampleRate + Padding (if there is padding)

		remember that bitrate is in kbps and sample rate in Hz, so we need to
		multiply our BitRate with 1000.

		For our purpose, just getting the average frame size, will make the
		padding obsolete, so our formula looks like:

		FrameSize = (layer1?12:144) * 1000 * BitRate / SampleRate;
		*/
		float nAvgFrameSize = (float)(((m_FrameHeader.getLayerIndex()==3) ? 12 : 144) * (FDIV((1000.0 * (float)m_FrameHeader.getBitrate()),(float)m_FrameHeader.getFrequency())));

		return((int)DIV(m_qwFileSize,nAvgFrameSize));
	}
	else
		return(m_VBitRate.GetFrameCount());
}
