/*
===========================================================================
Copyright (C) 2006 Tony J. White (tjw@tjw.org)

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifdef USE_CURL
#include "client.h"
cvar_t *cl_cURLLib;

#ifdef USE_CURL_DLOPEN

char*		(*qcurl_version)(void);

CURL*		(*qcurl_easy_init)(void);
CURLcode	(*qcurl_easy_setopt)(CURL *curl, CURLoption option, ...);
CURLcode	(*qcurl_easy_perform)(CURL *curl);
void		(*qcurl_easy_cleanup)(CURL *curl);
CURLcode	(*qcurl_easy_getinfo)(CURL *curl, CURLINFO info, ...);
const char *(*qcurl_easy_strerror)(CURLcode);

CURLM*		(*qcurl_multi_init)(void);
CURLMcode	(*qcurl_multi_add_handle)(CURLM *multi_handle,
                                                CURL *curl_handle);
CURLMcode	(*qcurl_multi_remove_handle)(CURLM *multi_handle,
                                                CURL *curl_handle);
CURLMcode	(*qcurl_multi_perform)(CURLM *multi_handle,
                                                int *running_handles);
CURLMcode	(*qcurl_multi_cleanup)(CURLM *multi_handle);
CURLMsg		*(*qcurl_multi_info_read)(CURLM *multi_handle,
                                                int *msgs_in_queue);
const char	*(*qcurl_multi_strerror)(CURLMcode);

static void	*cURLLib = NULL;

/*
=================
GPA
=================
*/
static void *GPA(char *str)
{
	void *rv;

	rv = Sys_LoadFunction(cURLLib, str);
	if(!rv)
	{
		Com_Printf("Can't load symbol %s\n", str);
		clc.cURLEnabled = qfalse;
		return NULL;
	}
	else
	{
		Com_DPrintf("Loaded symbol %s (0x%p)\n", str, rv);
        return rv;
	}
}
#endif /* USE_CURL_DLOPEN */

/*
=================
CL_cURL_Init
=================
*/
qboolean CL_cURL_Init()
{
#ifdef USE_CURL_DLOPEN
	if(cURLLib)
		return qtrue;


	Com_Printf("Loading \"%s\"...", cl_cURLLib->string);
	if( (cURLLib = Sys_LoadLibrary(cl_cURLLib->string)) == 0 )
	{
#ifdef _WIN32
		return qfalse;
#else
		char fn[1024];

		Q_strncpyz( fn, Sys_Pwd(), sizeof( fn ) );
		strncat( fn, "/", sizeof( fn ) - strlen( fn ) - 1 );
		strncat( fn, cl_cURLLib->string, sizeof( fn ) - strlen( fn ) - 1 );

		if((cURLLib = Sys_LoadLibrary(fn)) == 0)
		{
#ifdef ALTERNATE_CURL_LIB
			// On some linux distributions there is no libcurl.so.3, but only libcurl.so.4. That one works too.
			if( (cURLLib = Sys_LoadLibrary(ALTERNATE_CURL_LIB)) == 0 )
			{
				return qfalse;
			}
#else
			return qfalse;
#endif
		}
#endif /* _WIN32 */
	}

	clc.cURLEnabled = qtrue;

	qcurl_version = GPA("curl_version");

	qcurl_easy_init = GPA("curl_easy_init");
	qcurl_easy_setopt = GPA("curl_easy_setopt");
	qcurl_easy_perform = GPA("curl_easy_perform");
	qcurl_easy_cleanup = GPA("curl_easy_cleanup");
	qcurl_easy_getinfo = GPA("curl_easy_getinfo");
	qcurl_easy_strerror = GPA("curl_easy_strerror");
	
	qcurl_multi_init = GPA("curl_multi_init");
	qcurl_multi_add_handle = GPA("curl_multi_add_handle");
	qcurl_multi_remove_handle = GPA("curl_multi_remove_handle");
	qcurl_multi_perform = GPA("curl_multi_perform");
	qcurl_multi_cleanup = GPA("curl_multi_cleanup");
	qcurl_multi_info_read = GPA("curl_multi_info_read");
	qcurl_multi_strerror = GPA("curl_multi_strerror");

	if(!clc.cURLEnabled)
	{
		CL_cURL_Shutdown();
		Com_Printf("FAIL One or more symbols not found\n");
		return qfalse;
	}
	Com_Printf("OK\n");

	return qtrue;
#else
	clc.cURLEnabled = qtrue;
	return qtrue;
#endif /* USE_CURL_DLOPEN */
}

