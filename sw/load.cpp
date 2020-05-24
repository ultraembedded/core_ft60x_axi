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
#define GETOPTS_ARGS "d:a:s:f:h"

static struct option long_options[] =
{
    {"device",       required_argument, 0, 'd'},
    {"address",      required_argument, 0, 'a'},
    {"size",         required_argument, 0, 's'},
    {"filename",     required_argument, 0, 'f'},
    {"help",         no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

static void help_options(void)
{
    fprintf (stderr,"Usage:\n");
    fprintf (stderr,"  --device     | -d IDX        Device index to use (default: 0)\n");
    fprintf (stderr,"  --address    | -a ADDR       Address to load file to (default: 0)\n");
    fprintf (stderr,"  --filename   | -f FILENAME   File to load\n");
    fprintf (stderr,"  --size       | -s SIZE       File size (default: actual file size)\n");
    exit(-1);
}
//-----------------------------------------------------------------
// load_file_to_mem
//-----------------------------------------------------------------
static uint8_t* load_file_to_mem(const char *filename, long size_override, int *pSize)
{
    uint8_t *buf = NULL;
    FILE *f = fopen(filename, "rb");

    *pSize = 0;

    if (f)
    {
        long size;

        // Get size of file
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        rewind(f);

        // User overriden file size
        if (size_override >= 0)
        {
            if (size > size_override)
                size = size_override;
        }

        buf = new uint8_t[size];
        if (buf)
        {
            // Read file data into allocated memory
            int len = fread(buf, 1, size, f);
            if (len != size)
            {
                delete[] buf;
                buf = NULL;
            }
            else
                *pSize = size;
        }
        fclose(f);
    }

    return buf;
}
//-----------------------------------------------------------------
// main:
//-----------------------------------------------------------------
int main(int argc, char *argv[])
{
    int      c;
    int      help      = 0;
    int      device    = 0;
    uint32_t addr      = 0;
    long     size_override = -1;
    char *   filename = NULL;

    int option_index = 0;
    while ((c = getopt_long (argc, argv, GETOPTS_ARGS, long_options, &option_index)) != -1)
    {
        switch(c)
        {
            case 'd':
                 device = strtoul(optarg, NULL, 0);
                 break;
            case 'a':
                 addr = strtoul(optarg, NULL, 0);
                 break;
            case 'f':
                 filename = optarg;
                 break;
            case 's':
                 size_override = strtol(optarg, NULL, 0);
                 break;
            default:
                help = 1;
                break;
        }
    }

    if (help || filename == NULL)
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

    // Read file into memory
    bool ok = true;
    int size = 0;
    uint8_t *buf = load_file_to_mem(filename, size_override, &size);
    if (buf)
    {
        printf("Loading %s (%dKB) to 0x%x...\n", filename, (size + 1023) / 1024, addr);

        // Upload file to target
        ok = driver.write(addr, buf, size);

        // Free file memory
        delete[] buf;
        buf = NULL;

        if (ok)
            printf("Done!\n");
        else
            printf("Failed!\n");
    }
    else
    {
        fprintf (stderr,"Error: Could not open file\n");
        ok = false;
    }

    port.close();
    return ok ? 0: -1;
}
