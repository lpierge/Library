/*$
	CWinsock.cpp
	Classe base per interfaccia Winsock.
	Luca Piergentili, 15/07/96
	lpiergentili@yahoo.com

	La classe viene usata come wrapper per gestire in locale i servizi implementati.

	Rimappa l'API Winsock a seconda della definizione della macro _DEBUGSOCKET:
	- se non viene definita la macro _DEBUGSOCKET le chiamate Winsock vengono rimappate sull'API originale
	- se viene definita la macro _DEBUGSOCKET vengono rimappate le chiamate winsock implementando unicamente
	  i servizi SMTP/POP3 (non viene effettuato nessun controllo sull'esistenza delle directory utilizzate, 
	  da create a parte)

	OCCHIO:
	la NON definizione della macro WIN32_LEAN_AND_MEAN fa si che si generino warnings ed errori per colpa del 
	disegno erratico (AC/DC) delle librerie e dei files include di Microsoft, soprattutto quando si tratta di 
	Winsock
	definire la macro WIN32_LEAN_AND_MEAN qui o nel progetto
	per problemi specifici con winsock2, provare a definire (sempre a livello di progetto) anche le macro 
	_WINSOCKAPI_ e _WINSOCK_DEFS
*/
#ifdef _DEBUGSOCKET
#include "env.h"
#include "pragma.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include "window.h"
#include "lmhosts.h"
#include "CDateTime.h"
#include "CWinsock.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

#define WSA_MAX_SOCKETS	1								// numero max di socket gestiti (1-255)
#define WSA_MAX_MTU		4096								// valore MTU (512-8192)
#define POP3_PORT		110								// porta POP3
#define SMTP_PORT		25								// porta SMTP
#define BUF_SIZE		1048576							// dimensione del buffer per la lettura dal file (POP3)
#define MAX_DELE_MARK	512								// numero max di mark per il comando DELE

// directory per le mailbox
#ifdef _WINNT
	#define MAIL_DIRECTORY	"c:\\inetpub\\mail\\"
#else
	#define MAIL_DIRECTORY	"c:\\webshare\\mail\\"
#endif
#pragma message("\t\t\t"__FILE__"("STR(__LINE__)"): _DEBUGSOCKET: MAIL_DIRECTORY mapped to: "MAIL_DIRECTORY)

// file per il socket
#define SOCKET_FILE		MAIL_DIRECTORY"log\\socket.txt"
#pragma message("\t\t\t"__FILE__"("STR(__LINE__)"): _DEBUGSOCKET: SOCKET_FILE mapped to: "SOCKET_FILE)

// file per i totali
#define COUNT_FILE		"tot.txt"
#pragma message("\t\t\t"__FILE__"("STR(__LINE__)"): _DEBUGSOCKET: COUNT_FILE mapped to: "COUNT_FILE)

// file utenti
#define USERS_FILE		"users.txt"
#pragma message("\t\t\t"__FILE__"("STR(__LINE__)"): _DEBUGSOCKET: USERS_FILE mapped to: "USERS_FILE)

#define MAX_USR_LEN		20								// lunghezza max nome utente
#define MAX_EMAIL_LEN	35								// lunghezza max email
#define CRLF_LEN		2								// CR+LF
#define USR_ENTRY_LEN	(MAX_USR_LEN+MAX_EMAIL_LEN+CRLF_LEN)	// dimensione del record

