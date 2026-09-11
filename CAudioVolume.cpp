/*
	CAudioVolume.cpp
	Classe per l'impostazione del volume.
	Luca Piergentili, 22/08/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdlib.h>
#include "strings.h"
#include "window.h"
#include <mmsystem.h>
#include "CAudioVolume.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

BEGIN_MESSAGE_MAP(CAudioVolume,CWnd)
	ON_WM_DESTROY()
	ON_MESSAGE(MM_MIXM_CONTROL_CHANGE,OnMixerControlChange)
END_MESSAGE_MAP()

/*
	CAudioVolume()
*/
CAudioVolume::CAudioVolume()
{
	m_nMixersCount = 0L;
	m_hMixer = NULL;
	memset(&m_stMixerCaps,'\0',sizeof(MIXERCAPS));
	m_lLevel = m_lMinLevel = m_lMaxLevel = m_lStepValue = 0L;
	m_dwVolumeControlID = 0L;

	// crea la finestra per la ricezione dei messaggi di notifica inviati da MCI
	CString strClassName = AfxRegisterWndClass(CS_BYTEALIGNCLIENT|CS_BYTEALIGNWINDOW,0,0,0);
	CreateEx(0,strClassName,"AudioVolumeClass",0,1,1,1,1,NULL,NULL,NULL);

	if(mixerInitialize())
		if(mixerGetMasterVolumeControl())
			mixerGetMasterVolumeValue(m_lLevel);
}

/*
	~CAudioVolume()
*/
CAudioVolume::~CAudioVolume()
{
	mixerUninitialize();

	// per la derivazione da CWnd
	DestroyWindow();
}

/*
	OnDestroy()
*/
void CAudioVolume::OnDestroy(void)
{
	CWnd::OnDestroy();
}

/*
	OnMixerControlChange()
*/
LRESULT CAudioVolume::OnMixerControlChange(WPARAM wParam,LPARAM lParam)
{
	if((HMIXER)wParam==m_hMixer && (DWORD)lParam==m_dwVolumeControlID)
		mixerGetMasterVolumeValue(m_lLevel);
	return(0L);
}

/*
	Increase()
*/
BOOL CAudioVolume::Increase(long lValue/* = -1L*/)
{
	BOOL bRes = TRUE;
	if(lValue==-1L)
		lValue = m_lStepValue;
	m_lLevel += lValue;
	if(m_lLevel > m_lMaxLevel)
	{
		m_lLevel = m_lMaxLevel;
		bRes = FALSE;
		::MessageBeep(MB_ICONASTERISK);
	}
	mixerSetMasterVolumeValue(m_lLevel);
	return(bRes);
}

/*
	Decrease()
*/
BOOL CAudioVolume::Decrease(long lValue/* = -1L*/)
{
	BOOL bRes = TRUE;
	if(lValue==-1L)
		lValue = m_lStepValue;
	m_lLevel -= lValue;
	if(m_lLevel < m_lMinLevel)
	{
		m_lLevel = m_lMinLevel;
		bRes = FALSE;
		::MessageBeep(MB_ICONASTERISK);
	}
	mixerSetMasterVolumeValue(m_lLevel);
	return(bRes);
}

/*
	mixerInitialize()
*/
BOOL CAudioVolume::mixerInitialize(void)
{
	BOOL bRes = FALSE;

	if(!m_hMixer)
	{
		m_nMixersCount = 0L;
		m_hMixer = NULL;
		memset(&m_stMixerCaps,'\0',sizeof(MIXERCAPS));
		m_lLevel = m_lMinLevel = m_lMaxLevel = m_lStepValue = 0L;
		m_dwVolumeControlID = 0L;

		if((m_nMixersCount = ::mixerGetNumDevs()) > 0L)
		{
			if(::mixerOpen(&m_hMixer,0,(DWORD)CWnd::m_hWnd,NULL,MIXER_OBJECTF_MIXER|CALLBACK_WINDOW)==MMSYSERR_NOERROR)
				if(::mixerGetDevCaps((UINT)m_hMixer,&m_stMixerCaps,sizeof(MIXERCAPS))==MMSYSERR_NOERROR)
					bRes = TRUE;
		}
	}

	return(bRes);
}

/*
	mixerUninitialize()
*/
BOOL CAudioVolume::mixerUninitialize(void)
{
	BOOL bRes = FALSE;

	if(m_hMixer)
	{
		bRes = ::mixerClose(m_hMixer)==MMSYSERR_NOERROR;
		m_hMixer = NULL;
	}

	return(bRes);
}

/*
	mixerGetMasterVolumeControl
*/
BOOL CAudioVolume::mixerGetMasterVolumeControl(void)
{
	BOOL bRes = FALSE;

	if(m_hMixer)
	{
		MIXERLINE mxl;
		mxl.cbStruct = sizeof(MIXERLINE);
		mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
		if(::mixerGetLineInfo((HMIXEROBJ)m_hMixer,&mxl,MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE)==MMSYSERR_NOERROR)
		{
			MIXERCONTROL mxc;
			MIXERLINECONTROLS mxlc;
			mxlc.cbStruct = sizeof(MIXERLINECONTROLS);
			mxlc.dwLineID = mxl.dwLineID;
			mxlc.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
			mxlc.cControls = 1;
			mxlc.cbmxctrl = sizeof(MIXERCONTROL);
			mxlc.pamxctrl = &mxc;
			if(::mixerGetLineControls((HMIXEROBJ)m_hMixer,&mxlc,MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE)==MMSYSERR_NOERROR)
			{
				m_lMinLevel = (long)mxc.Bounds.dwMinimum;
				m_lMaxLevel = (long)mxc.Bounds.dwMaximum;
				m_lStepValue = m_lMaxLevel / 100L;
				m_dwVolumeControlID = mxc.dwControlID;
				bRes = TRUE;
			}
		}
	}

	return(bRes);
}

/*
	mixerGetMasterVolumeValue
*/
BOOL CAudioVolume::mixerGetMasterVolumeValue(long &lValue)
{
	BOOL bRes = FALSE;
	
	if(m_hMixer)
	{
		MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
		MIXERCONTROLDETAILS mxcd;
		mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
		mxcd.dwControlID = m_dwVolumeControlID;
		mxcd.cChannels = 1;
		mxcd.cMultipleItems = 0;
		mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
		mxcd.paDetails = &mxcdVolume;
		if(::mixerGetControlDetails((HMIXEROBJ)m_hMixer,&mxcd,MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE)==MMSYSERR_NOERROR)
		{
			lValue = mxcdVolume.dwValue;
			bRes = TRUE;
		}
	}

	return(bRes);
}

/*
	mixerSetMasterVolumeValue()
*/
BOOL CAudioVolume::mixerSetMasterVolumeValue(long lValue)
{
	BOOL bRes = FALSE;

	if(m_hMixer)
	{
		MIXERCONTROLDETAILS_UNSIGNED mxcdVolume;
		mxcdVolume.dwValue = lValue;
		MIXERCONTROLDETAILS mxcd;
		mxcd.cbStruct = sizeof(MIXERCONTROLDETAILS);
		mxcd.dwControlID = m_dwVolumeControlID;
		mxcd.cChannels = 1;
		mxcd.cMultipleItems = 0;
		mxcd.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
		mxcd.paDetails = &mxcdVolume;
		if(::mixerSetControlDetails((HMIXEROBJ)m_hMixer,&mxcd,MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE)==MMSYSERR_NOERROR)
			bRes = TRUE;
	}
		
	return(bRes);
}
