#ifndef FTDI_AXI_DRIVER_H
#define FTDI_AXI_DRIVER_H

#include "ftdi_driver_api.h"

#define MAX_WR_CHUNKS   128
#define MAX_RD_CHUNKS   128

#define MAX_CHUNK_SIZE  512

//-------------------------------------------------------------
// ftdi_axi_driver: Wrapper interface for AXI bus master
//-------------------------------------------------------------
class ftdi_axi_driver
{
public:
    ftdi_axi_driver(ftdi_driver_api *port);

    bool send_drain(int timeout_ms);
    bool send_echo(uint8_t *data, int length, int timeout_ms = 100);
    bool write8(uint32_t addr, uint8_t data, int timeout_ms = 100, bool posted = false);
    bool write32(uint32_t addr, uint32_t data, int timeout_ms = 100, bool posted = false);
    bool read32(uint32_t addr, uint32_t &data, int timeout_ms = 100);
    bool write(uint32_t addr, uint8_t *data, int length, int timeout_ms = 100, bool posted = true);
    bool read(uint32_t addr, uint8_t *data, int length, int timeout_ms = 100);

    bool gpio_write(uint32_t value, int timeout_ms = 100);
    bool gpio_read(uint32_t &value, int timeout_ms = 100);

protected:

    bool send_command(uint8_t cmd_id, uint32_t addr, uint8_t *data, int length, int timeout_ms);
    uint8_t* recv_data(uint16_t seq_num, int length, int timeout_ms);
    int fill_command(uint8_t *wr_buf, uint8_t cmd_id, uint32_t addr, uint8_t *data, int length);

    uint16_t         m_seq_num;
    ftdi_driver_api *m_port;

    uint8_t          m_write_buf[(MAX_CHUNK_SIZE * MAX_WR_CHUNKS) + (16 * MAX_WR_CHUNKS)];
    uint8_t          m_read_buf[(MAX_RD_CHUNKS * MAX_CHUNK_SIZE) + (MAX_RD_CHUNKS * 4)];
};

#endif