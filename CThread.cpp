/*$
	CThread.cpp
	Classe per il thread.
	Luca Piergentili, 19/11/02
	lpiergentili@yahoo.com
*/
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "window.h"
#include <process.h>
#include "CThread.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	CThread()
*/
CThread::CThread()
{
	m_bAutoDelete = TRUE;
	m_hHandle = INVALID_HANDLE_VALUE;
	m_nID = 0L;
	m_nState = THREAD_UNDEFINED;
	m_nPriority = -1;
	memset(&m_ThreadParams,'\0',sizeof(THREADPARAMS));
}

/*
	~CThread()
*/
CThread::~CThread()
{
	if(m_hHandle!=INVALID_HANDLE_VALUE)
		::CloseHandle(m_hHandle);
	
	m_hHandle = INVALID_HANDLE_VALUE;
	m_nID = 0L;
	m_nState = THREAD_UNDEFINED;
	m_nPriority = -1;
	memset(&m_ThreadParams,'\0',sizeof(THREADPARAMS));
}

/*
	SetAutoDelete()
*/
void CThread::SetAutoDelete(BOOL bAutoDelete)
{
	m_bAutoDelete = bAutoDelete;
}

/*
	GetHandle()
*/
const HANDLE CThread::GetHandle(void) const
{
	return(m_hHandle);
}

/*
	GetId()
*/
const UINT CThread::GetId(void) const
{
	return(m_nID);
}

/*
	GetStatus()
*/
const THREAD_STATE CThread::GetStatus(void) const
{
	return(m_nState);
}

/*
	GetPriority()
*/
DWORD CThread::GetPriority(void)
{
	return(m_nPriority);
}

/*
	SetPriority()
*/
BOOL CThread::SetPriority(DWORD nPriority)
{
	BOOL bRet = FALSE;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
	{
		m_nPriority = nPriority;
		bRet = ::SetThreadPriority(m_hHandle,m_nPriority);
	}

	return(bRet);
}

