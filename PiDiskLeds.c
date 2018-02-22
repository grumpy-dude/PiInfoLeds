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
 * Block device (disk) activity indication for the Raspberry Pi, using one
 *  or two LEDs connected to one or two GPIO pins.
 * 
 * GPIO pin ----|>|----[330]----+
 *              LED             |
 *                             ===
 *                            Ground
 *
 * 
 * Based on hddledPi.c - https://github.com/RagnarJensen/PiLEDlights
 * This program uses the WiringPi library by Gordon Henderson - 
 *  http://wiringpi.com/ - Thanks, Gordon!
 *
 *
 * To compile:
 *   gcc PiDiskLeds.c -std=gnu11 -Wall -O3 -o PiDiskLeds -lwiringPi
 *
 * 
 * NOTE: The default LED pin for both receive and transmit activity is
 *  WiringPi pin 10, BCM_GPIO pin 8, physical pin 24 on the Pi's P1 header
 *  (this is true for all models of the Raspberry Pi). This pin is also used
 *  for CS0/CE0 of the primary SPI interface. If you have SPI add-ons
 *  connected, you will likely need to connect your LED(s) to some other
 *  unused pin(s), and then use both the -r and -w options.
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
#include "pidiskleds.h"
#include "pidiskledsstrings.h"

#define VERSION_MAJOR                     0
#define VERSION_MINOR                     1

/* Version string for GLIBC's argp helper functions */
const char*          argp_program_version      = "PiDiskLeds v" MACRO_VALUE_AS_STRING(VERSION_MAJOR) "." MACRO_VALUE_AS_STRING(VERSION_MINOR);


static unsigned int  Option_Poll_Interval_Time = DEFAULT_POLL_TIME_MILLISECONDS;
static unsigned int  Option_Wr_Led_GPIO_Pin    = DEFAULT_WR_LED_GPIO_PIN;        /* wiringPi numbering scheme */
static unsigned int  Option_Rd_Led_GPIO_Pin    = DEFAULT_RD_LED_GPIO_PIN;        /* wiringPi numbering scheme */
static bool          Option_Detach             = false;

static volatile bool Keep_Running              = true;


/* Reread the vmstat file */
int Activity( FILE* fp_vm_stats_file, bool* p_wrAct, bool* p_rdAct )
{
    static unsigned int prev_pgpgin                    = 0;
    static unsigned int prev_pgpgout                   = 0;

    unsigned int        cur_pgpgin                     = 0;
    unsigned int        cur_pgpgout                    = 0;
    char*               vm_stats_file_line_buffer      = NULL; // Let getline() worry about the line length
    bool                found_pgpgin                   = false;
    bool                found_pgpgout                  = false;
    size_t              vm_stats_file_line_buffer_size = 0;

    int                 result;

    /* Reload the vmstat file */
    result = TEMP_FAILURE_RETRY( fseek(fp_vm_stats_file, 0L, SEEK_SET) );
    if( result != 0 )
    {
            perror( VM_STATS_FILE_SEEK_ERROR_MSG );
            return result;
    }

    /* Clear glibc's buffer */
    result = TEMP_FAILURE_RETRY( fflush(fp_vm_stats_file) );
    if( result != 0 )
    {
            perror( FILE_BUFFER_FLUSH_ERROR_MSG );
            return result;
    }

    /* Extract the I/O stats */
    while( (getline(&vm_stats_file_line_buffer, &vm_stats_file_line_buffer_size, fp_vm_stats_file) != -1) && (errno != EINTR) )
    {
        if( sscanf(vm_stats_file_line_buffer, "pgpgin %u", &cur_pgpgin) != 0 )
        {
            found_pgpgin = true;

            if( (*p_wrAct = (cur_pgpgin != prev_pgpgin)) == true )
            {
                prev_pgpgin = cur_pgpgin;
            }
        }
        else if( sscanf(vm_stats_file_line_buffer, "pgpgout %u", &cur_pgpgout) !=0 )
        {
            found_pgpgout = true;

            if( (*p_rdAct = (cur_pgpgout != prev_pgpgout)) == true )
            {
                prev_pgpgout = cur_pgpgout;
            }
        }

        if( (found_pgpgin == true) && (found_pgpgout == true) )
        {
            break;
        }
    }

    if( vm_stats_file_line_buffer != NULL )
        free( vm_stats_file_line_buffer );

    return result;
}


