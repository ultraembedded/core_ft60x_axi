#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <getopt.h>
#include <sys/time.h> 

#include "ftdi_axi_driver.h"
#include "ftdi_ft60x.h"

#define MEASURE_START(_t) gettimeofday(&_t, NULL)

#define MEASURE_STOP(_t2, _t1, _elapsed) do { \
                gettimeofday(&t2, NULL); \
                _elapsed = (t2.tv_sec - t1.tv_sec) * 1000.0; \
                _elapsed += (t2.tv_usec - t1.tv_usec) / 1000.0; \
            } while (0)

//-----------------------------------------------------------------
// Command line options
//-----------------------------------------------------------------
#define GETOPTS_ARGS "d:t:a:s:h"

static struct option long_options[] =
{
    {"device",     required_argument, 0, 'd'},
    {"test",       required_argument, 0, 't'},
    {"addr",       required_argument, 0, 'a'},
    {"size",       required_argument, 0, 's'},
    {"help",       no_argument,       0, 'h'},
    {0, 0, 0, 0}
};

static void help_options(void)
{
    fprintf (stderr,"Usage:\n");
    fprintf (stderr,"  --device     | -d IDX        Device index to use (default: 0)\n");
    fprintf (stderr,"  --test       | -t IDX        Test index (default: 0)\n");
    fprintf (stderr,"  --addr       | -a ADDR       Test arg address\n");
    fprintf (stderr,"  --size       | -s SIZE       Test arg size\n");
    exit(-1);
}
//-----------------------------------------------------------------
// main:
//-----------------------------------------------------------------
int main(int argc, char *argv[])
{
    int c;
    int help      = 0;
    int device    = 0;
    int test_idx  = 0;
    uint32_t addr = 0;
    uint32_t size = (64 * 1024);

    int option_index = 0;
    while ((c = getopt_long (argc, argv, GETOPTS_ARGS, long_options, &option_index)) != -1)
    {
        switch(c)
        {
            case 'd':
                 device = strtoul(optarg, NULL, 0);
                 break;
            case 't':
                 test_idx = strtoul(optarg, NULL, 0);
                 break;
            case 'a':
                 addr = strtoul(optarg, NULL, 0);
                 break;
            case 's':
                 size = strtol(optarg, NULL, 0);
                 break;
            default:
                help = 1;
                break;
        }
    }

    if (help)
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

    switch (test_idx)
    {
        case 0:
        {
            printf("TEST: ECHO loopback - press CTRL-C to stop...\n");

            while (true)
            {
                uint8_t echo_buf[128];
                for (int i=0;i<sizeof(echo_buf);i++)
                    echo_buf[i] = i;
                if (!driver.send_echo(echo_buf, sizeof(echo_buf), 1000))
                    return -1;
            }
        }
        break;
        case 1:
        {
            printf("TEST: Write performance test - press CTRL-C to stop...\n");

            uint32_t total_data = 0;
            struct timeval t1, t2;
            double duration;
            uint8_t *write_buf = new uint8_t[size];
            MEASURE_START(t1);
            while (true)
            {
                if (!driver.write(addr, write_buf, size))
                    return -1;
                total_data += size;

                MEASURE_STOP(t2, t1, duration); 
                if (duration >= 1000.0)
                {
                    printf("Data rate: %dKB per s\n", total_data / 1024);
                    total_data = 0;
                    MEASURE_START(t1);
                }
            }
        }
        break;
        case 2:
        {
            printf("TEST: Read performance test - press CTRL-C to stop...\n");

            uint32_t total_data = 0;
            struct timeval t1, t2;
            double duration;
            uint8_t *read_buf = new uint8_t[size];
            MEASURE_START(t1);
            while (true)
            {
                if (!driver.read(addr, read_buf, size))
                    return -1;
                total_data += size;

                MEASURE_STOP(t2, t1, duration); 
                if (duration >= 1000.0)
                {
                    printf("Data rate: %dKB per s\n", total_data / 1024);
                    total_data = 0;
                    MEASURE_START(t1);
                }
            }
        }
        break;
        case 3:
        {
            printf("TEST: Read/write data test - press CTRL-C to stop...\n");

            uint32_t total_data = 0;
            struct timeval t1, t2;
            double duration;
            uint8_t *read_buf  = new uint8_t[size];
            uint8_t *write_buf = new uint8_t[size];
            MEASURE_START(t1);
            while (true)
            {
                for (int i=0;i<size;i++)
                   write_buf[i] = rand();

                if (!driver.write(addr, write_buf, size))
                    return -1;
                if (!driver.read(addr, read_buf, size))
                    return -1;

                for (int i=0;i<size;i++)
                    if (write_buf[i] != read_buf[i])
                    {
                        fprintf(stderr, "ERROR: Data read/write mistmatch %d: %02x != %02x\n", i, read_buf[i], write_buf[i]);
                        exit(-1);
                    }

                total_data += size;
                MEASURE_STOP(t2, t1, duration); 
                if (duration >= 1000.0)
                {
                    printf("Data rate: %dKB per s\n", total_data / 1024);
                    total_data = 0;
                    MEASURE_START(t1);
                }
            }
        }
        break;
    }

    port.close();
    return 0;
}
