#ifndef _UART_FUNCS_H
#define _UART_FUNCS_H

#ifndef UART_BAUD_RATE
#define UART_BAUD_RATE 2048
#endif


// Set up UART         2048 baud is what the watch can do with the clock crystal timer only.
// Note, must also put "if( RCIE && RCIF ) { ... }" in interrupt function to catch received bytes
void uart_init()
{
    BRGH=0;	      // No Divider
    BRG16=1;      // 16 bit mode

#if UART_BAUD_RATE == 2048
    SPBRGL=184;   //  2048 @ 48MHz, SPBRG = (48MHz/(16*BAUD_RATE))-1 = 1463.8;
    SPBRGH=5;     //256 * 5 + 184 = 1464
#elif UART_BAUD_RATE == 2400
    SPBRGL = 225;
    SPBRGH = 4;
#warning "UART_BAUD_RATE=2400"
#else 
#warning "UART_BAUD_RATE not set. support values are 2048 and 2400"
#endif

    //Set Up Transmitter
    TXEN=1;
    SYNC=0;
    SPEN=1;

    //Set Up Reciever
    CREN = 1;
    RCIE = 1;
}



// put a char or byte
void uart_putc(unsigned char data)
{
    while(!TRMT);  // wait for empty
    TXREG = data;
    timeOutCounter=0; //because it takes time on the watch side to process, and it can time out
}

// put a null-terminated string
void uart_puts(const char *data)
{
    /*
    do
    {
        while(!TRMT);
        TXREG = *data;
    } while( *data++ );
    */
    while (*data) 
        uart_putc(*data++);
}

#endif
