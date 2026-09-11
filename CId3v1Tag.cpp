/*
	CId3v1Tag.cpp
	Classe per i tag mp3 (ID3v1), implementata a partire dal codice di Gustav Munkby.
	Luca Piergentili, 07/08/03
	lpiergentili@yahoo.com
*/
#include "pragma.h"
#include "env.h"
#include "macro.h"
#include <stdio.h>
#include <string.h>
#include "strings.h"
#include "CId3v1Tag.h"
#include "CBinFile.h"

#include "traceexpr.h"
//#define _TRACE_FLAG			_TRFLAG_TRACEOUTPUT // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG			_TRFLAG_NOTRACE // opzioni: _TRFLAG_NOTRACE, _TRFLAG_TRACEFILE, _TRFLAG_TRACECONSOLE, _TRFLAG_TRACEOUTPUT, _TRFLAG_TRACEBREAKPOINT
#define _TRACE_FLAG_INFO	_TRACE_FLAG
#define _TRACE_FLAG_WARN	_TRACE_FLAG
#define _TRACE_FLAG_ERR		_TRACE_FLAG

// vedi in globals.h su id3lib
//const char* genre_table[] = {
static const char* const genre_table[] = {
	"Blues",				//0
	"Classic Rock",			//1
	"Country",				//2
	"Dance",				//3
	"Disco",				//4
	"Funk",					//5
	"Grunge",				//6
	"Hip-Hop",				//7
	"Jazz",					//8
	"Metal",				//9
	"New Age",				//10
	"Oldies",				//11
	"Other",				//12
	"Pop",					//13
	"R&B",					//14
	"Rap",					//15
	"Reggae",				//16
	"Rock",					//17
	"Techno",				//18
	"Industrial",			//19
	"Alternative",			//20
	"Ska",					//21
	"Death Metal",			//22
	"Pranks",				//23
	"Soundtrack",			//24
	"Euro-Techno",			//25
	"Ambient",				//26
	"Trip-Hop",				//27
	"Vocal",				//28
	"Jazz+Funk",			//29
	"Fusion",				//30
	"Trance",				//31
	"Classical",			//32
	"Instrumental",			//33
	"Acid",					//34
	"House",				//35
	"Game",					//36
	"Sound Clip",			//37
	"Gospel",				//38
	"Noise",				//39
	"AlternRock",			//40
	"Bass",					//41
	"Soul",					//42
	"Punk",					//43
	"Space",				//44
	"Meditative",			//45
	"Instrumental Pop",		//46
	"Instrumental Rock",	//47
	"Ethnic",				//48
	"Gothic",				//49
	"Darkwave",				//50
	"Techno-Industrial",	//51
	"Electronic",			//52
	"Pop-Folk",				//53
	"Eurodance",			//54
	"Dream",				//55
	"Southern Rock",		//56
	"Comedy",				//57
	"Cult",					//58
	"Gangsta",				//59
	"Top 40",				//60
	"Christian Rap",		//61
	"Pop/Funk",				//62
	"Jungle",				//63
	"Native American",		//64
	"Cabaret",				//65
	"New Wave",				//66
	"Psychadelic",			//67
	"Rave",					//68
	"Showtunes",			//69
	"Trailer",				//70
	"Lo-Fi",				//71
	"Tribal",				//72
	"Acid Punk",			//73
	"Acid Jazz",			//74
	"Polka",				//75
	"Retro",				//76
	"Musical",				//77
	"Rock & Roll",			//78
	"Hard Rock",			//79

	// winamp extensions
	"Folk",					//80
	"Folk-Rock",			//81
	"National Folk",		//82
	"Swing",				//83
	"Fast Fusion",			//84
	"Bebob",				//85
	"Latin",				//86
	"Revival",				//87
	"Celtic",				//88
	"Bluegrass",			//89
	"Avantgarde",			//90
	"Gothic Rock",			//91
	"Progressive Rock",		//92
	"Psychedelic Rock",		//93
	"Symphonic Rock",		//94
	"Slow Rock",			//95
	"Big Band",				//96
	"Chorus",				//97
	"Easy Listening",		//98
	"Acoustic",				//99
	"Humour",				//100
	"Speech",				//101
	"Chanson",				//102
	"Opera",				//103
	"Chamber Music",		//104
	"Sonata",				//105
	"Symphony",				//106
	"Booty Bass",			//107
	"Primus",				//108
	"Porn Groove",			//109
	"Satire",				//110
	"Slow Jam",				//111
	"Club",					//112
	"Tango",				//113
	"Samba",				//114
	"Folklore",				//115
	"Ballad",				//116
	"Power Ballad",			//117
	"Rhythmic Soul",		//118
	"Freestyle",			//119
	"Duet",					//120
	"Punk Rock",			//121
	"Drum Solo",			//122
	"A capella",			//123
	"Euro-House",			//124
	"Dance Hall",			//125
	"Goa",					//126
	"Drum & Bass",			//127
	"Club-House",			//128
	"Hardcore",				//129
	"Terror",				//130
	"Indie",				//131
	"Britpop",				//132
	"Negerpunk",			//133
	"Polsk Punk",			//134
	"Beat",					//135
	"Christian Gangsta Rap",//136
	"Heavy Metal",			//137
	"Black Metal",			//138
	"Crossover",			//139
	"Contemporary Christian",//140
	"Christian Rock ",		//141
	"Merengue",				//142
	"Salsa",				//143
	"Trash Metal",			//144
	"Anime",				//145
	"JPop",					//146
	"Synthpop",				//147

	// mp3t extensions
	"Flamenco",				//148
	"Rumba",				//149
	"Saeta",				//150
	"Rock andaluz",			//151
	"Flamenco pop",			//152
	"Jazz fusion"			//153
};
/*
 123 | A capella
 34  | Acid
 74  | Acid Jazz
 73  | Acid Punk
 99  | Acoustic
 40  | AlternRock
 20  | Alternative
 26  | Ambient
 145 | Anime
 90  | Avantgarde
 116 | Ballad
 41  | Bass
 135 | Beat
 85  | Bebob
 96  | Big Band
 138 | Black Metal
 89  | Bluegrass
 0   | Blues
 107 | Booty Bass
 132 | Britpop
 65  | Cabaret
 88  | Celtic
 104 | Chamber Music
 102 | Chanson
 97  | Chorus
 136 | Christian Gangsta Rap
 61  | Christian Rap
 141 | Christian Rock
 1   | Classic Rock
 32  | Classical
 112 | Club
 128 | Club-House
 57  | Comedy
 140 | Contemporary Christian
 2   | Country
 139 | Crossover
 58  | Cult
 3   | Dance
 125 | Dance Hall
 50  | Darkwave
 22  | Death Metal
 4   | Disco
 55  | Dream
 127 | Drum & Bass
 122 | Drum Solo
 120 | Duet
 98  | Easy Listening
 52  | Electronic
 48  | Ethnic
 124 | Euro-House
 25  | Euro-Techno
 54  | Eurodance
 84  | Fast Fusion
*148 | Flamenco
 80  | Folk
 81  | Folk-Rock
 115 | Folklore
 119 | Freestyle
 5   | Funk
 30  | Fusion
 36  | Game
 59  | Gangsta
 126 | Goa
 38  | Gospel
 49  | Gothic
 91  | Gothic Rock
 6   | Grunge
 79  | Hard Rock
 129 | Hardcore
 137 | Heavy Metal
 7   | Hip-Hop
 35  | House
 100 | Humour
 131 | Indie
 19  | Industrial
 33  | Instrumental
 46  | Instrumental Pop
 47  | Instrumental Rock
 146 | JPop
 8   | Jazz
 29  | Jazz+Funk
 63  | Jungle
 86  | Latin
 71  | Lo-Fi
 45  | Meditative
 142 | Merengue
 9   | Metal
 77  | Musical
 82  | National Folk
 64  | Native American
 133 | Negerpunk
 10  | New Age
 66  | New Wave
 39  | Noise
 11  | Oldies
 103 | Opera
 12  | Other
 75  | Polka
 134 | Polsk Punk
 13  | Pop
 53  | Pop-Folk
 62  | Pop/Funk
 109 | Porn Groove
 117 | Power Ballad
 23  | Pranks
 108 | Primus
 92  | Progressive Rock
 67  | Psychadelic
 93  | Psychedelic Rock
 43  | Punk
 121 | Punk Rock
 14  | R&B
 15  | Rap
 68  | Rave
 16  | Reggae
 76  | Retro
 87  | Revival
 118 | Rhythmic Soul
 17  | Rock
 78  | Rock & Roll
*149 | Rumba
 143 | Salsa
 114 | Samba
 110 | Satire
 69  | Showtunes
 21  | Ska
 111 | Slow Jam
 95  | Slow Rock
 105 | Sonata
 42  | Soul
 37  | Sound Clip
 24  | Soundtrack
 56  | Southern Rock
 44  | Space
 101 | Speech
 83  | Swing
 94  | Symphonic Rock
 106 | Symphony
 147 | Synthpop
 113 | Tango
 18  | Techno
 51  | Techno-Industrial
 130 | Terror
 60  | Top 40
 70  | Trailer
 31  | Trance
 144 | Trash Metal
 72  | Tribal
 27  | Trip-Hop
 28  | Vocal
*/

