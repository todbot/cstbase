
#ifndef HARDWARE_PROFILE_H
#define HARDWARE_PROFILE_H

#if defined(_PIC14E)
  #if defined (_16F1455) || defined (_16F1454) || defined (_16LF1455) || defined (_16LF1454)
    #include "HardwareProfile-PIC16F1455-1454.h"
  #else
    #error "no HardwareProfile defined!"
  #endif
#endif


#endif  //HARDWARE_PROFILE_H