/*
=================
CL_cURL_Shutdown
=================
*/
void CL_cURL_Shutdown( void )
{
	CL_cURL_Cleanup();
#ifdef USE_CURL_DLOPEN
	if(cURLLib)
	{
		Sys_UnloadLibrary(cURLLib);
		cURLLib = NULL;
	}
	qcurl_easy_init = NULL;
	qcurl_easy_setopt = NULL;
	qcurl_easy_perform = NULL;
	qcurl_easy_cleanup = NULL;
	qcurl_easy_getinfo = NULL;

	qcurl_multi_init = NULL;
	qcurl_multi_add_handle = NULL;
	qcurl_multi_remove_handle = NULL;
	qcurl_multi_perform = NULL;
	qcurl_multi_cleanup = NULL;
	qcurl_multi_info_read = NULL;
	qcurl_multi_strerror = NULL;
#endif /* USE_CURL_DLOPEN */
}

void CL_cURL_Cleanup(void)
{
	if(clc.downloadCURLM) {
		if(clc.downloadCURL) {
			qcurl_multi_remove_handle(clc.downloadCURLM,
				clc.downloadCURL);
			qcurl_easy_cleanup(clc.downloadCURL);
		}
		qcurl_multi_cleanup(clc.downloadCURLM);
		clc.downloadCURLM = NULL;
		clc.downloadCURL = NULL;
	}
	else if(clc.downloadCURL) {
		qcurl_easy_cleanup(clc.downloadCURL);
		clc.downloadCURL = NULL;
	}
}

static int CL_cURL_CallbackProgress( void *dummy, double dltotal, double dlnow,
	double ultotal, double ulnow )
{
	clc.downloadSize = (int)dltotal;
	Cvar_SetValue( "cl_downloadSize", clc.downloadSize );
	clc.downloadCount = (int)dlnow;
	Cvar_SetValue( "cl_downloadCount", clc.downloadCount );
	return 0;
}

static size_t CL_cURL_CallbackWrite(void *buffer, size_t size, size_t nmemb,
	void *stream)
{
	FS_Write( buffer, size*nmemb, ((fileHandle_t*)stream)[0] );
	return size*nmemb;
}