/*
	CWinsock()
*/
CWinsock::CWinsock()
{
	iWSALastError	= 0;
	iWSAFlag		= 0;
	hFile		= HFILE_ERROR;
	hSocket		= INVALID_SOCKET;
	pHostName		= LOCAL_HOST_NAME;
	pHostIp		= LOCAL_HOST;
	iService		= -1;
	iCommand		= 0;
	pHostLastSend	= new char[WSA_MAX_MTU + 1];
	memset(pHostLastSend,'\0',WSA_MAX_MTU + 1);
	pHostLastRecv	= new char[WSA_MAX_MTU + 1];
	memset(pHostLastRecv,'\0',WSA_MAX_MTU + 1);
	pPop3Buffer	= new char[BUF_SIZE + 1];
	memset(pPop3Buffer,'\0',BUF_SIZE + 1);
	bRetr		= FALSE;
	hMailBox		= HFILE_ERROR;
	pMailBox		= new char[MAX_EMAIL_LEN + 1];
	memset(pMailBox,'\0',MAX_EMAIL_LEN + 1);
	iTotMail		= 0;
	iDelArray		= new int[MAX_DELE_MARK + 1];
	memset(iDelArray,'\0',MAX_DELE_MARK + 1);
	iDelIndex		= -1;
	date.Reset();
}

/*
	~CWinsock()
*/
CWinsock::~CWinsock()
{
	if(pHostLastSend)
		delete [] pHostLastSend,pHostLastSend = NULL;
	if(pHostLastRecv)
		delete [] pHostLastRecv,pHostLastRecv = NULL;
	if(pPop3Buffer)
		delete [] pPop3Buffer,pPop3Buffer = NULL;
	if(pMailBox)
		delete [] pMailBox,pMailBox = NULL;
	if(iDelArray)
		delete [] iDelArray,iDelArray = NULL;
}

/*
	WSAStartup()
*/
int PASCAL FAR CWinsock::WSAStartup(WORD wVersionRequired,LPWSADATA lpWSAData)
{
	lpWSAData->wVersion		=	MAKEWORD(1,1);					// versione
	lpWSAData->wHighVersion	=	MAKEWORD(1,1);					// versione
	lstrcpy(lpWSAData->szDescription,"Dummy Winsock library v.1.0");	// vendor info
	lstrcpy(lpWSAData->szSystemStatus,"Written by Luca Piergentili");// more info
	lpWSAData->iMaxSockets	=	WSA_MAX_SOCKETS;				// numero massimo di socket supportati
	lpWSAData->iMaxUdpDg	=	WSA_MAX_MTU;					// valore MTU
	lpWSAData->lpVendorInfo	=	NULL;						// struttura fornita dal vendor
	iWSAFlag = 1;
	return(0);
}

/*
	WSACleanup()
*/
int PASCAL FAR CWinsock::WSACleanup(void)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(SOCKET_ERROR);
	}
	else
		return(0);
}

/*
	WSASetLastError()
*/
void PASCAL FAR CWinsock::WSASetLastError(int iError)
{
	iWSALastError = iError;
}

/*
	WSAGetLastError()
*/
int PASCAL FAR CWinsock::WSAGetLastError(void)
{
	return(iWSALastError);
}

/*
	setsockopt()
*/
int CWinsock::setsockopt(SOCKET s,int level,int optname,const char FAR *optval,int optlen)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(SOCKET_ERROR);
	}
	else
		return(0);
}

/*
	socket()

	Creazione del socket.
*/
SOCKET PASCAL FAR CWinsock::socket(int af,int type,int protocol)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(INVALID_SOCKET);
	}
		
	// controlla gli specificatori del socket
	if(af!=PF_INET)
	{
		WSASetLastError(WSAEAFNOSUPPORT);
		return(INVALID_SOCKET);
	}
	if(type!=SOCK_STREAM && type!=SOCK_DGRAM)
	{
		WSASetLastError(WSAESOCKTNOSUPPORT);
		return(INVALID_SOCKET);
	}
		
	// implementa il socket su file
	if((hFile = _lcreat(SOCKET_FILE,0))==HFILE_ERROR)
	{
		WSASetLastError(WSAEBADF);
		hSocket = INVALID_SOCKET;
	}
	else
		hSocket = WSA_MAX_SOCKETS;

	return(hSocket);
}

/*
	getsockname()
*/
int CWinsock::getsockname(SOCKET s,struct sockaddr FAR *name,int FAR *namelen)
{
	return(SOCKET_ERROR);
}

