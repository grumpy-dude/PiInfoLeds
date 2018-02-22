#ifndef _PI_DISK_LEDS_STRINGS_H

    #define _PI_DISK_LEDS_STRINGS_H

    #include "macroasstring.h"
    #include "pidiskleds.h"

    #define OPTION_DETACH_NAME                "detach"
    #define OPTION_DETACH_KEY                 'd'
    #define OPTION_DETATCH_DOCUMENTATION      "Detach from terminal (run as background process)\n"

    #define OPTION_POLL_TIME_NAME             "poll interval"
    #define OPTION_POLL_TIME_KEY              'p'
    #define OPTION_POLL_TIME_ARG_TYPE         "MILLISECONDS"
    #define OPTION_POLL_TIME_DOCUMENTATION    "Polling time interval in milliseconds\n"\
                                              "(Default: " MACRO_VALUE_AS_STRING(DEFAULT_POLL_TIME_MILLISECONDS) " ms)\n"

    #define OPTION_RD_PIN_NAME                "read led"
    #define OPTION_RD_PIN_KEY                 'r'
    #define OPTION_RD_PIN_ARG_TYPE            "PIN"
    #define OPTION_RD_PIN_DOCUMENTATION       "GPIO pin number where disk read activity LED is connected\n"\
                                              "(Uses WiringPi numbering scheme. Default: WiringPi pin " MACRO_VALUE_AS_STRING(DEFAULT_RD_LED_GPIO_PIN) ")\n"

    #define OPTION_WR_PIN_NAME                "write led"
    #define OPTION_WR_PIN_KEY                 'w'
    #define OPTION_WR_PIN_ARG_TYPE            "PIN"
    #define OPTION_WR_PIN_DOCUMENTATION       "GPIO pin number where disk write activity LED is connected\n"\
                                              "(Uses WiringPi numbering scheme. Default: WiringPi pin " MACRO_VALUE_AS_STRING(DEFAULT_WR_LED_GPIO_PIN) ")\n"


    #if (DEFAULT_RD_LED_GPIO_PIN == 10 ) || (DEFAULT_WR_LED_GPIO_PIN == 10)
        #define HELP_NOTE_PIN_10              "NOTE: The default GPIO pin (WiringPi pin 10, BCM GPIO pin 8, physical pin 24) is used for CE0 in "\
                                              "the default configuration of the SPI0 interface. If you have SPI add-ons, you will likely need to "\
                                              "connect the LED(s) to some other, unused pin(s).\n\n"
    #else
        #define HELP_NOTE_PIN_10              ""
    #endif

    #if (DEFAULT_RD_LED_GPIO_PIN == 11 ) || (DEFAULT_WR_LED_GPIO_PIN == 11)
        #define HELP_NOTE_PIN_11              "NOTE: The default GPIO pin (WiringPi pin 11, BCM GPIO pin 7, physical pin 26) is used for CE1 in "\
                                              "the default configuration of the SPI0 interface. If you have SPI add-ons, you may need to "\
                                              "connect the LED(s) to some other, unused pin(s).\n\n"
    #else
        #define HELP_NOTE_PIN_11              ""
    #endif

    #define HELP_DOCUMENTATION                "Blink LEDs on disk activity\v"\
                                              "Indicates read and write activity for all mounted block devices (e.g. USB flash drive, USB disk drive, etc.) "\
                                              "in addition to the SD card by blinking one or two LEDs connected to GPIO pins.\n\n" HELP_NOTE_PIN_10 HELP_NOTE_PIN_11 \
                                              "To show the mapping of WiringPi pin numbers to physical pins on this Raspberry Pi, the \"gpio readall\" command "\
                                              "may be used (requires the \"wiringpi\" package to be installed).\n\n"

    #define VM_STATS_FILE_OPEN_ERROR_MSG      "Could not open " VM_STATS_FILE_NAME " for reading"
    #define VM_STATS_FILE_SEEK_ERROR_MSG      "Could not return to start of " VM_STATS_FILE_NAME
    #define FILE_BUFFER_FLUSH_ERROR_MSG       "Could not clear out file buffer"
    #define DETACH_FAILURE_MSG                "Could not detach from terminal"
    #define INVALID_POLL_TIME_OPTION_MESSAGE  "poll time interval must be at least " MACRO_VALUE_AS_STRING(MIN_POLL_TIME_MILLISECONDS) " milliseconds"
    #define INVALID_WR_PIN_OPTION_MESSAGE     "pin number must be between " MACRO_VALUE_AS_STRING(MIN_VALID_WR_PIN) " and " MACRO_VALUE_AS_STRING(MAX_VALID_WR_PIN)
    #define INVALID_RD_PIN_OPTION_MESSAGE     "pin number must be between " MACRO_VALUE_AS_STRING(MIN_VALID_RD_PIN) " and " MACRO_VALUE_AS_STRING(MAX_VALID_RD_PIN)

#endif