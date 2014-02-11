

#include <stdio.h>

#include "cstbase-lib.h"


//
int main(int argc, char** argv)
{
    printf("cstbase-timeset: ");

    cstbase_device* dev = cstbase_open();

    if( dev == NULL ) { 
        printf("base station not connected\n");
        return 1;
    }

    cstbase_setTime( dev );

    cstbase_close(dev);

    printf("time set done\n");

    return 0;
}