/*
	connect()
*/
int PASCAL FAR CWinsock::connect(SOCKET s,const struct sockaddr FAR *name,int namelen)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(SOCKET_ERROR);
	}
          
	// controlla che il socket (ossia il file) sia stato aperto
	if(hFile==HFILE_ERROR)
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
    
	// errore sul socket
	if(s==INVALID_SOCKET)
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
	else if(s==WSA_MAX_SOCKETS)
	{
		// implementa solo i servizi SMTP/POP3
		if(iService!=SMTP_PORT && iService!=POP3_PORT)
		{
			WSASetLastError(WSAEPROTONOSUPPORT);
			return(SOCKET_ERROR);
		}
		else
		{
			// imposta per il comando successivo
			strcpy(pHostLastSend,"CONNECT");
			return(0);
		}
	}
	else
	{
		WSASetLastError(WSAEISCONN);
		return(SOCKET_ERROR);
	}
}

/*
	shutdown()
*/
int CWinsock::shutdown(SOCKET s,int how)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(SOCKET_ERROR);
	}
	else
		return(0);
}

/*
	closesocket()
*/
int FAR PASCAL CWinsock::closesocket(SOCKET s)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(SOCKET_ERROR);
	}
          
	// controlla che il socket (ossia il file) sia stato aperto
	if(hFile==HFILE_ERROR)
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
          
	// errore sul socket
	if(s==INVALID_SOCKET)
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
	else if(s==WSA_MAX_SOCKETS)
	{
		_lclose(hFile);
		hFile = HFILE_ERROR;
		hSocket = INVALID_SOCKET;
		return(0);
	}
	else
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
}

/*
	send()
*/
int PASCAL FAR CWinsock::send(SOCKET s,const char FAR *buf,int len,int flags)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(SOCKET_ERROR);
	}
          
	// controlla che il socket (ossia il file) sia stato aperto
	if(hFile==HFILE_ERROR)
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
          
	// errore sul socket
	if(s==INVALID_SOCKET)
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
	else if(s==WSA_MAX_SOCKETS)
	{
		// invia i dati all'host tracciando il socket su file
		if(send_to_host(buf,len)!=0)
		{
			WSASetLastError(WSANO_DATA);
			return(SOCKET_ERROR);
		}
		else
			return(_lwrite(hFile,buf,len));
	}
	else
	{
		WSASetLastError(WSAENOTCONN);
		return(SOCKET_ERROR);
	}
}

/*
	recv()
*/
int PASCAL FAR CWinsock::recv(SOCKET s,char FAR *buf,int len,int flags)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(SOCKET_ERROR);
	}
          
	// controlla che il socket (ossia il file) sia stato aperto
	if(hFile==HFILE_ERROR)
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
          
	// errore sul socket
	if(s==INVALID_SOCKET)
	{
		WSASetLastError(WSAENOTSOCK);
		return(SOCKET_ERROR);
	}
	else if(s==WSA_MAX_SOCKETS)
	{
		// riceve i dati dall'host tracciando il socket su file
		if(recv_from_host(buf,len)!=0)
		{
			WSASetLastError(WSANO_DATA);
			return(SOCKET_ERROR);
		}
		else
			return(_lwrite(hFile,(const char FAR *)buf,strlen(buf)));
	}
	else
	{
		WSASetLastError(WSAENOTCONN);
		return(SOCKET_ERROR);
	}
}

/*
	gethostbyname()
*/	
struct hostent FAR * PASCAL FAR CWinsock::gethostbyname(const char FAR *name)
{
	static char* host_alias[] = {NULL,NULL};
	static struct in_addr st_addr;
	st_addr.S_un.S_addr = LOCAL_HOST_NUM;
	static char* host_names[] = {(char FAR *)&st_addr,NULL};
	static struct hostent host_ent_struct = {0};

	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return((struct hostent FAR *)NULL);
	}
	else
	{
		// imposta la struttura
		host_ent_struct.h_name      = pHostName;
		host_ent_struct.h_aliases   = host_alias;
		host_ent_struct.h_addrtype  = PF_INET;
		host_ent_struct.h_length    = 4;
		host_ent_struct.h_addr_list = host_names;
		
		return((struct hostent FAR *)&host_ent_struct);
	}
}