/* Update the LEDs */
void LedsOn( bool want_wrAct_on, bool want_rdAct_on )
{
        static bool wrAct_on = true; /* Ensure the LEDs turn off on first call */
        static bool rdAct_on = true;

        if( (want_wrAct_on == wrAct_on) && (want_rdAct_on == rdAct_on) )
                return;

        if( want_wrAct_on || ((Option_Wr_Led_GPIO_Pin == Option_Rd_Led_GPIO_Pin) && want_rdAct_on) )
        {
            digitalWrite( Option_Wr_Led_GPIO_Pin, HIGH );
            wrAct_on = true;
        }
        else
        {
            digitalWrite( Option_Wr_Led_GPIO_Pin, LOW );
            wrAct_on = false;
        }

        if( want_rdAct_on || ((Option_Wr_Led_GPIO_Pin == Option_Rd_Led_GPIO_Pin) && want_wrAct_on) )
        {
            digitalWrite( Option_Rd_Led_GPIO_Pin, HIGH );
            rdAct_on = true;
        }
        else
        {
            digitalWrite( Option_Rd_Led_GPIO_Pin, LOW );
            rdAct_on = false;
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

        case OPTION_WR_PIN_KEY:
            Option_Wr_Led_GPIO_Pin = strtol( arg, NULL, NUMERIC_OPTION_BASE );
            if( (Option_Wr_Led_GPIO_Pin < MIN_VALID_WR_PIN) || (Option_Wr_Led_GPIO_Pin > MAX_VALID_WR_PIN) )
                argp_failure( state, EXIT_FAILURE, 0, INVALID_WR_PIN_OPTION_MESSAGE );
            break;

        case OPTION_RD_PIN_KEY:
            Option_Rd_Led_GPIO_Pin = strtol( arg, NULL, NUMERIC_OPTION_BASE );
            if( (Option_Rd_Led_GPIO_Pin < MIN_VALID_RD_PIN) || (Option_Rd_Led_GPIO_Pin > MAX_VALID_RD_PIN) )
                argp_failure( state, EXIT_FAILURE, 0, INVALID_RD_PIN_OPTION_MESSAGE );
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
            {    OPTION_RD_PIN_NAME,    OPTION_RD_PIN_KEY,    OPTION_RD_PIN_ARG_TYPE, 0,    OPTION_RD_PIN_DOCUMENTATION, 0 },
            {    OPTION_WR_PIN_NAME,    OPTION_WR_PIN_KEY,    OPTION_WR_PIN_ARG_TYPE, 0,    OPTION_WR_PIN_DOCUMENTATION, 0 },
            { 0 }
        };

        struct argp parser =
        {
                NULL, ParseOptions, NULL,
                HELP_DOCUMENTATION,
                NULL, NULL, NULL
        };

        int   status      = EXIT_FAILURE;
        FILE* fp_vmstat   = NULL;
        bool  wr_activity = false;
        bool  rd_activity = false;

        struct timespec delay;

        /* Parse the command-line */
        parser.options = options;
        if( argp_parse(&parser, argc, argv, ARGP_NO_ARGS, NULL, NULL) )
                goto out;

        delay.tv_sec  = Option_Poll_Interval_Time / 1000;
        delay.tv_nsec = 1000000 * (Option_Poll_Interval_Time % 1000);

        /* Ensure the LEDs are off */
        wiringPiSetup();
        pinMode( Option_Rd_Led_GPIO_Pin, OUTPUT );
        pinMode( Option_Wr_Led_GPIO_Pin, OUTPUT );
        LedsOn( false, false );

        /* Open the vmstat file */
        fp_vmstat = fopen( VM_STATS_FILE_NAME, "r" );
        if( fp_vmstat == NULL )
        {
            perror( VM_STATS_FILE_OPEN_ERROR_MSG );
            goto out;
        }

        /* Save the current I/O stat values */
        if( Activity(fp_vmstat, &wr_activity, &rd_activity) != 0 )
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

                activity_result = Activity( fp_vmstat, &wr_activity, &rd_activity );

                if( activity_result != 0 )
                        break;

                LedsOn( wr_activity, rd_activity );
        }

        status = EXIT_SUCCESS;

out:
        /* Ensure the LEDs are off */
        LedsOn( false, false );

        if( fp_vmstat != NULL )
            fclose( fp_vmstat );

        return status;
}
