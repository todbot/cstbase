/*
 * CST Base Station C library -- 
 *
 *
 * 2014, Tod E. Kurt, http://todbot.com/blog/ , http://thingm.com/
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>  // for toupper()
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#define   swprintf   _snwprintf
#else
//#include <unistd.h>    // for usleep()
#endif

#include "cstbase-lib.h"

// cstbase copy of some hid_device_info and other bits. 
// this seems kinda dumb, though. is there a better way?
typedef struct cstbase_info_ {
    cstbase_device* dev;  // device, if opened, NULL otherwise
    char path[pathstrmax];  // platform-specific device path
    char serial[serialstrmax];
    int type;  // from cstbasetypes
} cstbase_info;

static cstbase_info cstbase_infos[cache_max];
static int cstbase_cached_count = 0;  // number of cached entities

static int cstbase_enable_degamma = 1;

// set in Makefile to debug HIDAPI stuff
#ifdef DEBUG_PRINTF
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) do {} while (0)
#endif


void cstbase_sortCache(void);


//----------------------------------------------------------------------------
// implementation-varying code 

#if USE_HIDAPI
#include "cstbase-lib-lowlevel-hidapi.h"
#elif USE_HIDDATA
#include "cstbase-lib-lowlevel-hiddata.h"
#else
#error "Need to define USE_HIDAPI or USE_HIDDATA"
#endif



// -------------------------------------------------------------------------
// everything below here doesn't need to know about USB details
// except for a "cstbase_device*"
// -------------------------------------------------------------------------

//
int cstbase_getCachedCount(void)
{
    return cstbase_cached_count;
}

//
const char* cstbase_getCachedPath(int i)
{
    return cstbase_infos[i].path;
}
//
const char* cstbase_getCachedSerial(int i)
{
    return cstbase_infos[i].serial;
}

int cstbase_getCacheIndexByPath( const char* path ) 
{
    for( int i=0; i< cache_max; i++ ) { 
        if( strcmp( cstbase_infos[i].path, (const char*) path ) == 0 ) return i;
    }
    return -1;
}

int cstbase_getCacheIndexBySerial( const char* serial ) 
{
    for( int i=0; i< cache_max; i++ ) { 
        if( strcmp( cstbase_infos[i].serial, serial ) == 0 ) return i;
    }
    return -1;
}

int cstbase_getCacheIndexByDev( cstbase_device* dev ) 
{
    for( int i=0; i< cache_max; i++ ) { 
        if( cstbase_infos[i].dev == dev ) return i;
    }
    return -1;
}

const char* cstbase_getSerialForDev(cstbase_device* dev)
{
    int i = cstbase_getCacheIndexByDev( dev );
    if( i>=0 ) return cstbase_infos[i].serial;
    return NULL;
}

int cstbase_clearCacheDev( cstbase_device* dev ) 
{
    int i = cstbase_getCacheIndexByDev( dev );
    if( i>=0 ) cstbase_infos[i].dev = NULL; // FIXME: hmmmm
    return i;
}

int cstbase_isMk2ById( int i )
{
    if( i>=0  && cstbase_infos[i].type == CSTBASE_MK2 ) return 1;
    return 0;
}

int cstbase_isMk2( cstbase_device* dev )
{
    return cstbase_isMk2ById( cstbase_getCacheIndexByDev(dev) );
}


//
int cstbase_getVersion(cstbase_device *dev)
{
    char buf[cstbase_buf_size] = {cstbase_report_id, 'v' };
    int len = sizeof(buf);

    //hid_set_nonblocking(dev, 0);
    int rc = cstbase_write(dev, buf, sizeof(buf));
    cstbase_sleep( 50 ); //FIXME:
    if( rc != -1 ) // no error
        rc = cstbase_read(dev, buf, len);
    if( rc != -1 ) // also no error
        rc = ((buf[3]-'0') * 100) + (buf[4]-'0'); 
    // rc is now version number or error  
    // FIXME: we don't know vals of errcodes
    return rc;
}



//
int cstbase_testtest( cstbase_device *dev)
{
    uint8_t buf[cstbase_buf_size] = { cstbase_report_id, '!', 0,0,0, 0,0,0 };

    int rc = cstbase_write(dev, buf, sizeof(buf) );
    cstbase_sleep( 50 ); //FIXME:
    if( rc != -1 ) { // no error
        rc = cstbase_read(dev, buf, sizeof(buf));
        for( int i=0; i<sizeof(buf); i++ ) { 
            printf("%2.2x,",(uint8_t)buf[i]);
        }
        printf("\n");
    }
    else { 
        printf("testtest error: rc=%d\n", rc);
    }
    return rc;
}



/* ------------------------------------------------------------------------- */

