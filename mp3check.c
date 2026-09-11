/*
	mp3check.c
	Codice per verificare se il file e' effettivamente in formato MP3.
	La maggior parte dei players, quando il file non e' in un formato valido, semplicemente
	rimangono in silenzio, senza avvisare.
	Luca Piergentili, 23/08/2026
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "mp3check.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	get_id3v2_size()

	Los tamaños en ID3v2 son "synchsafe integers" (7 bits por byte).
	Esta función decodifica esos 4 bytes para saber el tamaño real de la cabecera.
*/
uint32_t get_id3v2_size(const uint8_t *header)
{
	return	((header[6] & 0x7F) << 21) |
			((header[7] & 0x7F) << 14) |
			((header[8] & 0x7F) << 7) |
			(header[9] & 0x7F);
}

/*
	is_valid_mp3()

	Averigua que el fichero sea en formato MP3.
*/
bool is_valid_mp3(const char *filepath)
{
	FILE *file = fopen(filepath,"rb");
	if(!file)
		return(false);

	uint8_t buffer[4096] = {0};
	size_t bytes_read = fread(buffer,1,sizeof(buffer),file);

	if(bytes_read < 10) /* archivo demasiado pequeño */
	{
		fclose(file);
		return(false);
	}

	uint32_t offset = 0;

	/* averigua si existe una cabecera ID3v2 (al principio del fichero, la version 1 esta al final) */
	if(buffer[0]=='I' && buffer[1]=='D' && buffer[2]=='3')
	{
		/* el tamaño de la cabecera + los 10 bytes iniciales */
		uint32_t tag_size = get_id3v2_size(buffer);
		uint32_t total_header_size = tag_size + 10;

		/* salta directamente hasta donde debería empezar el audio */
		if(fseek(file,total_header_size,SEEK_SET)!=0)
		{
			fclose(file);
			return(false);
		}

		/* lee el siguiente bloque donde espera encontrar el audio */
		bytes_read = fread(buffer,1,sizeof(buffer),file);
		if(bytes_read < 2)
		{
			fclose(file);
			return(false); /* archivo truncado justo después de la etiqueta */
		}
	}

	/* busca el Sync Word del frame MPEG (11 bits en 1: 0xFF y 0xE0)
	   escanea el buffer en caso de que haya unos pocos bytes de padding antes del frame */
	for(size_t i = offset; i < bytes_read - 1; i++)
	{
		if(buffer[i]==0xFF && (buffer[i + 1] & 0xE0)==0xE0)
		{
			fclose(file);
			return(true); /* ha encontrado un frame de audio válido */
		}
	}

	fclose(file);

	return(false); /* ningun frame de audio válido */
}
