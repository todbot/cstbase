

void uart_init()
{
    //Set up UART         2048 baud is what the watch can do with the clock crystal timer only.
    BRGH=0;	      // No Divider
    BRG16=1;      // 16 bit mode
    SPBRGL=184;   //  2048 @ 48MHz, SPBRG = (48MHz/(16*BAUD_RATE))-1 = 1463.8;
    SPBRGH=5;     //256 * 5 + 184 = 1464

    //Set Up Transmitter
    TXEN=1;
    SYNC=0;
    SPEN=1;

    //Set Up Reciever
    CREN = 1;
    RCIE = 1;
}



//
void uart_putByte(unsigned char data)
{
    while(!(TXSTA & 0x02));
    TXREG = data;
    timeOutCounter=0; //because it takes time on the watch side to process, and it can time out
}

//
void uart_putrs(const char *data)
{
    do
    {
        while(!(TXSTA & 0x02));
        TXREG = *data;
    }while( *data++ );
}
