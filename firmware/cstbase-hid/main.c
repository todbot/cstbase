/********************************************************************
 * CST Base station firmware
 *
 * For Microchip PIC16F1454/5
 * - http://www.microchip.com/wwwproducts/Devices.aspx?dDocName=en556969
 *
 * Uses Microchip's USB Framework (aka "MCHPFSUSB Library") 
 * (recently renamed "MLA" - Microchip Library for Applications)
 * http://www.microchip.com/pagehandler/en-us/devtools/mla/
 * This firmware is built against the 2013-06-15 "Legacy MLA"
 *
 * Note: to compile, must have the Microchip Application Library frameworks
 * symlinked in the same directory level as the "cstbase-hid" directory.
 * e.g. if you installed the framework in ~/microchip_solutions_v2013-06-15/Microchip
 * then do: "ln -s ~/microchip_solutions_v2013-06-15/Microchip Microchip"
 *
 *
 * A modified version of "usb_device.c" from MCHPFSUSB is needed for 
 * RAM-based serial numbers and is provided.
 *
 * PIC16F1454 pins used:
 *
 * QFN pkg                                                            DIP pkg
 * -------                                                            -------
 * pin 1  - RA5            - input : "+" button              - RA5 -  pin 2
 * pin 2  - RA4            - input : "12/24 hr" button       - RA4 -  pin 3
 * pin 3  - RA3/MCLR/VPP   - input : "-" button              - RA3 -  pin 4 
 * pin 4  - RC5            - input : UART RX, to watch       - RC5 -  pin 5
 * pin 5  - RC4            - output: UART TX, to watch       - RC4 -  pin 6
 * pin 6  - RC3            - output: LED                     - RC3 -  pin 7
 * pin 7  - RC2            - output: 12V or 5V , to watch    - RC2 -  pin 8
 * pin 8  - RC1/ICSPCLK    - n/c                             - RC1 -  pin 9
 * pin 9  - RC0/ICSPDAT    - n/c                             - RC0 -  pin 10
 * pin 10 - VUSB3V3        - 100nF cap to Gnd                -VUSB -  pin 11
 * pin 11 - RA1/D-/ICSPC   - D-  on USB                      - RA1 -  pin 12
 * pin 12 - RA0/D+/ICSPD   - D+  on USB                      - RA0 -  pin 13
 * pin 13 - VSS            - Gnd on USB                      - VSS -  pin 14
 * pin 14 - n/c            - n/c                             -     -  -
 * pin 15 - n/c            - n/c                             -     -  -
 * pin 16 - VDD            - +5V on USB                      - VDD -  pin 1
 *
 *
 * 2014, Tod E. Kurt, http://thingm.com/
 *
 ********************************************************************/

#ifndef MAIN_C
#define MAIN_C

#include <xc.h>
#include "./USB/usb.h"
#include "HardwareProfile.h"
#include "./USB/usb_function_hid.h"

#include <stdint.h>


#if 1 // to fix stupid IDE error issues with __delay_ms
#ifndef _delay_ms(x)
#define _delay_ms(x) __delay_ms(x)
#endif
#ifndef _delay_us(x)
#define _delay_us(x) __delay_us(x)
#endif
#endif

//------------ chip configuration ------------------------------------

#pragma config FOSC     = INTOSC
#pragma config WDTE     = OFF
#pragma config PWRTE    = ON
#pragma config MCLRE    = OFF
#pragma config CP       = OFF
#pragma config BOREN    = ON
#pragma config CLKOUTEN = OFF
#pragma config IESO     = OFF
#pragma config FCMEN    = OFF

#pragma config WRT      = OFF
#pragma config CPUDIV   = NOCLKDIV
#pragma config USBLSCLK = 48MHz
#pragma config PLLMULT  = 3x
#pragma config PLLEN    = ENABLED
#pragma config STVREN   = ON
#pragma config BORV     = LO
#pragma config LPBOR    = OFF
#pragma config LVP      = OFF  // keep that ON when using LVP programmer

// -------------------------------------------------------------------


#define cstbase_ver_major  '1'
#define cstbase_ver_minor  '1'

#define cstbase_report_id 0x01


// serial number of this cst base station
// stored in a packed format at address 0x1FF8
const uint8_t serialnum_packed[4] @ 0x1FF8 = {0x10, 0x42, 0xca, 0xfe };

//** VARIABLES ******************************************************

#if defined( RAM_BASED_SERIALNUMBER )
RAMSNt my_RAMSN;
#endif

