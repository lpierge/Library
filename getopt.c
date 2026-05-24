/*$
	getopt.c
	Gestione opzioni/parametri ricevuti da linea di comando.
	Luca Piergentili, 31/08/98
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "typeval.h"
#include "getopt.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	GetCmdParameters()

	Per ovviare allo spazio che la shell di Win32 piazza alla fine di argv[0], ed allo stesso tempo saltare gli 
	eventuali spazi che potrebbe contenere il nome dell'eseguibile (argv[0]) come in: "C:\Program Files\test.exe".
*/
char* GetCmdParameters(char* pCmdLine) 
{
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
	getopt()
	
	Carica le opzioni/argomenti presenti sulla linea di comando.

	Restituisce:
	-2 = linea di comando > MAX_CMDLINE
	-1 = nessun parametro ricevuto
	 0 = ok (nessun errore)
	>0 = numero di parametri invalidi

	Note:
	- riguardo ai parametri, o si passano quelli ricevuti dalla funzione main(), o la linea di comando completa -> vedi modifica sotto
	- gli argomenti che includono spazi vanno racchiusi tra apici doppi ("..."), e se passati via debugger o da linea di comando i 
	  doppi apici vanno preceduti dal backslash (\"...\"), pero'... -> vedi modifica sotto che annulla la necessita' del backslash
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
	int invalidOptions = 0;
	char cOpt = 0;
	const char* p = NULL;
	char cmdBuffer[MAX_CMDLINE+1] = {0};
	char arg[MAX_CMDLINE+1] = {0};
	bool invalidOption = FALSE;
	int wrongOptsIdx = 0;

	/*
	via debugger o via linea di comando (come conseguenza del comportamento della libreria CRT), l'argomento racchiuso tra doppi apici 
	viene 'ripulito' automaticamente, ossia vengono rimossi i doppi apici, a meno che non si specifichino preceduti da un backslash
	per ovviare al problema, ignora argc/argv e recupera direttamente l'intera linea di comando cosi' come e' stata passata alla shell,
	in modo tale che si possano usare i doppi apici senza precederli con un backslash
	in poche parole, passare la cmd invece degli argomenti in teoria e' decisione del chiamante, ma qui se ne frega e carica si' o si'
	la cmd per ovviare al problema della traslazione automatica di cui sopra
	*/

	/*
	PERO' molto occhio perche' la zoccola di Win32 aggiunge uno spazio al nome dell'eseguibile anche se non vengono specificati parametri,
	per compatibilita' con il vecchio PSP (Program Segment Prefix) del DOS o per puttanate varie come le utility di sistema (incluso cmd.exe) 
	che internamente hanno una logica del tipo: sprintf(buffer,"%s %s",exe,params); e se params e' una stringa vuota, terminano producendo
	exe + spazio + NULL
	*/
	cmdLine = GetCmdParameters(GetCommandLine());

	/* distingue tra passaggio cmd e passaggio argc/argv */
	if(cmdLine)
	{
		/* nessun parametro */
		if(strlen(cmdLine) <= 0)
			return(-1);

		/* copia in locale la linea di comando */
		ASSERTEXPR(strlen(cmdLine) < MAX_CMDLINE);
		if(strlen(cmdLine) >= MAX_CMDLINE)
			return(-2);

		strcpyn(cmdBuffer,cmdLine,sizeof(cmdBuffer));
	}
	else
	{
		/* nessun parametro */
		if(argc==-1 || !argv)
			return(-1);

		/* concatena i parametri di *argv[] in una linea di comando unica e continua */
		int i;
		int nTot = 0;
		for(i = 1; i < argc; i++)
			nTot += ((int)strlen(argv[i]) + 1); /* + 1 per spazi separatori tra i parametri */
		if(nTot > 0)
		{
			ASSERTEXPR(nTot < MAX_CMDLINE);
			if(nTot >= MAX_CMDLINE)
				return(-2);

			int n = 0;
			for(i = 1; i < argc; i++)
				n += snprintf(cmdBuffer+n,sizeof(cmdBuffer)-n,"%s%s",argv[i],i==argc-1 ? "" : " ");
		}
	}

	/* buffer per opzioni errate */
	if(wrongOptsSize!=-1)
		strzero(wrongOptsBuffer,wrongOptsSize);

	/* scorre la linea di comando */
	p = cmdBuffer;
	while(*p)
	{
		/* trovato il carattere che precede l'opzione, lo salta */
		if(*p==cFlag)
		{
			invalidOption = FALSE;

			/* ricava il carattere che identifica l'opzione */
			cOpt = *++p;

			/* scorre l'array delle opzioni cercando quella trovata qui sopra */
			for(int i = 0; i < (int)optsSize; i++)
			{
				/* ha trovato l'opzione */
				if(opts[i].cOpt==cOpt)
				{
					invalidOption = opts[i].bFound = TRUE;
					
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
							invalidOption = 0;
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
			if(!invalidOption)
			{
				if(wrongOptsSize!=-1)
					wrongOptsBuffer[wrongOptsIdx++] = cOpt;
				invalidOptions++;
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
				wrongOptsBuffer[wrongOptsIdx++] = *p;
			invalidOptions++;
			p++;
		}

	} /* while, per tutti i caratteri della linea di comando */

	/* restituisce il numero di opzioni/parametri invalidi */
	return(invalidOptions);
}
