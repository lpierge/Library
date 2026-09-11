/*
	iTunes.cpp
*/
#include "pragma.h"
#include "env.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include "window.h"
#include "strings.h"
#include "cJSON.h"
#include "iTunes.h"

#include "traceexpr.h"
#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
//#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

/*
	ConvertUTF8ToANSI()
*/
void ConvertUTF8ToANSI(const char* utf8Str,char* ansiStr,size_t ansiSize)
{
	memset(ansiStr,'\0',ansiSize);

	// converte da UTF-8 a Wide Char (UTF-16) temporaneo
	wchar_t wbuffer[512] = {0};
	int wlen = MultiByteToWideChar(CP_UTF8,0,utf8Str,-1,wbuffer,512);
	if(wlen <= 0)
		return;

	// converte da Wide Char alla Code Page ANSI locale del sistema
	// usa WC_COMPOSITECHECK | WC_SEPCHARS per provare a separare i diacritici
	BOOL bUsedDefaultChar = FALSE;
    
	int bytesWritten = WideCharToMultiByte(	CP_ACP, 
											WC_COMPOSITECHECK | WC_SEPCHARS, 
											wbuffer, 
											-1, 
											ansiStr, 
											(int)ansiSize, 
											"?",				// carattere di default se la conversione fallisce
											&bUsedDefaultChar	// per sapere se ha dovuto usare '?'
											);

	// mette una toppa se la conversione con i flag speciali fallisce (es. ERROR_INVALID_FLAGS)
	if(bytesWritten==0)
		WideCharToMultiByte(CP_ACP,0,wbuffer,-1,ansiStr,(int)ansiSize,NULL,NULL);

	ansiStr[ansiSize-1] = '\0';
}