#if defined(_16F1459) || defined(_16F1455) || defined(_16F1454) || \
    defined(_16LF1459) || defined(_16LF1455) || defined(_16LF1454)
#define RX_DATA_BUFFER_ADDRESS @0x2050
#define TX_DATA_BUFFER_ADDRESS @0x20A0
// FIXME: the below is ugly & redundant
#define IN_DATA_BUFFER_ADDRESS 0x2050
#define OUT_DATA_BUFFER_ADDRESS (IN_DATA_BUFFER_ADDRESS + HID_INT_IN_EP_SIZE)
#define FEATURE_DATA_BUFFER_ADDRESS (OUT_DATA_BUFFER_ADDRESS + HID_INT_OUT_EP_SIZE)
#define FEATURE_DATA_BUFFER_ADDRESS_TAG @FEATURE_DATA_BUFFER_ADDRESS
#endif

uint8_t hid_send_buf[USB_EP0_BUFF_SIZE] FEATURE_DATA_BUFFER_ADDRESS_TAG;

uint8_t usbHasBeenSetup = 0;  // set in USBCBInitEP()
#define usbIsSetup (USBGetDeviceState() == CONFIGURED_STATE)

// original base station variables
//bit plusButtonPressed = 0;
//bit minusButtonPressed = 0;
bit modeButtonPressed = 0;
//bit allButtonsPressed = 0;
bit LEDdirection = 0; //1=up, 0=down
int timeOutCounter=0;
int batteryCounter;
volatile bit batteryCharged=0;
//unsigned char udata;
// new things
volatile bit timeToUpdateState=0;
volatile uint8_t lastRxByte=0;


//#define UART_BAUD_RATE 2400
#include "uart_funcs.h"


//
static void InitializeSystem(void);
void USBCBSendResume(void);
void USBHIDCBSetReportComplete(void);

static char tohex(uint8_t num);
inline void loadSerialNumber(void);
void updateState(void);
void handleKeys(void);
unsigned char countStepsRA3(void);
unsigned char countStepsRA4(void);


// ****************************************************************************
// main
//
int main(void)
{
    InitializeSystem();

    USBDeviceAttach();
    
    while (1) {
        updateState();
        handleKeys();
        CLRWDT();  // tickle watchdog
    }

} //end main


// ****************************************************************************
//
static void InitializeSystem(void)
{
    // setup oscillator
    //OSCTUNE = 0;
    //OSCCON = 0xFC;          //16MHz HFINTOSC with 3x PLL enabled (48MHz operation)
    //ACTCON = 0x90;          //Enable active clock tuning with USB
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
    //
    PR2=0xFF;
    PWM2DCH=0x00;
    PWM2DCL=0x00;
    //
    PWM2EN=1;
    PWM2OE=1;
    //
    TMR2IF=0;
    T2CKPS0=0;
    T2CKPS1=0;
    TMR2ON=1;
    //
    TRISCbits.TRISC3 = 0; //enable

    //Interrupt Timer
    // 
    //  frequency=48mhz
    // Time Period = 0.083uS
    // Prescaller Period = .083 x 8 = 51.2uS
    // Overflow Period   = 51.2 x 256 = 13107.2 uS
    //                  = 0.04369067 sec
    // Setup Timer0
    OPTION_REGbits.PS0=1;
    OPTION_REGbits.PS1=1;
    OPTION_REGbits.PS1=1;

    PSA=0;      //Timer Clock Source is from Prescaler
    T0CS=0;     //Prescaler gets clock from FCPU (48MHz)

    TMR0IE=0;   //Disable TIMER0 Interrupt

    // set up UART
    uart_init();
#if 0
    uart_puts("hello!\n");
#endif

    loadSerialNumber();

    TRISCbits.TRISC3 = 0; //enable  // FIXME: look into why he's doing this

    // Set up buttons
#if defined(_16F1459) || defined(_16F1455) || defined(_16LF1459) || defined(_16LF1455)
    ANSA4=0;  // this reg doesn't exist on 16F1454 (see datasheet TABLE 3-12)
#endif
    ANSELA = 0x00;  // Disable analog inputs  (see http://www.microchip.com/forums/m767575.aspx)
    TRISA = 0b00111000;
    WPUA = 0b00111000;     

    // Set up LEDs 
    //Configure Output
    TRISCbits.TRISC2 = 0;
    LATCbits.LATC2 = 0; // Switch on output to 12V
    TRISCbits.TRISC3 = 0; //enable

    PEIE = 1;     //Enable Peripheral Interrupt (uart receive)

    ei(); // enable global interrupts

    USBDeviceInit(); //usb_device.c.  Initializes USB module SFRs & firmware vars to known states.
    
    //Watchdog Timer
    WDTCON = 0x25; //256 seconds (max)

} //end InitializeSystem


