
#include "hiddata.h"


static cstbase_device* static_dev;


//
char *cstbase_error_msg(int errCode)
{
    static char buffer[80];

    switch(errCode){
        case USBOPEN_ERR_ACCESS:    return "Access to device denied";
        case USBOPEN_ERR_NOTFOUND:  return "The specified device was not found";
        case USBOPEN_ERR_IO:        return "Communication error with device";
        default:
            sprintf(buffer, "Unknown USB error %d", errCode);
            return buffer;
    }
    return NULL;    /* not reached */
}

//
int cstbase_enumerate(void)
{
    LOG("cstbase_enumerate!\n");
    return cstbase_enumerateByVidPid( cstbase_vid(), cstbase_pid() );
}

// get all matching devices by VID/PID pair
int cstbase_enumerateByVidPid(int vid, int pid)
{
    int p = 0; 
    if( cstbase_open() ) { 
        cstbase_close(static_dev);
        p = 1;
    }

    /*
    struct hid_device_info *devs, *cur_dev;

    devs = hid_enumerate(vid, pid);
    cur_dev = devs;    
    while (cur_dev) {
        if( (cur_dev->vendor_id != 0 && cur_dev->product_id != 0) &&  
            (cur_dev->vendor_id == vid && cur_dev->product_id == pid) ) { 
            if( cur_dev->serial_number != NULL ) { // can happen if not root
                strcpy( cstbase_infos[p].path,   cur_dev->path );
                sprintf( cstbase_infos[p].serial, "%ls", cur_dev->serial_number);
                //wcscpy( cstbase_infos[p].serial, cur_dev->serial_number );
                //uint32_t sn = wcstol( cur_dev->serial_number, NULL, 16);
                uint32_t serialnum = strtol( cstbase_infos[p].serial, NULL, 16);
                cstbase_infos[p].type = CSTBASE_MK1;
                if( serialnum >= cstbasemk2_serialstart ) {
                    cstbase_infos[p].type = CSTBASE_MK2;
                }
                p++;
            }
        }
        cur_dev = cur_dev->next;
    }
    hid_free_enumeration(devs);
*/
    
    cstbase_cached_count = p;

    cstbase_sortCache();

    return p;
}

//
cstbase_device* cstbase_openByPath(const char* path)
{
    //if( path == NULL || strlen(path) == 0 ) {
    //    LOG("openByPath: empty path");
    //    return NULL;
    //}

    LOG("cstbase_openByPath %s\n", path);
    /*
    cstbase_device* handle = hid_open_path( path ); 

    int i = cstbase_getCacheIndexByPath( path );
    if( i >= 0 ) {  // good
        cstbase_infos[i].dev = handle;
    }
    else { // uh oh, not in cache, now what?
    }
    
    return handle;
    */
    return cstbase_open();
}

//
cstbase_device* cstbase_openBySerial(const char* serial)
{
    if( serial == NULL || strlen(serial) == 0 ) {
        LOG("openByPath: empty path");
        return NULL;
    }
    int vid = cstbase_vid();
    int pid = cstbase_pid();
    
    LOG("cstbase_openBySerial %s at vid/pid %x/%x\n", serial, vid,pid);

    /*
    wchar_t wserialstr[serialstrmax] = {L'\0'};
#ifdef _WIN32   // omg windows you suck
    swprintf( wserialstr, serialstrmax, L"%S", serial); // convert to wchar_t*
#else
    swprintf( wserialstr, serialstrmax, L"%s", serial); // convert to wchar_t*
#endif
    LOG("serialstr: '%ls' \n", wserialstr );
    cstbase_device* handle = hid_open(vid,pid, wserialstr ); 
    if( handle ) LOG("got a cstbase_device handle\n"); 

    int i = cstbase_getCacheIndexBySerial( serial );
    if( i >= 0 ) {
        LOG("good, serial was in cache\n");
        cstbase_infos[i].dev = handle;
    }
    else { // uh oh, not in cache, now what?
        LOG("uh oh, serial was not in cache\n");
    }

    return handle;
    */
    return cstbase_open();
}

//
cstbase_device* cstbase_openById( uint32_t i ) 
{ 
    if( i > cstbase_max_devices ) { // then i is a serial number not array index
        char serialstr[serialstrmax];
        sprintf( serialstr, "%X", i);  // convert to wchar_t* 
        return cstbase_openBySerial( serialstr );  
    } 
    else {
        return cstbase_openByPath( cstbase_getCachedPath(i) );
    }
}

//
cstbase_device* cstbase_open(void)
{
    int rc = usbhidOpenDevice( &static_dev, 
                               cstbase_vid(), NULL,
                               cstbase_pid(), NULL,
                               1);  // NOTE: '0' means "not using report IDs"
    LOG("cstbase_open\n");
    if( rc != USBOPEN_SUCCESS ) { 
        LOG("cannot open: \n");
    }
    return static_dev;
}

//
// FIXME: search through cstbases list to zot it too?
void cstbase_close( cstbase_device* dev )
{
    if( dev != NULL ) {
        cstbase_clearCacheDev(dev); // FIXME: hmmm 
        usbhidCloseDevice(dev);
    }
    dev = NULL;
    //hid_exit();// FIXME: this cleans up libusb in a way that hid_close doesn't
}

//
int cstbase_write( cstbase_device* dev, void* buf, int len)
{
    int rc;
    if( dev==NULL ) {
        return -1; // CSTBASE_ERR_NOTOPEN;
    }
    if( (rc = usbhidSetReport(dev, buf, len) != 0) ){
        LOG( "cstbase_write error: %s\n", cstbase_error_msg(rc));
    }

    return rc;
}

// len should contain length of buf
// after call, len will contain actual len of buf read
int cstbase_read( cstbase_device* dev, void* buf, int len)
{
    if( dev==NULL ) {
        return -1; // CSTBASE_ERR_NOTOPEN;
    }
    int rc = cstbase_write( dev, buf, len); // FIXME: check rc
    if((rc = usbhidGetReport(dev, 1, (char*)buf, &len)) != 0) {
        LOG("error reading data: %s\n", cstbase_error_msg(rc));
    }
    return rc;
}


