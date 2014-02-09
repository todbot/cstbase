/*
 * blink(1) C library -- 
 *
 * 2012, Tod E. Kurt, http://todbot.com/blog/ , http://thingm.com/
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

#define cstbasemk2_serialstart 0x20000000

#define  CSTBASE_VENDOR_ID       0x27B8 /* = 0x27B8 = thingm */
#define  CSTBASE_DEVICE_ID       0x4242 /* = 0x4242 = cstbase */

#define cstbase_report_id  1
#define cstbase_report_size 8
#define cstbase_buf_size (cstbase_report_size+1)

enum { 
    CSTBASE_UNKNOWN = 0,
    CSTBASE_MK1,   // the original one from the kickstarter
    CSTBASE_MK2    // the updated one 
}; 

struct cstbase_device_;

#if USE_HIDAPI
typedef struct hid_device_ cstbase_device; /**< opaque cstbase structure */
#elif USE_HIDDATA
typedef struct usbDevice   cstbase_device; /**< opaque cstbase structure */
#endif


int cstbase_vid(void);  // return VID for blink(1)
int cstbase_pid(void);  // return PID for blink(1)


const char*  cstbase_getCachedPath(int i);
const char*  cstbase_getCachedSerial(int i);
int          cstbase_getCacheIndexByPath( const char* path );
int          cstbase_getCacheIndexBySerial( const char* serial );
int          cstbase_getCacheIndexByDev( cstbase_device* dev );
int          cstbase_clearCacheDev( cstbase_device* dev );

const char*  cstbase_getSerialForDev(cstbase_device* dev);
int          cstbase_getCachedCount(void);

int          cstbase_isMk2ById(int i);
int          cstbase_isMk2(cstbase_device* dev);

// scan USB for blink(1) devices
int          cstbase_enumerate();
// scan USB for devices by given VID,PID
int          cstbase_enumerateByVidPid(int vid, int pid);

// open first found blink(1) device
cstbase_device* cstbase_open(void);

// open blink(1) by USB path  note: this is platform-specific, and port-specific
cstbase_device* cstbase_openByPath(const char* path);

// open blink(1) by 8-digit serial number
cstbase_device* cstbase_openBySerial(const char* serial);

// open by "id", which if from 0-cstbase_max_devices is index
// or if >cstbase_max_devices, is numerical representation of serial number
cstbase_device* cstbase_openById( uint32_t i );

void cstbase_close( cstbase_device* dev );

int cstbase_write( cstbase_device* dev, void* buf, int len);
int cstbase_read( cstbase_device* dev, void* buf, int len);

int cstbase_getVersion(cstbase_device *dev);




int cstbase_testtest(cstbase_device *dev);

char *cstbase_error_msg(int errCode);

void cstbase_enableDegamma();
void cstbase_disableDegamma();
int cstbase_degamma(int n);

void cstbase_sleep(uint16_t delayMillis);

#ifdef __cplusplus
}
#endif

#endif