//
// Interrupt handling routines
// PIC16F has only one interrupt vector,
// so must check all possible interrupt sources when interrupt occurs
// must get in and out of ISR quickly so we don't starve other interrupts
//
void interrupt ISRCode()
{
    //Check which interrupt flag caused the interrupt.
    //Service the interrupt
    //Clear the interrupt flag
    //Etc.
#if defined(USB_INTERRUPT)
    USBDeviceTasks();
#endif

    //TMR0 Overflow ISR
    if(TMR0IE && TMR0IF) {  // timer0 overflow enabled and it overflowed

        timeToUpdateState = 1;  // just signal so we can handle it outside the ISR

        TMR0IF=0; //Clear Flag 
    }
    
    // uart receive interrupt
    if( RCIE && RCIF ) {    // receive interrupt enabled and p recieve a byte
        lastRxByte = RCREG;
        if(RCREG=='H') { //it said "Hi"
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
            PORTCbits.RC2=0; //once we get the answer back we can bump voltage back up to 12V out
        }
        else if(RCREG=='N'){
            batteryCharged=0;
            //PWM2DCH=0x44;       
            PORTCbits.RC2=0; //12V out
        }
    }
    
}

//
// Update the base station LED and charge control
// Called in main loop, triggered by Timer0 overflows
// code from original CST-Base_Station-1454
//
void updateState(void)
{
    if( !timeToUpdateState ) return;

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
    
    if(batteryCounter==1000){ //time to check battery
        PORTCbits.RC2=1; //5V out
        uart_putc('T'); // check voltage
    }else if(batteryCounter>=2000){
        PORTCbits.RC2=0; //12V out
        batteryCounter=0;
    }
    
    if(timeOutCounter>=500) { //timed out
        //LATCbits.LATC3 = 0; //turn off LED
        PWM2DCH=0x00;
        TMR0IE=0;
        timeOutCounter=0;
    }
    
    timeOutCounter++;//Increment Over Flow Counter
    batteryCounter++;//Increment Over Flow Counter
    timeToUpdateState = 0;
}

//
// Deal with keypresses on base station.
// Called in main loop to handle keypresses
// code from original CST-Base_Station-1454
//
void handleKeys(void)
{
    static int i=0;
    while(PORTAbits.RA5==0 && PORTAbits.RA3==1 && PORTAbits.RA4==1){//only plus pressed
        i++;
        unsigned char extraPresses;
        if(i<=10){// 5 sec
            uart_putc('u'); //up one
        }else if(i>22){
            uart_putc('P'); //up hour
            _delay_ms(250);
        }else if(i>10){
            uart_putc('U'); //up ten
            _delay_ms(125);
        }
        extraPresses = countStepsRA3();//returns extra button presses the user inputs before the watch has the chance to update
        if (extraPresses==1){
            uart_putc('u');
        }else if(extraPresses==2){
            uart_putc('a');
        }else if(extraPresses>2){
            uart_putc('b');
        }
    }
    
    while(PORTAbits.RA5==1 && PORTAbits.RA3==0 && PORTAbits.RA4==1){//only minus pressed
        i++;
        unsigned char extraPresses;
        if(i<=10){// 5 sec
            uart_putc('d'); //up one
        }else if(i>22){
            uart_putc('W'); //up ten
            _delay_ms(250);
        }else if(i>10){
            uart_putc('D'); //up hour
            _delay_ms(125);
        }
        extraPresses = countStepsRA4();//returns extra button presses the user inputs before the watch has the chance to update
        if (extraPresses==1){
            uart_putc('d');
        }else if(extraPresses==2){
            uart_putc('z');
        }else if(extraPresses>2){
            uart_putc('y');
        }
    }
    
    if(PORTAbits.RA4==0 && !modeButtonPressed){ //mode pressed, changes between 12 and 24 hour mode
        modeButtonPressed=1;
        uart_putc('M');
        _delay_ms(10);   //debounce
        
    }else if(PORTAbits.RA4==1 && modeButtonPressed){
        modeButtonPressed=0;
        _delay_ms(10);   //debounce
    }
    
    // easter egg to swap color
    while(PORTAbits.RA5==0 && PORTAbits.RA3==0 && PORTAbits.RA4==0){
        _delay_ms(1);
        if(i>5000){// 5 sec
            uart_putc('S'); //swap colors
            i=0;
        }
        i++;
    }
    
    //Diagnostic Mode
    while(PORTAbits.RA5==0 && PORTAbits.RA3==0 && PORTAbits.RA4==1){
        _delay_ms(1);
        if(i>5000){// 5 sec
            uart_putc('X'); //diagnostic mode
            PORTCbits.RC2=1; //5V out
            _delay_ms(1000); //delay, so an accurate battery reading can be made 12V charging brings battery up to 4.15 automatically)
            _delay_ms(1000);
            _delay_ms(1000);
            PORTCbits.RC2=0; //12V out
            i=0;
        }
        i++;
    }
}

