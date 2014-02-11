/* 
 * cstbase-tool.c -- command-line tool for controlling CST Base Station
 *
 * 2014, Tod E. Kurt, http://todbot.com/blog/ , http://thingm.com/
 *
 *
 * Set time to current system time:
 * ./cstbase-tool --settime
 *
 * Set time on all connected CSTs to current system time:
 * ./cstbase-tool --settime  --all
 *
 * Set time to arbitrary time:
 * ./cstbase-tool --settime 12:34:56
 *
 * Get firmware verison of base station:
 * ./cstbase-tool --version
 *
 *
 */

#include <stdio.h>
#include <stdarg.h>    // vararg stuff
#include <string.h>    // for memset(), strcmp(), et al
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>    // for getopt_long()
#include <time.h>
#include <unistd.h>    // getuid()

#include "cstbase-lib.h"

#define CSTBASE_TOOL_VERSION "1.0-0001"   // FIXME: how to pull github rev?

int delayMillis = 500;
int numDevicesToUse = 1;
int ledn = 0;

cstbase_device* dev;
uint32_t  deviceIds[cstbase_max_devices];

uint8_t cmdbuf[cstbase_buf_size]; 

int verbose;
int quiet=0;


// --------------------------------------------------------------------------- 

//
static void usage(char *myName)
{
    fprintf(stderr,
"Usage: \n"
"  %s <cmd> [options]\n"
"where <cmd> is one of:\n"
"  --settime                   Set time to current localtime\n"
"  --settimeto HH:MM           Set time to specified HH:MM time\n"
"  --buttons                   Get base station button states\n"
"  --send                      Send byte sequence to watch\n"
"  --get                       Read last received byte from watch\n"
"  --list                      List connected CST Base devices \n"
" Nerd functions: (not used normally) \n"
"  --version                   Display cstbase-tool & basestation version info \n"
"and [options] are: \n"
"  -d dNums --id all|deviceIds Use these cstbase ids (from --list) \n"
"  -q, --quiet                 Mutes all stdout output (supercedes --verbose)\n"
"  -v, --verbose               verbose debugging msgs\n"
"\n"
"Examples \n"
"  cstbase-tool --settime --all   # set all connected devices to current time\n"
"  cstbase-tool --settimeto 6:11  # set time to 6:11\n"
"  cstbase-tool --buttons         # get button state of base station\n"
"\n"
//"  -t ms,   --delay=millis     Set millisecs between events (default 500)\n"
            ,myName);
}

// local states for the "cmd" option variable
enum { 
    CMD_NONE = 0,
    CMD_LIST,
    CMD_VERSION,
    CMD_SETTIME,
    CMD_SETTIMETO,
    CMD_BUTTONS,
    CMD_SENDCHARS,
    CMD_SENDBYTES,
    CMD_GETCHAR,
    CMD_GETBYTE,
    CMD_TESTTEST,
};


void msg(char* fmt, ...);
void hexdump(uint8_t *buffer, int len);
int hexread(uint8_t *buffer, char *string, int buflen);

