

/*
 * File:   main.c
 * Author: Dave Vondle, Central Standard Timing
 *
 * Created on June 4, 2013, 3:21 PM
 *
 *
 */


#include <xc.h>
#include <stdio.h>
#include <stdlib.h>

/********CONFIGURATION**********/
// Brown-out Reset Enable: Brown-out Reset enabled
// Internal/External Switchover Mode: Internal/External Switchover Mode is disabled
// Oscillator Selection Bits: Internal INTOSC
// Fail-Safe Clock Monitor Enable: Fail-Safe Clock Monitor is disabled
// MCLR Pin Function Select: MCLR/VPP pin function is digital input, off
// Watchdog Timer Enable: WDT enabled
// Watchdog Timer Loop:1 sec (01011)
// Flash Program Memory Code Protection: Program memory code protection is disabled
// Power-up Timer Enable: PWRT disabled
// Clock Out Enable: CLKOUT function is disabled. I/O or oscillator function on the CLKOUT pin
#pragma config  BOREN = ON, IESO = OFF, FOSC = INTOSC, FCMEN = OFF, MCLRE = OFF, \
                WDTE = ON, CP = OFF, PWRTE = OFF, CLKOUTEN = OFF \
                USBLSCLK=48MHz, LPBOR=OFF, \
                CPUDIV=NOCLKDIV, PLLEN=ENABLED, WRT=OFF, \
                STVREN=ON, PLLMULT=3x, BORV=HI, LVP=OFF


#define _XTAL_FREQ 48000000

bit plusButtonPressed = 0;
bit minusButtonPressed = 0;
bit modeButtonPressed = 0;
bit allButtonsPressed = 0;
bit LEDdirection = 0; //1=up, 0=down
int timeOutCounter=0;
int batteryCounter;
bit batteryCharged=0;
unsigned char udata;

void putByteUSART(unsigned char data);
void putrsUSART(const char *data);
unsigned char countStepsRA3(void);
unsigned char countStepsRA4(void);



void main(void) {
    int i;

    //*** Initialisation
    // Set up clock for 48Mhz
    OSCCONbits.SCS=0;
    OSCCONbits.IRCF=15;
    OSCCONbits.SPLLMULT = 1;

    OPTION_REGbits.nWPUEN=0; // clear this bit to enable weak pull ups

    //Configure LED
    
    TRISCbits.TRISC3 = 1; //disable
    PWM2EN=0;
    PWM2OE=0;
    PWM2OUT=0;
    PWM2POL=0;

    PR2=0xFF;
    PWM2DCH=0x00;
    PWM2DCL=0x00;

    PWM2EN=1;
    PWM2OE=1;

    TMR2IF=0;
    T2CKPS0=0;
    T2CKPS1=0;
    TMR2ON=1;
    
    TRISCbits.TRISC3 = 0; //enable

    //Configure Output
    TRISCbits.TRISC2 = 0;
    LATCbits.LATC2 = 0; // Switch on output to 12V

    ANSA4=0;
    TRISA = 0b00111000; // Set RA<5:3> as inputs
    WPUA = 0b00111000; // Weak pull ups on buttons

    //Interrupt Timer
    /*
     * frequency=48mhz
     Time Period = 0.083uS
    Prescaller Period = .083 x 8 = 51.2uS
    Overflow Period   = 51.2 x 256 = 13107.2 uS
                      = 0.04369067 sec

     */

    //Setup Timer0
    OPTION_REGbits.PS0=1;
    OPTION_REGbits.PS1=1;
    OPTION_REGbits.PS1=1;

    PSA=0;      //Timer Clock Source is from Prescaler
    T0CS=0;     //Prescaler gets clock from FCPU (48MHz)

    TMR0IE=0;   //Disable TIMER0 Interrupt
    PEIE=1;     //Enable Peripheral Interrupt
    GIE=1;      //Enable INTs globally

    //Set up UART                               2048 baud is what the watch can do with the clock crystal timer only.
    BRGH=0;					//No Divider
    BRG16=1;                                    //16 bit mode
    SPBRGL=184;                              //  2048 @ 48MHz, SPBRG = (48MHz/(16*BAUD_RATE))-1 = 1463.8;
    SPBRGH=5;                                   //256 * 5 + 184 = 1464

    //Set Up Transmitter
    TXEN=1;
    SYNC=0;
    SPEN=1;

    //Set Up Reciever
    CREN = 1;
    RCIE = 1;

    PORTCbits.RC2=0; //12V out

    //Watchdog Timer
    WDTCON = 0x25; //256 seconds (max)

    while(1){
        i=0;
        while(PORTAbits.RA5==0 && PORTAbits.RA3==1 && PORTAbits.RA4==1){//only plus pressed
            i++;
            unsigned char extraPresses;
            if(i<=10){// 5 sec
                putByteUSART('u'); //up one
            }else if(i>22){
                putByteUSART('P'); //up hour
                __delay_ms(250);
            }else if(i>10){
                putByteUSART('U'); //up ten
                __delay_ms(125);
            }
            extraPresses = countStepsRA3();//returns extra button presses the user inputs before the watch has the chance to update
            if (extraPresses==1){
                putByteUSART('u');
            }else if(extraPresses==2){
                putByteUSART('a');
            }else if(extraPresses>2){
                putByteUSART('b');
            }
        }

        while(PORTAbits.RA5==1 && PORTAbits.RA3==0 && PORTAbits.RA4==1){//only minus pressed
            i++;
            unsigned char extraPresses;
            if(i<=10){// 5 sec
                putByteUSART('d'); //up one
            }else if(i>22){
                putByteUSART('W'); //up ten
                __delay_ms(250);
            }else if(i>10){
                putByteUSART('D'); //up hour
                __delay_ms(125);
            }
            extraPresses = countStepsRA4();//returns extra button presses the user inputs before the watch has the chance to update
            if (extraPresses==1){
                putByteUSART('d');
            }else if(extraPresses==2){
                putByteUSART('z');
            }else if(extraPresses>2){
                putByteUSART('y');
            }
        }

        if(PORTAbits.RA4==0 && !modeButtonPressed){ //mode pressed, changes between 12 and 24 hour mode
            modeButtonPressed=1;
            putByteUSART('M');
            __delay_ms(10);   //debounce

        }else if(PORTAbits.RA4==1 && modeButtonPressed){
            modeButtonPressed=0;
            __delay_ms(10);   //debounce
        }

       

        // easter egg to swap color
        while(PORTAbits.RA5==0 && PORTAbits.RA3==0 && PORTAbits.RA4==0){
            __delay_ms(1);
            if(i>5000){// 5 sec
                putByteUSART('S'); //swap colors
                i=0;
            }
            i++;
        }

        //Diagnostic Mode
        while(PORTAbits.RA5==0 && PORTAbits.RA3==0 && PORTAbits.RA4==1){
            __delay_ms(1);
            if(i>5000){// 5 sec
                putByteUSART('X'); //diagnostic mode
                PORTCbits.RC2=1; //5V out
                __delay_ms(1000); //delay, so an accurate battery reading can be made 12V charging brings battery up to 4.15 automatically)
                __delay_ms(1000);
                __delay_ms(1000);
                PORTCbits.RC2=0; //12V out
                i=0;
            }
            i++;
        }

        

        CLRWDT();
    }
}

