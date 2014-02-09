/* 
 * cstbase-mini-tool -- minimal command-line tool for controlling CST Base Stations
 *                     
 * Will work on small unix-based systems that have just libusb-0.1.4
 * No need for pthread & iconv, which is needed for hidapi-based tools
 * 
 * Known to work on:
 * - Ubuntu Linux
 * - Mac OS X 
 * - TomatoUSB WRT / OpenWrt / DD-WRT
 *
 * 2012, Tod E. Kurt, http://todbot.com/blog/ , http://thingm.com/
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>    // for memset() et al
#include <stdint.h>    // for uint8_t
#include <unistd.h>    // for usleep()

#include "hiddata.h"

// taken from cstbase/hardware/firmware/usbconfig.h
#define IDENT_VENDOR_NUM        0x27B8
#define IDENT_PRODUCT_NUM       0x4242

#define cstbase_buf_size  9
#define cstbase_report_id 1


int millis = 100;

int delayMillis = 1000;

const int cstbase_debug = 2;


int cstbase_open(usbDevice_t **dev);
char *cstbase_error_msg(int errCode);
void cstbase_close(usbDevice_t *dev);
int cstbase_setRGB(usbDevice_t *dev, uint8_t r, uint8_t g, uint8_t b );
int cstbase_getVersion(usbDevice_t* dev);
void cstbase_sleep(uint16_t millis);

static int  hexread(char *buffer, char *string, int buflen);

//
static void usage(char *myName)
{
    fprintf(stderr, "usage:\n");
    fprintf(stderr, "  %s blink [<num>] \n", myName);
    fprintf(stderr, "  %s version \n", myName);
    fprintf(stderr, "  %s rgb <red>,<green>,<blue> \n", myName);
    //fprintf(stderr, "  %s read\n", myName);
    //fprintf(stderr, "  %s write <listofbytes>\n", myName);
}

//
int main(int argc, char **argv)
{
    usbDevice_t *dev;
    int         rc;
    
    char argbuf[16];  
    
    if(argc < 2) {
        usage(argv[0]);
        exit(1);
    }
    char* cmd = argv[1];

    if( cstbase_open(&dev) ) {
        fprintf(stderr, "error: couldn't open cstbase\n");
        exit(1);
    }

    if( strcasecmp(cmd, "version") == 0 ) { 
        rc = cstbase_getVersion(dev);
        printf("verison: %d\n", rc);
    }
    else if( strcasecmp(cmd, "blink") == 0 ) {
        if( argc < 3 ) { 
            argbuf[0] = 3;  // blink 3 times if none specified
        } else {
            hexread(argbuf, argv[2], sizeof(argbuf));
        }

        uint8_t v = 0;
        for( int i=0; i< argbuf[0]*2; i++ ) {
            uint8_t r = v;
            uint8_t g = v;
            uint8_t b = v;
            printf("cmd:%s rgb:%2.2x,%2.2x,%2.2x in %d ms\n",cmd, r,g,b,millis);
            rc = cstbase_setRGB( dev, v,v,v );
            if( rc ) { // on error, do something
                printf("error on setRGB\n");
            }
            v = (v) ? 0 : 255;
            usleep(millis * 1000 ); // sleep milliseconds
        }
    }
    else { 
        usage(argv[0]);
        exit(1);
    }
    /*
    else if(strcasecmp(cmd, "read") == 0){
        int len = sizeof(buffer);
        if((rc = usbhidGetReport(dev, 0, buffer, &len)) != 0){
            fprintf(stderr,"error reading data: %s\n",cstbase_error_msg(rc));
        }else{
            hexdump(buffer + 1, sizeof(buffer) - 1);
        }
    }
    else if(strcasecmp(cmd, "write") == 0){
        int i, pos;
        memset(buffer, 0, sizeof(buffer));
        for(pos = 1, i = 2; i < argc && pos < sizeof(buffer); i++){
            pos += hexread(buffer + pos, argv[i], sizeof(buffer) - pos);
        }

        // add a dummy report ID 
        if((rc = usbhidSetReport(dev, buffer, sizeof(buffer))) != 0) 
            fprintf(stderr,"error writing data: %s\n",cstbase_error_msg(rc));

    }
    else 
    */

}


/**
 * Open up a CST Base for transactions.
 * returns 0 on success, and opened device in "dev"
 * or returns non-zero error that can be decoded with cstbase_error_msg()
 * FIXME: what happens when multiple are plugged in?
 */
int cstbase_open(usbDevice_t **dev)
{
    return usbhidOpenDevice(dev, 
                            IDENT_VENDOR_NUM,  NULL,
                            IDENT_PRODUCT_NUM, NULL,
                            1);  // NOTE: '0' means "not using report IDs"
}

/**
 * Close a Cstbase 
 */
void cstbase_close(usbDevice_t *dev)
{
    usbhidCloseDevice(dev);
}


//
int cstbase_getVersion(usbDevice_t *dev)
{
    char buf[cstbase_buf_size] = {cstbase_report_id, 'v' };
    int len = sizeof(buf);
    int rc;

    if( (rc = usbhidSetReport(dev, buf, len)) != 0) {
        fprintf(stderr,"getVersion: error writing: %s\n",cstbase_error_msg(rc));
        return rc;
    }
    //cstbase_sleep( 50 ); //FIXME:

    if((rc = usbhidGetReport(dev, 1, (char*)buf, &len)) != 0) {
        fprintf(stderr, "getVersion: error reading data: %s\n", cstbase_error_msg(rc));
        return rc;
    }        
    rc = ((buf[3]-'0') * 100) + (buf[4]-'0'); 
    // rc is now version number or error  
    // FIXME: we don't know vals of errcodes
    return rc;
}

/**
 *
 */
int cstbase_setRGB(usbDevice_t *dev, uint8_t r, uint8_t g, uint8_t b )
{
    char buf[cstbase_buf_size];
    int err;

    if( dev==NULL ) {
        return -1; // CSTBASE_ERR_NOTOPEN;
    }

    buf[0] = 1;
    buf[1] = 'n';
    buf[2] = r;
    buf[3] = g;
    buf[4] = b;
    
    if( (err = usbhidSetReport(dev, buf, sizeof(buf))) != 0) {
        fprintf(stderr,"setRGB: error writing: %s\n",cstbase_error_msg(err));
    }
    return err;  // FIXME: remove fprintf
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
static int  hexread(char *buffer, char *string, int buflen)
{
char    *s;
int     pos = 0;

    while((s = strtok(string, ", ")) != NULL && pos < buflen){
        string = NULL;
        buffer[pos++] = (char)strtol(s, NULL, 0);
    }
    return pos;
}

