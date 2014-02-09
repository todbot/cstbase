
/********************************************************************
 * CST Base station firmware
 *
 * For Microchip PIC16F1454/5
 * - http://www.microchip.com/wwwproducts/Devices.aspx?dDocName=en556969
 * Uses Microchip's USB Framework (aka "MCHPFSUSB Library")
 * - http://www.microchip.com/stellent/idcplg?IdcService=SS_GET_PAGE&nodeId=2680&dDocName=en547784
 *
 * Note: to compile, must have the Microchip Application Library frameworks
 * symlinked in the same directory level as the "blink1mk2" directory.
 * e.g. if you installed the framework in ~/microchip_solutions_v2013-06-15/Microchip
 * then do: ln -s ~/microchip_solutions_v2013-06-15/Microchip Microchip
 *
 *
 * A modified version of "usb_device.c" from MCHPFSUSB is needed for 
 * RAM-based serial numbers and is provided.
 *
 * PIC16F1454 pins used:
 * MLF/QFN                                                            DIP pkg
 * pin 1  - RA5            - input : "+" button                       2
 * pin 2  - RA4            - input : "12/24 hr" button                3
 * pin 3  - RA3/MCLR/VPP   - input : "-" button                       4 
 * pin 4  - RC5            - output: UART TX, to watch                5
 * pin 5  - RC4            - input : UART RX, to watch                6
 * pin 6  - RC3            - output: LED                              7
 * pin 7  - RC2            - output: 12V or 5V , to watch             8
 * pin 8  - RC1            - n/c                                      -
 * pin 9  - RC0            - n/c                                      -
 * pin 10 - VUSB3V3        - 100nF cap to Gnd                        11
 * pin 11 - RA1/D-/ICSPC   - D-  on USB                              12
 * pin 12 - RA0/D+/ICSPD   - D+  on USB                              13
 * pin 13 - VSS            - Gnd on USB                              14
 * pin 14 - n/c            - n/c                                      -
 * pin 15 - n/c            - n/c                                      -
 * pin 16 - VDD            - +5V on USB, 10uF & 100nF cap to Gnd      1
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

//#warning Using Internal Oscillator
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
#pragma config LVP      = ON  // keep that on when using LVP programmer

// -------------------------------------------------------------------


#define cstbase_ver_major  '1'
#define cstbase_ver_minor  '0'

#define cstbase_report_id 0x01


// serial number of this cst base station
// stored in a packed format at address 0x1FF8
const uint8_t serialnum_packed[4] @ 0x1FF8 = {0x21, 0x43, 0xba, 0xdc };

//** VARIABLES ******************************************************

#if defined( RAM_BASED_SERIALNUMBER )
RAMSNt my_RAMSN;
#endif

#if defined(__XC8)
#if defined(_16F1459) || defined(_16F1455) || defined(_16F1454)
#define RX_DATA_BUFFER_ADDRESS @0x2050
#define TX_DATA_BUFFER_ADDRESS @0x20A0
// FIXME: this is ugly
#define IN_DATA_BUFFER_ADDRESS 0x2050
#define OUT_DATA_BUFFER_ADDRESS (IN_DATA_BUFFER_ADDRESS + HID_INT_IN_EP_SIZE)
#define FEATURE_DATA_BUFFER_ADDRESS (OUT_DATA_BUFFER_ADDRESS + HID_INT_OUT_EP_SIZE)
#define FEATURE_DATA_BUFFER_ADDRESS_TAG @FEATURE_DATA_BUFFER_ADDRESS
//
#endif
#else
#define RX_DATA_BUFFER_ADDRESS
#define TX_DATA_BUFFER_ADDRESS
#endif

uint8_t hid_send_buf[USB_EP0_BUFF_SIZE] FEATURE_DATA_BUFFER_ADDRESS_TAG;

uint8_t usbHasBeenSetup = 0;  // set in USBCBInitEP()
#define usbIsSetup (USBGetDeviceState() == CONFIGURED_STATE)

//
static void InitializeSystem(void);
void USBCBSendResume(void);
void USBHIDCBSetReportComplete(void);


// ----------------------- millis time keeping -----------------------------

volatile uint32_t tick;  // for "millis()" function, a count of 1.024msec units

// also see Timer2 handling in interrupt
// hack because this compiler has no optimizations
#define millis() (tick)


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



// ------------- USB command handling ----------------------------------------

// handleMessage(char* msgbuf) -- main command router
//
// msgbuf[] is 8 bytes long
//  byte0 = report-id
//  byte1 = command
//  byte2..byte7 = args for command
//
// Available commands:
//    - Set time                format: { 1, 'T', H,M,S, ...
//    - Send byte to watch      format: { 1, 'S', b, ...
//    - Receive byte from watch format: { 1, 'R', ...
//    - 
//    - Set Base LED            format: { 1, 'L',  
//    - Get Base Version        format: { 1, 'V', 0,0,0
//
//
// Available commands:
//    - Fade to RGB color       format: { 1, 'c', r,g,b,     th,tl, n }
//    - Set RGB color now       format: { 1, 'n', r,g,b,       0,0, n } (*)
//    - Read current RGB color  format: { 1, 'r', n,0,0,       0,0, n } (2)
//    - Serverdown tickle/off   format: { 1, 'D', on,th,tl,  st,sp,ep } (*)
//    - PlayLoop                format: { 1, 'p', on,sp,ep,c,    0, 0 } (2)
//    - Playstate readback      format: { 1, 'S', 0,0,0,       0,0, 0 } (2)
//    - Set color pattern line  format: { 1, 'P', r,g,b,     th,tl, p }
//    - Save color patterns     format: { 1, 'W', 0,0,0,       0,0, 0 } (2)
//    - read color pattern line format: { 1, 'R', 0,0,0,       0,0, p }
//    - Read EEPROM location    format: { 1, 'e', ad,0,0,      0,0, 0 } (1)
//    - Write EEPROM location   format: { 1, 'E', ad,v,0,      0,0, 0 } (1)
//    - Get version             format: { 1, 'v', 0,0,0,       0,0, 0 }
//    - Test command            format: { 1, '!', 0,0,0,       0,0, 0 }
//
//  Fade to RGB color        format: { 1, 'c', r,g,b,      th,tl, ledn }
//  Set RGB color now        format: { 1, 'n', r,g,b,        0,0, ledn }
//  Play/Pause, with pos     format: { 1, 'p', {1/0},pos,0,  0,0,    0 }
//  Play/Pause, with pos     format: { 1, 'p', {1/0},pos,endpos, 0,0,0 }
//  Write color pattern line format: { 1, 'P', r,g,b,      th,tl,  pos }
//  Read color pattern line  format: { 1, 'R', 0,0,0,        0,0, pos }
//  Server mode tickle       format: { 1, 'D', {1/0},th,tl, {1,0},0, 0 }
//  Get version              format: { 1, 'v', 0,0,0,        0,0, 0 }
//
void handleMessage(const char* msgbuf)
{
    // pre-load response with request, contains report id
    memcpy( hid_send_buf, msgbuf, 8 );

    uint8_t cmd;

    //rid= msgbuf[0];
    cmd  = msgbuf[1];
    //c.r  = msgbuf[2];
    //c.g  = msgbuf[3];
    //c.b  = msgbuf[4];

    //  Fade to RGB color         format: { 1, 'c', r,g,b,      th,tl, ledn }
    //   where time 't' is a number of 10msec ticks
    //
    if(      cmd == 'c' ) {
    }
    //  Set RGB color now         format: { 1, 'n', r,g,b,        0,0, ledn }
    //
    else if( cmd == 'n' ) {
        mLED_4_Off();
    }
    //  Read current color        format: { 1, 'r', 0,0,0,        0,0, 0 }
    //
    else if( cmd == 'r' ) {
    }
    //  Play/Pause, with pos      format: { 1, 'p', {1/0},startpos,endpos,  0,0, 0 }
    //
    else if( cmd == 'p' ) {
    }
    // Play state readback        format: { 1, 'S', 0,0,0, 0,0,0 }
    // resonse format:  {
    else if( cmd == 'S' ) { 
        hid_send_buf[2] = 1; 
        hid_send_buf[3] = 3;
        hid_send_buf[4] = 5;
        hid_send_buf[5] = 7;
        hid_send_buf[6] = 9;
        hid_send_buf[7] = 0;
    }
    // Write color pattern line   format: { 1, 'P', r,g,b,      th,tl, pos }
    //
    else if( cmd == 'P' ) {
    }
    //  Read color pattern line   format: { 1, 'R', 0,0,0,        0,0, pos }
    //
    else if( cmd == 'R' ) {
        uint8_t pos = msgbuf[7];
        hid_send_buf[2] = 30;
        hid_send_buf[3] = 40;
        hid_send_buf[4] = pos;
        hid_send_buf[5] = 60;
        hid_send_buf[6] = 70;
    }
    // Write color pattern to flash memory: { 1, 'W', 0x55,0xAA, 0xCA,0xFE, 0,0}
    //
    else if( cmd == 'W' ) {
    }
    // read eeprom byte           format: { 1, 'e', addr, 0,0, 0,0,0,0}
    //
    // ...
    // not impelemented in mk2
    // ...

    //  Server mode tickle        format: { 1, 'D', {1/0},th,tl, {1,0}, sp, ep }
    //
    else if( cmd == 'D' ) {
    }
    //  Get version               format: { 1, 'v', 0,0,0,        0,0, 0 }
    //
    else if( cmd == 'v' ) {
        mLED_4_On();
        hid_send_buf[3] = cstbase_ver_major;
        hid_send_buf[4] = cstbase_ver_minor;
    }
    else if( cmd == '!' ) { // testing testing
    }
    else {

    }
}


//These are your actual interrupt handling routines.
// PIC16F has only one interrupt vector,
// so must check all possible interrupts when interrupt occurs
void interrupt ISRCode()
{
    //Check which interrupt flag caused the interrupt.
    //Service the interrupt
    //Clear the interrupt flag
    //Etc.
#if defined(USB_INTERRUPT)
    USBDeviceTasks();
#endif

    // Timer2 Interrupt- Freq = 1045.75 Hz - Period = 0.000956 seconds
    if( TMR2IF ) { // timer 2 interrupt flag
        tick++;
        TMR2IF = 0; // clears TMR2IF       bit 1 TMR2IF: TMR2 to PR2 Match Interrupt Flag bit
    }
}


// ****************************************************************************
// main
//
int main(void)
{
    InitializeSystem();

    USBDeviceAttach();
    
    while (1) {
        //updateLEDs();
        //updateMisc();
    }

} //end main


//
static void InitializeSystem(void)
{
    // set IO pin state
    ANSELA = 0x00;
    ANSELC = 0x00;
    TRISA = 0x00;
    TRISC = 0x00;

    // setup oscillator
    OSCTUNE = 0;
    OSCCON = 0xFC;          //16MHz HFINTOSC with 3x PLL enabled (48MHz operation)
    ACTCON = 0x90;          //Enable active clock tuning with USB

    // setup timer2 for tick functionality
    T2CONbits.T2CKPS = 0b01;    // 1:4 prescaler
    T2CONbits.T2OUTPS = 0b1011; //1:12 Postscaler
    PR2 = 242;  // at delay(10), PR2=250 => 1.15msec (w/overhead), PR2=245 => 1.083, PR2=240 => 1.060
    T2CONbits.TMR2ON = 1;       // bit 2 turn timer2 on;
    PIE1bits.TMR2IE  = 1;       // enable Timer2 interrupts
    INTCONbits.PEIE = 1;        // bit6 Peripheral Interrupt Enable bit...1 = Enables all unmasked peripheral interrupts

    loadSerialNumber();

    mInitAllLEDs();

    for( int i=0; i<10; i++ ) { 
        mLED_4_Toggle();
        _delay_ms(200);
    }
    mLED_4_Off();

    ei(); // enable global interrupts

    USBDeviceInit(); //usb_device.c.  Initializes USB module SFRs and firmware vars to known states.
} //end InitializeSystem



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
    // USB cable during the transmission) this will typically set
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