void CL_cURL_BeginDownload( const char *localName, const char *remoteURL )
{
	clc.cURLUsed = qtrue;
	Com_Printf("URL: %s\n", remoteURL);
	Com_DPrintf("***** CL_cURL_BeginDownload *****\n"
		"Localname: %s\n"
		"RemoteURL: %s\n"
		"****************************\n", localName, remoteURL);
	CL_cURL_Cleanup();
	Q_strncpyz(clc.downloadURL, remoteURL, sizeof(clc.downloadURL));
	Q_strncpyz(clc.downloadName, localName, sizeof(clc.downloadName));
	Com_sprintf(clc.downloadTempName, sizeof(clc.downloadTempName),
		"%s.tmp", localName);

	// Set so UI gets access to it
	Cvar_Set("cl_downloadName", localName);
	Cvar_Set("cl_downloadSize", "0");
	Cvar_Set("cl_downloadCount", "0");
	Cvar_SetValue("cl_downloadTime", cls.realtime);

	clc.downloadBlock = 0; // Starting new file
	clc.downloadCount = 0;

	clc.downloadCURL = qcurl_easy_init();
	if(!clc.downloadCURL) {
		Com_Error(ERR_DROP, "CL_cURL_BeginDownload: qcurl_easy_init() "
			"failed\n");
		return;
	}
	clc.download = FS_SV_FOpenFileWrite( clc.downloadTempName );
	if(!clc.download) {
		Com_Error(ERR_DROP, "CL_cURL_BeginDownload: failed to open "
			"%s for writing\n", clc.downloadTempName);
		return;
	}

	if ( com_developer->integer )
		qcurl_easy_setopt( clc.downloadCURL, CURLOPT_VERBOSE, 1 );
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_URL, clc.downloadURL);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_TRANSFERTEXT, 0);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_REFERER, va("ioQ3://%s",
		NET_AdrToString(clc.serverAddress)));
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_USERAGENT, Q3_VERSION);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_WRITEFUNCTION,
		CL_cURL_CallbackWrite);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_WRITEDATA, &clc.download);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_NOPROGRESS, 0);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_PROGRESSFUNCTION,
		CL_cURL_CallbackProgress);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_PROGRESSDATA, NULL);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_FAILONERROR, 1);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_FOLLOWLOCATION, 1);
	qcurl_easy_setopt(clc.downloadCURL, CURLOPT_MAXREDIRS, 5);
	clc.downloadCURLM = qcurl_multi_init();	
	if(!clc.downloadCURLM) {
		qcurl_easy_cleanup(clc.downloadCURL);
		clc.downloadCURL = NULL;
		Com_Error(ERR_DROP, "CL_cURL_BeginDownload: qcurl_multi_init() "
			"failed\n");
		return;
	}
	qcurl_multi_add_handle(clc.downloadCURLM, clc.downloadCURL);

	if(!(clc.sv_allowDownload & DLF_NO_DISCONNECT) &&
		!clc.cURLDisconnected) {

		CL_AddReliableCommand("disconnect", qtrue);
		CL_WritePacket();
		CL_WritePacket();
		CL_WritePacket();
		clc.cURLDisconnected = qtrue;
	}
}

void CL_cURL_PerformDownload(void)
{
	CURLMcode res;
	CURLMsg *msg;
	int c;
	int i = 0;

	res = qcurl_multi_perform(clc.downloadCURLM, &c);
	while(res == CURLM_CALL_MULTI_PERFORM && i < 100) {
		res = qcurl_multi_perform(clc.downloadCURLM, &c);
		i++;
	}
	if(res == CURLM_CALL_MULTI_PERFORM)
		return;
	msg = qcurl_multi_info_read(clc.downloadCURLM, &c);
	if(msg == NULL) {
		return;
	}
	FS_FCloseFile( clc.download );
	clc.download = FS_INVALID_HANDLE;
	if ( msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK ) {
		FS_SV_Rename( clc.downloadTempName, clc.downloadName );
		clc.downloadRestart = qtrue;
	}
	else {
		long code;

		qcurl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE,
			&code);	
		Com_Error(ERR_DROP, "Download Error: %s Code: %ld URL: %s",
			qcurl_easy_strerror(msg->data.result),
			code, clc.downloadURL);
	}

	CL_NextDownload();
}


/*  
==================================

Common CURL downloading functions

==================================
*/

/*
==================================
stristr

case-insensitive sub-string search
==================================
*/
char* stristr( char *source, char *target ) 
{
	char *p0, *p1, *p2, *pn;
	char c1, c2;

	if ( *target == '\0' )  
	{
		return source;
	}

	pn = source;
	p1 = source;
	p2 = target;
	
	while ( *++p2 )
	{
	    pn++;
	}

	while ( *pn != '\0' ) 
	{

    	p0 = p1;
	    p2 = target;

    	while ( (c1 = *p1) != '\0' && (c2 = *p2) != '\0' )
		{
				if ( c1 <= 'Z' && c1 >= 'A' )
					c1 += ('a' - 'A');

				if ( c2 <= 'Z' && c2 >= 'A' )
					c2 += ('a' - 'A');

				if ( c1 != c2 ) 
				{
					break;
				}

				p1++;
				p2++;
		}

		if ( *p2 == '\0' )  
		{
			return p0;
		}

		p1 = p0 + 1;
	    pn++;
  }

  return NULL;
}

