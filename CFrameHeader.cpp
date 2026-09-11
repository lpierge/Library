/*
	CFrameHeader.cpp

	CFrameHeader class is used to retrieve a MP3's FrameHeader
	and load that into a usable structure.

	This code will be well commented, so that everyone can
	understand, as it's made for the public and not for
	private use, although private use is allowed. :)

	all functions specified both in the header and .cpp file
	will have explanations in both locations.

	everything here by: Gustav "Grim Reaper" Munkby
				   http://home.swipnet.se/grd/
				   grd@swipnet.se

	LPI 7/8/03: modifiche minori al codice originale.
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "CFrameHeader.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	loadHeader()
*/
void CFrameHeader::loadHeader(char c[4])
{
	// This function is quite easy to understand, it loads
	// 4 chars of information into the CFrameHeader class
	// The validity is not tested, so qith this function
	// an invalid FrameHeader could be retrieved

	// this thing is quite interesting, it works like the following
	// c[0] = 00000011
	// c[1] = 00001100
	// c[2] = 00110000
	// c[3] = 11000000
	// the operator << means that we'll move the bits in that direction
	// 00000011 << 24 = 00000011000000000000000000000000
	// 00001100 << 16 =         000011000000000000000000
	// 00110000 <<  8 =                 0011000000000000
	// 11000000       =                         11000000
	//                +_________________________________
	//                  00000011000011000011000011000000
	m_ulBithdr = (unsigned long)(
						( (c[0] & 255) << 24) |
						( (c[1] & 255) << 16) |
						( (c[2] & 255) <<  8) |
						( (c[3] & 255)      )
						);
}

/*
	isValidHeader()
*/
bool CFrameHeader::isValidHeader(void)
{
	// This function is a supplement to the loadHeader
	// function, the only purpose is to detect if the
	// header loaded by loadHeader is a valid header
	// or just four different chars

	return(	((getFrameSync()      & 2047)==2047) &&
			((getVersionIndex()   &    3)!=   1) &&
			((getLayerIndex()     &    3)!=   0) && 
			((getBitrateIndex()   &   15)!=   0) &&  // due to lack of support of the .mp3 format no "public" .mp3's should contain information like this anyway... :)
			((getBitrateIndex()   &   15)!=  15) &&
			((getFrequencyIndex() &    3)!=   3) &&
			((getEmphasisIndex()  &    3)!=   2)
			);
}

/*
	getVersion()
*/
float CFrameHeader::getVersion(void)
{
	// this returns the MPEG version [1.0-2.5]
	
	// a table to convert the indexes into
	// something informative...
	static const float table[4] = {2.5, 0.0, 2.0, 1.0};

	// return modified value
	return(table[getVersionIndex()]);
}

/*
	GetLayerNumber()
*/
int CFrameHeader::GetLayerNumber(void)
{
	// this returns the Layer [1-3]

	// when speaking of layers there is a 
	// cute coincidence, the Layer always
	// equals 4 - layerIndex, so that's what
	// we will return
	return(4 - getLayerIndex());
}

/*
	getBitrate()
*/
int CFrameHeader::getBitrate(void)
{
	// this returns the current bitrate [8-448 kbps]
	
	// a table to convert the indexes into
	// something informative...
	static const int table[2][3][16] = {
	{ //MPEG 2 & 2.5
	{0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,0}, //Layer III
	{0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160,0}, //Layer II
	{0, 32, 48, 56, 64, 80, 96,112,128,144,160,176,192,224,256,0}  //Layer I
	}
	,
	{ //MPEG 1
	{0, 32, 40, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,0}, //Layer III
	{0, 32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384,0}, //Layer II
	{0, 32, 64, 96,128,160,192,224,256,288,320,352,384,416,448,0}  //Layer I
	}
	};

	// the bitrate is not only dependent of the bitrate index,
	// the bitrate also varies with the MPEG version and Layer version
	return(table[(getVersionIndex() & 1)][(getLayerIndex() -1)][getBitrateIndex()]);
}

/*
	getFrequency()
*/
int CFrameHeader::getFrequency(void)
{
	// this returns the current frequency [8000-48000 Hz]

	// a table to convert the indexes into
	// something informative...
	static const int table[4][3] = {
	{32000, 16000,  8000}, //MPEG 2.5
	{    0,     0,     0}, //reserved
	{22050, 24000, 16000}, //MPEG 2
	{44100, 48000, 32000}  //MPEG 1
	};

	// the frequency is not only dependent of the bitrate index,
	// the bitrate also varies with the MPEG version
	return(table[getVersionIndex()][getFrequencyIndex()]);
}

/*
	getMode()
*/
const char* CFrameHeader::getMode(void)
{
	// the purpose of getMode is to get information about
	// the current playing mode, such as:
	// "Joint Stereo"

	// here you could use a array of strings instead
	// but I think this method is nicer, at least
	// when not dealing with that many variations
	switch(getModeIndex())
	{
		case 1:
			return ("joint stereo");
		case 2:
			return ("dual channel");
		case 3:
			return ("single channel");
		default:
			return ("stereo");
	}
}