/*
	ParseiTunesJSON()
*/
bool ParseiTunesJSON(const char* jsonBuffer,size_t bufferSize,iTunesTrackInfo* pOutInfo)
{
	bool ret = false;

	if(!jsonBuffer || !pOutInfo)
		return(ret);

	memset(pOutInfo,'\0',sizeof(iTunesTrackInfo));

	// AC/DC EVERYWHERE: il BOM UTF-8 e' una marca invisibile all'inizio di un archivio
	// che indica l'uso della codifica Unicode..., ma che risulta innecessaria in UTF-8!
	// il parser JSON fallisce se la trova

	// salta l'eventuale BOM UTF-8 se presente (0xEF, 0xBB, 0xBF)
	const char* pStart = jsonBuffer;
	if(bufferSize >= 3 && (unsigned char)pStart[0]==0xEF && (unsigned char)pStart[1]==0xBB && (unsigned char)pStart[2]==0xBF) 
		pStart += 3;

	// salta eventuali spazi, newline o caratteri non stampabili prima della graffa '{'
	while(*pStart!='\0' && *pStart!='{')
		pStart++;

	// inizia il parsing dei dati
	cJSON* root = cJSON_Parse(pStart);
	if(!root)
	{
		const char* error_ptr = cJSON_GetErrorPtr();
		if(error_ptr)
			printf("JSON fatal error at: %.20s\n",error_ptr);
		return(ret);
    }

	// record trovati, se <= 0 non ha trovato la traccia
	cJSON* resultCount = cJSON_GetObjectItemCaseSensitive(root,"resultCount");
	if(cJSON_IsNumber(resultCount) && (resultCount->valueint > 0))
	{
		// estrae l'array 'results'
		cJSON* resultsArray = cJSON_GetObjectItemCaseSensitive(root,"results");
		if(cJSON_IsArray(resultsArray))
		{
			// considera solo il primo risultato (indice 0)
			cJSON* item = cJSON_GetArrayItem(resultsArray,0);
			if(item)
			{
				// artistName
				cJSON* artistName = cJSON_GetObjectItemCaseSensitive(item,"artistName");
				if(cJSON_IsString(artistName) && (artistName->valuestring!=NULL))
					ConvertUTF8ToANSI(artistName->valuestring,pOutInfo->artistName,sizeof(pOutInfo->artistName));
					//strcpyn(pOutInfo->artistName,artistName->valuestring,sizeof(pOutInfo->artistName));

				// trackName
				cJSON* trackName = cJSON_GetObjectItemCaseSensitive(item,"trackName");
				if(cJSON_IsString(trackName) && (trackName->valuestring!=NULL))
					ConvertUTF8ToANSI(trackName->valuestring,pOutInfo->trackName,sizeof(pOutInfo->trackName));
					//strcpyn(pOutInfo->trackName,trackName->valuestring,sizeof(pOutInfo->trackName));

				// collectionName
				cJSON* collectionName = cJSON_GetObjectItemCaseSensitive(item,"collectionName");
				if(cJSON_IsString(collectionName) && (collectionName->valuestring!=NULL))
					ConvertUTF8ToANSI(collectionName->valuestring,pOutInfo->collectionName,sizeof(pOutInfo->collectionName));
					//strcpyn(pOutInfo->collectionName,collectionName->valuestring,sizeof(pOutInfo->collectionName));

				// releaseDate (Es: "2021-12-17T12:00:00Z" -> estrae solo "2021")
				cJSON* date = cJSON_GetObjectItemCaseSensitive(item,"releaseDate");
				if(cJSON_IsString(date) && (date->valuestring!=NULL) && strlen(date->valuestring) >= 4)
				{
					memcpy(pOutInfo->releaseYear,date->valuestring,4);
					pOutInfo->releaseYear[4] = '\0';
				}

				// artistViewUrl
				cJSON* artistViewUrl = cJSON_GetObjectItemCaseSensitive(item,"artistViewUrl");
				if(cJSON_IsString(artistViewUrl) && (artistViewUrl->valuestring!=NULL))
					strcpyn(pOutInfo->artistViewUrl,artistViewUrl->valuestring,sizeof(pOutInfo->artistViewUrl));

				// artworkUrl100 -> convertito al volo
				cJSON* artwork = cJSON_GetObjectItemCaseSensitive(item,"artworkUrl100");
				if(cJSON_IsString(artwork) && (artwork->valuestring!=NULL))
				{
					char tempUrl[1024] = {0};
					strcpyn(tempUrl,artwork->valuestring,sizeof(tempUrl));

					// sostituisce "100x100bb" con "600x600bb" per la copertina HD
					char* pos = strstr(tempUrl,"100x100bb");
					if(pos)
						memcpy(pos,"600x600bb",9);
					strcpyn(pOutInfo->artworkUrl600,tempUrl,sizeof(pOutInfo->artworkUrl600));
				}

				// discNumber
				cJSON* discNumber = cJSON_GetObjectItemCaseSensitive(item,"discNumber");
				if(cJSON_IsNumber(discNumber))
					pOutInfo->discNumber = discNumber->valueint;

				// trackCount
				cJSON* trackCount = cJSON_GetObjectItemCaseSensitive(item,"trackCount");
				if(cJSON_IsNumber(trackCount))
					pOutInfo->trackCount = trackCount->valueint;

				// trackNumber
				cJSON* trackNumber = cJSON_GetObjectItemCaseSensitive(item,"trackNumber");
				if(cJSON_IsNumber(trackNumber))
					pOutInfo->trackNumber = trackNumber->valueint;

				// country
				cJSON* country = cJSON_GetObjectItemCaseSensitive(item,"country");
				if(cJSON_IsString(country) && (country->valuestring!=NULL))
					strcpyn(pOutInfo->country,country->valuestring,sizeof(pOutInfo->country));

				// primaryGenreName
				cJSON* primaryGenreName = cJSON_GetObjectItemCaseSensitive(item,"primaryGenreName");
				if(cJSON_IsString(primaryGenreName) && (primaryGenreName->valuestring!=NULL))
					ConvertUTF8ToANSI(primaryGenreName->valuestring,pOutInfo->primaryGenreName,sizeof(pOutInfo->primaryGenreName));
					//strcpyn(pOutInfo->primaryGenreName,primaryGenreName->valuestring,sizeof(pOutInfo->primaryGenreName));

				ret = true;
			}
		}
	}

	if(root)
		cJSON_Delete(root);

	return(ret);
}
