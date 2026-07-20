/*$
	getopt.c
	Gestione opzioni/parametri ricevuti da linea di comando.
	Luca Piergentili, 31/08/98
	lpiergentili@yahoo.com

	Tenere presente che il codice originale e' del 1998, l'epoca di Windows 95/98 e dei primi NT: alcuni programmi
	richiedevano argomenti nel formato /opzione:valore o /opzione attaccato al valore; il modo in cui i compilatori
	dell'epoca (Borland, Watcom, Zortech, prime versioni MSVC) processavano i doppi apici e popolavano l'array argv 
	non era standardizzato come oggigiorno; e cosi0 via, quindi all'epoca il codice venne strutturato per funzionare
	come un vero e proprio parser per l'intera linea di comando ricevuta, mentre attualmente e' la CRT si incarica
	di tutto cio', organizzando i vari elementi nell'array argv ed eliminando quindi i doppi apici specificati negli
	argomenti sulla linea di comando.
	Per questo, parallelamente alla versione originale, e' stata implementata una versione "moderna" che prende in
	considerazione solo argv, senza infognarsi nel parsing della linea di comando intera e risolvendo una volta per
	tutte il problema dei doppi apici.

	Le macro GETOPT_CMD/GETOPT_ARGV controllano quale versione usare: l'originale, basata sul parsing dell'intera
	linea di comando, o la nuova, basata sugli elementi di argv. La macro va definita a livello di progetto, dato
	che i files (getopt.c/.h) vengono condivisi tra vari progetti.
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include "typeval.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "strings.h"
#include <ctype.h>
#include "window.h"
#include "getopt.h"

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
//#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	GetSafeCommandLine()

	Elimina dalla linea di comando il nome dell'eseguibile (argv[0]).

	Viene usata per ovviare allo spazio che la shell di Win32 piazza alla fine di argv[0], ed per saltare gli eventuali
	spazi che potrebbe contenere il nome dell'eseguibile, come in: "C:\Program Files\test.exe".

	Lo spazio alla fine del nome dell'eseguibile (anche se non sono presenti parametri) viene aggiunto dalla shell per
	compatibilita' con il vecchio PSP (Program Segment Prefix) del DOS o per puttanate varie come le utility di sistema
	(incluso cmd.exe) che internamente hanno una logica del tipo: sprintf(buffer,"%s %s",exe,params); e se params e' una
	stringa vuota, terminano producendo exe + spazio + NULL
*/
char* GetSafeCommandLine(char* pCmdLine)
{
#ifdef DEBUG
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"GetSafeCommandLine(): cmd = %s\n",pCmdLine));
#endif

	if(!pCmdLine)
		return(NULL);

	char* p = pCmdLine;
	BOOL bInQuotes = FALSE;

	/* salta il nome dell'eseguibile */
	while(*p!='\0') 
	{
		if(*p=='\"') 
		{
			bInQuotes = !bInQuotes;
		} 
		else if(*p==' ' && !bInQuotes) 
		{
			break; /* ha trovato lo spazio che separa l'eseguibile dai parametri */
		}
		p++;
	}

	/* ora 'p' punta allo spazio dopo l'eseguibile o alla fine della stringa, deve quindi saltare tutti gli spazi bianchi iniziali prima dei parametri reali */
	while(isspace(*p)) 
		p++;

	/* se arriva al '\0' finale, non c'erano parametri reali, solo lo spazio AC/DC */
	return(p);
}

/*
	GetCommandLineAsString()

	Concatena gli argomenti presenti in argv in una linea di comando unica e continua, omettendo il nome dell'eseguibile (argv[0]).
*/
char* GetCommandLineAsString(int argc,char* argv[],char* szBuffer,size_t nBuffer)
{
#ifdef DEBUG
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"GetCommandLineAsString(): argc = %d\n",argc));
	for(int i=0; i < argc; i++)
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"GetCommandLineAsString(): argv[%d] = %s\n",i,argv[i]));
#endif

	int i;
	int nTot = 0;
	for(i = 1; i < argc; i++)
		nTot += ((int)strlen(argv[i]) + 1); /* + 1 per spazi separatori tra i parametri */

	if(nTot > 0)
	{
		ASSERTEXPR(nTot < MAX_CMDLINE);
		if(nTot >= MAX_CMDLINE)
			return(NULL);

		int n = 0;
		for(i = 1; i < argc; i++)
			n += wtfsnprintf(szBuffer+n,nBuffer-n,"%s%s",argv[i],i==argc-1 ? "" : " ");
	}

	return(szBuffer);
}