/*
==================================
replace
==================================
*/
int replace1( const char src, const char dst, char *str ) 
{
	int count;

	if ( !str ) 
		return 0;

	count = 0;

	while ( *str != '\0' ) 
	{
		if ( *str == src )	
		{
			*str = dst;
			count++;
		}
		str++;
	}

	return count;
}


/*
=================
Com_DL_Done
=================
*/
void Com_DL_Done( download_t *dl ) 
{
	if ( dl->func.lib )
		Sys_UnloadLibrary( dl->func.lib );
	dl->func.lib = NULL;
	memset( &dl->func, 0, sizeof( dl->func ) );
}


/*
=================
Com_DL_Init
=================
*/
qboolean Com_DL_Init( download_t *dl )
{
#ifdef USE_CURL_DLOPEN
	Com_Printf( "Loading \"%s\"...", cl_cURLLib->string );
	if( ( dl->func.lib = Sys_LoadLibrary( cl_cURLLib->string ) ) == NULL )
	{
#ifdef _WIN32
		return qfalse;
#else
		char fn[1024];

		Q_strncpyz( fn, Sys_Pwd(), sizeof( fn ) );
		strncat( fn, "/", sizeof( fn ) - strlen( fn ) - 1 );
		strncat( fn, cl_cURLLib->string, sizeof( fn ) - strlen( fn ) - 1 );

		if ( ( dl->func.lib = Sys_LoadLibrary( fn ) ) == NULL )
		{
#ifdef ALTERNATE_CURL_LIB
			// On some linux distributions there is no libcurl.so.3, but only libcurl.so.4. That one works too.
			if( ( dl->func.lib = Sys_LoadLibrary( ALTERNATE_CURL_LIB ) ) == NULL )
			{
				return qfalse;
			}
#else
			return qfalse;
#endif
		}
#endif /* _WIN32 */
	}

	Sys_LoadFunctionErrors(); // reset error count;

	dl->func.version = Sys_LoadFunction( dl->func.lib, "curl_version" );

	dl->func.easy_init = Sys_LoadFunction( dl->func.lib, "curl_easy_init" );
	dl->func.easy_setopt = Sys_LoadFunction( dl->func.lib, "curl_easy_setopt" );
	dl->func.easy_perform = Sys_LoadFunction( dl->func.lib, "curl_easy_perform" );
	dl->func.easy_cleanup = Sys_LoadFunction( dl->func.lib, "curl_easy_cleanup" );
	dl->func.easy_getinfo = Sys_LoadFunction( dl->func.lib, "curl_easy_getinfo" );
	dl->func.easy_strerror = Sys_LoadFunction( dl->func.lib, "curl_easy_strerror" );
	
	dl->func.multi_init = Sys_LoadFunction( dl->func.lib, "curl_multi_init" );
	dl->func.multi_add_handle = Sys_LoadFunction( dl->func.lib, "curl_multi_add_handle" );
	dl->func.multi_remove_handle = Sys_LoadFunction( dl->func.lib, "curl_multi_remove_handle" );
	dl->func.multi_perform = Sys_LoadFunction( dl->func.lib, "curl_multi_perform" );
	dl->func.multi_cleanup = Sys_LoadFunction( dl->func.lib, "curl_multi_cleanup" );
	dl->func.multi_info_read = Sys_LoadFunction( dl->func.lib, "curl_multi_info_read" );
	dl->func.multi_strerror = Sys_LoadFunction( dl->func.lib, "curl_multi_strerror" );

	if ( Sys_LoadFunctionErrors() )
	{
		Com_DL_Done( dl );
		Com_Printf( "FAIL: One or more symbols not found\n" );
		return qfalse;
	}

	Com_Printf( "OK\n" );

	return qtrue;
#else

	dl->func.lib = NULL;

	dl->func.version = curl_version;

	dl->func.easy_init = curl_easy_init;
	dl->func.easy_setopt = curl_easy_setopt;
	dl->func.easy_perform = curl_easy_perform;
	dl->func.easy_cleanup = curl_easy_cleanup;
	dl->func.easy_getinfo = curl_easy_getinfo;
	dl->func.easy_strerror = curl_easy_strerror;
	
	dl->func.multi_init = curl_multi_init;
	dl->func.multi_add_handle = curl_multi_add_handle;
	dl->func.multi_remove_handle = curl_multi_remove_handle;
	dl->func.multi_perform = curl_multi_perform;
	dl->func.multi_cleanup = curl_multi_cleanup;
	dl->func.multi_info_read = curl_multi_info_read;
	dl->func.multi_strerror = curl_multi_strerror;

	return qtrue;
#endif /* USE_CURL_DLOPEN */
}


