/*$
	csvlib.c
	Operazioni su files in formato CSV.
	Luca Piergentili, Ottobre '25
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strings.h"
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include "csvlib.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	csv_parse_line()

	Analizza una singola linea CSV nel formato robusto (con virgolette e escape "")
	effettuandone il parsing (estrazione dei campi) e caricando il valore dei campi
	nelle variabili passate con la mappatura.
	
	Passare la linea letta dal .csv, l'array per la mappatura campi .csv/variabili di 
	memoria e la dimensione dell'array.

	Restituisce 0 se termina correttamente o un valore negativo in caso di errore.
*/
int csv_parse_line(const char* csv_line,CSV_FIELD_MAP* field_map,size_t num_fields)
{
    const char* current_char = csv_line; 
    size_t current_field_idx = 0;
    
    /* inizializza i buffer di destinazione */
	for(size_t i=0; i < num_fields; i++)
		memset(field_map[i].dest_ptr,'\0',field_map[i].max_len);

    /* per tutti i campi presenti nella linea letta dal .csv */
    while(current_field_idx < num_fields) 
    {
        char* dest_ptr = field_map[current_field_idx].dest_ptr;
        size_t max_len = field_map[current_field_idx].max_len;
        size_t dest_idx = 0;
        
        int is_quoted = 0;
        int is_done = 0;
        int needs_truncation = 0;

        /* gestione delimitatore iniziale */
        while(*current_char==' ' || *current_char=='\t')
            current_char++;
        
        if(*current_char=='"')
		{
            is_quoted = 1;
            current_char++; /* salta la virgoletta di apertura */
        }

        /* scansione carattere x carattere (ciclo di copia/troncamento) */
        while(*current_char!='\0' && !is_done) 
        {
            char current_char_value = *current_char;

            /* logica x gestione virgolette */
            if(is_quoted)
            {
                if(current_char_value=='"') 
                {
                    current_char++;
                    if(*current_char=='"') 
                    {
                        /* Escape: copia un singolo apice */
                        current_char_value = '"';
                        current_char++; 
                    } 
                    else 
                    {
                        /* virgoletta di chiusura: il campo e' completato */
                        is_done = 1; 
                        continue; /* passa alla gestione finale dei delimitatori */
                    }
                } 
                else 
                {
                    current_char++;
                }
            } 
            else 
            {
                /* logica x campi senza virgolette */
                if(current_char_value==',') 
                {
                    is_done = 1;
                    continue; /* passa alla gestione finale dei delimitatori */
                } 
                current_char++;
            }
            
            /* gestione copia e troncamento */
            if(dest_idx < max_len - 1)
                /* copia il carattere nel buffer */
                dest_ptr[dest_idx++] = current_char_value;
            else
                /* invece di generare l'errore CSV_ERR_BUFFER_FULL, imposta il flag di troncamento e continua con la seguente scansione */
                needs_truncation = 1;
        }

        /* termina la stringa nel buffer di destinazione (se rimane spazio) */
        if(dest_idx < max_len)
             dest_ptr[dest_idx] = '\0';

        /* se si esce dal ciclo perché *current_char=='\0' e non si e' ancora in is_done */
        if(!is_done && *current_char=='\0')
            is_done = 1; /* chiude l'ultimo campo (fine della linea) */
        
        /* gestione delimitatori finali */
        if(is_done)
        {
            /* se c'e' stato troncamento, deve trovare il vero delimitatore finale (virgola o \0) */
            if(needs_truncation)
            {
                /* scansiona fino a trovare il delimitatore finale */
                while(*current_char!='\0')
                {
                    if(is_quoted)
                    {
                        /* cerca solo la virgoletta di chiusura */
                        if(*current_char=='"')
                        {
                            current_char++;
                            if(*current_char!='"') /* non e' una virgoletta di escape */
                            {
                                /* trovata virgoletta di chiusura */
                                break; 
                            }
                            /* se e' virgoletta di escape, continua */
                        }
                    }
                    else
                    {
                        /* cerca solo la virgola */
                        if(*current_char==',')
                            break; 
                    }
                    current_char++;
                }
            }
            
            /* salta gli spazi bianchi finali (dopo il valore ma prima del delimitatore) */
            if(!is_quoted)
                 while(*current_char==' ' || *current_char=='\t')
                    current_char++;

            if(*current_char==',')
			{
				/* salta la virgola, pronto per il campo successivo */
                current_char++;
            } 
            else if(*current_char=='"' && is_quoted)
			{
                 /* salta la virgoletta di chiusura */
                 current_char++;
                 /* salta la virgola se presente */
                 if(*current_char==',')
					current_char++;
            }
            else if(*current_char!='\0') 
            {
                 /* errore: caratteri non validi tra la fine del campo e la virgola/fine */
                 return(CSV_ERR_MALFORMED);
            }
        }
        else
        {
            /* se l'uscita e' dovuta a errore, viene gestita altrove */
        }

        current_field_idx++;

        /* controlla se ha processato tutti i campi */
        if(*current_char!='\0' && current_field_idx==num_fields)
            return(CSV_ERR_TOO_MANY_FIELDS);
        
        /* se arriva alla fine della linea ma ci sono ancora campi da popolare */
        if(*current_char=='\0' && current_field_idx < num_fields)
            break;
    }

    /* se ci sono ancora caratteri nella linea CSV ma tutti i campi sono stati popolati */
    if(*current_char!='\0')
        return(CSV_ERR_TOO_MANY_FIELDS);

    return(CSV_SUCCESS);
}