//
// code from original CST-Base_Station-1454
unsigned char countStepsRA3(void)
{
    int i;
    int pressCount=0;
    unsigned char buttonUp=0;
    for(i=0; i<510; i++){
        if(PORTAbits.RA5==1 && !buttonUp){ // released
            buttonUp=1;
            _delay_ms(10);   //debounce
            i+=10;
        }else if(PORTAbits.RA5==0 && buttonUp){ // pressed
            buttonUp=0;
            pressCount++;
            _delay_ms(10);   //debounce
            i+=10;
        }
        _delay_ms(1);
    }
    return pressCount;
}

//
// code from original CST-Base_Station-1454
unsigned char countStepsRA4(void)
{
    int i;
    int pressCount=0;
    unsigned char buttonUp=0;
    for(i=0; i<510; i++){
        if(PORTAbits.RA3==1 && !buttonUp){ // released
            buttonUp=1;
            _delay_ms(10);   //debounce
            i+=10;
        }else if(PORTAbits.RA3==0 && buttonUp){ // pressed
            buttonUp=0;
            pressCount++;
            _delay_ms(10);   //debounce
            i+=10;
        }
        _delay_ms(1);
    }
    return pressCount;
}



// ------------- USB command handling ----------------------------------------

// handleMessage(char* msgbuf) -- main command router
//
// msgbuf[] is 8 bytes long
//  byte0 = report-id
//  byte1 = command
//  byte2..byte7 = args for command
//
// Available commands:
//  - Set time                  format: { 1, 'T', H,M,S, ...
//  - Send byte to watch        format: { 1, 'S', n, b, ...
//  - Receive byte from watch   format: { 1, 'R', ...
//  - 
//  - Set Base LED              format: { 1, 'l', ...
//  - Get Base Button State     format: { 1, 'b', ...
//  - Get Base Version          format: { 1, 'v', 0,0,0
//
//
void handleMessage(const char* msgbuf)
{
    // pre-load response with request, contains report id
    memcpy( hid_send_buf, msgbuf, 8 );

    uint8_t cmd = msgbuf[1];

    //
    //  Set Time                  format: { 1, 'T', H,M,S,      0,0,0 }
    //
    if(      cmd == 'T' ) {
        uint8_t H = msgbuf[2];
        uint8_t M = msgbuf[3];
        //uint8_t S = msgbuf[4];
        
        char buf[10];
        sprintf(buf, "F%2.2d:%2.2d", H,M);
     
        uart_puts( buf );  // send command to watch
        
    }
    //
    // Send bytes to watch        format: { 1, 'S', n, b1,b2,b3,b4,b5,b6 }
    //
    else if( cmd == 'S' ) { 
        uint8_t cnt = msgbuf[2];
        for( int i=0; i< cnt; i++ ) { 
            uart_putc( msgbuf[3+i] );
        }
    }
    //
    // Get last byte from watch   format: { 1, 'R', 0,0,0, 0,0,0 }
    //
    else if( cmd == 'R' ) { 
        hid_send_buf[3] = lastRxByte;
    }
    //
    // Base Station button state  format: { 1, 'b' 0,0,0, 0,0,0 }
    // 
    else if( cmd == 'b' ) {
        hid_send_buf[3] =  PORTA;  // just return all of PORTA because why not?
    }
    //
    //  Get version               format: { 1, 'v', 0,0,0,        0,0, 0 }
    //
    else if( cmd == 'v' ) {
        hid_send_buf[3] = cstbase_ver_major;
        hid_send_buf[4] = cstbase_ver_minor;
    }
    else {

    }
}

// ------------------- utility functions -----------------------------------
//
static char tohex(uint8_t num)
{
    num &= 0x0f;
    if( num<= 9 ) return num + '0';
    return num - 10 + 'A';
}

