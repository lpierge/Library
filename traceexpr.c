/*$
	traceexpr.c
	Luca Piergentili, 16/01/97
	lpiergentili@yahoo.com
*/
#if defined(_DEBUG) && defined(_WINDOWS)

#include "pragma.h"
#include "env.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#define STRICT 1
#include <windows.h>
#include "traceexpr.h"

static HANDLE hConsoleHandle = INVALID_HANDLE_VALUE;

/*
	trace()
*/
void trace(unsigned long flag,const char* file,unsigned int line,const char* fmt,...)
{
    va_list pArgs;
    DWORD dwWrite = 0L;
    char trace_buf[TRACE_BUF+1] = {0};
    char buf[TRACE_BUF+1] = {0};

    // recupera gli argomenti in modo sicuro
    va_start(pArgs,fmt);
    vsnprintf(buf,sizeof(buf)-1,fmt,pArgs);
    va_end(pArgs);

    // costruisce la linea di debug
    if(file!=__NOFILE__ && line!=__NOLINE__)
    {
        // lo spazio per il file e la riga deve rientrare nel totale di TRACE_BUF
        snprintf(trace_buf,sizeof(trace_buf)-1,"%s(%u): %s",file,line,buf);
    }
    else
    {
        strncpy(trace_buf,buf,sizeof(trace_buf)-1);
    }

    // normalizzazione terminatore (aggiunta \r\n se manca)
    int n = (int)strlen(trace_buf);
    if(n > 0 && n < (sizeof(trace_buf)-2))
    {
        if(trace_buf[n-1]=='\n')
        {
            if(n==1 || trace_buf[n-2]!='\r')
            {
                trace_buf[n-1] = '\r';
                trace_buf[n] = '\n';
                trace_buf[n+1] = '\0';
                n++; // ka lunghezza e' aumentata di 1 (\n -> \r\n)
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
            WriteFile(hFileHandle,trace_buf,(DWORD)strlen(trace_buf),&dwWrite,NULL);
            CloseHandle(hFileHandle);
        }
    }

    /* --- output su console Win32 --- */
    if(flag & _TRFLAG_TRACECONSOLE)
    {
        if(hConsoleHandle==INVALID_HANDLE_VALUE)
        {
            if(AllocConsole())
            {
                SetConsoleTitle("TRACE");
                hConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
            }
        }
        if(hConsoleHandle!=INVALID_HANDLE_VALUE)
            WriteConsole(hConsoleHandle,trace_buf,(DWORD)strlen(trace_buf),&dwWrite,NULL);
    }

    /* --- output nel debugger di Visual Studio --- */
    if(flag & _TRFLAG_TRACEOUTPUT)
    {
        OutputDebugString(trace_buf);

        if(flag & _TRFLAG_TRACEBREAKPOINT)
			__debugbreak();
    }
}

/*
	trace_call()
*/
void trace_call(int flag,int call,char *func)
{
	static int nDeep = 0;
	DWORD dwWrite = 0L;
	char trace_buf[TRACE_BUF] = {0};
	char buf[(TRACE_BUF/2)] = {0};

	if(call==0)
	{
		nDeep += 2;
		snprintf(buf,sizeof(buf),"%c%ds%cs %c%c",'%',nDeep,'%','\r','\n');
		snprintf(trace_buf,sizeof(trace_buf),buf," ",func);
	}
	else
	{
		snprintf(buf,sizeof(buf),"%c%ds%cs %c%c",'%',nDeep,'%','\r','\n');
		snprintf(trace_buf,sizeof(trace_buf),buf," ",func);
		nDeep -= 2;
	}

	if(nDeep <= 0)
		lstrcat(trace_buf,"\r\n");

	if(flag & _TRFLAG_TRACEFILE)
	{
		/* registra su file */
		HANDLE hFileCallHandle = INVALID_HANDLE_VALUE;
		if((hFileCallHandle = CreateFile(TRACECALL_LOG_FILE,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
		{
			SetFilePointer(hFileCallHandle,0L,NULL,FILE_END);
			WriteFile(hFileCallHandle,trace_buf,lstrlen(trace_buf),&dwWrite,NULL);
			CloseHandle(hFileCallHandle);
		}
	}

	if(flag & _TRFLAG_TRACECONSOLE)
	{
		/* visualizza in finestra */
		if(hConsoleHandle==INVALID_HANDLE_VALUE)
		{
			if(AllocConsole())
			{
				SetConsoleTitle("TRACE");
				hConsoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
			}
		}
		if(hConsoleHandle!=INVALID_HANDLE_VALUE)
			WriteConsole(hConsoleHandle,trace_buf,lstrlen(trace_buf),&dwWrite,NULL);
	}

	if(flag & _TRFLAG_TRACEOUTPUT)
	{
		/* visualizza nella finestra del debugger */
		OutputDebugString(trace_buf);
	}
}

/*
	trace_init()
*/
void trace_init(void)
{
	HANDLE hFileHandle = INVALID_HANDLE_VALUE;
	HANDLE hFileCallHandle = INVALID_HANDLE_VALUE;
	if((hFileHandle=CreateFile(TRACE_LOG_FILE,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
		CloseHandle(hFileHandle);
	if((hFileCallHandle = CreateFile(TRACECALL_LOG_FILE,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL))!=INVALID_HANDLE_VALUE)
		CloseHandle(hFileCallHandle);
}

/*
	assert_expr()
*/
void assert_expr(void *expr,void *file,unsigned line)
{
	int nCode = 0;
	char assertbuf[TRACE_BUF] = {0};
	char assertfmt[] = {
		"Assertion failed:\r\n\r\n"\
		"program: %s\r\n"\
		"file: %s\r\n"\
		"line: %ld\r\n\r\n"\
		"expression:\r\n"\
		"%s\r\n\r\n"\
		"Abort to exit program, Ignore to continue and Retry to raise an exception (shows the calls stack)"
		/* "Press Cancel to abort application or Ok to return..." */
	};

	char progname[_MAX_PATH+1] = {0};
	if(!GetModuleFileName(NULL,progname,sizeof(progname)))
		strcpy(progname,"<unknown>");

	memset(assertbuf,'\0',sizeof(assertbuf));
	snprintf(assertbuf,sizeof(assertbuf),assertfmt,progname,file,line,expr);

#if 1
	nCode = MessageBox(NULL,assertbuf,"assert()",MB_ABORTRETRYIGNORE | MB_ICONHAND | MB_TASKMODAL | MB_SETFOREGROUND | MB_TOPMOST);

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
void assert_expr_with_msg(const char *expr,const char *file,unsigned line,const char *msg)
{
	int nCode = 0;
	char assertbuf[TRACE_BUF] = {0};

	/* modificata la stringa di formattazione per includere un nuovo placeholder per il messaggio, include il messaggio solo se presente */
	const char* assertfmt_with_msg = {
		"Assertion failed:\r\n\r\n" \
		"program: %s\r\n" \
		"file: %s\r\n" \
		"line: %ld\r\n\r\n" \
		"expression:\r\n" \
		"%s\r\n\r\n" \
		"message:\r\n" \
		"%s\r\n\r\n" \
		"Abort to exit program, Ignore to continue and Retry to raise an exception (shows the calls stack)"
	};
	const char* assertfmt_no_msg = {
		"Assertion failed:\r\n\r\n" \
		"program: %s\r\n" \
		"file: %s\r\n" \
		"line: %ld\r\n\r\n" \
		"expression:\r\n" \
		"%s\r\n\r\n" \
		"Abort to exit program, Ignore to continue and Retry to raise an exception (shows the calls stack)"
	};

	char progname[_MAX_PATH+1] = {0};
	if(!GetModuleFileName(NULL,progname,sizeof(progname)))
		strcpy(progname,"<unknown>");

	memset(assertbuf,'\0',sizeof(assertbuf));

	if(msg && *msg) 
		snprintf(assertbuf,sizeof(assertbuf),assertfmt_with_msg,progname,file,line,expr,msg);
	else 
		snprintf(assertbuf,sizeof(assertbuf),assertfmt_no_msg,progname,file,line,expr);
	
#if 1
	nCode = MessageBox(NULL,assertbuf,"assert()",MB_ABORTRETRYIGNORE | MB_ICONHAND | MB_TASKMODAL | MB_SETFOREGROUND | MB_TOPMOST);

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
