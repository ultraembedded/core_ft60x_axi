#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>

#include "ftdi_axi_driver.h"
#include "ftdi_ft60x.h"

//-----------------------------------------------------------------
// Command line options
//-----------------------------------------------------------------
#define GETOPTS_ARGS "d:a:v:h"

static struct option long_options[] =
{
    {"device",     required_argument, 0, 'd'},
    {"address",    required_argument, 0, 'a'},
    {"value",      required_argument, 0, 'v'},
    {"help",       no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

static void help_options(void)
{
    fprintf (stderr,"Usage:\n");
    fprintf (stderr,"  --device     | -d IDX        Device index to use (default: 0)\n");
    fprintf (stderr,"  --address    | -a ADDR       Address to write\n");
    fprintf (stderr,"  --value      | -v DATA       Value to write\n");
    exit(-1);
}
//-----------------------------------------------------------------
// main:
//-----------------------------------------------------------------
int main(int argc, char *argv[])
{
    int c;
    int help       = 0;
    int device     = 0;
    uint32_t addr  = 0xFFFFFFFF;
    uint32_t value = 0;

    int option_index = 0;
    while ((c = getopt_long (argc, argv, GETOPTS_ARGS, long_options, &option_index)) != -1)
    {
        switch(c)
        {
            case 'd':
                 device = strtoul(optarg, NULL, 0);
                 break;
            case 'a':
                 addr = strtoul(optarg, NULL, 0) & ~3;
                 break;
            case 'v':
                 value = strtoul(optarg, NULL, 0);
                 break;
            default:
                help = 1;
                break;
        }
    }

    if (help || addr == 0xFFFFFFFF)
    {
        help_options();
        return -1;
    }

    // Open the port
    ftdi_ft60x port;
    if (!port.open(0))
        return -1;

    // Reset target state machines
    ftdi_axi_driver driver(&port);
    driver.send_drain(1000);
    port.sleep(10000);

    if (!driver.write32(addr, value))
        return -1;

    printf("0x%08x: 0x%08x (%d)\n", addr, value, value);

    port.close();
    return 0;
}
