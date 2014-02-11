/*
 * cstbased C library -- Library for controlling CST Base Station
 *
 * 2014, Tod E. Kurt, http://todbot.com/blog/ , http://thingm.com/
 *
 */


#ifndef __CSTBASE_LIB_H__
#define __CSTBASE_LIB_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define cstbase_max_devices 16

#define cache_max 16  
#define serialstrmax (8 + 1) 
#define pathstrmax 128

#define cstbasemk2_serialstart 0x10000000

#define  CSTBASE_VENDOR_ID       0x27B8 /* = 0x27B8 = thingm */
#define  CSTBASE_DEVICE_ID       0xC570 /* = 0xC570 = cstbase */

#define cstbase_report_id  1
#define cstbase_report_size 8
#define cstbase_buf_size (cstbase_report_size+1)

struct cstbase_device_;

void cstbase_sortCache(void);

#if USE_HIDAPI

typedef struct hid_device_ cstbase_device;  // <-- opaque cstbase data structure 

#elif USE_HIDDATA

typedef struct usbDevice   cstbase_device;  // <-- opaque cstbase data structure 

#endif


//
// public functions
// 

// scan USB for CST Base devices
int          cstbase_enumerate();

// scan USB for devices by given VID,PID
int          cstbase_enumerateByVidPid(int vid, int pid);

// open first found CST Base device
cstbase_device* cstbase_open(void);

// open CST Base by USB path  note: this is platform-specific, and port-specific
cstbase_device* cstbase_openByPath(const char* path);

// open CST Base by 8-digit serial number
cstbase_device* cstbase_openBySerial(const char* serial);

// open by "id", which if from 0-cstbase_max_devices is index
// or if >cstbase_max_devices, is numerical representation of serial number
cstbase_device* cstbase_openById( uint32_t i );

// close open device
void cstbase_close( cstbase_device* dev );

// low-level write
int cstbase_write( cstbase_device* dev, void* buf, int len);

// low-level read 
int cstbase_read( cstbase_device* dev, void* buf, int len);


//
// actual functionality
// 

// set time to current localtime
int cstbase_setTime(cstbase_device *dev);

// set time to given hours, mins, secs (0-23, 0-59, 0-59)
int cstbase_setTimeTo(cstbase_device *dev, uint8_t hours, uint8_t mins, uint8_t secs);

// get current button state of base station, returns bitfield in lower 3-bits
int cstbase_getButtons(cstbase_device *dev);

// send arbitrary byte/char stream (up to 6 chars) to watch
int cstbase_sendBytesToWatch(cstbase_device *dev, uint8_t* bytebuf, uint8_t len );

// receive last byte sent from watch to base station
int cstbase_getByteFromWatch(cstbase_device *dev);

// get firmware version of base station
int cstbase_getVersion(cstbase_device *dev);


//
// misc utilities
//

// return the current local time as H,M,S (0-23,0-59,0-59)
void cstbase_getLocalTime(uint8_t* hours, uint8_t* mins, uint8_t* secs);

const char*  cstbase_getCachedPath(int i);
const char*  cstbase_getCachedSerial(int i);
int          cstbase_getCacheIndexByPath( const char* path );
int          cstbase_getCacheIndexBySerial( const char* serial );
int          cstbase_getCacheIndexByDev( cstbase_device* dev );
int          cstbase_clearCacheDev( cstbase_device* dev );

const char*  cstbase_getSerialForDev(cstbase_device* dev);
int          cstbase_getCachedCount(void);

// return VID for CST Base Station
int cstbase_vid(void);  
// return PID for CST Base Station
int cstbase_pid(void); 

// sleep for some millis
void cstbase_sleep(uint16_t delayMillis);

char *cstbase_error_msg(int errCode);


#ifdef __cplusplus
}
#endif

#endif