/*
	gethostbyaddr()
*/	
struct hostent FAR * CWinsock::gethostbyaddr(const char FAR *addr,int len,int type)
{
	return(gethostbyname(NULL));
}

/*
	gethostname()
*/
int PASCAL FAR CWinsock::gethostname(char FAR *name,int namelen)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(SOCKET_ERROR);
	}
	else
	{
		strcpyn(name,pHostName,namelen);
		return(0);
	}
}

/*
	getservbyname()
*/
struct servent FAR * CWinsock::getservbyname(const char FAR *name,const char FAR *proto)
{
	static struct servent se;
	se.s_port = -1;
	return(&se);
}

/*
	getservbyport()
*/
struct servent FAR * CWinsock::getservbyport(int port,const char FAR *proto)
{
	static struct servent se;
	se.s_name = "???";
	return(&se);
}

/*
	htons()
*/
u_short PASCAL FAR CWinsock::htons(u_short hostshort)
{
	// imposta il servizio
	iService = hostshort;

	return(hostshort);
}

/*
	inet_addr()
*/
unsigned long PASCAL FAR CWinsock::inet_addr(const char FAR *cp)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return(INADDR_NONE);
	}
	else
		return(LOCAL_HOST_NUM);
}

/*
	inet_ntoa()
*/
char FAR * CWinsock::inet_ntoa(struct in_addr in)
{
	if(!iWSAFlag)
	{
		WSASetLastError(WSANOTINITIALISED);
		return((char FAR *)NULL);
	}
	else
		return(pHostIp);
}

/*
	send_to_host()

	Utilizzata per inviare i dati all'host.
	I dati vengono registrati nella directory relativa alla casella email.
	Che l'handle del file per la mailbox sia chiuso (HFILE_ERROR) non viene considerato un 
	errore, dato che la funzione viene utilizzata anche per inviare i comandi al server.
*/
int CWinsock::send_to_host(const char FAR *buf,int len)
{
	// controlla il servizio
	if(iService < 0)
	{
		WSASetLastError(WSAEPROTONOSUPPORT);
		return(1);
	}
	
	// imposta per il comando successivo
	if(strstr(buf,"\r\n.\r\n")!=NULL)	
		strcpy(pHostLastSend,"END");
	else
		strcpyn(pHostLastSend,buf,WSA_MAX_MTU + 1);
	
	// registra i dati nella mailbox
	// il file viene aperto alla ricezione del comando DATA e chiuso con il comando END
	if(hMailBox!=HFILE_ERROR)
		_lwrite(hMailBox,buf,len);
	
	return(0);
}

