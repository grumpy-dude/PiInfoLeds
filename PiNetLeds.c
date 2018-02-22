/**************************************************************************
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 **************************************************************************/

/**************************************************************************
 * Network activity indication for the Raspberry Pi, using one or two LEDs 
 *  connected to one or two GPIO pins.
 * 
 * GPIO pin ----|>|----[330]----+
 *              LED             |
 *                             ===
 *                            Ground
 *
 * 
 * Based on netledPi.c - https://github.com/RagnarJensen/PiLEDlights
 * This program uses the WiringPi library by Gordon Henderson - 
 *  http://wiringpi.com/ - Thanks, Gordon!
 *
 *
 * To compile:
 *   gcc PiNetLeds.c -std=gnu11 -Wall -O3 -o PiNetLeds -lwiringPi
 *
 * 
 * NOTE: The default LED pin for both receive and transmit activity is
 *  WiringPi pin 11, BCM_GPIO pin 7, physical pin 26 on the Pi's P1 header
 *  (this is true for all models of the Raspberry Pi). This pin is also used
 *  for CS1/CE1 of the primary SPI interface. If you have SPI add-ons
 *  connected, you may need to connect your LED(s) to some other unused
 *  pin(s), and then use both the -r and -t options.
 **************************************************************************/


/* Using GNU extensions to ISO standards */
#define _GNU_SOURCE

#include <argp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>

#include <wiringPi.h>

#include "macroasstring.h"
#include "pinetleds.h"
#include "pinetledsstrings.h"

#define VERSION_MAJOR                     0
#define VERSION_MINOR                     1

/* Version string for GLIBC's argp helper functions */
const char*          argp_program_version      = "PiNetLeds v" MACRO_VALUE_AS_STRING(VERSION_MAJOR) "." MACRO_VALUE_AS_STRING(VERSION_MINOR);

static unsigned int  Option_Poll_Interval_Time = DEFAULT_POLL_TIME_MILLISECONDS;
static unsigned int  Option_Tx_Led_GPIO_Pin    = DEFAULT_TX_LED_GPIO_PIN;        /* wiringPi numbering scheme */
static unsigned int  Option_Rx_Led_GPIO_Pin    = DEFAULT_RX_LED_GPIO_PIN;        /* wiringPi numbering scheme */
static bool          Option_Detach             = false;

static volatile bool Keep_Running              = true;


/* Reread the network statistics file */
int Activity( FILE* netstatsfile, bool* p_txAct, bool* p_rxAct )
{
        static unsigned int prev_total_rx_packets         = 0;
        static unsigned int prev_total_tx_packets         = 0;

        unsigned int        cur_total_rx_packets          = 0;
        unsigned int        cur_total_tx_packets          = 0;
        char*               netstatsfile_line_buffer      = NULL; // Let getline() worry about the line length
        size_t              netstatsfile_line_buffer_size = 0;

        int                 result;
 
        /* Go to the beginning of the network statistics file */
        result = TEMP_FAILURE_RETRY( fseek(netstatsfile, 0L, SEEK_SET) );
        if( result != 0 )
        {
            perror( NETWORK_STATS_FILE_SEEK_ERROR_MSG );
            return result;
        }

        /* Clear glibc's buffer */
        result = TEMP_FAILURE_RETRY( fflush(netstatsfile) );
        if( result != 0 )
        {
            perror( FILE_BUFFER_FLUSH_ERROR_MSG );
            return result;
        }

        /* Extract the I/O stats */
        while( (getline(&netstatsfile_line_buffer, &netstatsfile_line_buffer_size, netstatsfile) != -1) && (errno != EINTR) )
        {
            unsigned int this_dev_rx_packets             = 0;
            unsigned int this_dev_tx_packets             = 0;
            bool         use_packet_counts_from_this_dev = false;
            char*        dev_name_buffer                 = NULL; // Let sscanf() worry about the length of the device name (use "%ms")

            /* The format of the network statistics file is assumed to be one line per network device, each of which contains the following (@=to be ignored):
             *  Network interface name,
             *  @rx bytes, rx packets, @rx errs, @rx drop, @rx fifo, @rx frame, @rx compressed, @rx multicast,
             *  @tx bytes, tx packets, @tx errs, @tx drop, @tx fifo, @tx colls, @tx carrier, @tx compressed
             */
            if( sscanf(netstatsfile_line_buffer, "%ms %*u %u %*u %*u %*u %*u %*u %*u %*u %u %*u %*u %*u %*u %*u %*u", &dev_name_buffer, &this_dev_rx_packets, &this_dev_tx_packets) != 0 )
            {
                use_packet_counts_from_this_dev = (strstr(dev_name_buffer, DEFAULT_NET_LOOPBACK_DEVICE_NAME) == NULL); // Skip the "loopback" device

                if( use_packet_counts_from_this_dev == true )
                {
                    cur_total_rx_packets += this_dev_rx_packets;
                    cur_total_tx_packets += this_dev_tx_packets;
                }
            }

            if( dev_name_buffer != NULL )
                free( dev_name_buffer );
        }

        /* Anything changed? */
        if( (*p_txAct = (prev_total_tx_packets != cur_total_tx_packets)) == true )
        {
            prev_total_tx_packets = cur_total_tx_packets;
        }

        if( (*p_rxAct = (prev_total_rx_packets != cur_total_rx_packets)) == true )
        {
            prev_total_rx_packets = cur_total_rx_packets;
        }

        if( netstatsfile_line_buffer != NULL )
            free( netstatsfile_line_buffer );

        return result;
}