void Com_DL_Cleanup( download_t *dl )
{
	if( dl->cURLM ) 
	{
		if ( dl->cURL ) 
		{
			dl->func.multi_remove_handle( dl->cURLM, dl->cURL );
			dl->func.easy_cleanup( dl->cURL );
		}
		dl->func.multi_cleanup( dl->cURLM );
		dl->cURLM = NULL;
		dl->cURL = NULL;
	}
	else if( dl->cURL ) 
	{
		dl->func.easy_cleanup( dl->cURL );
		dl->cURL = NULL;
	}
	if ( dl->fHandle != FS_INVALID_HANDLE ) 
	{
		FS_FCloseFile( dl->fHandle );
		dl->fHandle = FS_INVALID_HANDLE;
	}

	dl->Size = 0;
	dl->Count = 0;

	dl->URL[0] = '\0';
	dl->Name[0] = '\0';
	dl->TempName[0] = '\0';
	dl->progress[0] = '\0';
	dl->headerCheck = qfalse;
	dl->pk3ext = qfalse;

	Com_DL_Done( dl );
}


static int Com_DL_CallbackProgress( void *data, double dltotal, double dlnow, double ultotal, double ulnow )
{
	double percentage;
	download_t *dl = (download_t *)data;
	
	dl->Size = (int)dltotal;
	dl->Count = (int)dlnow;

	if ( dltotal )
		percentage = ( dlnow / dltotal ) * 100.0;
	else {
		dl->progress[0] = '\0';
		return 0;
	}

	sprintf( dl->progress, " downloading %s: %2.1f%%", dl->Name, percentage );
	
	return 0;
}


static size_t Com_DL_CallbackWrite( void *ptr, size_t size, size_t nmemb, void *userdata )
{
	download_t *dl;

	dl = (download_t *)userdata;

	if ( dl->fHandle == FS_INVALID_HANDLE )
	{
		if ( !dl->pk3ext ) 
		{
			Com_Printf( "Com_DL_CallbackWrite(): file must have pk3 extension.\n" );
			return (size_t)-1;
		}

		dl->fHandle = FS_SV_FOpenFileWrite( dl->TempName );
		if ( dl->fHandle == FS_INVALID_HANDLE ) 
		{
			return (size_t)-1;
		}
	}

	FS_Write( ptr, size*nmemb, dl->fHandle );

	return (size * nmemb);
}


