#ifndef FTDI_FT60x_H
#define FTDI_FT60x_H

#include "ftdi_driver_api.h"

//-------------------------------------------------------------
// ftdi_ft60x: FT60x interface
//-------------------------------------------------------------
class ftdi_ft60x: public ftdi_driver_api
{
public:
    ftdi_ft60x();
    bool open(int device_idx);
    void close(void);
    int  read(uint8_t *data, int length, int timeout_ms);
    int  write(uint8_t *data, int length, int timeout_ms);
    void sleep(int wait_us);

protected:
    bool configure(int device_idx, uint8_t clock);

protected:
    void *m_handle;
};

#endif