/*
	CId3v1Tag()
*/
CId3v1Tag::CId3v1Tag(LPCSTR lpcszFileName/* = NULL*/)
{
	memset(m_szTitle,'\0',sizeof(m_szTitle));
	memset(m_szArtist,'\0',sizeof(m_szArtist));
	memset(m_szAlbum,'\0',sizeof(m_szAlbum));
	m_nYear = 0;
	memset(m_szComment,'\0',sizeof(m_szComment));
	m_nTrackNumber = 0;
	m_nGenre = (unsigned char)-1;
	m_nVersion = 0.0;
	memset(m_szFileName,'\0',sizeof(m_szFileName));
	memset(m_ID3v1,'\0',sizeof(m_ID3v1));
	memset(m_ID3v1ext,'\0',sizeof(m_ID3v1ext));
	m_bSaveChangesWhenUnload = FALSE;
	
	if(lpcszFileName)
		Link(lpcszFileName);
}

/*
	~CId3v1Tag()
*/
CId3v1Tag::~CId3v1Tag()
{
	Unlink();
}

/*
	Link()
*/
BOOL CId3v1Tag::Link(LPCSTR lpcszFileName)
{
	BOOL bRet = FALSE;
	
	Unlink();

	strcpyn(m_szFileName,lpcszFileName,sizeof(m_szFileName));
	{
		CBinFileEx file;

		if(file.OpenExistingReadOnly(m_szFileName))
		{
			if(file.SeekEx((LONGLONG)sizeof(m_ID3v1) * -1L,FILE_END)!=FILE_EEOF)
				if(file.Read(m_ID3v1,sizeof(m_ID3v1))!=FILE_EOF)
				{
					SetTagsFromTag();
					bRet = TRUE;
				}
			
			file.Close();
		}
	}

	return(bRet);
}