/*
	BuildCustomArgs()

	Ricava gli argomenti della linea di comando (completi degli eventuali doppi apici) e li imposta nell'argv,argc
	custom ricevuto in input, in modo tale che possano essere usati al posto degli argv,argc originali, dato che la
	CRT di M$ ripulisce (ossia elimina) tutti i doppi apici presenti nella linea di comando.
*/
void BuildCustomArgs(const char* cmd,int* custom_argc,char*** custom_argv)
{
	/* linea di comando passata come parametro o (originale) via API Win32 */
	LPCSTR cmdLine = cmd ? cmd : GetCommandLine();
    
	/* la duplica in locale */
	char* cmdCopy = _strdup(cmdLine);
	if(!cmdCopy)
	{
		*custom_argc = 0;
		*custom_argv = NULL;
		return;
	}

	/* alloca dinamicamente l'array di puntatori per gli argomenti */
	int capacity = 16;
	*custom_argv = (char**)malloc(capacity * sizeof(char*));
	*custom_argc = 0;

	/* estrae i token gestendo lo spazio come separatore ed i doppi apici come inclusore */
	char* token = strtokargs(cmdCopy,' ','"');
    
	while(token)
	{
		/* se raggiunge il limite dell'array, raddoppia la capacita' */
		if(*custom_argc >= capacity-1) /* -1 per lasciare spazio al NULL finale */
		{
			capacity *= 2;
			*custom_argv = (char**)realloc(*custom_argv,capacity * sizeof(char*));
		}
        
		/* assegna il token estratto all'array */
		(*custom_argv)[*custom_argc] = token;
		(*custom_argc)++;
        
		/* estrae il prossimo argomento */
		token = strtokargs(NULL,' ','"');
	}
    
	/* lo standard POSIX e C richiedono che l'ultimo elemento di argv sia NULL */
	(*custom_argv)[*custom_argc] = NULL;
}

/*
	Il problema del codice originale (parsing degli argomenti come linea, NON come array argv) viene alla luce quando si 
	usano chiamate che alla fine referenziano sempre e solo gli argc/argc della CRT (di MSVC), come ad es. il meccanismo
	di chiamate a matrioska del main() di wchg: alla prima chiamata la getopt() originale va come un treno, ma quando
	iniziano le chiamate a matrioska, li' si riparte dagli argc/argv originali del CRT e sono dolori perche' i doppi apici
	scompaiono, scasinando completamente il parser della getopt() originale.
	Le macro GETOPT_CMD/GETOPT_ARGV controllano quindi quale versione usare: l'originale, basata sul parsing dell'intera
	linea di comando, o la nuova, basata sugli elementi di argv.
*/
#if defined(GETOPT_CMD)
#pragma message("\t\t\t" __FILE__ "(" STR(__LINE__) "): using GETOPT_CMD version of getopt()")
/*
	getopt()
	
	Carica le opzioni/argomenti presenti sulla linea di comando.

	Restituisce:
	-2 = linea di comando > MAX_CMDLINE
	-1 = nessun parametro ricevuto
	 0 = ok (nessun errore)
	>0 = numero di parametri invalidi

	Note:
	- riguardo ai parametri, o si passano quelli ricevuti dalla funzione main(), o la linea di comando completa -> vedi modifica sotto
	- gli argomenti che includono spazi vanno racchiusi tra apici doppi ("..."), e se passati via debugger o da linea di comando i doppi
	  apici devono essere preceduti dal backslash per essere mantenuti, pero'... -> vedi modifica sotto che annulla tale necessita'
	- se un argomento usa lo stesso carattere per indicare l'opzione ('/' o '-'), deve essere racchiuso tra apici doppi affinche' tale
	  carattere non sia considerato come una opzione (es. '-f' opzione, 'Blur;-90' argomento: -fBlur;-90 -> NO, -f"Blur;-90" -> SI)
*/
int getopt(	char		cFlag,				/* carattere da usare per indicare l'opzione ('/' o '-') */
			GETOPT*		opts,				/* puntatore all'array GETOPT per le opzioni */
			int			optsSize,			/* dimensione dell'array */
			int			argc,				/* numero di parametri della funzione main() */
			char		*argv[],			/* array di puntatori ai parametri della funzione main */
			char		*wrongOptsBuffer,	/* buffer per ricevere l'opzione erronea */
			int			wrongOptsSize,		/* dimensione del buffer */
			const char*	cmdLine				/* linea di comando completa */
			)
{
	int mTotInvalidOptions = 0;
	char cOpt = 0;
	const char* p = NULL;
	char szCmdLine[MAX_CMDLINE+1] = {0};
	char arg[MAX_CMDLINE+1] = {0};
	bool bInvalidOption = FALSE;
	int nWrongOptsIdx = 0;

#ifdef DEBUG
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"getopt(): argc = %d\n",argc));
	for(int i=0; i < argc; i++)
		TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"getopt(): argv[%d] = <%s>\n",i,argv[i]));
	TRACEEXPR((_TRACE_FLAG_INFO,__FILE__,__LINE__,"getopt(): cmd = %s\n",szCmdLine));
