#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>


#ifdef __cplusplus
extern "C" {
#endif

struct MemoryStruct {
  char *memory;
  size_t size;
};

CURL *curl_handle[128];
int active_curls;

struct MemoryStruct chunk[128];

int InternetOpen( char *iname, int itype, int *dum, int *dum2, int dum3);
int InternetOpenUrl( int g_hSession, char *url, int *dum1, int *dum2, int dum3, int dum4 );
void InternetQueryDataAvailable(int handle, long *bcount, int dum1, int dum2);
void InternetReadFile(int handle, char *lpBuffer, long bcount, long *bread);
void InternetCloseHandle( int handle );
size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data);


size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
  register int realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)data;
  
  mem->memory = (char *)realloc(mem->memory, mem->size + realsize + 1);
  if (mem->memory) {
    memcpy(&(mem->memory[mem->size]), ptr, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
  }
  return realsize;
}

int InternetOpen( char *iname, int itype, int *dum, int *dum2, int dum3)
{ 
  int i;

  for (i=0;i<128;i++) {
    chunk[i].memory=NULL; /* we expect realloc(NULL, size) to work */
    chunk[i].size = 0;    /* no data at this point */
  }
  curl_global_init(CURL_GLOBAL_ALL);
  active_curls = 0;
  return(1);

}

int InternetOpenUrl( int g_hSession, char *url, int *dum1, int *dum2, int dum3, int dum4 )
{
  /* init the curl session */
  active_curls++;
  curl_handle[active_curls] = curl_easy_init();

  /* specify URL to get */
  curl_easy_setopt(curl_handle[active_curls], CURLOPT_URL, url);

  /* send all data to this function  */
  curl_easy_setopt(curl_handle[active_curls], CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

  /* we pass our 'chunk' struct to the callback function */
  curl_easy_setopt(curl_handle[active_curls], CURLOPT_WRITEDATA, (void *)&chunk[active_curls]);

  /* get it! */
  curl_easy_perform(curl_handle[active_curls]);

  return(active_curls);

}

void InternetQueryDataAvailable(int handle, long *bcount, int dum1, int dum2)
{
   *bcount = chunk[handle].size;
   return;
}

void InternetReadFile(int handle, char *lpBuffer, long bcount, long *bread)
{

    if (chunk[handle].size > 0) {
      memcpy(lpBuffer, chunk[handle].memory, bcount);
      *bread = bcount;
    }
    return;
}


void InternetCloseHandle( int handle )
{
  /* cleanup curl stuff */
  curl_easy_cleanup(curl_handle[handle]);
  chunk[handle].size = 0;
  active_curls--;

}


#ifdef __cplusplus
}
#endif


