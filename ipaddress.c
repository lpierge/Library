/*
	ipaddress.c
	Gestione stringhe IP.
	Luca Piergentili, Settembre '25
*/
#include "pragma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include <time.h>
#include <ctype.h>
#include "ipaddress.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	generate_ip_address()

	Genera un indirizzo IP casuale nel formato "xxx.xxx.xxx.xxx", secondo l'intervallo specificato.
	Evita le classi di indirizzi non validi/riservati, come: localhost (127), multicast (224) e 
	riservati (240).
	Se non si vuole specificare l'intervallo per la generazione dell'ip, passare -1 e la funzione 
	si incarica di generarlo automaticamente.
  */  
void generate_ip_address(char* buffer,size_t size,int min_octet,int max_octet)
{
	int octet1, octet2, octet3, octet4;

	if(min_octet==-1 && max_octet==-1)
	{
		/* il ciclo per il primo ottetto e' per assicurarsi che non rientri negli intervalli riservati */
		do {
			octet1 = rand() % 256;
		} while (octet1==127 || (octet1 >= 224 && octet1 <= 255));

		octet2 = rand() % 256;
		octet3 = rand() % 256;
		octet4 = rand() % 256;
	}
	else
	{
		do {
			octet1 = (rand() % (max_octet - min_octet + 1)) + min_octet;
		} while (octet1==127 || (octet1 >= 224 && octet1 <= 255));

		octet2 = (rand() % (max_octet - min_octet + 1)) + min_octet;
		octet3 = (rand() % (max_octet - min_octet + 1)) + min_octet;
		octet4 = (rand() % (max_octet - min_octet + 1)) + min_octet;
	}

	strzero(buffer,size);
    snprintf(buffer,size,"%d.%d.%d.%d",octet1,octet2,octet3,octet4);
}

/*
	replace_ip_address()

	Cerca, in una stringa, l'indirizzo ip a partire dal gancio (ad es. "ip=") e lo sostituisce 
	con uno nuovo, generato dentro l'intervallo specificato.
	Se non si dispone di nessun gancio, passare NULL e la funzione usera' i ganci "1." o "2." 
	per cercare l'inizio dell'indirizzo ip all'interno della stringa.
	Se non si vuole specificare l'intervallo per la generazione dell'ip, passare -1 e la funzione 
	si incarica di generarlo automaticamente.

	Restituisce 1 se riesce o 0 per errore.
*/
int replace_ip_address(const char* str,char* buffer,size_t size,int min_octet,int max_octet,const char* hook)
{
    /* cerca l'<hook> (ad es. "ip=") */
    const char* ip_start = NULL;
	if(!hook)
	{
		const char* p = str;
    
		while(*p && *(p+1))
		{
			if(*p=='1' || *p=='2')
			{
				if(*(p+1)=='.')
				{
					ip_start = p-2;
					break;
				}
			}
			p++;
		}
	}
	else
	{
		ip_start = strstr(str,hook);
	}
	if(!ip_start)
		return(0);

	/* memorizza dove inizia e dove finisce l'ip */
    const char* ip_value_start = ip_start + (hook ? strlen(hook) : 0);
    const char* ip_value_end = ip_value_start;

    while(*ip_value_end && (*ip_value_end=='.' || isdigit(*ip_value_end)))
        ip_value_end++;

    /* genera il nuovo ip */
    char random_ip[IP_ADDRESS_LEN+1] = {0};
	if(min_octet==-1 && max_octet==-1)
		generate_ip_address(random_ip,sizeof(random_ip),-1,-1);
	else
	    generate_ip_address(random_ip,sizeof(random_ip),min_octet,max_octet);
    
	/* calcola la lunghezza dela sottostring che viene prima e di quella che viene dopo l'ip */
    size_t prefix_len = ip_value_start - str;
    size_t suffix_len = strlen(ip_value_end);
    
	/* aggiunge la lunghezza dell'ip e verifica che entri tutto nel buffer di output */
    size_t required_size = prefix_len + strlen(random_ip) + suffix_len + 1;
    if(required_size > (size_t)size)
        return(0);

    /* compone la nuova stringa */
    snprintf(buffer,size,"%.*s%s%s",(int)prefix_len,str,random_ip,ip_value_end);

    return(1);
}

/*
	extract_ip_address()

	Estrae l'indirizzo ip contenuto in una stringa nel buffer.

	Restituisce 1 se ha estratto l'indirizzo ip o 0 se non ha trovato un ip valido.
*/
int extract_ip_address(const char* str,char* buffer,size_t size)
{
    int octet1, octet2, octet3, octet4;
    int chars_read;
    const char* current_pos = str;

    while(*current_pos)
	{
		/* cerca il pattern con sscanf */
        if(sscanf(current_pos,"%d.%d.%d.%d%n",&octet1,&octet2,&octet3,&octet4,&chars_read)==4)
		{
            /* controlla che gli ottetti siano nel range valido (0-255) */
            if((octet1 >= 0 && octet1 <= 255) && (octet2 >= 0 && octet2 <= 255) && (octet3 >= 0 && octet3 <= 255) && (octet4 >= 0 && octet4 <= 255))
			{
                /* controlla che l'indirizzo sia un'entita' autonoma */
                if((current_pos==str || !isalnum(*(current_pos-1))) && (current_pos[chars_read]=='\0' || !isalnum(current_pos[chars_read])))
				{
                    /* calcola la lunghezza della stringa dell'ip */
                    int ip_len = chars_read;

                    /* verifica che il buffer di destinazione sia abbastanza grande */
                    if(ip_len + 1 <= (int)size)
					{
                        /* copia la sottostringa dell'ip nel buffer */
                        memcpy(buffer,current_pos,ip_len);
                        buffer[ip_len] = '\0';
                        return(1);
                    }
                }
            }
        }
        current_pos++;
    }

    return(0);
}
