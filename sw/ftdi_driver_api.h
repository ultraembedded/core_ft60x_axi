#ifndef FTDI_DRIVER_API_H
#define FTDI_DRIVER_API_H

#include <stdint.h>

//-------------------------------------------------------------
// ftdi_driver_api: API for FTDI drivers
//-------------------------------------------------------------
class ftdi_driver_api
{
public:
    virtual bool open(int device_idx) = 0;
    virtual void close(void) = 0;
    virtual int  read(uint8_t *data, int length, int timout_ms) = 0;
    virtual int  write(uint8_t *data, int length, int timout_ms) = 0;
    virtual void sleep(int wait_us) = 0;
};

#endif