/* Update the LEDs */
void LedsOn( bool want_txAct_on, bool want_rxAct_on )
{
        static bool txAct_on = true; /* Ensure the LEDs turn off on first call */
        static bool rxAct_on = true;

        if( (want_txAct_on == txAct_on) && (want_rxAct_on == rxAct_on) )
                return;

        if( want_txAct_on || ((Option_Tx_Led_GPIO_Pin == Option_Rx_Led_GPIO_Pin) && want_rxAct_on) )
        {
            digitalWrite( Option_Tx_Led_GPIO_Pin, HIGH );
            txAct_on = true;
        }
        else
        {
            digitalWrite( Option_Tx_Led_GPIO_Pin, LOW );
            txAct_on = false;
        }

        if( want_rxAct_on || ((Option_Rx_Led_GPIO_Pin == Option_Tx_Led_GPIO_Pin) && want_txAct_on) )
        {
            digitalWrite( Option_Rx_Led_GPIO_Pin, HIGH );
            rxAct_on = true;
        }
        else
        {
            digitalWrite( Option_Rx_Led_GPIO_Pin, LOW );
            rxAct_on = false;
        }
}


/* Signal handler -- break out of the main loop */
void Shutdown( int sig )
{
        Keep_Running = false;
}


/* Argp parser function */
error_t ParseOptions( int key, char* arg, struct argp_state* state )
{
    switch( key )
    {
        case OPTION_DETACH_KEY:
            Option_Detach = true;
            break;

        case OPTION_POLL_TIME_KEY:
            Option_Poll_Interval_Time = strtol( arg, NULL, NUMERIC_OPTION_BASE );
            if( Option_Poll_Interval_Time < MIN_POLL_TIME_MILLISECONDS )
                argp_failure( state, EXIT_FAILURE, 0, INVALID_POLL_TIME_OPTION_MESSAGE );
            break;

        case OPTION_TX_PIN_KEY:
            Option_Tx_Led_GPIO_Pin = strtol( arg, NULL, NUMERIC_OPTION_BASE );
            if( (Option_Tx_Led_GPIO_Pin < MIN_VALID_TX_PIN) || (Option_Tx_Led_GPIO_Pin > MAX_VALID_TX_PIN) )
                argp_failure( state, EXIT_FAILURE, 0, INVALID_TX_PIN_OPTION_MESSAGE );
            break;

        case OPTION_RX_PIN_KEY:
            Option_Rx_Led_GPIO_Pin = strtol( arg, NULL, NUMERIC_OPTION_BASE );
            if( (Option_Rx_Led_GPIO_Pin < MIN_VALID_RX_PIN) || (Option_Rx_Led_GPIO_Pin > MAX_VALID_RX_PIN) )
                argp_failure( state, EXIT_FAILURE, 0, INVALID_RX_PIN_OPTION_MESSAGE );
            break;

        default:
            return ARGP_ERR_UNKNOWN;
            break;
    }

    return 0;
}


