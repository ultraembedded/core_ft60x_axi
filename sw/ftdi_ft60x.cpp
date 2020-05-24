#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <stdlib.h>

#include "ftdi_ft60x.h"
#include "ftd3xx.h"

//-----------------------------------------------------------------------------
// configure: Check and update device configuration
//-----------------------------------------------------------------------------
bool ftdi_ft60x::configure(int device_idx, uint8_t clock)
{
    FT_HANDLE handle = NULL;

    // Open connection to device
    // TODO: Device index not working...
    assert(device_idx == 0);
    FT_Create(0, FT_OPEN_BY_INDEX, &handle);
    if (!handle)
        return false;

    // Check device type is supported
    DWORD dwType = FT_DEVICE_UNKNOWN;
    FT_GetDeviceInfoDetail(0, NULL, &dwType, NULL, NULL, NULL, NULL, NULL);
    if (dwType != FT_DEVICE_600 && dwType != FT_DEVICE_601)
    {
        fprintf(stderr, "FT60X: Incompatible device detected\n");
        return false;
    }

    // Avoid rev-a parts - too many errata to workaround
    DWORD dwVersion;
    FT_GetFirmwareVersion(handle, &dwVersion);
    if (dwVersion <= 0x105)
    {
        fprintf(stderr, "FT60X: Incompatible device (rev-A) detected\n");
        return false;
    }

    // Get current configuration
    FT_60XCONFIGURATION current_cfg;
    if (FT_OK != FT_GetChipConfiguration(handle, &current_cfg))
    {
        fprintf(stderr, "FT60X: Could not fetch current configuration\n");
        return false;
    }

    // Create new configuration
    FT_60XCONFIGURATION new_cfg;
    memcpy(&new_cfg, &current_cfg, sizeof(FT_60XCONFIGURATION));
    new_cfg.FIFOClock     = clock;
    new_cfg.FIFOMode      = CONFIGURATION_FIFO_MODE_245;
    new_cfg.ChannelConfig = CONFIGURATION_CHANNEL_CONFIG_1;

    // Disable stop on underrun
    new_cfg.OptionalFeatureSupport = CONFIGURATION_OPTIONAL_FEATURE_DISABLECANCELSESSIONUNDERRUN;

    // Detect delta in configuration and apply if there is
    if (memcmp(&new_cfg, &current_cfg, sizeof(FT_60XCONFIGURATION)))
    {
        if (FT_SetChipConfiguration(handle, &new_cfg) != FT_OK)
        {
            fprintf(stderr, "FT60X: Could not write new configuration\n");
            return false;
        }
        else
        {
            printf("FT60x: Configuration updated...\n");
            ftdi_ft60x::sleep(1000000);
        }
    }

    FT_Close(handle);

#if !defined(_WIN32) && !defined(_WIN64)
    // Enable non thread safe transfer to increase throughput
    {
        FT_TRANSFER_CONF conf;

        memset(&conf, 0, sizeof(FT_TRANSFER_CONF));
        conf.wStructSize = sizeof(FT_TRANSFER_CONF);
        conf.pipe[FT_PIPE_DIR_IN].fNonThreadSafeTransfer = true;
        conf.pipe[FT_PIPE_DIR_OUT].fNonThreadSafeTransfer = true;
        for (DWORD i = 0; i < 4; i++)
            FT_SetTransferParams(&conf, i);
    }
#endif

    return true;
}
//-------------------------------------------------------------
// Constructor
//-------------------------------------------------------------
ftdi_ft60x::ftdi_ft60x()
{
    m_handle = NULL;
}
//-------------------------------------------------------------
// open: Try and open FT60x interface and configure
//-------------------------------------------------------------
bool ftdi_ft60x::open(int device_idx)
{
    // Make sure device is configured as expected
    if (!configure(device_idx, CONFIGURATION_FIFO_CLK_100))
    {
        printf("FT60x: Failed to configure device\n");
        return false;
    }

    ULONG arg = device_idx; // TODO: Not working
    assert(device_idx == 0);

    // Create device handle
    FT_Create(NULL, FT_OPEN_BY_INDEX, &m_handle);
    if (!m_handle)
    {
        printf("FT60x: Failed to create device\n");
        return false;
    }

    return true;
}
//-------------------------------------------------------------
// close: Close connection to FT60x
//-------------------------------------------------------------
void ftdi_ft60x::close(void)
{
    if (m_handle != NULL)
    {
        FT_Close(m_handle);
    }
}
//-------------------------------------------------------------
// read: Read a chunk of data
//-------------------------------------------------------------
int ftdi_ft60x::read(uint8_t *data, int length, int timeout_ms)
{
    DWORD count;
    FT_STATUS err;

    if ((err = FT_ReadPipeEx(m_handle, 0, data, length, &count, timeout_ms)) != FT_OK)
    {
        if (err == FT_TIMEOUT)
            return 0;

        printf("FT60x: FT_ReadPipeEx err %d\n", err);
        return -1;
    }

    return (int)count;
}
//-------------------------------------------------------------
// write: Write a chunk of data
//-------------------------------------------------------------
int ftdi_ft60x::write(uint8_t *data, int length, int timeout_ms)
{
    DWORD count;

    //printf("Write: %d\n", length);
    FT_STATUS status = FT_WritePipeEx(m_handle, 0, data, length, &count, timeout_ms);
    if (status  != FT_OK)
    {
        printf("FT_WritePipeEx: %d\n", status);
        return -1;
    }

#if 1
    DWORD queued_data = 0;
    int loops = 0;
    do
    {
        status = FT_GetWriteQueueStatus(m_handle, 0, &queued_data);
    }
    while (queued_data != 0);
#endif
    if ((int)count != length)
        return -1;

    return (int)count;
}
//-------------------------------------------------------------
// sleep: Wait for some time
//-------------------------------------------------------------
void ftdi_ft60x::sleep(int wait_us)
{
    usleep(wait_us);
}