static size_t Com_DL_HeaderCallback( void *ptr, size_t size, size_t nmemb, void *userdata ) 
{
	char name[MAX_QPATH];
	char header[1024], *s, quote, *d;
	download_t *dl;
	int len;

	if ( size*nmemb >= sizeof( header ) ) 
	{
		Com_Printf( S_COLOR_RED "Com_DL_HeaderCallback: header is too large." );
		return (size_t)-1;
	}

	dl = (download_t *)userdata;
	
	memcpy( header, ptr, size*nmemb+1 );
	header[ size*nmemb ] = '\0';

	//Com_Printf( "h: %s\n--------------------------\n", header );

	s = stristr( header, "content-disposition:" );
	if ( s ) 
	{
		s += 20; // strlen( "content-disposition:" )	
		s = stristr( s, "filename=" );
		if ( s ) 
		{
			s += 9; // strlen( "filename=" )
			
			d = name;
			replace1( '\r', '\0', s );
			replace1( '\n', '\0', s );

			// prevent overflow
			if ( strlen( s ) >= sizeof( name ) ) 
				s[ sizeof( name ) - 1 ] = '\0';

			if ( *s == '\'' || *s == '"' )
				quote = *s++;
			else
				quote = '\0';

			// copy filename
			while ( *s != '\0' && *s != quote ) 
				*d++ = *s++;
			len = d - name;
			*d++ = '\0';

			// validate
			if ( len < 5 || !stristr( name + len - 4, ".pk3" ) || strchr( name, '/' ) )
			{
				Com_Printf( "Com_DL_HeaderCallback: bad file name '%s'\n", name );
				return (size_t)-1;
			}

			// strip extension
			dl->pk3ext = FS_StripExt( name, ".pk3" );

			// preserve gamename
			s = strchr( dl->Name, '/' );
			if ( s )
				s++;
			else
				s = dl->Name;

			// store in
			strcpy( s, name );
		}
	}
	
	return size*nmemb;
}

/*

===============================================================
Com_DL_Begin()

Start downloading file from remoteURL and save it under localName

localName may contain game path - otherwise file will be saved
in current baseq3 directory
==============================================================
*/

qboolean Com_DL_Begin( download_t *dl, const char *localName, const char *remoteURL, qboolean headerCheck )
{
	char *s;

	if ( dl->cURL && dl->URL[0] ) 
	{
		Com_Printf( S_COLOR_YELLOW " already downloading %s\n", dl->Name );
		return qfalse;
	}

	Com_Printf( "URL: %s\n", remoteURL );

	Com_DL_Cleanup( dl );

	if ( !Com_DL_Init( dl ) ) 
	{
		Com_Printf( S_COLOR_YELLOW "Error initializing cURL library\n" );		
		return qfalse;
	}

	dl->cURL = dl->func.easy_init();
	if ( !dl->cURL ) 
	{
		Com_Printf( S_COLOR_RED "Com_DL_Begin: easy_init() failed\n" );
		Com_DL_Cleanup( dl );
		return qfalse;
	}

	Q_strncpyz( dl->URL, remoteURL, sizeof( dl->URL ) );

	// try to extract game path from localName
	// dl->Name should contain only pak name without game dir and extension
	s = strchr( localName, '/' );
	if ( !s ) 
		Com_sprintf( dl->Name, sizeof( dl->Name ), "%s/%s", FS_GetBaseGameDir(), localName );
	else
		Q_strncpyz( dl->Name, localName, sizeof( dl->Name ) );

	dl->pk3ext = FS_StripExt( dl->Name, ".pk3" );
	dl->headerCheck = headerCheck;

	Com_sprintf( dl->TempName, sizeof( dl->TempName ), 
		"%s.%04x.tmp", dl->Name, rand() );

	if ( com_developer->integer )
		dl->func.easy_setopt( dl->cURL, CURLOPT_VERBOSE, 1 );

	dl->func.easy_setopt( dl->cURL, CURLOPT_URL, dl->URL );
	dl->func.easy_setopt( dl->cURL, CURLOPT_TRANSFERTEXT, 0 );
	//dl->func.easy_setopt( dl->cURL, CURLOPT_REFERER, "q3a://127.0.0.1" );
	dl->func.easy_setopt( dl->cURL, CURLOPT_REFERER, dl->URL );
	dl->func.easy_setopt( dl->cURL, CURLOPT_USERAGENT, Q3_VERSION );
	dl->func.easy_setopt( dl->cURL, CURLOPT_WRITEFUNCTION,	Com_DL_CallbackWrite );
	dl->func.easy_setopt( dl->cURL, CURLOPT_WRITEDATA, dl );
	if ( headerCheck ) 
	{
		dl->func.easy_setopt( dl->cURL, CURLOPT_HEADERFUNCTION, Com_DL_HeaderCallback );
		dl->func.easy_setopt( dl->cURL, CURLOPT_HEADERDATA, dl );
	}
	dl->func.easy_setopt( dl->cURL, CURLOPT_NOPROGRESS, 0 );
	dl->func.easy_setopt( dl->cURL, CURLOPT_PROGRESSFUNCTION, Com_DL_CallbackProgress );
	dl->func.easy_setopt( dl->cURL, CURLOPT_PROGRESSDATA, dl );
	dl->func.easy_setopt( dl->cURL, CURLOPT_FAILONERROR, 1 );
	dl->func.easy_setopt( dl->cURL, CURLOPT_FOLLOWLOCATION, 1 );
	dl->func.easy_setopt( dl->cURL, CURLOPT_MAXREDIRS, 5 );
	
	dl->cURLM = dl->func.multi_init();	
	
	if ( !dl->cURLM ) 
	{
		Com_DL_Cleanup( dl );	
		Com_Printf( S_COLOR_RED "Com_DL_Begin: multi_init() failed\n" );
		return qfalse;
	}

	if ( dl->func.multi_add_handle( dl->cURLM, dl->cURL ) != CURLM_OK ) 
	{
		Com_DL_Cleanup( dl );
		Com_Printf( S_COLOR_RED "Com_DL_Begin: multi_add_handle() failed\n" );
		return qfalse;
	}

	return qtrue;
}