int main( int argc, char **argv )
{
        struct argp_option options[] =
        {
            {    OPTION_DETACH_NAME,    OPTION_DETACH_KEY,                      NULL, 0,   OPTION_DETATCH_DOCUMENTATION, 0 },
            { OPTION_POLL_TIME_NAME, OPTION_POLL_TIME_KEY, OPTION_POLL_TIME_ARG_TYPE, 0, OPTION_POLL_TIME_DOCUMENTATION, 0 },
            {    OPTION_RX_PIN_NAME,    OPTION_RX_PIN_KEY,    OPTION_RX_PIN_ARG_TYPE, 0,    OPTION_RX_PIN_DOCUMENTATION, 0 },
            {    OPTION_TX_PIN_NAME,    OPTION_TX_PIN_KEY,    OPTION_TX_PIN_ARG_TYPE, 0,    OPTION_TX_PIN_DOCUMENTATION, 0 },
            { 0 }
        };

        struct argp parser =
        {
                NULL, ParseOptions, NULL,
                HELP_DOCUMENTATION,
                NULL, NULL, NULL
        };

        int   status          = EXIT_FAILURE;
        FILE* fp_netstatsfile = NULL;
        bool  tx_activity     = false;
        bool  rx_activity     = false;

        struct timespec delay;

        /* Parse the command-line */
        parser.options = options;
        if( argp_parse(&parser, argc, argv, ARGP_NO_ARGS, NULL, NULL) )
            goto out;

        delay.tv_sec  = Option_Poll_Interval_Time / 1000;
        delay.tv_nsec = 1000000 * (Option_Poll_Interval_Time % 1000);

        /* Ensure the LEDs are off */
        wiringPiSetup();
        pinMode( Option_Rx_Led_GPIO_Pin, OUTPUT );
        pinMode( Option_Tx_Led_GPIO_Pin, OUTPUT );
        LedsOn( false, false );

        /* Open the network statistics file */
        fp_netstatsfile = fopen( NETWORK_STATS_FILE_NAME, "r" );
        if( fp_netstatsfile == NULL )
        {
            perror( NETWORK_STATS_FILE_OPEN_ERROR_MSG );
            goto out;
        }

        /* Save the current I/O stat values */
        if( Activity(fp_netstatsfile, &tx_activity, &rx_activity) != 0 )
            goto out;

        /* Detach from terminal? */
        if( Option_Detach == true )
        {
            pid_t child = fork();
            if( child < 0 )
            {
                perror( DETACH_FAILURE_MSG );
                goto out;
            }

            if( child > 0 )
            {
                /* I am the parent */
                status = EXIT_SUCCESS;
                goto out;
            }
        }

        /* We catch these signals so we can clean up */
        {
            struct sigaction sig_action;

            memset( &sig_action, 0, sizeof(sig_action) );

            sig_action.sa_handler = Shutdown;
            sig_action.sa_flags   = 0; /* We block on usleep; don't use SA_RESTART */

            sigemptyset( &sig_action.sa_mask );

            sigaction( SIGHUP,  &sig_action, NULL );
            sigaction( SIGINT,  &sig_action, NULL );
            sigaction( SIGTERM, &sig_action, NULL );
        }

        /* Loop until signal received */
        while( Keep_Running == true )
        {
            int activity_result;

            if( nanosleep(&delay, NULL) < 0 )
                break;

            activity_result = Activity( fp_netstatsfile, &tx_activity, &rx_activity );

            if( activity_result != 0 )
                break;

            LedsOn( tx_activity, rx_activity );
        }

        status = EXIT_SUCCESS;

out:
        /* Ensure the LEDs are off */
        LedsOn( false, false );

        if( fp_netstatsfile != NULL )
            fclose( fp_netstatsfile );

        return status;
}