/*
	Unlink()
*/
void CId3v1Tag::Unlink(void)
{
	if(m_bSaveChangesWhenUnload)
		Update();

	memset(m_szTitle,'\0',sizeof(m_szTitle));
	memset(m_szArtist,'\0',sizeof(m_szArtist));
	memset(m_szAlbum,'\0',sizeof(m_szAlbum));
	m_nYear = 0;
	memset(m_szComment,'\0',sizeof(m_szComment));
	m_nTrackNumber = 0;
	m_nGenre = (unsigned char)-1;
	m_nVersion = 0.0;
	memset(m_szFileName,'\0',sizeof(m_szFileName));
	memset(m_ID3v1,'\0',sizeof(m_ID3v1));
	memset(m_ID3v1ext,'\0',sizeof(m_ID3v1ext));
	m_bSaveChangesWhenUnload = FALSE;
}

/*
	Update()
*/
BOOL CId3v1Tag::Update(void)
{
	BOOL bFlag = FALSE;
	
	if(*m_ID3v1 && *m_szFileName)
	{
		UpdateTagFromTags();
		{
			CBinFileEx file;

			if(file.Open(m_szFileName,FALSE))
			{
				if(file.SeekEx((LONGLONG)sizeof(m_ID3v1) * -1L,FILE_END))
					if(file.Write(m_ID3v1,sizeof(m_ID3v1))!=FILE_EOF)
						bFlag = TRUE;
				
				file.Close();
			}
		}
	}
	
	return(bFlag);
}

/*
	SetTagsFromTag()
*/
BOOL CId3v1Tag::SetTagsFromTag(void)
{
	if(memcmp(m_ID3v1,"TAG",ID3V1_HEADER_SIZE)!=0)
		return(FALSE);

	memset(m_szTitle,'\0',sizeof(m_szTitle));
	memcpy(m_szTitle,m_ID3v1+ID3V1_HEADER_SIZE,ID3V1_TITLE_SIZE);

	memset(m_szArtist,'\0',sizeof(m_szArtist));
	memcpy(m_szArtist,m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE,ID3V1_ARTIST_SIZE);

	memset(m_szAlbum,'\0',sizeof(m_szAlbum));
	memcpy(m_szAlbum,m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE+ID3V1_ARTIST_SIZE,ID3V1_ALBUM_SIZE);

	char year[ID3V1_YEAR_SIZE+1] = {0};
	memcpy(year,m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE+ID3V1_ARTIST_SIZE+ID3V1_ALBUM_SIZE,ID3V1_YEAR_SIZE);
	m_nYear = atoi(year);

	if((m_ID3v1[ID3V1_TAG_ZEROBYTE_OFS]=='\0') && (m_ID3v1[ID3V1_TAG_TRACK_OFS]!='\0'))
	{
		m_nVersion = (float)1.1;
		m_nTrackNumber = (int)m_ID3v1[ID3V1_TAG_TRACK_OFS];
		memset(m_szComment,'\0',sizeof(m_szComment));
		memcpy(m_szComment,m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE+ID3V1_ARTIST_SIZE+ID3V1_ALBUM_SIZE+ID3V1_YEAR_SIZE,ID3V1_COMMENT_SIZE-2);
	}
	else
	{
		m_nVersion = (float)1.0;
		memset(m_szComment,'\0',sizeof(m_szComment));
		memcpy(m_szComment,m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE+ID3V1_ARTIST_SIZE+ID3V1_ALBUM_SIZE+ID3V1_YEAR_SIZE,ID3V1_COMMENT_SIZE);
	}

	m_nGenre = m_ID3v1[ID3V1_TAG_SIZE-1];

	return(TRUE);
}