#if defined( RAM_BASED_SERIALNUMBER )
 // load the serial number from packed flash into RAM
inline void loadSerialNumber(void)
{
    for( uint8_t i=0; i< 4; i++ )  {
        uint8_t v = serialnum_packed[i];
        uint8_t c0 = tohex( v>>4 );
        uint8_t c1 = tohex( v );
        uint8_t p = 0 + (2*i);
        my_RAMSN.SerialNumber[p+0] = c0;
        my_RAMSN.SerialNumber[p+1] = c1;
    }
}
#else
#define loadSerialNumber()
#endif


// ******************************************************************************************************
// ************** USB Callback Functions ****************************************************************
// ******************************************************************************************************
// The USB firmware stack will call the callback functions USBCBxxx() in response to certain USB related
// events.  For example, if the host PC is powering down, it will stop sending out Start of Frame (SOF)
// packets to your device.  In response to this, all USB devices are supposed to decrease their power
// consumption from the USB Vbus to <2.5mA* each.  The USB module detects this condition (which according
// to the USB specifications is 3+ms of no bus activity/SOF packets) and then calls the USBCBSuspend()
// function.  You should modify these callback functions to take appropriate actions for each of these
// conditions.  For example, in the USBCBSuspend(), you may wish to add code that will decrease power
// consumption from Vbus to <2.5mA (such as by clock switching, turning off LEDs, putting the
// microcontroller to sleep, etc.).  Then, in the USBCBWakeFromSuspend() function, you may then wish to
// add code that undoes the power saving things done in the USBCBSuspend() function.

// The USBCBSendResume() function is special, in that the USB stack will not automatically call this
// function.  This function is meant to be called from the application firmware instead.  See the
// additional comments near the function.

// Note *: The "usb_20.pdf" specs indicate 500uA or 2.5mA, depending upon device classification. However,
// the USB-IF has officially issued an ECN (engineering change notice) changing this to 2.5mA for all 
// devices.  Make sure to re-download the latest specifications to get all of the newest ECNs.

/******************************************************************************
 * Function:        void USBCBSuspend(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        Call back that is invoked when a USB suspend is detected
 * Note:            None
 *****************************************************************************/
void USBCBSuspend(void)
{
    //Example power saving code.  Insert appropriate code here for the desired
    //application behavior.  If the microcontroller will be put to sleep, a
    //process similar to that shown below may be used:

    //ConfigureIOPinsForLowPower();
    //SaveStateOfAllInterruptEnableBits();
    //DisableAllInterruptEnableBits();
    //EnableOnlyTheInterruptsWhichWillBeUsedToWakeTheMicro();	//should enable at least USBActivityIF as a wake source
    //Sleep();
    //RestoreStateOfAllPreviouslySavedInterruptEnableBits();	//Preferrably, this should be done in the USBCBWakeFromSuspend() function instead.
    //RestoreIOPinsToNormal();									//Preferrably, this should be done in the USBCBWakeFromSuspend() function instead.

    //IMPORTANT NOTE: Do not clear the USBActivityIF (ACTVIF) bit here.  This bit is
    //cleared inside the usb_device.c file.  Clearing USBActivityIF here will cause
    //things to not work as intended.

}

/******************************************************************************
 * Function:        void USBCBWakeFromSuspend(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The host may put USB peripheral devices in low power
 *					suspend mode (by "sending" 3+ms of idle).  Once in suspend
 *					mode, the host may wake the device back up by sending non-
 *					idle state signalling.
 *					
 *					This call back is invoked when a wakeup from USB suspend 
 *					is detected.
 * Note:            None
 *****************************************************************************/
void USBCBWakeFromSuspend(void)
{
    // If clock switching or other power savings measures were taken when
    // executing the USBCBSuspend() function, now would be a good time to
    // switch back to normal full power run mode conditions.  The host allows
    // 10+ milliseconds of wakeup time, after which the device must be
    // fully back to normal, and capable of receiving and processing USB
    // packets.  In order to do this, the USB module must receive proper
    // clocking (IE: 48MHz clock must be available to SIE for full speed USB
    // operation).
    // Make sure the selected oscillator settings are consistent with USB
    // operation before returning from this function.
}

/********************************************************************
 * Function:        void USBCB_SOF_Handler(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The USB host sends out a SOF packet to full-speed
 *                  devices every 1 ms. This interrupt may be useful
 *                  for isochronous pipes. End designers should
 *                  implement callback routine as necessary.
 * Note:            None
 *******************************************************************/