/*
	csv_format_line()

	Formatta i valori delle variabili presenti nella mappatura in una singola riga CSV robusta 
	(con virgolette ed escape ""), passando la linea completa al buffer di destinazione.

	Passare il buffer di destinazione e la sua dimensione (il sizeof), l'array per la mappatura 
	campi .csv/variabili di memoria e la dimensione dell'array.

	Restituisce 0 se termina correttamente o un valore negativo in caso di errore.
*/
int csv_format_line(char* out_buffer,size_t buf_size,CSV_FIELD_MAP* field_map,size_t num_fields)
{
    size_t current_len = 0; /* caratteri scritti nel buffer */
    
    /* spazio utile nel buffer di destinazione */
    const size_t max_data_len = buf_size > 0 ? buf_size - 1 : 0; 
    if(buf_size==0)
        return(CSV_ERR_BUFFER_TOO_SMALL);

    memset(out_buffer,'\0',buf_size);

    // macro x scrivere un singolo carattere nel buffer, gestendo l'overflow
    #define WRITE_CHAR(c) {if(current_len < max_data_len) out_buffer[current_len++] = (char)(c); else {out_buffer[max_data_len] = '\0'; return(CSV_ERR_BUFFER_TOO_SMALL);}}

    for(size_t i=0; i < num_fields; i++)
    {
        const char* field_value = field_map[i].dest_ptr;
        
        /* verifica se il campo necessita virgolette */
        int needs_quotes = (strchr(field_value, ',') != NULL) || 
                           (strchr(field_value, '"') != NULL) ||
                           (strchr(field_value, '\n') != NULL);

        if(needs_quotes)
            WRITE_CHAR('"');

        /* trascrive il valore del campo, gestendo l'escape */
        const char* p = field_value;
        while(*p!='\0')
        {
            if(*p=='"')
			{
				/* se e' un apice doppio, scrive un doppio apice di escape ("") */
				WRITE_CHAR('"');
				WRITE_CHAR('"');
            }
			else
			{
				/* scrive tutti gli altri caratteri normalmente */
				WRITE_CHAR(*p);
            }
            p++;
        }

        /* chiude il campo e aggiunge il delimitatore */
        if(needs_quotes)
            WRITE_CHAR('"');

        /* aggiunge la virgola come separatore, tranne dopo l'ultimo campo */
        if(i < num_fields - 1)
            WRITE_CHAR(',');
    }

    /* termina la riga con CR+LF e NULL */
	/* WRITE_CHAR('\n'); */
    out_buffer[current_len] = '\0';

    #undef WRITE_CHAR

    return(CSV_SUCCESS);
}