/*
	recv_from_host()

	Utilizzata per ricevere i dati dall'host.
	I dati vengono prelevati dalla directory relativa alla casella email.
*/
int CWinsock::recv_from_host(char FAR * buf,int len)
{
	int ret = 0;
	static int tot = 0;
	
	// controlla che il servizio sia tra quelli gestiti, rispondendo al comando
	switch(iService)
	{
		/*
			SMTP

			- il messaggio non puo' eccedere i BUF_SIZE caratteri (per POP3)
			- supporta un solo destinatario per volta (non supporta ne' Cc: ne' Bcc:)
			- il comando RCPT TO non controlla l'esistenza dell'email
			- gestiti: CONNECT (interno), HELO, MAIL FROM, RCPT TO, DATA, END (interno), QUIT
		*/
		case SMTP_PORT:
		{
			/*
			CONNECT
			*/
			if(memcmp(pHostLastSend,"CONNECT",7)==0)
			{
				date.SetDateFormat(GMT);

				// comando
				sprintf(	pHostLastRecv,
						"220-%s ready at %s\r\n220 ESMTP spoken here\r\n",
						pHostName,
						date.GetFormattedDate()
						);
			}
			/*
			HELO
			*/
			else if(memcmp(pHostLastSend,"HELO",4)==0)
			{
				// comando
				sprintf(	pHostLastRecv,
						"250 %s Hello [%s], pleased to meet you\r\n",
						pHostName,
						pHostIp
						);
			}
			/*
			MAIL FROM
			*/
			else if(memcmp(pHostLastSend,"MAIL FROM",9)==0)
			{
				char* p;
				char buffer[MAX_EMAIL_LEN + 1] = {0};

				// ricava l'email del mittente
				strcpyn(buffer,pHostLastSend+10,sizeof(buffer));
				if((p = strchr(buffer,'\r'))!=NULL)
					*p = '\0';

				// comando
				sprintf(	pHostLastRecv,
						"250 %s Sender ok\r\n",
						buffer
						);
			}
			/*
			RCPT TO
			*/
			else if(memcmp(pHostLastSend,"RCPT TO",7)==0)
			{
				char* p;
				char buffer[MAX_EMAIL_LEN + 1] = {0};

				// imposta il nome della mailbox (ossia della directory) con l'email del mittente
				strcpyn(pMailBox,pHostLastSend+9,MAX_EMAIL_LEN + 1);
				if((p = strchr(pMailBox,'>'))!=NULL)
					*p = '\0';

				// comando
				sprintf(	pHostLastRecv,
						"250 <%s> Recipient ok\r\n",
						pMailBox
						);
			}
			/*
			DATA
			*/
			else if(memcmp(pHostLastSend,"DATA",4)==0)
			{
				int h,tot;
				char	buffer[_MAX_PATH + 1] = {0};

				// ricava il nome del file relativo ai totali
				sprintf(buffer,"%s%s\\%s",MAIL_DIRECTORY,pMailBox,COUNT_FILE);
					
				// apre (o crea) il file dei totali e ricava il numero di email presenti
				if((h = _lopen(buffer,OF_READWRITE))!=HFILE_ERROR)
				{
					char buffer[6] = {0};
						
					if(_lread(h,buffer,sizeof(buffer)-1) <= 0)
						tot = 1;
					else
						tot = atoi(buffer)+1;
				}
				else
				{
					h = _lcreat(buffer,0);
					tot = 1;
				}
				
				// apertura/creazione fallita
				if(h==HFILE_ERROR)
				{
					WSASetLastError(WSAEBADF);
					return(SOCKET_ERROR);
				}
					
				// imposta il nome del file per la registrazione del messaggio email
				sprintf(buffer,"%s%s\\%d.txt",MAIL_DIRECTORY,pMailBox,tot);
					
				// crea il file per il messaggio email e registra sul file dei totali il numero di email
				// (il file verra' chiuso con il comando END)
				if((hMailBox = _lcreat(buffer,0))!=HFILE_ERROR)
				{
					sprintf(buffer,"%d     ",tot);
					_llseek(h,0,0);
					_lwrite(h,buffer,strlen(buffer));
				}
				else
				{
					WSASetLastError(WSAEBADF);
					return(SOCKET_ERROR);
				}
					
				_lclose(h);

				// comando
				strcpyn(pHostLastRecv,"354 Enter mail, end with \".\" on a line by itself\r\n",WSA_MAX_MTU + 1);
			}
			/*
			END
			*/
			else if(memcmp(pHostLastSend,"END",3)==0)
			{   
				// chiude il file per il messaggio email
				// (aperto con il comando DATA)
				_lclose(hMailBox);
				hMailBox = HFILE_ERROR;
				
				// comando
				strcpy(pHostLastRecv,"250 Message accepted for delivery\r\n");
			}
			/*
			QUIT
			*/
			else if(memcmp(pHostLastSend,"QUIT",4)==0)
			{
				// comando
				sprintf(	pHostLastRecv,
						"221 %s closing connection\r\n",
						pHostName
						);
			}
			else if(pHostLastSend[0]=='\0')
			{
				memset(pHostLastRecv,'\0',WSA_MAX_MTU + 1);
				WSASetLastError(0);
				ret = 0;
			}
			/*
			?
			*/
			else
			{
				// comando
				strcpy(pHostLastRecv,"421");
				WSASetLastError(WSAEOPNOTSUPP);
				ret = SOCKET_ERROR;
			}
			
			memset(pHostLastSend,'\0',WSA_MAX_MTU + 1);

			break;
		}
        
		/*
			POP3
			
			- gestisce solo gli utenti presenti in USERS_FILE
			- il messaggio non puo' eccedere i BUF_SIZE caratteri
			- il comando DELE puo' gestire fino a MAX_DELE_MARK mark
			- il comando DELE elimina solo i messaggi marcati, pero', dato che il file per i totali viene
			  azzerato comunque ed i messaggi non vengono rinumerati, la chiamata seguente restituisce 0
			  messaggi presenti
			- il comando USER non controlla l'esistenza dell'email (directory su disco)
			- il comando PASS non controlla l'esistenza della password (USERS_FILE)
			- il comando STAT restituisce solo il numero di email presenti, senza la dimensione del messaggio
			- ogni record del file USERS_FILE deve essere di USR_ENTRY_LEN caratteri (compresi i CRLF)
			- non gestisce la possibilita' che durante la lettura dal buffer interno su quello per la
			  ricezione dall'host la sequenza \r\n.\r\n possa trovarsi a cavallo di due cicli
			- gestiti: CONNECT (interno), USER, PASS, STAT, RETR, TOP, DELE, QUIT, NOP (interno)
		*/
		case POP3_PORT:
		{
			/*
			CONNECT
			*/
			if(memcmp(pHostLastSend,"CONNECT",7)==0)
			{
				int i;
				
				// inizializza l'array per il comando DELE
				for(iDelIndex = -1,i = 0; i < MAX_DELE_MARK; i++)
					iDelArray[i] = -1;
					
				// comando
				sprintf(	pHostLastRecv,
						"+OK %s POP3 Server (Version 1.0) ready.\r\n",
						pHostName
						);
			}
			/*
			USER
			*/
			else if(memcmp(pHostLastSend,"USER",4)==0)
			{
				int h;
				char buffer[_MAX_PATH + 1];
				
				// azzera il nome della mailbox
				memset(pMailBox,'\0',MAX_EMAIL_LEN + 1);
				
				// imposta il nome per il file con l'elenco degli utenti
				sprintf(buffer,"%s%s",MAIL_DIRECTORY,USERS_FILE);
					
				// apre l'elenco degli utenti
				if((h = _lopen(buffer,OF_READWRITE))!=HFILE_ERROR)
				{
					// legge cercando il nome dell'utente
					while(_lread(h,buffer,USR_ENTRY_LEN)==USR_ENTRY_LEN)
					{
						char* p;

						// ricava il nome dell'utente
						if((p = strchr(buffer,' '))!=NULL)
							*p = '\0';
						
						// lo confronta con quanto ricevuto
						if(memcmp(buffer,pHostLastSend+5,strlen(buffer))==0)
						{
							// ricava l'email (ossia il nome della directory)
							if((p = strchr(buffer+MAX_USR_LEN,' '))!=NULL)
								*p = '\0';
							strcpyn(pMailBox,buffer+MAX_USR_LEN,MAX_EMAIL_LEN + 1);
							break;
						}
					}
						
					_lclose(h);
				}

				// comando
				if(pMailBox[0]!='\0')
					strcpy(pHostLastRecv,"+OK please send PASS command\r\n");
				else
					strcpy(pHostLastRecv,"-ERR unknown user\r\n");
			}
			/*
			PASS
			*/
			else if(memcmp(pHostLastSend,"PASS",4)==0)
			{
				int h;
				char buffer[_MAX_PATH + 1];
					
				// ricava il nome del file relativo ai totali
				sprintf(buffer,"%s%s\\%s",MAIL_DIRECTORY,pMailBox,COUNT_FILE);
					
				// legge il numero totale di email presenti
				if((h = _lopen(buffer,OF_READWRITE))==HFILE_ERROR)
				{
					if((h = _lcreat(buffer,0))!=HFILE_ERROR)
					{
						_lwrite(h,"0",1);
						iTotMail = 0;
					}
					else
					{
						WSASetLastError(WSAECONNREFUSED);
						return(SOCKET_ERROR);
					}
				}
				else
				{
					_lread(h,buffer,4);
					iTotMail = atoi(buffer);
				}
				
				if(h!=HFILE_ERROR)
					_lclose(h);
				
				// comando
				sprintf(	pHostLastRecv,
						"+OK %d message ready for %s in %s\\%s\r\n",
						iTotMail,
						pMailBox,
						MAIL_DIRECTORY,
						pMailBox
						);
			}
			/*
			STAT
			*/
			else if(memcmp(pHostLastSend,"STAT",4)==0)
			{
				// comando
				sprintf(	pHostLastRecv,
						"+OK %d 0\r\n",
						iTotMail
						);
			}
			/*
			RETR
			*/
			else if(memcmp(pHostLastSend,"RETR",4)==0)
			{
				int h,num;
				char buffer[_MAX_PATH + 1];
				
				// numero dell'email da ricevere
				num = atoi(pHostLastSend+5);
				
				// imposta il nome del file relativo al messaggio email richiesto
				sprintf(buffer,"%s%s\\%d.txt",MAIL_DIRECTORY,pMailBox,num);

				memset(pPop3Buffer,'\0',BUF_SIZE+1);

				// apre (e legge) il file contenente il messaggio email
				if((h = _lopen(buffer,OF_READWRITE))!=HFILE_ERROR)
				{
					int n = _lread(h,pPop3Buffer,BUF_SIZE);
					_lclose(h);
					
					// controlla che sia presente la sequenza di fine messaggio
					if(strstr(pPop3Buffer,"\r\n.\r\n")==NULL)
					{
						if((n+6) >= (BUF_SIZE))
							n = BUF_SIZE-6;
							
						pPop3Buffer[n+1] = '\r';
						pPop3Buffer[n+2] = '\n';
						pPop3Buffer[n+3] = '.';
						pPop3Buffer[n+4] = '\r';
						pPop3Buffer[n+5] = '\n';
						pPop3Buffer[n+6] = '\0';
					}
				}
				else
				{
					WSASetLastError(WSAEBADF);
					return(SOCKET_ERROR);
				}
                
				// imposta il flag per la lettura del file dal buffer
				bRetr = TRUE;

				// comando
				strcpy(pHostLastSend,"NOP");
				sprintf(	pHostLastRecv,
						"+OK message %d (0 octets):\r\n",
						num
						);
			}
			/*
			TOP
			*/
			else if(memcmp(pHostLastSend,"TOP",3)==0)
			{
				int h,num;
				char buffer[_MAX_PATH + 1];
				
				// numero dell'email da ricevere
				num = atoi(pHostLastSend+4);
				
				// imposta il nome del file relativo al messaggio email richiesto
				sprintf(buffer,"%s%s\\%d.txt",MAIL_DIRECTORY,pMailBox,num);

				memset(pPop3Buffer,'\0',BUF_SIZE+1);

				// apre (e legge) il file contenente il messaggio email
				if((h = _lopen(buffer,OF_READWRITE))!=HFILE_ERROR)
				{
					int n = _lread(h,pPop3Buffer,BUF_SIZE-1);
					_lclose(h);
					
					// lascia nel buffer solo l'header del messaggio
					*strstr(pPop3Buffer,"\r\n\r\n") = '\0';
					strcat(pPop3Buffer,"\r\n.\r\n");
				}
				else
				{
					WSASetLastError(WSAEBADF);
					return(SOCKET_ERROR);
				}
                
				// imposta il flag per la lettura del file dal buffer
				bRetr = TRUE;

				// comando
				strcpy(pHostLastSend,"NOP");
				sprintf(	pHostLastRecv,
						"+OK message %d (0 octets):\r\n",
						num
						);
			}
			/*
			DELE
			*/
			else if(memcmp(pHostLastSend,"DELE",4)==0)
			{
				int num;
				
				// numero dell'email da eliminare
				num = atoi(pHostLastSend+5);
				
				// inserisce il progressivo dell'email da eliminare nell'array
				if(++iDelIndex < MAX_DELE_MARK)
					iDelArray[iDelIndex] = num;
				
				// comando
				sprintf(	pHostLastRecv,
						"+OK message %d marked for deletion\r\n",
						num
						);
			}
			/*
			QUIT
			*/
			else if(memcmp(pHostLastSend,"QUIT",4)==0)
			{
				int i,h;
				char buffer[_MAX_PATH + 1];
				
				// elimina le email marcate con DELE
				for(i = 0; i < MAX_DELE_MARK; i++)
				{
					if(iDelArray[i]!=-1)
					{
						snprintf(buffer,sizeof(buffer),"%s%s\\%d.txt",MAIL_DIRECTORY,pMailBox,iDelArray[i]);
						remove(buffer);
					}
				}

				snprintf(buffer,sizeof(buffer),"%s%s\\%s",MAIL_DIRECTORY,pMailBox,COUNT_FILE);
				h = _lcreat(buffer,0);
				_lclose(h);

				for(iDelIndex = -1,i = 0; i < MAX_DELE_MARK; i++)
					iDelArray[i] = -1;
				
				// comando
				sprintf(	pHostLastRecv,
						"+OK %s POP3 Server (Version 1.0) shutdown.\r\n",
						pHostName
						);
			}
			/*
			NOP
			*/
			else if(memcmp(pHostLastSend,"NOP",3)==0)
			{
				; // per i cicli di lettura dal buffer interno su quello di ricezione con RETR e TOP
			}
			else if(pHostLastSend[0]=='\0')
			{
				memset(pHostLastRecv,'\0',WSA_MAX_MTU + 1);
				WSASetLastError(0);
				ret = 0;
			}
			/*
			?
			*/
			else
			{
				// comando
				strcpy(pHostLastRecv,"-ERR unknown command\r\n");
				WSASetLastError(WSAEOPNOTSUPP);
				ret = SOCKET_ERROR;
			}

			memset(pHostLastSend,'\0',sizeof(pHostLastSend));
            
			/*
			sta' leggendo il file dal buffer interno su quello per la ricezione dall'host
			*/
			if(bRetr)
			{
				int i = 0,n = 0,lenght = len;
				char* b = (pPop3Buffer+tot);
				char* p = pHostLastRecv;

				// per la prima lettura dal buffer (tot=0) posiziona il puntatore dopo +OK...
				if(tot==0)
				{
					while(*p)
						p++,n++;
						
					lenght -= n;
				}
				
				if(*b)
				{
					// copia dal buffer interno sul buffer di ricezione
					while(*b && i < lenght)
					{
						*p++ = *b++;
						i++;
					}
	                
					// aggiorna l'offset per il buffer interno
					tot += i;
					
					// termina il buffer di ricezione 
					*p = '\0';
				}
				else
				{   
					strcpy(pHostLastRecv,"\r\n.\r\n");
				}

				if(strstr(pHostLastRecv,"\r\n.\r\n"))
				{
					tot = 0;
					bRetr = FALSE;
				}
			}

			break;
		}
        
		/*
		?
		*/
		default:
		{
			memset(pHostLastRecv,'\0',WSA_MAX_MTU + 1);
			WSASetLastError(WSAEOPNOTSUPP);
			ret = SOCKET_ERROR;
			break;
		}
	}
	
	// copia dal buffer di ricezione su quello del chiamante
	if((int)strlen(pHostLastRecv) > len)
		pHostLastRecv[len-1] = '\0';
		
	strcpyn(buf,pHostLastRecv,len);
	
	return(ret);
}
#endif // _DEBUGSOCKET