unsigned char countStepsRA3()
{
    int i;
    int pressCount=0;
    unsigned char buttonUp=0;
    for(i=0; i<510; i++){
        if(PORTAbits.RA5==1 && !buttonUp){ // released
            buttonUp=1;
            __delay_ms(10);   //debounce
            i+=10;
        }else if(PORTAbits.RA5==0 && buttonUp){ // pressed
            buttonUp=0;
            pressCount++;
            __delay_ms(10);   //debounce
            i+=10;
        }
        __delay_ms(1);
    }
    return pressCount;
}

unsigned char countStepsRA4()
{
    int i;
    int pressCount=0;
    unsigned char buttonUp=0;
    for(i=0; i<510; i++){
        if(PORTAbits.RA3==1 && !buttonUp){ // released
            buttonUp=1;
            __delay_ms(10);   //debounce
            i+=10;
        }else if(PORTAbits.RA3==0 && buttonUp){ // pressed
            buttonUp=0;
            pressCount++;
            __delay_ms(10);   //debounce
            i+=10;
        }
        __delay_ms(1);
    }
    return pressCount;
}

void interrupt ISR(){
    //Check if it is TMR0 Overflow ISR
    
   if(TMR0IE && TMR0IF)
   {
        //TMR0 Overflow ISR

       //Code for LED pulse
       if(!batteryCharged){
           if(LEDdirection){
               PWM2DCH++;
               if(PWM2DCH==0xFF){
                   LEDdirection=0;
               }

           }else{
               PWM2DCH--;
               if(PWM2DCH==0x00){
                   LEDdirection=1;
               }
           }
       }


      if(batteryCounter==1000){//time to check battery
          PORTCbits.RC2=1; //5V out
          putByteUSART('T'); // check voltage
      }else if(batteryCounter>=2000){
          PORTCbits.RC2=0; //12V out
          batteryCounter=0;
      }


      if(timeOutCounter>=500)//timed out
      {
         //LATCbits.LATC3 = 0; //turn off LED
         PWM2DCH=0x00;
         TMR0IE=0;
         timeOutCounter=0;
      }

      timeOutCounter++;//Increment Over Flow Counter
      batteryCounter++;//Increment Over Flow Counter
      TMR0IF=0; //Clear Flag

   }else if(RCIE && RCIF){ //recieve a byte
        if(RCREG=='H'){ //it said "Hi"
            if(!TMR0IE){
                TMR0IE=1;   //Enable TIMER0 Interrupt
                //LATCbits.LATC3 = 1;
                batteryCharged=0;
                PWM2DCH=0xFF;
            }
            timeOutCounter=0;
        }else if(RCREG=='B'){
            batteryCharged=1;
            PWM2DCH=0x44;
            PORTCbits.RC2=0; //once we get the answer back we can bump the voltage back up to 12V out
        }
        else if(RCREG=='N'){
            batteryCharged=0;
            //PWM2DCH=0x44;       
            PORTCbits.RC2=0; //12V out
        }
   }
}


void putByteUSART(unsigned char data)
{
    while(!(TXSTA & 0x02));
    TXREG = data;
    timeOutCounter=0; //because it takes time on the watch side to process, and it can time out
}

void putrsUSART(const char *data)
{
    do
    {
        while(!(TXSTA & 0x02));
        TXREG = *data;
    }while( *data++ );
}


