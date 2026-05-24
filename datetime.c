/*$
	datetime.c
	Operazioni su data/ora.
	Luca Piergentili, '98
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <time.h>
#include "strings.h"
#include "typedef.h"
#include "datetime.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	unixTimeStamp()

	Calcola il numero di secondi trascorsi dallo UNIX Timestamp, restituendolo come long long.
*/
long long unixTimeStamp(void)
{
    time_t now = time(NULL); /* secondi dal 1970-01-01 00:00:00 UTC */
    if(now==(time_t)-1)
		return(0LL);

    return((long long)now);
}

/*
	unixTimeStampToDate()

	Converte la data impacchettata con DWORD nel formato dd/mm/yyyy hh:mm:ss.

	Chiaramente, se restituisce 1 de enero de 1970 a las 00:00:00 UTC (ossia la data di inizio 
	per il timestamp di Unix), e' perche' il valore del parametro DWORD di input e' 0...

	UNIX Timestamp: il numero di secondi trascorsi dal 1° gennaio 1970 ore 00:00:00, denominato 
	anche "Coordinated Universal Time" (UTC) o "Epoch Unix".
*/
int unixTimeStampToDate(unsigned long ulValue,char *buffer,size_t size)
{
/*
    BYTE buffer[4];		// suponemos que la funcion de lectura del registro llena este buffer
    buffer[0] = 0x48;	// representacion de 0x6024f748 en bytes (Little-endian)
    buffer[1] = 0xF7;
    buffer[2] = 0x24;
    buffer[3] = 0x60;
    // convertir los bytes del buffer a un DWORD numerico:
    // asegurarse de que el buffer tenga al menos 4 bytes y este correctamente alineado o usar memcpy
    // para seguridad y compatibilidad, es mejor usar memcpy para evitar problemas de alineación o type 
	// punning directo si no se esta 100% seguro de la alineación del 'buffer'
    DWORD ulValue;
    memcpy(&ulValue,buffer,sizeof(DWORD)); // ulValue ahora contiene 0x6024f748 como un entero sin signo
    printf("Valor REG_DWORD leído: 0x%lX (decimal: %lu)\n",ulValue,ulValue);
*/
	// convierte el DWORD (Unix timestamp) a una estructura time_t
    // el valor de InstallDate es el tiempo en segundos desde el epoch
    time_t installTime = (time_t)ulValue;

    // convierte a una estructura struct tm (tiempo local)
    // usa localtime para obtener la hora local, o gmtime para UTC
    struct tm *localTimeInfo = localtime(&installTime); // ojo, localtime no es thread-safe
    if(localTimeInfo==NULL)
        return(0);

    // formatea la estructura tiempo a una cadena legible
    char date[128] = {0};

    // strftime permite formatear la fecha y hora de muchas maneras
    // ejemplos de formato:
    // "%Y-%m-%d %H:%M:%S" -> AAAA-MM-DD HH:MM:SS
    // "%d/%m/%Y %H:%M:%S" -> DD/MM/AAAA HH:MM:SS
    size_t bytesWritten = strftime(date,sizeof(date),"%d/%m/%Y %H:%M:%S",localTimeInfo);

    if(bytesWritten==0)
        return(0);
	else
		strcpyn(buffer,date,size);

    // para mostrar tambien la zona horaria (util para depurar UTC vs. local):
    // char timezoneString[10];
    // strftime(timezoneString,sizeof(timezoneString),"%Z",localTimeInfo);
    // printf("Zona horaria: %s\n",timezoneString);

    return(1);
}

/*
	dateToUnixTimeStamp()

	Converte dal formato "DD/MM/YYYY HH:MM:SS" a Unix timestamp (DWORD) vedi note sopra.
*/
unsigned long dateToUnixTimeStamp(char *date)
{
    struct tm tm_struct = {0};
    time_t unixTimestamp = 0LL;
	unsigned long dwInstallDate = 0L;
    int day,month,year,hour,minute,second;

    // analiza la cadena de fecha usando sscanf
    // usa %d para el año tambien, esperando un numero de 4 dígitos
    if(sscanf(date,"%d/%d/%d %d:%d:%d",&day,&month,&year,&hour,&minute,&second)!=6)
        return(dwInstallDate); // formato inesperado

    // llena la estructura tm_struct
    tm_struct.tm_sec = second;
    tm_struct.tm_min = minute;
    tm_struct.tm_hour = hour;
    tm_struct.tm_mday = day;
    tm_struct.tm_mon = month - 1;		// los meses van de 0 a 11
    tm_struct.tm_year = year - 1900;	// los años son desde 1900 (2025 -> 125)

    // campos que mktime calculara o son opcionales:
    tm_struct.tm_wday = 0;    // dia de la semana (0-6, domingo=0), calculado por mktime
    tm_struct.tm_yday = 0;    // dia del año (0-365), calculado por mktime
    tm_struct.tm_isdst = -1;  // indica si el horario de verano esta en efecto
                              // -1 = mktime debe determinarlo (recomendado)
                              // 0 = no esta en efecto
                              // 1 = esta en efecto

    // convierte la estructura tm_struct a time_t (Unix timestamp)
    unixTimestamp = mktime(&tm_struct);

    if(unixTimestamp==(time_t)-1)
	    return(dwInstallDate); // error de conversion

    // convierte time_t a DWORD
    // el Unix timestamp es el numero de segundos desde el 1 de enero de 1970 00:00:00 UTC
    // un DWORD (unsigned long) de 32 bits es suficiente hasta el año 2038
    dwInstallDate = (unsigned long)unixTimestamp;

	// verifica convirtiendo de nuevo a fecha para confirmar
    struct tm *check_tm = localtime(&unixTimestamp);
    char checkDateString[128] = {0};
    strftime(checkDateString,sizeof(checkDateString),"%d/%m/%Y %H:%M:%S",check_tm);
 
    // verifica en UTC (opcional)
    struct tm *gm_check_tm = gmtime(&unixTimestamp);
    char gmCheckDateString[128] = {0};
    strftime(gmCheckDateString,sizeof(gmCheckDateString),"%d/%m/%Y %H:%M:%S",gm_check_tm);
 
    return(dwInstallDate);
}
