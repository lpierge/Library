/*$
	traceexpr.c
	Luca Piergentili, 16/01/97
	lpiergentili@yahoo.com

	Vedi le note in traceexpr.h.
*/
#if defined(_DEBUG) && defined(_WINDOWS)

#include "pragma.h"
#include "env.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <tchar.h> /* TCHAR, _T(), etc. */
#define STRICT 1
#include <windows.h>
#include <psapi.h>
#pragma comment(lib,"psapi.lib")
#include "traceexpr.h"

static HANDLE m_hConsoleHandle = INVALID_HANDLE_VALUE;		// handle della console
static CRITICAL_SECTION m_csTraceLock = {0};				// sezione critica per coordinare l'accesso multithread
static INIT_ONCE m_TraceInitOnce = INIT_ONCE_STATIC_INIT;	// per l'inizializzazione (automatica) della sezione critica
static volatile LONG m_bCsDestroyed = 0;					// per l'eliminazione (automatica) della sezione critica

void WhoAmI(void)
{
	HMODULE hMyModule = NULL;
	HMODULE hExeModule = GetModuleHandle(NULL); // ritorna SEMPRE l'HMODULE dell'EXE principale

	// chiede al sistema di trovare il modulo partendo dall'indirizzo di questa funzione
	if(GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,(LPCTSTR)WhoAmI,&hMyModule))
	{
		if(hMyModule==hExeModule)
		{
			// il codice sta eseguendo nel contesto dell'EXE principale
			OutputDebugString(_T("running in an executable\n"));
		}
		else
		{
			// il codice sta eseguendo dentro una DLL
			TCHAR szDllPath[MAX_PATH+1] = {0};
			GetModuleFileName(hMyModule,szDllPath,MAX_PATH);
            
			// szDllPath conterra' il percorso completo della DLL (es. C:\Programmi\libreria.dll)
			OutputDebugString(_T("running in a DLL\n"));
		}
	}
}

DWORD GetCurrentProcessIdAndName(TCHAR* pszNameBuffer,DWORD dwNameSize)
{
	DWORD dwPid = GetCurrentProcessId();
    
	if(pszNameBuffer && dwNameSize > 0L)
	{
		HANDLE hProcess = GetCurrentProcess();
		if(hProcess)
		{
			// prova prima con GetModuleBaseName (piu' efficiente)
			if(GetModuleBaseName(hProcess,NULL,pszNameBuffer,dwNameSize)==0L)
			{
				// prova con GetProcessImageFileName
				TCHAR szPath[MAX_PATH+1] = {0};
				if(GetProcessImageFileName(hProcess,szPath,MAX_PATH) > 0L)
				{
					// estrae solo il nome del file dal percorso completo
					TCHAR* pszFileName = _tcsrchr(szPath, _T('\\'));
					if(pszFileName)
						_tcscpy_s(pszNameBuffer,dwNameSize,pszFileName + 1);
					else
						_tcscpy_s(pszNameBuffer,dwNameSize,szPath);
				}
				else
				{
					_tcscpy_s(pszNameBuffer,dwNameSize,_T("<unknown>"));
				}
			}
		}
	}
    
	return(dwPid);
}

/*
	Nota: qui si pongono due problemi diferenti:

	1) La gestione multithread, che si risolve con l'uso della sezione critica.
	2) La gestione della chiamata automatica delle due funzioni trace_init() e trace_term(),
	   per ovviare all'eventuale problema dell'inizializzazione/terminazione della sezione
	   critica se chi usa il codice di TRACE non chiama tali funzioni
*/

// mettere qui le forward declarations
void __cdecl trace_cleanup(void);

/*
	InitTraceCallback()

	Callback per il meccanismo INIT_ONCE.
	Per assicurare la inizializzazione univoca della sezione critica se l'utente si dimentica (o NO) di chiamare trace_init().
*/
BOOL CALLBACK InitTraceCallback(PINIT_ONCE InitOnce,PVOID Parameter,PVOID *Context)
{
	InitializeCriticalSection(&m_csTraceLock);
    atexit(trace_cleanup); // registra la funzione di pulizia automatica (al termine dell'eseguibile)
	return(TRUE);
}