void USBCB_SOF_Handler(void)
{
    // No need to clear UIRbits.SOFIF to 0 here.
    // Callback caller is already doing that.
}

/*******************************************************************
 * Function:        void USBCBErrorHandler(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The purpose of this callback is mainly for
 *                  debugging during development. Check UEIR to see
 *                  which error causes the interrupt.
 * Note:            None
 *******************************************************************/
void USBCBErrorHandler(void)
{
    // No need to clear UEIR to 0 here.
    // Callback caller is already doing that.

    // Typically, user firmware does not need to do anything special
    // if a USB error occurs.  For example, if the host sends an OUT
    // packet to your device, but the packet gets corrupted (ex:
    // because of a bad connection, or the user unplugs the
    // USB cable during the transmission) this will typically setv
    // one or more USB error interrupt flags.  Nothing specific
    // needs to be done however, since the SIE will automatically
    // send a "NAK" packet to the host.  In response to this, the
    // host will normally retry to send the packet again, and no
    // data loss occurs.  The system will typically recover
    // automatically, without the need for application firmware
    // intervention.

    // Nevertheless, this callback function is provided, such as
    // for debugging purposes.
}

/*******************************************************************
 * Function:        void USBCBCheckOtherReq(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        When SETUP packets arrive from the host, some
 * 					firmware must process the request and respond
 *					appropriately to fulfill the request.  Some of
 *					the SETUP packets will be for standard
 *					USB "chapter 9" (as in, fulfilling chapter 9 of
 *					the official USB specifications) requests, while
 *					others may be specific to the USB device class
 *					that is being implemented.  For example, a HID
 *					class device needs to be able to respond to
 *					"GET REPORT" type of requests.  This
 *					is not a standard USB chapter 9 request, and 
 *					therefore not handled by usb_device.c.  Instead
 *					this request should be handled by class specific 
 *					firmware, such as that contained in usb_function_hid.c.
 * Note:            None
 *******************************************************************/
void USBCBCheckOtherReq(void)
{
    USBCheckHIDRequest();
}//end

/*******************************************************************
 * Function:        void USBCBStdSetDscHandler(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The USBCBStdSetDscHandler() callback function is
 *					called when a SETUP, bRequest: SET_DESCRIPTOR request
 *					arrives.  Typically SET_DESCRIPTOR requests are
 *					not used in most applications, and it is
 *					optional to support this type of request.
 * Note:            None
 *******************************************************************/
void USBCBStdSetDscHandler(void)
{
    // Must claim session ownership if supporting this request
}//end

/*******************************************************************
 * Function:        void USBCBInitEP(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        This function is called when the device becomes
 *                  initialized, which occurs after the host sends a
 * 					SET_CONFIGURATION (wValue not = 0) request.  This 
 *					callback function should initialize the endpoints 
 *					for the device's usage according to the current 
 *					configuration.
 * Note:            None
 *******************************************************************/
void USBCBInitEP(void)
{
    //enable the HID endpoint
    USBEnableEndpoint(HID_EP, USB_IN_ENABLED | USB_OUT_ENABLED | USB_HANDSHAKE_ENABLED | USB_DISALLOW_SETUP);
    //USBEnableEndpoint(HID_EP, USB_HANDSHAKE_ENABLED | USB_DISALLOW_SETUP);
    //Re-arm the OUT endpoint for the next packet
    //USBOutHandle = HIDRxPacket(HID_EP, (BYTE*) & ReceivedDataBuffer, USB_EP0_BUFF_SIZE);
    usbHasBeenSetup++;
}