#endif

	/*
	via debugger o via linea di comando (per come funziona la libreria CRT), l'argomento racchiuso tra doppi apici viene ripulito
	automaticamente dei doppi apici, a meno che non si specifichino preceduti da un backslash
	per ovviare al problema, si potrebbe ignorare argc/argv e recuperare direttamente l'intera linea di comando cosi' come e' stata
	passata alla shell, in modo tale che si possano usare i doppi apici senza precederli con un backslash
	in poche parole, il chiamante puo' decidere se passare gli argc/argv originali o la cmd completa, da ricavare tramite la chiamata
	a GetSafeCommandLine(GetCommandLine()) che elimina il nome dell'eseguibile, in ogni caso il codice sotto gestisce gli spazi tra i
	componenti del argomento
	*/

	/* distingue tra passaggio cmd e passaggio argc/argv, lo scopo e' ottenere SEMPRE una linea di comando completa per il parsing */

	if(cmdLine) /* cmd */
	{
		/* nessun parametro */
		if(strlen(cmdLine) <= 0)
			return(-1);

		/* copia in locale la linea di comando SENZA il nome dell'eseguibile (argv[0]) */
		ASSERTEXPR(strlen(cmdLine) < MAX_CMDLINE);
		if(strlen(cmdLine) >= MAX_CMDLINE)
			return(-2);

		strcpyn(szCmdLine,cmdLine,sizeof(szCmdLine));
	}
	else /* argc/argv */
	{
		/* nessun parametro */
		if(argc <= 1)
			return(-1);

		/* concatena i parametri in una stringa SENZA il nome dell'eseguibile */
		if(!GetCommandLineAsString(argc,argv,szCmdLine,sizeof(szCmdLine)))
			return(-2);
	}

	/* buffer per opzioni errate */
	if(wrongOptsSize!=-1)
		strzero(wrongOptsBuffer,wrongOptsSize);

	/* linea di comando completa SENZA il nome dell'eseguibile (argv[0]) */
	p = szCmdLine;

	/* scorre la linea di comando */
	while(*p)
	{
		/* trovato il carattere che precede l'opzione, lo salta */
		if(*p==cFlag)
		{
			bInvalidOption = FALSE;

			/* ricava il carattere che identifica l'opzione */
			cOpt = *++p;

			/* scorre l'array delle opzioni cercando quella trovata qui sopra */
			for(int i = 0; i < (int)optsSize; i++)
			{
				/* ha trovato l'opzione */
				if(opts[i].cOpt==cOpt)
				{
					bInvalidOption = opts[i].bFound = TRUE;
					
					/* se l'opzione prevede un argomento, lo ricava */
					if(opts[i].bArgs)
					{
						memset(arg,'\0',sizeof(arg));
						
						/* salta il carattere dell'opzione */
						++p;

						/* salta gli eventuali spazi tra il carattere dell'opzione e l'argomento */
						while(*p && isspace((unsigned char)*p))
							p++;

					/* inizio estrazione argomento, lo ricava verificando se e' racchiuso tra apici doppi ("...") */

						/*
						se l'argomento contiene uno dei caratteri usati per specificare l'opzione (/ o -), come in:
						C:\Users\lpier\Documents\Luca\Pictures\Bici\BICICLETTE MIE\GIANT CADEX CFR-1 (1990~92)
						l'argomento DEVE essere racchiuso tra "" in modo che possa ignorare i / o - contenuti al suo 
						interno
						se tale argomento si passa via debugger o via linea di comando (shell) il doppio apice deve
						essere preceduto da \ affinche' venga passato letteralmente, se no lo eliminerebbe:
						-d \"C:\Users\lpier\Documents\Luca\Pictures\Bici\BICICLETTE MIE\GIANT CADEX CFR-1 (1990~92)\"
						*/
						bool bHaveQuotationMarks = FALSE;
						if(*p=='"')
						{
							p++;
							bHaveQuotationMarks = TRUE;
						}

						/*
						ricava l'argomento, nel ciclo non considera gli spazi perche' potrebbero fare parte del
						argomento, es. "C:\Unusual dir name\more shit to come\", occhio se l'argomento inizia con "
						allora continua il ciclo fino a incontrare il " seguente, dato che / o - potrebbero fare
						parte dell'argomento stesso
						*/
						int n;
						for(	n = 0; 
								*p && (bHaveQuotationMarks ? (*p!='"') : (*p!=cFlag)) && n < sizeof(arg)-1;
								n++
								)
						{
							arg[n] = *p;
							p++;
						}

						/* elimina l'ultimo " */
						if(bHaveQuotationMarks)
							if(*p=='"')
							{
								p++;
								arg[++n] = '\0';
							}

						strrtrim(arg); /* strltrim(arg); quelli a sinistra gia' li ha eliminati sopra */

					/* fine estrazione argomento */

						/* argomento nullo/vuoto, ossia l'opzione e' stata passata senza argomento */
						if(!*arg)
						{
							opts[i].bArgs = FALSE; /* occhio! marca a FALSE il flag impostato dal chiamante per dire che l'opzione richiede un argomento */
							bInvalidOption = 0;
							break;
						}

						/* argomento valido, imposta il valore relativo, distinguendo in base al tipo */
						switch(opts[i].eType)
						{
							/* int */
							case word_type:
								opts[i].uValue.wValue = atoi(arg);
								break;

							/* long */
							case doubleword_type:
								opts[i].uValue.dwValue = atol(arg);
								break;

							/* long long */
							case quadword_type:
								opts[i].uValue.qwValue = atoll(arg);
								break;

							/* float */
							case float_type:
								opts[i].uValue.fValue = (float)atof(arg);
								break;

							/* double */
							case double_type:
								opts[i].uValue.dValue = atof(arg);
								break;

							/* str */
							case string_type:
								strcpyn(opts[i].uValue.szValue,arg,STR_MAX_VALUE+1);
								break;
						}
					}
					else /* opzione senza argomenti, segue scorrendo */
						p++;

					/* ogni volta che trova l'opzione nell'array (ed elabora l'argomento), deve poi interrompere il ciclo di ricerca */
					break;

				} /* if, per opzione trovata nell'array */

			} /* for, per scorrere l'array delle opzioni */

			/* elenca (tutte) le opzioni invalide... */
			if(!bInvalidOption)
			{
				if(wrongOptsSize!=-1)
					wrongOptsBuffer[nWrongOptsIdx++] = cOpt;
				mTotInvalidOptions++;
				p++;
			}
		}
		/* trovato un carattere a spazio, in teoria solo puo' essere un carattere che precede l'opzione */
		else if(isspace((unsigned char)*p))
		{
			p++;
		}
		/* trovato un carattere AC/DC, non un opzione, potrebbe essere un argomento passato senza opzione */
		else
		{
			if(wrongOptsSize!=-1)
				wrongOptsBuffer[nWrongOptsIdx++] = *p;
			mTotInvalidOptions++;
			p++;
		}

	} /* while, per tutti i caratteri della linea di comando */

	/* restituisce il numero di opzioni/parametri invalidi */
	return(mTotInvalidOptions);
}
#elif defined(GETOPT_ARGV)
#pragma message("\t\t\t" __FILE__ "(" STR(__LINE__) "): using GETOPT_ARGV version of getopt()")