/*
	CriticalSectionTeardown()

	Eliminazione (centralizzata) della sezione critica (l'inizializzazione viene garantita da InitTraceCallback()).
	Per assicurare la eliminazione univoca della sezione critica se l'utente si dimentica (o NO) di chiamare trace_term().
*/
static void CriticalSectionTeardown(void)
{
	BOOL bPending = FALSE;
	PVOID pContext = NULL;
    
	// controlla se l'INIT_ONCE e' mai partito
	// se nessuno ha mai chiamato trace()/trace_call(), non c'e' nulla da distruggere
	if(InitOnceBeginInitialize(&m_TraceInitOnce,INIT_ONCE_CHECK_ONLY,&bPending,&pContext))
	{
		// trucco atomico: cambia il flag a 1
		// InterlockedExchange() restituisce il vecchio valore
		// solo il primo che arriva vedra' restituito '0' ed entrara' nell'if
		if(InterlockedExchange(&m_bCsDestroyed,1)==0)
			DeleteCriticalSection(&m_csTraceLock);
	}
}

/*
	trace_cleanup()

	Garantisce la pulizia automatica alla chiusura dell'eseguibile.
*/
void __cdecl trace_cleanup(void)
{
	// viene chiamata dal runtime C alla fine del programma
	// per compensare l'eventuale mancata chiamata (da parte del chiamante) della trace_term()
	CriticalSectionTeardown();
}