/********************************************************************
 * Function:        void USBCBSendResume(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        The USB specifications allow some types of USB
 * 					peripheral devices to wake up a host PC (such
 *					as if it is in a low power suspend to RAM state).
 *					This can be a very useful feature in some
 *					USB applications, such as an Infrared remote
 *					control	receiver.  If a user presses the "power"
 *					button on a remote control, it is nice that the
 *					IR receiver can detect this signalling, and then
 *					send a USB "command" to the PC to wake up.
 *					
 *					The USBCBSendResume() "callback" function is used
 *					to send this special USB signalling which wakes 
 *					up the PC.  This function may be called by
 *					application firmware to wake up the PC.  This
 *					function will only be able to wake up the host if
 *                  all of the below are true:
 *					
 *					1.  The USB driver used on the host PC supports
 *						the remote wakeup capability.
 *					2.  The USB configuration descriptor indicates
 *						the device is remote wakeup capable in the
 *						bmAttributes field.
 *					3.  The USB host PC is currently sleeping,
 *						and has previously sent your device a SET 
 *						FEATURE setup packet which "armed" the
 *						remote wakeup capability.   
 *
 *                  If the host has not armed the device to perform remote wakeup,
 *                  then this function will return without actually performing a
 *                  remote wakeup sequence.  This is the required behavior, 
 *                  as a USB device that has not been armed to perform remote 
 *                  wakeup must not drive remote wakeup signalling onto the bus;
 *                  doing so will cause USB compliance testing failure.
 *                  
 *					This callback should send a RESUME signal that
 *                  has the period of 1-15ms.
 *
 * Note:            This function does nothing and returns quickly, if the USB
 *                  bus and host are not in a suspended condition, or are 
 *                  otherwise not in a remote wakeup ready state.  Therefore, it
 *                  is safe to optionally call this function regularly, ex: 
 *                  anytime application stimulus occurs, as the function will
 *                  have no effect, until the bus really is in a state ready
 *                  to accept remote wakeup. 
 *
 *                  When this function executes, it may perform clock switching,
 *                  depending upon the application specific code in 
 *                  USBCBWakeFromSuspend().  This is needed, since the USB
 *                  bus will no longer be suspended by the time this function
 *                  returns.  Therefore, the USB module will need to be ready
 *                  to receive traffic from the host.
 *
 *                  The modifiable section in this routine may be changed
 *                  to meet the application needs. Current implementation
 *                  temporary blocks other functions from executing for a
 *                  period of ~3-15 ms depending on the core frequency.
 *
 *                  According to USB 2.0 specification section 7.1.7.7,
 *                  "The remote wakeup device must hold the resume signaling
 *                  for at least 1 ms but for no more than 15 ms."
 *                  The idea here is to use a delay counter loop, using a
 *                  common value that would work over a wide range of core
 *                  frequencies.
 *                  That value selected is 1800. See table below:
 *                  ==========================================================
 *                  Core Freq(MHz)      MIP         RESUME Signal Period (ms)
 *                  ==========================================================
 *                      48              12          1.05
 *                       4              1           12.6
 *                  ==========================================================
 *                  * These timing could be incorrect when using code
 *                    optimization or extended instruction mode,
 *                    or when having other interrupts enabled.
 *                    Make sure to verify using the MPLAB SIM's Stopwatch
 *                    and verify the actual signal on an oscilloscope.
 *******************************************************************/
void USBCBSendResume(void)
{
    static WORD delay_count;

    //First verify that the host has armed us to perform remote wakeup.
    //It does this by sending a SET_FEATURE request to enable remote wakeup,
    //usually just before the host goes to standby mode (note: it will only
    //send this SET_FEATURE request if the configuration descriptor declares
    //the device as remote wakeup capable, AND, if the feature is enabled
    //on the host (ex: on Windows based hosts, in the device manager 
    //properties page for the USB device, power management tab, the 
    //"Allow this device to bring the computer out of standby." checkbox 
    //should be checked).
    if(USBGetRemoteWakeupStatus() == TRUE) 
    {
        //Verify that the USB bus is in fact suspended, before we send
        //remote wakeup signalling.
        if(USBIsBusSuspended() == TRUE) 
        {
            USBMaskInterrupts();

            //Clock switch to settings consistent with normal USB operation.
            USBCBWakeFromSuspend();
            USBSuspendControl = 0;
            USBBusIsSuspended = FALSE;  //So we don't execute this code again,
                                        //until a new suspend condition is detected.

            //Section 7.1.7.7 of the USB 2.0 specifications indicates a USB
            //device must continuously see 5ms+ of idle on the bus, before it sends
            //remote wakeup signalling.  One way to be certain that this parameter
            //gets met, is to add a 2ms+ blocking delay here (2ms plus at 
            //least 3ms from bus idle to USBIsBusSuspended() == TRUE, yeilds
            //5ms+ total delay since start of idle).
            delay_count = 3600U;
            do {
                delay_count--;
            } while (delay_count);

            //Now drive the resume K-state signalling onto the USB bus.
            USBResumeControl = 1; // Start RESUME signaling
            delay_count = 1800U; // Set RESUME line for 1-13 ms
            do {
                delay_count--;
            } while (delay_count);
            USBResumeControl = 0; //Finished driving resume signalling

            USBUnmaskInterrupts();
        }
    }
}

