#ifndef _PI_NET_LEDS_H

    #define _PI_NET_LEDS_H

    #define DEFAULT_POLL_TIME_MILLISECONDS    20
    #define DEFAULT_TX_LED_GPIO_PIN           11
    #define DEFAULT_RX_LED_GPIO_PIN           11             
    #define NUMERIC_OPTION_BASE               10
    #define MIN_POLL_TIME_MILLISECONDS        10
    #define MIN_VALID_TX_PIN                  0
    #define MAX_VALID_TX_PIN                  29
    #define MIN_VALID_RX_PIN                  0
    #define MAX_VALID_RX_PIN                  29

    #define NETWORK_STATS_FILE_NAME           "/proc/net/dev"
    #define DEFAULT_NET_LOOPBACK_DEVICE_NAME  "lo:"

#endif