/*
	UpdateTagFromTags()
*/
BOOL CId3v1Tag::UpdateTagFromTags(void)
{
	char year[ID3V1_YEAR_SIZE+1] = {0};
	_itoa(m_nYear,year,10);

	memset(m_ID3v1,'\0',ID3V1_TAG_SIZE);
	memcpy(m_ID3v1,"TAG",3);
	memcpy(m_ID3v1+ID3V1_HEADER_SIZE,m_szTitle,ID3V1_TITLE_SIZE);
	memcpy(m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE,m_szArtist,ID3V1_ARTIST_SIZE);
	memcpy(m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE+ID3V1_ARTIST_SIZE,m_szAlbum,ID3V1_ALBUM_SIZE);
	memcpy(m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE+ID3V1_ARTIST_SIZE+ID3V1_ALBUM_SIZE,year,ID3V1_YEAR_SIZE);
	memcpy(m_ID3v1+ID3V1_HEADER_SIZE+ID3V1_TITLE_SIZE+ID3V1_ARTIST_SIZE+ID3V1_ALBUM_SIZE+ID3V1_YEAR_SIZE,m_szComment,ID3V1_COMMENT_SIZE);

	// if version 1.1 got a tracknumber
	if(m_nVersion==(float)1.1)
	{
		// make sure comment[28]=='\0' & then set comment[29] to m_nTrackNumber
		m_ID3v1[ID3V1_TAG_ZEROBYTE_OFS] = '\0';
		m_ID3v1[ID3V1_TAG_TRACK_OFS] = (unsigned char)(unsigned int)m_nTrackNumber;
	}

	m_ID3v1[ID3V1_TAG_SIZE-1] = m_nGenre;

	return(TRUE);
}

/*
	GetGenres()
*/
//const char** CId3v1Tag::GetGenres(int& nTot)
const char* const* CId3v1Tag::GetGenres(int& nTot) const
{
	nTot = ARRAY_SIZE(genre_table);
	return(genre_table);
}

/*
	GetGenre()
*/
/* static */ LPCSTR CId3v1Tag::GetGenre(int nIndex)
{
	return(nIndex < 0 || nIndex > ARRAY_SIZE(genre_table) ? "" : genre_table[nIndex]);
}

/*
	GetGenre()
*/
LPCSTR CId3v1Tag::GetGenre(void)
{
	return(m_nGenre >= ARRAY_SIZE(genre_table) ? "" : genre_table[m_nGenre]);
}

/*
	GetGenre()

	Restiruisce ID3V1_GENRE_NOT_FOUND (255) se non trova il genere.
*/
unsigned char CId3v1Tag::GetGenre(LPCSTR lpcszGenre)
{
	for(int i=0; i < ARRAY_SIZE(genre_table); i++)
		if(stricmp(lpcszGenre,genre_table[i])==0)
			return((unsigned char)(unsigned int)i);

	return(ID3V1_GENRE_NOT_FOUND);
}

/*
	SetGenre()
*/
void CId3v1Tag::SetGenre(LPCSTR lpcszGenre)
{
	for(int i=0; i < ARRAY_SIZE(genre_table); i++)
	{
		if(stricmp(lpcszGenre,genre_table[i])==0)
		{
			m_nGenre = (unsigned char)(unsigned int)i;
			return;
		}
	}

	m_nGenre = 255;
}

/*
	SetTrack()
*/
void CId3v1Tag::SetTrack(int nTrackNumber)
{
	if(nTrackNumber==-1)
	{ 
		m_nVersion = (float)1.0;
		m_nTrackNumber = -1;
	}
	else if(nTrackNumber < 0 || nTrackNumber > 255) // invalid tracknumber specified, should be between 0 and 255
	{
		;
	}
	else // no errors, set the tracknumber
	{
		m_nVersion = (float)1.1;
		m_nTrackNumber = nTrackNumber;
	}
}