/*
	trace_init()
*/
void trace_init(unsigned long flag)
{
	// Windows controlla atomicamente se la callback e' gia' stata eseguita
    // se NO, la esegue (bloccando temporaneamente gli altri thread)
    // se SI, salta la callback all'istante (questione di frazioni di nanosecondo)
    InitOnceExecuteOnce(&m_TraceInitOnce,InitTraceCallback,NULL,NULL);
	
	if(flag & _TRFLAG_TRACEFILE)
	{
		/* azzera il file per ogni nuova sessione */
		HANDLE hFileHandle = INVALID_HANDLE_VALUE;
		HANDLE hFileCallHandle = INVALID_HANDLE_VALUE;
		if((hFileHandle=CreateFile(TRACE_LOG_FILE,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
			CloseHandle(hFileHandle);
		if((hFileCallHandle = CreateFile(TRACECALL_LOG_FILE,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
			CloseHandle(hFileCallHandle);
	}
	if(flag & _TRFLAG_TRACECONSOLE)
	{
		;
	}
}

/*
	trace_term()
*/
void trace_term(unsigned long flag)
{
	// vedi le note in trace_init()
	CriticalSectionTeardown();

	if(flag & _TRFLAG_TRACEFILE)
	{
		;
	}
	if(flag & _TRFLAG_TRACECONSOLE)
	{
		if(m_hConsoleHandle!=INVALID_HANDLE_VALUE)
		{
			FreeConsole();
			m_hConsoleHandle = INVALID_HANDLE_VALUE;
		}
	}
}

/*
	trace()
*/
void trace(unsigned long flag,const TCHAR* file,unsigned int line,const TCHAR* fmt,...)
{
    va_list pArgs;
    DWORD dwWrite = 0L;
    TCHAR trace_buf[TRACE_BUF+1] = {0};
    TCHAR buf[TRACE_BUF+1] = {0};

	// vedi le note in trace_init()
    InitOnceExecuteOnce(&m_TraceInitOnce,InitTraceCallback,NULL,NULL);

	// nel caso dovesse entrare la chiamata di un thread dopo che un altro thread abbia gia' chiuso tutto
	if(m_bCsDestroyed)
		return;

	EnterCriticalSection(&m_csTraceLock);

    // recupera gli argomenti in modo sicuro
    va_start(pArgs,fmt);
	_vsntprintf_s(buf,_countof(buf),_TRUNCATE,fmt,pArgs);
    va_end(pArgs);

    // costruisce la linea di debug
    if(file!=__NOFILE__ && line!=__NOLINE__)
    {
        // lo spazio per il file e la riga deve rientrare nel totale di TRACE_BUF
		_sntprintf_s(trace_buf,_countof(trace_buf),_TRUNCATE,_T("TRACE: %s(%u): %s"),file,line,buf);

		// per char: sizeof -> restituisce dimensione in byte
		// per wchar_t: -> countof restituisce il numero di elementi (caratteri) dell'array, indipendentemente dalla dimensione (ogni wchar_t sono 2 bytes)
		// (NON usare countof su puntatori, solo su array statici)
    }
    else
    {
        //strncpy(trace_buf,buf,sizeof(trace_buf)-1);
		_tcsncpy_s(trace_buf,_countof(trace_buf),buf,_TRUNCATE); // countof per array statici, dimensione per puntatori
    }

    // normalizzazione terminatore (aggiunta \r\n se manca)
	int n = (int)_tcslen(trace_buf);
    if(n > 0 && n < (_countof(trace_buf)-2))
    {
        if(trace_buf[n-1]==_T('\n'))
        {
            if(n==1 || trace_buf[n-2]!=_T('\r'))
            {
                trace_buf[n-1] = _T('\r');
                trace_buf[n] = _T('\n');
                trace_buf[n+1] = _T('\0');
                n++; // la lunghezza e' aumentata di 1 (\n -> \r\n)
            }
        }
    }

    /* --- output su file --- */
    if(flag & _TRFLAG_TRACEFILE)
    {
        HANDLE hFileHandle = CreateFile(TRACE_LOG_FILE,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        if(hFileHandle!=INVALID_HANDLE_VALUE)
        {
            SetFilePointer(hFileHandle,0L,NULL,FILE_END);
            WriteFile(hFileHandle,trace_buf,(DWORD)_tcslen(trace_buf),&dwWrite,NULL);
            CloseHandle(hFileHandle);
        }
    }

    /* --- output su console Win32 --- */
    if(flag & _TRFLAG_TRACECONSOLE)
    {
        if(m_hConsoleHandle==INVALID_HANDLE_VALUE)
        {
			BOOL bAttached = AllocConsole();
			if(!bAttached)
			{
				if(GetLastError()==ERROR_ACCESS_DENIED) /* gia' attaccato ad una console (la stessa del programma in esecuzione quando questo e' da linea di comando) */
					bAttached = TRUE;
			}
			if(!bAttached)
				bAttached = AttachConsole(ATTACH_PARENT_PROCESS);
            if(bAttached)
            {
				TCHAR szProcess[_MAX_PATH+1] = {0};
				DWORD dwPid = GetCurrentProcessIdAndName(szProcess,_MAX_PATH);
				TCHAR szTitle[_MAX_PATH+1] = {0};
				_sntprintf_s(szTitle,_MAX_PATH,_TRUNCATE,_T("TRACE (process:%s pid:%ld)"),szProcess,dwPid);
                SetConsoleTitle(szTitle);
                m_hConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            }
        }
        if(m_hConsoleHandle!=INVALID_HANDLE_VALUE)
            WriteConsole(m_hConsoleHandle,trace_buf,(DWORD)_tcslen(trace_buf),&dwWrite,NULL);
    }

    /* --- output nel debugger di Visual Studio --- */
    if(flag & _TRFLAG_TRACEOUTPUT)
    {
        OutputDebugString(trace_buf);

        if(flag & _TRFLAG_TRACEBREAKPOINT)
			__debugbreak();
    }

	LeaveCriticalSection(&m_csTraceLock);
}

/*
	trace_call()
*/
void trace_call(int flag,int call,char *func)
{
	static int nDeep = 0;
	DWORD dwWrite = 0L;
	TCHAR trace_buf[TRACE_BUF] = {0};
	TCHAR buf[(TRACE_BUF/2)] = {0};

	// vedi le note in trace_init()
    InitOnceExecuteOnce(&m_TraceInitOnce,InitTraceCallback,NULL,NULL);

	// nel caso dovesse entrare la chiamata di un thread dopo che un altro thread abbia gia' chiuso tutto
	if(m_bCsDestroyed)
		return;

	EnterCriticalSection(&m_csTraceLock);

	if(call==0)
	{
		nDeep += 2;
		_sntprintf_s(buf,_countof(buf),_TRUNCATE,_T("%c%ds%cs %c%c"),_T('%'),nDeep,_T('%'),_T('\r'),_T('\n'));
		_sntprintf_s(trace_buf,_countof(trace_buf),_TRUNCATE,buf,_T(" "),func);
	}
	else
	{
		_sntprintf_s(buf,_countof(buf),_TRUNCATE,_T("%c%ds%cs %c%c"),_T('%'),nDeep,_T('%'),_T('\r'),_T('\n'));
		_sntprintf_s(trace_buf,_countof(trace_buf),_TRUNCATE,buf,_T(" "),func);
		nDeep -= 2;
	}

	if(nDeep <= 0)
		_tcscat_s(trace_buf,_countof(trace_buf),_T("\r\n"));

	if(flag & _TRFLAG_TRACEFILE)
	{
		/* registra su file */
		HANDLE hFileCallHandle = INVALID_HANDLE_VALUE;
		if((hFileCallHandle = CreateFile(TRACECALL_LOG_FILE,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
		{
			SetFilePointer(hFileCallHandle,0L,NULL,FILE_END);
			WriteFile(hFileCallHandle,trace_buf,(DWORD)_tcslen(trace_buf),&dwWrite,NULL);
			CloseHandle(hFileCallHandle);
		}
	}

	if(flag & _TRFLAG_TRACECONSOLE)
	{
		/* visualizza in finestra */
        if(m_hConsoleHandle==INVALID_HANDLE_VALUE)
        {
			BOOL bAttached = AllocConsole();
			if(!bAttached)
			{
				if(GetLastError()==ERROR_ACCESS_DENIED) /* gia' attaccato ad una console (la stessa del programma in esecuzione quando questo e' da linea di comando) */
					bAttached = TRUE;
			}
			if(!bAttached)
				bAttached = AttachConsole(ATTACH_PARENT_PROCESS);
            if(bAttached)
            {
				TCHAR szProcess[_MAX_PATH+1] = {0};
				DWORD dwPid = GetCurrentProcessIdAndName(szProcess,_MAX_PATH);
				TCHAR szTitle[_MAX_PATH+1] = {0};
				_sntprintf_s(szTitle,_MAX_PATH,_TRUNCATE,_T("TRACE(%ld:%s)"),dwPid,szProcess);
                SetConsoleTitle(szTitle);
                m_hConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            }
        }
        if(m_hConsoleHandle!=INVALID_HANDLE_VALUE)
            WriteConsole(m_hConsoleHandle,trace_buf,(DWORD)_tcslen(trace_buf),&dwWrite,NULL);
	}

    if(flag & _TRFLAG_TRACEOUTPUT)
    {
		/* visualizza nella finestra del debugger */
        OutputDebugString(trace_buf);

        if(flag & _TRFLAG_TRACEBREAKPOINT)
			__debugbreak();
    }

	LeaveCriticalSection(&m_csTraceLock);
}

/*
	assert_expr()
*/
void assert_expr(void *expr,void *file,unsigned line)
{
	int nCode = 0;
	TCHAR assertbuf[TRACE_BUF] = {0};
	TCHAR assertfmt[TRACE_BUF] = {
		_T("Assertion failed:\r\n\r\n")\
		_T("program: %s\r\n")\
		_T("file: %s\r\n")\
		_T("line: %ld\r\n\r\n")\
		_T("expression:\r\n")\
		_T("%s\r\n\r\n")\
		_T("Abort to exit program, Ignore to continue and Retry to raise an exception (shows the calls stack)")
		/* "Press Cancel to abort application or Ok to return..." */
	};

	TCHAR progname[_MAX_PATH+1] = {0};
	if(!GetModuleFileName(NULL,progname,_countof(progname)))
		_tcscpy_s(progname,_countof(progname),_T("<unknown>"));

	memset(assertbuf,'\0',_countof(assertbuf));
	_sntprintf_s(assertbuf,_countof(assertbuf),_TRUNCATE,assertfmt,progname,(const TCHAR*)file,line,(const TCHAR*)expr);


#if 1
	nCode = MessageBox(NULL,assertbuf,_T("assert()"),MB_ABORTRETRYIGNORE | MB_ICONHAND | MB_TASKMODAL | MB_SETFOREGROUND | MB_TOPMOST);

	/* termina il programma */
	if(nCode==IDABORT)
	{
		raise(SIGABRT);
		_exit(3);
	}
	/* continua con l'esecuzione */
	else if(nCode==IDIGNORE)
	{
		return;
	}
	/* break nel debugger */
	else if(nCode==IDRETRY)
	{
		__debugbreak();
	}
#else
	nCode = MessageBox(NULL,assertbuf,"assert()",MB_OKCANCEL|MB_ICONHAND|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
	if(nCode==IDCANCEL)
	{
		/* raise abort signal */
		raise(SIGABRT);

		/* we usually won't get here, but it's possible that SIGABRT was ignored, so exit the program anyway */
		_exit(3);
	}
	/* Ignore: continue execution */
	else if(nCode==IDOK)
		return;

	abort();
#endif
}

/*
	assert_expr_with_msg()
*/
void assert_expr_with_msg(const TCHAR* expr,const TCHAR* file,unsigned line,const TCHAR* msg)
{
	int nCode = 0;
	TCHAR assertbuf[TRACE_BUF] = {0};

	/* modificata la stringa di formattazione per includere un nuovo placeholder per il messaggio, include il messaggio solo se presente */
	const TCHAR* assertfmt_with_msg = {
		_T("Assertion failed:\r\n\r\n") \
		_T("program: %s\r\n") \
		_T("file: %s\r\n") \
		_T("line: %ld\r\n\r\n") \
		_T("expression:\r\n") \
		_T("%s\r\n\r\n") \
		_T("message:\r\n") \
		_T("%s\r\n\r\n") \
		_T("Abort to exit program, Ignore to continue and Retry to raise an exception (shows the calls stack)")
	};
	const TCHAR* assertfmt_no_msg = {
		_T("Assertion failed:\r\n\r\n") \
		_T("program: %s\r\n") \
		_T("file: %s\r\n") \
		_T("line: %ld\r\n\r\n") \
		_T("expression:\r\n") \
		_T("%s\r\n\r\n") \
		_T("Abort to exit program, Ignore to continue and Retry to raise an exception (shows the calls stack)")
	};

	TCHAR progname[_MAX_PATH+1] = {0};
	if(!GetModuleFileName(NULL,progname,_countof(progname)))
		_tcscpy_s(progname,_countof(progname),_T("<unknown>"));

	memset(assertbuf,'\0',_countof(assertbuf));

	if(msg && *msg) 
		_sntprintf_s(assertbuf,_countof(assertbuf),_TRUNCATE,assertfmt_with_msg,progname,file,line,expr,msg);
	else 
		_sntprintf_s(assertbuf,_countof(assertbuf),_TRUNCATE,assertfmt_no_msg,progname,file,line,expr);
	
#if 1
	nCode = MessageBox(NULL,assertbuf,_T("assert()"),MB_ABORTRETRYIGNORE | MB_ICONHAND | MB_TASKMODAL | MB_SETFOREGROUND | MB_TOPMOST);

	/* termina il programma */
	if(nCode==IDABORT)
	{
		raise(SIGABRT);
		_exit(3);
	}
	/* continua con l'esecuzione */
	else if(nCode==IDIGNORE)
	{
		return;
	}
	/* break nel debugger */
	else if(nCode==IDRETRY)
	{
		__debugbreak();
	}
#else
	nCode = MessageBox(NULL,assertbuf,"assert()",MB_OKCANCEL|MB_ICONHAND|MB_TASKMODAL|MB_SETFOREGROUND|MB_TOPMOST);
	if(nCode==IDCANCEL)
	{
		/* raise abort signal */
		raise(SIGABRT);

		/* we usually won't get here, but it's possible that SIGABRT was ignored, so exit the program anyway */
		_exit(3);
	}
	/* Ignore: continue execution */
	else if(nCode==IDOK)
		return;

	abort();
#endif
}

#endif /* defined(_DEBUG) && defined(_WINDOWS) */

#undef _TRACE_C