void cstbase_enableDegamma()
{
    cstbase_enable_degamma = 1;
}
void cstbase_disableDegamma()
{
    cstbase_enable_degamma = 0;
}

// a simple logarithmic -> linear mapping as a sort of gamma correction
// maps from 0-255 to 0-255
static int cstbase_degamma_log2lin( int n )  
{
  //return  (int)(1.0* (n * 0.707 ));  // 1/sqrt(2)
    return (((1<<(n/32))-1) + ((1<<(n/32))*((n%32)+1)+15)/32);
}
// from http://rgb-123.com/ws2812-color-output/
//   GammaE=255*(res/255).^(1/.45)
//
uint8_t GammaE[] = { 
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2,
2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5,
6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 11, 11,
11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18,
19, 19, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 27, 27, 28,
29, 29, 30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 37, 38, 39, 40,
40, 41, 42, 43, 44, 45, 46, 46, 47, 48, 49, 50, 51, 52, 53, 54,
55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70,
71, 72, 73, 74, 76, 77, 78, 79, 80, 81, 83, 84, 85, 86, 88, 89,
90, 91, 93, 94, 95, 96, 98, 99,100,102,103,104,106,107,109,110,
111,113,114,116,117,119,120,121,123,124,126,128,129,131,132,134,
135,137,138,140,142,143,145,146,148,150,151,153,155,157,158,160,
162,163,165,167,169,170,172,174,176,178,179,181,183,185,187,189,
191,193,194,196,198,200,202,204,206,208,210,212,214,216,218,220,
222,224,227,229,231,233,235,237,239,241,244,246,248,250,252,255};
//
static int cstbase_degamma_better( int n ) 
{
    return GammaE[n];
}

//
int cstbase_degamma( int n ) 
{ 
    //return cstbase_degamma_log2lin(n);
    return cstbase_degamma_better(n);
}

// qsort char* string comparison function 
int cmp_cstbase_info_serial(const void *a, const void *b) 
{ 
    cstbase_info* bia = (cstbase_info*) a;
    cstbase_info* bib = (cstbase_info*) b;

    return strncmp( bia->serial, 
                    bib->serial, 
                    serialstrmax);
} 

void cstbase_sortCache(void)
{
    size_t elemsize = sizeof( cstbase_info ); //  
    
    qsort( cstbase_infos, 
           cstbase_cached_count, 
           elemsize, 
           cmp_cstbase_info_serial);
}


//
int cstbase_vid(void)
{
    //uint8_t  rawVid[2] = {USB_CFG_VENDOR_ID};
    //int vid = rawVid[0] + 256 * rawVid[1];
    return CSTBASE_VENDOR_ID;
}
//
int cstbase_pid(void)
{
    //uint8_t  rawPid[2] = {USB_CFG_DEVICE_ID};
    //int pid = rawPid[0] + 256 * rawPid[1];
    return CSTBASE_DEVICE_ID;
}

// simple cross-platform millis sleep func
void cstbase_sleep(uint16_t millis)
{
#ifdef WIN32
            Sleep(millis);
#else 
            usleep( millis * 1000);
#endif
}



