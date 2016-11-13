/*--------------------------------------------------------------------------------
The MIT License (MIT)

Copyright (c) 2016 Great Hill Corporation

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
--------------------------------------------------------------------------------*/
#include "utillib.h"
#include "curl_code.h"

#include <errno.h>
#include <curl/curl.h>

// forward declaration
class URL_FILE;

// exported functions
extern URL_FILE *url_fopen  ( const char *url, const char *operation );
extern int       url_feof   ( URL_FILE *file );
extern char     *url_fgets  ( char *ptr, size_t size, URL_FILE *file );

//---------------------------------------------------------------------------------------------------
class URL_FILE
{
public:
	CURL *curl;
	char *buffer;               /* buffer to store cached data*/
	size_t buffer_len;          /* currently allocated buffers length */
	size_t buffer_pos;          /* end of data in buffer*/
	int still_running;          /* Is background url fetch still in progress */
};

//---------------------------------------------------------------------------------------------------
/* we use a global one for convenience */
CURLM *multi_handle=0;

//---------------------------------------------------------------------------------------------------
/* curl calls this routine to get more data */
static size_t write_callback(char *buffer, size_t size, size_t nitems, void *userp)
{
	char *newbuff;
	size_t rembuff;

	URL_FILE *url = (URL_FILE *)userp;
	size *= nitems;

	rembuff=url->buffer_len - url->buffer_pos; /* remaining space in buffer */

	if(size > rembuff) {
		/* not enough space in buffer */
		newbuff = (char*)realloc(url->buffer,url->buffer_len + (size - rembuff));
		if(newbuff==NULL) {
			fprintf(stderr,"callback buffer grow failed\n");
			size=rembuff;
		}
		else {
			/* realloc suceeded increase buffer size*/
			url->buffer_len+=size - rembuff;
			url->buffer=newbuff;
		}
	}

	memcpy(&url->buffer[url->buffer_pos], buffer, size);
	url->buffer_pos += size;

	return size;
}

//---------------------------------------------------------------------------------------------------
/* use to attempt to fill the read buffer up to requested number of bytes */
static int fill_buffer(URL_FILE *file, size_t want)
{
	fd_set fdread;
	fd_set fdwrite;
	fd_set fdexcep;
	struct timeval timeout;
	int rc;

	/* only attempt to fill buffer if transactions still running and buffer
	 * doesnt exceed required size already
	 */
	if((!file->still_running) || (file->buffer_pos > want))
		return 0;

	/* attempt to fill buffer */
	do {
		int maxfd = -1;
		long curl_timeo = -1;

		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		/* set a suitable timeout to fail on */
		timeout.tv_sec = 60; /* 1 minute */
		timeout.tv_usec = 0;

		curl_multi_timeout(multi_handle, &curl_timeo);
		if(curl_timeo >= 0) {
			timeout.tv_sec = curl_timeo / 1000;
			if(timeout.tv_sec > 1)
				timeout.tv_sec = 1;
			else
				timeout.tv_usec = (curl_timeo % 1000) * 1000;
		}

		/* get file descriptors from the transfers */
		curl_multi_fdset(multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);

		/* In a real-world program you OF COURSE check the return code of the
		 function calls.  On success, the value of maxfd is guaranteed to be
		 greater or equal than -1.  We call select(maxfd + 1, ...), specially
		 in case of (maxfd == -1), we call select(0, ...), which is basically
		 equal to sleep. */

		rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);

		switch(rc) {
			case -1:
				/* select error */
				break;

			case 0:
			default:
				/* timeout or readable/writable sockets */
				curl_multi_perform(multi_handle, &file->still_running);
				break;
		}
	} while(file->still_running && (file->buffer_pos < want));
	return 1;
}

//---------------------------------------------------------------------------------------------------
/* use to remove want bytes from the front of a files buffer */
static int use_buffer(URL_FILE *file,int want)
{
	/* sort out buffer */
	if((file->buffer_pos - want) <=0) {
		/* ditch buffer - write will recreate */
		if(file->buffer)
			free(file->buffer);

		file->buffer=NULL;
		file->buffer_pos=0;
		file->buffer_len=0;
	}
	else {
		/* move rest down make it available for later */
		memmove(file->buffer,
				&file->buffer[want],
				(file->buffer_pos - want));

		file->buffer_pos -= want;
	}
	return 0;
}

//---------------------------------------------------------------------------------------------------
#define CFTYPE_FILE 1
#define CFTYPE_CURL 2

//---------------------------------------------------------------------------------------------------
URL_FILE *url_fopen(const char *url,const char *operation)
{
	/* this code could check for URLs or types in the 'url' and
     basicly use the real fopen() for standard files */

	URL_FILE *file;
	(void)operation;

	file = (URL_FILE*)malloc(sizeof(URL_FILE));
	if(!file)
		return NULL;

	memset(file, 0, sizeof(URL_FILE));

	file->curl = curl_easy_init();

	curl_easy_setopt(file->curl, CURLOPT_URL, url);
	curl_easy_setopt(file->curl, CURLOPT_WRITEDATA, file);
	curl_easy_setopt(file->curl, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(file->curl, CURLOPT_WRITEFUNCTION, write_callback);

	if (!multi_handle)
		multi_handle = curl_multi_init();

	curl_multi_add_handle(multi_handle, file->curl);

	/* lets start the fetch */
	curl_multi_perform(multi_handle, &file->still_running);

	if((file->buffer_pos == 0) && (!file->still_running)) {
		/* if still_running is 0 now, we should return NULL */

		/* make sure the easy handle is not in the multi handle anymore */
		curl_multi_remove_handle(multi_handle, file->curl);

		/* cleanup */
		curl_easy_cleanup(file->curl);

		free(file);

		file = NULL;
	}
	return file;
}

//---------------------------------------------------------------------------------------------------
int url_feof(URL_FILE *file)
{
	int ret=0;
	if((file->buffer_pos == 0) && (!file->still_running))
		ret = 1;
	return ret;
}

//---------------------------------------------------------------------------------------------------
char *url_fgets(char *ptr, size_t size, URL_FILE *file)
{
	size_t want = size - 1;/* always need to leave room for zero termination */
	size_t loop;

	fill_buffer(file,want);

	/* check if theres data in the buffer - if not fill either errored or
	 * EOF */
	if(!file->buffer_pos)
		return NULL;

	/* ensure only available data is considered */
	if(file->buffer_pos < want)
		want = file->buffer_pos;

	/*buffer contains data */
	/* look for newline or eof */
	for(loop=0;loop < want;loop++) {
		if(file->buffer[loop] == '\n') {
			want=loop+1;/* include newline */
			break;
		}
	}

	/* xfer data to caller */
	memcpy(ptr, file->buffer, want);
	ptr[want]=0;/* allways null terminate */

	use_buffer(file,(int)want);

	return ptr;/*success */
}

//---------------------------------------------------------------------------------------------------
SFString urlToString(const SFString& url)
{
	URL_FILE *handle = url_fopen((const char*)url, "r");
	if (!handle)
		return "Could not open URL: " + url + "\n";

	SFString ret;
	char buffer[2048];
	while (!url_feof(handle))
	{
		url_fgets(buffer,sizeof(buffer),handle);
		ret += buffer;
	}

	curl_multi_remove_handle(multi_handle, handle->curl);
	curl_easy_cleanup(handle->curl);
	if(handle->buffer)
		free(handle->buffer);/* free any allocated buffer space */
	free(handle);
	return ret;
}