/*******************************************************************
 * Function:        BOOL USER_USB_CALLBACK_EVENT_HANDLER(
 *                        USB_EVENT event, void *pdata, WORD size)
 * PreCondition:    None
 * Input:           USB_EVENT event - the type of event
 *                  void *pdata - pointer to the event data
 *                  WORD size - size of the event data
 * Output:          None
 * Side Effects:    None
 * Overview:        This function is called from the USB stack to
 *                  notify a user application that a USB event
 *                  occured.  This callback is in interrupt context
 *                  when the USB_INTERRUPT option is selected.
 * Note:            None
 *******************************************************************/
BOOL USER_USB_CALLBACK_EVENT_HANDLER(int event, void *pdata, WORD size)
{
    switch (event) {
        case EVENT_TRANSFER:
            //Add application specific callback task or callback function here if desired.
            break;
        case EVENT_SOF:
            USBCB_SOF_Handler();
            break;
        case EVENT_SUSPEND:
            USBCBSuspend();
            break;
        case EVENT_RESUME:
            USBCBWakeFromSuspend();
            break;
        case EVENT_CONFIGURED:
            USBCBInitEP();
            break;
        case EVENT_SET_DESCRIPTOR:
            USBCBStdSetDscHandler();
            break;
        case EVENT_EP0_REQUEST:
            USBCBCheckOtherReq();
            break;
        case EVENT_BUS_ERROR:
            USBCBErrorHandler();
            break;
        case EVENT_TRANSFER_TERMINATED:
            //Add application specific callback task or callback function here if desired.
            //The EVENT_TRANSFER_TERMINATED event occurs when the host performs a CLEAR
            //FEATURE (endpoint halt) request on an application endpoint which was 
            //previously armed (UOWN was = 1).  Here would be a good place to:
            //1.  Determine which endpoint the transaction that just got terminated was 
            //      on, by checking the handle value in the *pdata.
            //2.  Re-arm the endpoint if desired (typically would be the case for OUT 
            //      endpoints).
            break;
        default:
            break;
    }
    return TRUE;
}


//Secondary callback function that gets called when the below
//control transfer completes for the USBHIDCBSetReportHandler()
void USBHIDCBSetReportComplete(void)
{
    handleMessage((uint8_t*)&CtrlTrfData);
    //memcpy( msgbuf, &CtrlTrfData, sizeof(msgbuf));
    //handleMessage();
}

/********************************************************************
 * Function:        void USBHIDCBSetReportHandler(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    None
 * Overview:        USBHIDCBSetReportHandler() is used to respond to
 *					the HID device class specific SET_REPORT control
 *					transfer request (starts with SETUP packet on EP0 OUT).
 *
 * Note:            This function is called from the USB stack in
 *                  response to a class specific control transfer requests
 *                  arriving over EP0.  Therefore, this function executes in the
 *                  same context as the USBDeviceTasks() function executes (which
 *                  may be an interrupt handler, or a main loop context function,
 *                  depending upon usb_config.h options).
 *******************************************************************/
//void USBHIDCBSetReportHandler(void)
void UserSetReportHandler(void)
{
	//Prepare to receive the command data through a SET_REPORT
	//control transfer on endpoint 0. 
	//USBEP0Receive((BYTE*)&CtrlTrfData, USB_EP0_BUFF_SIZE, USBHIDCBSetReportComplete);
    USBEP0Receive((BYTE*)CtrlTrfData,SetupPkt.wLength, USBHIDCBSetReportComplete);
}


/********************************************************************
 * Function:        void UserGetReportHandler(void)
 * PreCondition:    None
 * Input:           None
 * Output:          None
 * Side Effects:    If either the USBEP0SendRAMPtr() or USBEP0Transmit()
 *                  functions are not called in this function then the
 *                  requesting GET_REPORT will be STALLed
 *
 * Overview:        This function is called by the HID function driver
 *                  in response to a GET_REPORT command.
 *
 * Note:            This function is called from the USB stack in
 *                  response to a class specific control transfer request
 *                  arriving over EP0.  Therefore, this function executes in the
 *                  same context as the USBDeviceTasks() function executes (which
 *                  may be an interrupt handler, or a main loop context function,
 *                  depending upon usb_config.h options).
 *                  If the firmware needs more time to process the request,
 *                  here would be a good place to use the USBDeferStatusStage()
 *                  USB stack API function.
 *******************************************************************/
void UserGetReportHandler(void)
{
    USBEP0SendRAMPtr((BYTE*) & hid_send_buf, USB_EP0_BUFF_SIZE, USB_EP0_NO_OPTIONS);
}


/** EOF main.c *************************************************/
#endif