int strunquote(char *str, char quote)
{
    if (str == NULL || *str == '\0') {
        return 0;
    }
    
    size_t len = strlen(str);
    
    // Verifica se il primo e l'ultimo carattere sono la virgoletta
    if (len >= 2 && str[0] == quote && str[len - 1] == quote) {
        // Sposta il contenuto di una posizione a sinistra (elimina la prima virgoletta)
        // Nota: memmove gestisce correttamente l'overlap
        memmove(str, str + 1, len - 1);
        
        // Ora la stringa è lunga len-1, ma dobbiamo eliminare l'ultima virgoletta
        // che ora si trova in posizione len-2 (perché abbiamo spostato tutto)
        str[len - 2] = '\0';
        
        return 1;
    }
    
    return 0;
}

/*
	getopt()

	Versione basata su argv.
    Carica le opzioni/argomenti presenti sulla linea di comando.
	Usa il parsing gia' effettuato dalla libreria CRT.

	Restituisce:
	-1 = nessun parametro ricevuto
	 0 = ok (nessun errore)
	>0 = numero di parametri invalidi
*/
int getopt( char        cFlag,              /* carattere da usare per indicare l'opzione ('/' o '-') */
            GETOPT*     opts,               /* puntatore all'array GETOPT per le opzioni */
            int         optsSize,           /* dimensione dell'array */
            int         argc,               /* numero di parametri della funzione main() */
            char        *argv[],            /* array di puntatori ai parametri della funzione main */
            char        *wrongOptsBuffer,   /* buffer per ricevere l'opzione erronea */
            int         wrongOptsSize,      /* dimensione del buffer */
            const char* cmdLine             /* IGNORATO in questa versione: usiamo argc/argv */
            )
{
	int mTotInvalidOptions = 0;
	int nWrongOptsIdx = 0;

	/* azzera il buffer delle opzioni errate */
	if(wrongOptsBuffer && wrongOptsSize > 0)
		memset(wrongOptsBuffer,'\0',wrongOptsSize);

	/* nessun parametro passato */
	if(argc <= 1 || argv==NULL)
		return(-1);

	/* scorre gli argomenti (salta il nome dell'eseguibile) */
	for(int i = 1; i < argc; i++)
	{
		char* currentArg = argv[i];

		/* verifica se l'argomento inizia con il flag richiesto (es. '-') */
		if(currentArg[0]==cFlag && currentArg[1]!='\0')
		{
			char cOpt = currentArg[1];
			bool bFoundOpt = FALSE;

			/* cerca l'opzione nell'array */
			for(int j = 0; j < optsSize; j++)
			{
				if(opts[j].cOpt==cOpt)
				{
					bFoundOpt = opts[j].bFound = TRUE;
                    
					if(opts[j].bArgs)
					{
						/* punta ai caratteri immediatamente successivi all'opzione */
						/* es. se currentArg e' "-t-5", argVal punta a "-5" */
						char* argVal = currentArg + 2; 

						/* se non c'e' nulla attaccato all'opzione (es. "-t" "5"), cerca nel token successivo */
						if(*argVal=='\0')
						{
							if(i + 1 < argc)
							{
								char* nextArg = argv[i+1];
                                
								/* se il token successivo inizia anch'esso per cFlag, e' un'altra opzione...
								...A MENO CHE non sia un numero (per consentire opzioni come -t -5) */
								bool isNextAnOption = (nextArg[0]==cFlag && nextArg[1]!='\0' && !isdigit((unsigned char)nextArg[1]));
                                
								if(!isNextAnOption)
								{
									argVal = nextArg;
									i++; /* consuma il token successivo incrementando l'indice del ciclo principale */
								}
								else
								{
									argVal = NULL; /* il parametro manca, c'e' subito un'altra opzione */
								}
							}
							else
							{
								argVal = NULL; /* e' l'ultimo token, niente parametro */
							}
						}

						/* se non ha trovato nessun argomento valido */
						if(argVal==NULL || *argVal=='\0')
						{
							opts[j].bArgs = FALSE;
						}
						else
						{
							char szArgVal[_MAX_PATH+1] = {0};
							strcpyn(szArgVal,argVal,sizeof(szArgVal));
							if(szArgVal[0]=='\'')
								strunquote(szArgVal,'\'');
							if(szArgVal[0]=='\"')
								strunquote(szArgVal,'\"');

							/* converte l'argomento in base al tipo */
							switch(opts[j].eType) {
								case word_type:			opts[j].uValue.wValue  = atoi(szArgVal);			break;
								case doubleword_type:	opts[j].uValue.dwValue = atol(szArgVal);			break;
								case quadword_type:		opts[j].uValue.qwValue = atoll(szArgVal);			break;
								case float_type:		opts[j].uValue.fValue  = (float)atof(szArgVal);		break;
								case double_type:		opts[j].uValue.dValue  = atof(szArgVal);			break;
								case string_type:		strcpyn(opts[j].uValue.szValue,szArgVal,STR_MAX_VALUE+1);
														strrtrim(opts[j].uValue.szValue);
														break;
							}
						}
					}
                    
					break; /* opzione trovata ed elaborata, esce dal for interno */
				}
			}

			/* se l'opzione non e' mappata nell'array opts[] */
			if(!bFoundOpt)
			{
				if(wrongOptsBuffer && nWrongOptsIdx < wrongOptsSize - 1)
					wrongOptsBuffer[nWrongOptsIdx++] = cOpt;
				mTotInvalidOptions++;
			}
		}
		else
		{
			/* e' un parametro 'orfano' senza il prefisso dell'opzione */
			if(wrongOptsBuffer && nWrongOptsIdx < wrongOptsSize - 1)
				wrongOptsBuffer[nWrongOptsIdx++] = currentArg[0];
			mTotInvalidOptions++;
		}
	}

	return(mTotInvalidOptions);
}
#else
  #error neither GETOPT_CMD nor GETOPT_ARGV are defined
#endif