qboolean Com_DL_Perform( download_t *dl )
{
	char name[MAX_OSPATH];
	CURLMcode res;
	CURLMsg *msg;
	long code;
	int c, n;
	int i;

	res = dl->func.multi_perform( dl->cURLM, &c );

	n = 128;

	i = 0;
	while( res == CURLM_CALL_MULTI_PERFORM && i < n ) 
	{
		res = dl->func.multi_perform( dl->cURLM, &c );
		i++;
	}
	if( res == CURLM_CALL_MULTI_PERFORM ) 
	{
		return qtrue;
	}

	msg = dl->func.multi_info_read( dl->cURLM, &c );
	if( msg == NULL ) 
	{
		return qtrue;
	}

	if ( dl->fHandle != FS_INVALID_HANDLE ) 
	{
		FS_FCloseFile( dl->fHandle );
		dl->fHandle = FS_INVALID_HANDLE;
	}

	if ( msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK ) 
	{
		Com_sprintf( name, sizeof( name ), "%s.pk3", dl->Name );

		if ( !FS_SV_FileExists( name ) ) 
		{
			FS_SV_Rename( dl->TempName, name );
		}
		else
		{
			n = FS_GetZipChecksum( name );
			Com_sprintf( name, sizeof( name ), "%s.%08x.pk3", dl->Name, n );

			if ( FS_SV_FileExists( name ) ) 
				FS_Remove( name );

			FS_SV_Rename( dl->TempName, name );
		}

		Com_DL_Cleanup( dl );
		FS_Reload(); //clc.downloadRestart = qtrue;
		Com_Printf( S_COLOR_GREEN "%s downloaded\n", name );
		return qfalse;
	}
	else 
	{
		dl->func.easy_getinfo( msg->easy_handle, CURLINFO_RESPONSE_CODE, &code );	
		Com_Printf( S_COLOR_RED "Download Error: %s Code: %ld\n",
			dl->func.easy_strerror( msg->data.result ), code );
		strcpy( name, dl->TempName );
		Com_DL_Cleanup( dl );
		FS_Remove( name ); 
	}

	return qtrue;
}


#endif /* USE_CURL */
