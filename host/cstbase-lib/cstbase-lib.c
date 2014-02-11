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
#include <time.h>

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


// set in Makefile to debug HIDAPI stuff
#ifdef DEBUG_PRINTF
#define LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define LOG(...) do {} while (0)
#endif

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

// set time to current localtime
int cstbase_setTime(cstbase_device *dev)
{
    uint8_t hours,mins,secs;
    cstbase_getLocalTime( &hours, &mins, &secs );
    return cstbase_setTimeTo( dev, hours, mins, secs );
}

// set time to given hours & mins (0-23, 0-59)
int cstbase_setTimeTo(cstbase_device *dev, uint8_t hours, uint8_t mins, uint8_t secs)
{
    char buf[cstbase_buf_size] = {cstbase_report_id, 'T', hours, mins, secs };
    int len = sizeof(buf);
    int rc = cstbase_write(dev, buf, sizeof(buf));
    return rc;
}

//
int cstbase_getButtons(cstbase_device *dev)
{
    char buf[cstbase_buf_size] = {cstbase_report_id, 'b' };
    int len = sizeof(buf);

    int rc = cstbase_write(dev, buf, sizeof(buf));
    cstbase_sleep( 50 ); //FIXME:
    if( rc != -1 ) // no error
        rc = cstbase_read(dev, buf, len);
    if( rc != -1 ) // also no error
        rc = buf[3] >> 3; // shift them down to bit pos 0,1,2
    // rc is now button state as bitfield
    return rc;
}

//
int cstbase_sendBytesToWatch(cstbase_device *dev, uint8_t* bytebuf, uint8_t len )
{
    uint8_t buf[cstbase_buf_size];

    if( len > 6 ) { // error
        LOG("cstbase_sendBytesToWatch: oops, len > 6\n");
        return -1;
    }

    buf[0] = cstbase_report_id;     // report id
    buf[1] = 'S';   // command code for "set rgb now"
    buf[2] = len;
    for( int i=0; i< len; i++ ) {
        buf[3+i] = bytebuf[i];
    }
    
    int rc = cstbase_write(dev, buf, sizeof(buf) );
    
    return rc; 
}

//
int cstbase_getByteFromWatch(cstbase_device *dev)
{
    char buf[cstbase_buf_size] = {cstbase_report_id, 'R' };
    int len = sizeof(buf);

    int rc = cstbase_write(dev, buf, sizeof(buf));
    //cstbase_sleep( 50 ); //FIXME:
    if( rc != -1 ) // no error
        rc = cstbase_read(dev, buf, len);
    // rc is now last received byte, or error -1
    if( rc != -1 ) 
        rc = buf[3];
    return rc;
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

//-----------------------------------------------------------------------------

//  return current H:M:S time as byte triplet (avoid inflicting time.h on caller)
void cstbase_getLocalTime(uint8_t* hours, uint8_t* mins, uint8_t* secs)
//const struct tm* cstbase_getCurrentTime(void)
{
    // get current time as seconds since epoch
    time_t current_time = time(NULL);
    struct tm* tminfo = localtime(&current_time);
    *hours = tminfo->tm_hour;
    *mins  = tminfo->tm_min;
    *secs  = tminfo->tm_sec;
}


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

//
int cstbase_getCacheIndexByPath( const char* path ) 
{
    for( int i=0; i< cache_max; i++ ) { 
        if( strcmp( cstbase_infos[i].path, (const char*) path ) == 0 ) return i;
    }
    return -1;
}

//
int cstbase_getCacheIndexBySerial( const char* serial ) 
{
    for( int i=0; i< cache_max; i++ ) { 
        if( strcmp( cstbase_infos[i].serial, serial ) == 0 ) return i;
    }
    return -1;
}

//
int cstbase_getCacheIndexByDev( cstbase_device* dev ) 
{
    for( int i=0; i< cache_max; i++ ) { 
        if( cstbase_infos[i].dev == dev ) return i;
    }
    return -1;
}

//
const char* cstbase_getSerialForDev(cstbase_device* dev)
{
    int i = cstbase_getCacheIndexByDev( dev );
    if( i>=0 ) return cstbase_infos[i].serial;
    return NULL;
}

//
int cstbase_clearCacheDev( cstbase_device* dev ) 
{
    int i = cstbase_getCacheIndexByDev( dev );
    if( i>=0 ) cstbase_infos[i].dev = NULL; // FIXME: hmmmm
    return i;
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
    return CSTBASE_VENDOR_ID;
}
//
int cstbase_pid(void)
{
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