/*
	Suspend()
*/
BOOL CThread::Suspend(void)
{
	BOOL bRet = FALSE;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
		bRet = ::SuspendThread(m_hHandle)!=(DWORD)-1L;

#ifdef _DEBUG
	if(!bRet)
	{
		char buffer[512] = {0};
		snprintf(buffer,sizeof(buffer),"unable to suspend thread 0x%x",m_nID);
		::MessageBox(NULL,buffer,"CThread::Suspend()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
	}
#endif

	return(bRet);
}

/*
	Resume()
*/
BOOL CThread::Resume(void)
{
	BOOL bRet = FALSE;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
		bRet = ::ResumeThread(m_hHandle)!=(DWORD)-1L;

#ifdef _DEBUG
	if(!bRet)
	{
		char buffer[512] = {0};
		snprintf(buffer,sizeof(buffer),"unable to resume thread 0x%x",m_nID);
		::MessageBox(NULL,buffer,"CThread::Resume()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
	}
#endif

	return(bRet);
}

/*
	Abort()
*/
BOOL CThread::Abort(void)
{
	BOOL bRet = FALSE;

	if(m_hHandle!=INVALID_HANDLE_VALUE)
	{
		bRet = ::TerminateThread(m_hHandle,0L);
		if(bRet)
			m_hHandle = INVALID_HANDLE_VALUE;
	}

#ifdef _DEBUG
	if(!bRet)
	{
		char buffer[512] = {0};
		snprintf(buffer,sizeof(buffer),"unable to terminate thread 0x%x",m_nID);
		::MessageBox(NULL,buffer,"CThread::Abort()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
	}
#endif

	return(bRet);
}

/*
	Create()
*/
HANDLE CThread::Create(PTHREADPROC pfnThreadProc,LPVOID pParam,int nPriority/* = THREAD_PRIORITY_NORMAL*/,UINT nStackSize/* = 0L*/,DWORD dwCreateFlags/* = 0L*/,LPSECURITY_ATTRIBUTES lpSecurityAttrs/* = NULL*/)
{
	if(m_hHandle==INVALID_HANDLE_VALUE)
	{
		m_ThreadParams.lpVoid = this;
		m_ThreadParams.pfnProc = pfnThreadProc;
		m_ThreadParams.pParam = pParam;

		m_hHandle = (HANDLE)_beginthreadex(
									(void*)lpSecurityAttrs,
									(unsigned)nStackSize,
									&ThreadProc,
									(void*)&m_ThreadParams,
									(unsigned)CREATE_SUSPENDED,
									(unsigned*)&m_nID
									);

		if(m_hHandle!=0)
		{
			m_nPriority = nPriority;
			::SetThreadPriority(m_hHandle,m_nPriority);

			if(!(dwCreateFlags & CREATE_SUSPENDED))
				::ResumeThread(m_hHandle);
		}
#ifdef _DEBUG
		else
		{
			m_hHandle = INVALID_HANDLE_VALUE;
			::MessageBox(NULL,"unable to create thread","CThread::Create()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
		}
#endif
	}
#ifdef _DEBUG
	else
	{
		char buffer[512] = {0};
		snprintf(buffer,sizeof(buffer),"unable to create thread: 0x%x already started",m_nID);
		::MessageBox(NULL,buffer,"CThread::Create()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
	}
#endif

	return(m_hHandle);
}

//#ifdef _WINNT
#if 0
QWORD FileTimeToQuadWord(PFILETIME pFileTime)
{
	QWORD qw;
	qw = pFileTime->dwHighDateTime;
	qw <<= 32;
	qw |= pFileTime->dwLowDateTime;
	return(qw);
}
PFILETIME QuadWordToFileTime(QWORD qw,PFILETIME pFileTime)
{
	pFileTime->dwHighDateTime = (DWORD)(qw >> 32);
	pFileTime->dwLowDateTime = (DWORD)(qw & 0xFFFFFFFF);
	return(pFileTime);
}
#endif

/*
	ThreadProc()
*/
UINT APIENTRY CThread::ThreadProc(LPVOID lpVoid)
{
	THREADPARAMS* pThreadParams = (THREADPARAMS*)lpVoid;
	CThread* This = (CThread*)pThreadParams->lpVoid;
	DWORD dwRet = 0L;

//#ifdef _WINNT
#if 0
	FILETIME ftKernelTimeStart;
	FILETIME ftKernelTimeEnd;
	FILETIME ftUserTimeStart;
	FILETIME ftUserTimeEnd;
	FILETIME ftDummy;
	FILETIME ftTotalTimeElapsed;
	SYSTEMTIME ftSysytemTime;
	QWORD qwKernelTimeElapsed,qwUserTimeElapsed,qwTotalTimeElapsed;
	::GetThreadTimes(GetCurrentThread(),&ftDummy,&ftDummy,&ftKernelTimeStart,&ftUserTimeStart);
#endif

	This->m_nState = THREAD_RUNNING;

#ifdef _DEBUG
	double start = (double)::GetTickCount();
#endif
	dwRet = pThreadParams->pfnProc(pThreadParams->pParam);
#ifdef _DEBUG
	double end = (double)::GetTickCount() - start;
#endif
	
	This->m_nState = THREAD_DONE;

#if 0 //def _WINNT
	::GetThreadTimes(GetCurrentThread(),&ftDummy,&ftDummy,&ftKernelTimeEnd,&ftUserTimeEnd);
	qwKernelTimeElapsed = FileTimeToQuadWord(&ftKernelTimeEnd) - FileTimeToQuadWord(&ftKernelTimeStart);
	qwUserTimeElapsed = FileTimeToQuadWord(&ftUserTimeEnd) - FileTimeToQuadWord(&ftUserTimeStart);
	qwTotalTimeElapsed = qwKernelTimeElapsed + qwUserTimeElapsed;
	QuadWordToFileTime(qwTotalTimeElapsed,&ftTotalTimeElapsed);
	::FileTimeToSystemTime(&ftTotalTimeElapsed,&ftSysytemTime);
	double secs = (double)ftSysytemTime.wMilliseconds / (double)1000;
#endif

#ifdef _DEBUG
	end /= (double)1000;
	TRACEEXPR((_TRACE_FLAG_INFO,__NOFILE__,__NOLINE__,(char*)"CThread::ThreadProc(): the thread 0x%x has spent %.3f secs.\n",This->m_nID,end));
#endif

	if(This->m_bAutoDelete)
		delete This;

	return(dwRet);
}

/*
	BeginThread()
*/
CThread* BeginThread(
				PTHREADPROC pfnThreadProc,
				LPVOID pParam,
				int nPriority/* = THREAD_PRIORITY_NORMAL*/,
				UINT nStackSize/* = 0L*/,
				DWORD dwCreateFlags/* = 0L*/,
				LPSECURITY_ATTRIBUTES lpSecurityAttrs/* = NULL*/
				)
{
	CThread* pThread = new CThread();	
	if(pThread)
	{
		pThread->Create(pfnThreadProc,pParam,nPriority,nStackSize,dwCreateFlags,lpSecurityAttrs);
	}
#ifdef _DEBUG
	else
	{
		::MessageBox(NULL,"unable to create a new thread.","BeginThread()",MB_OK|MB_ICONERROR|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
	}
#endif
	
	return(pThread);
}