//
int main(int argc, char** argv)
{
    int openall = 0;
    int16_t arg=0;

    int  rc;
    char serialnumstr[serialstrmax] = {'\0'}; 

    uint16_t seed = time(NULL);
    srand(seed);
    memset( cmdbuf, 0, sizeof(cmdbuf));

    static int cmd  = CMD_NONE;

    // parse options
    int option_index = 0, opt;
    char* opt_str = "aqvhm:t:d:U:u:l:";
    static struct option loptions[] = {
        {"all",        no_argument,       0,      'a'},
        {"verbose",    optional_argument, 0,      'v'},
        {"quiet",      optional_argument, 0,      'q'},
        {"millis",     required_argument, 0,      'm'},
        {"delay",      required_argument, 0,      't'},
        {"id",         required_argument, 0,      'd'},
        {"help",       no_argument,       0,      'h'},
        {"list",       no_argument,       &cmd,   CMD_LIST },
        {"version",    no_argument,       &cmd,   CMD_VERSION },
        {"settime",    no_argument,       &cmd,   CMD_SETTIME },
        {"settimeto",  required_argument, &cmd,   CMD_SETTIMETO },
        {"buttons",    no_argument,       &cmd,   CMD_BUTTONS },
        {"send",       required_argument, &cmd,   CMD_SENDCHARS },
        {"sendbytes",  required_argument, &cmd,   CMD_SENDBYTES },
        {"get",        no_argument,       &cmd,   CMD_GETCHAR },
        {"getbyte",    no_argument,       &cmd,   CMD_GETBYTE },
        {"testtest",   no_argument,       &cmd,   CMD_TESTTEST },
        {NULL,         0,                 0,      0}
    };
    while(1) {
        opt = getopt_long(argc, argv, opt_str, loptions, &option_index);
        if (opt==-1) break; // parsed all the args
        switch (opt) {
         case 0:             // deal with long opts that have no short opts
            switch(cmd) { 
            case CMD_SETTIMETO:
                hexread(cmdbuf, optarg, sizeof(cmdbuf));  // cmd w/ hexlist arg
                break;
            case CMD_SENDCHARS:
                strncpy((char*)cmdbuf, optarg, sizeof(cmdbuf));
                cmdbuf[ strlen(optarg) ] = '\0';
                break;
            case CMD_SENDBYTES:
                hexread(cmdbuf, optarg, sizeof(cmdbuf));  // cmd w/ hexlist arg
                break;
            } // switch(cmd)
            break;
        case 'a':
            openall = 1;
            break;
        case 't':
            delayMillis = strtol(optarg,NULL,10);
            break;
        case 'q':
            if( optarg==NULL ) quiet++;
            else quiet = strtol(optarg,NULL,0);
            break;
        case 'v':
            if( optarg==NULL ) verbose++;
            else verbose = strtol(optarg,NULL,0);
            if( verbose > 3 ) {
                fprintf(stderr,"going REALLY verbose\n");
            }
            break;
        case 'd':
            if( strcmp(optarg,"all") == 0 ) {
                numDevicesToUse = 0; //cstbase_max_devices;
                for( int i=0; i< cstbase_max_devices; i++) {
                    deviceIds[i] = i;
                }
            } 
            else if( strlen(optarg) == 8 ) { //  
                deviceIds[0] = strtol( optarg, NULL, 16);
                numDevicesToUse = 1;
                //sprintf( serialnumstr, "%s", optarg);  // strcpy
            } 
            else {
                numDevicesToUse = hexread((uint8_t*)deviceIds,optarg,sizeof(deviceIds));
            }
            break;
        case 'h':
            usage( "cstbase-tool" );
            exit(1);
            break;
        }
    }

    if(argc < 2){
        usage( "cstbase-tool" );
        exit(1);
    }

    // get a list of all devices and their paths
    int count = cstbase_enumerate();

    if( cmd == CMD_VERSION ) { 
        char verbuf[40] = "";
        if( count ) { 
            dev = cstbase_openById( deviceIds[0] );
            rc = cstbase_getVersion(dev);
            cstbase_close(dev);
            sprintf(verbuf, ", fw version: %d", rc);
        }
        msg("cstbase-tool version: %s %s\n",CSTBASE_TOOL_VERSION,verbuf);
        exit(0);
    }

    
    if( count == 0 ) {
        msg("no CST Base devices found\n");
        exit(1);
    }

    if( numDevicesToUse == 0 ) numDevicesToUse = count; 

    if( verbose ) { 
        printf("deviceId[0] = %X\n", deviceIds[0]);
        printf("cached list:\n");
        for( int i=0; i< count; i++ ) { 
            printf("%d: serial: '%s' '%s'\n", 
                   i, cstbase_getCachedSerial(i), cstbase_getCachedPath(i) );
        }
    }

    // actually open up the device to start talking to it
    if(verbose) printf("openById: %X\n", deviceIds[0]);
    dev = cstbase_openById( deviceIds[0] );

    if( dev == NULL ) { 
        msg("cannot open CST Base, bad id or serial number\n");
        exit(1);
    }

    //
    // begin command processing
    //
    if( cmd == CMD_LIST ) { 
        printf("CST Base list: \n");
        for( int i=0; i< count; i++ ) {
            printf("id:%d - serialnum:%s \n", i, cstbase_getCachedSerial(i) );
        }
        #ifdef USE_HIDDATA
        printf("(Listing not supported in HIDDATA builds)\n"); 
        #endif
    }
    else if( cmd == CMD_VERSION ) { 
        rc = cstbase_getVersion(dev);
        msg("cstbase-tool: firmware version: ");
        printf("%d\n",rc);
    }
    else if( cmd == CMD_SETTIME ) { 
        cstbase_close(dev); // close global device, open as needed
        uint8_t h,m,s;
        cstbase_getLocalTime(&h,&m,&s);
        for( int i=0; i< numDevicesToUse; i++ ) {
            dev = cstbase_openById( deviceIds[i] );
            if( dev == NULL ) continue;
            msg("set dev:%X to localtime %2.2d:%2.2d\n", deviceIds[i],h,m );
            rc = cstbase_setTime(dev);
            cstbase_close( dev );
        }
    }
    else if( cmd == CMD_SETTIMETO ) {
        // FIXME: not done yet
        uint8_t hours = cmdbuf[0];
        uint8_t mins  = cmdbuf[1];
        uint8_t secs  = cmdbuf[2];
        msg("setting time to ");
        printf("%2.2d:%2.2d\n", hours, mins);
        rc = cstbase_setTimeTo( dev, hours, mins, secs );
    }
    else if( cmd == CMD_BUTTONS ) {
        rc = cstbase_getButtons(dev);
        msg("cstbase-tool: button state: ");
        printf("0x%x\n",rc);
    }
    else if( cmd == CMD_SENDCHARS ) { 
        msg("send: %s\n", cmdbuf);
        rc = cstbase_sendBytesToWatch( dev, cmdbuf, strlen((char*)cmdbuf) );
    }
    else if( cmd == CMD_SENDBYTES ) { 
        msg("send bytes: %x\n", cmdbuf[0]);
        rc = cstbase_sendBytesToWatch( dev, cmdbuf, 1 );  // FIXME: only sends 1st byte
    }
    else if( cmd == CMD_GETCHAR ) { 
        rc = cstbase_getByteFromWatch( dev );
        msg("get char: %c", rc );
    } 
    else if( cmd == CMD_GETBYTE ) { 
        rc = cstbase_getByteFromWatch( dev );
        msg("get byte: ");
        printf("0x%x\n",rc);
    }


    return 0;
}


// printf that can be shut up
void msg(char* fmt, ...)
{
    va_list args;
    va_start(args,fmt);
    if( !quiet ) {
        vprintf(fmt,args);
    }
    va_end(args);
}

// take an array of bytes and spit them out as a hex string
void hexdump(uint8_t *buffer, int len)
{
    int     i;
    FILE    *fp = stdout;
    
    for(i = 0; i < len; i++){
        if(i != 0){
            if(i % 16 == 0){
                fprintf(fp, "\n");
            }else{
                fprintf(fp, " ");
            }
        }
        fprintf(fp, "0x%02x", buffer[i] & 0xff);
    }
    if(i != 0)
        fprintf(fp, "\n");
}

// parse a comma-delimited string containing numbers (dec,hex) into a byte arr
int hexread(uint8_t *buffer, char *string, int buflen)
{
    char    *s;
    int     pos = 0;
    memset(buffer,0,buflen);  // bzero() not defined on Win32?
    while((s = strtok(string, ", :")) != NULL && pos < buflen){
        string = NULL;
        buffer[pos++] = (char)strtol(s, NULL, 0);
    }
    return pos;
}

//---------------------------------------------------------------------------- 
/*
  TBD: replace printf()s with something like this
void logpri(int loglevel, char* fmt, ...)
{
    if( loglevel < verbose ) return;
    va_list ap;
    va_start(ap,fmt);
    vprintf(fmt,ap);
    va_end(ap);
}
*/
