#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "ftdi_axi_driver.h"

typedef struct CommandBlock
{
    uint8_t  command;
    uint8_t  length;
    uint16_t seq_num;
    uint32_t addr;
} tCommandBlock;

typedef struct StatusBlock
{
    uint16_t seq_num;
    uint16_t status;
} tStatusBlock;

#define CMD_ID_ECHO       0x01
#define CMD_ID_DRAIN      0x02
#define CMD_ID_READ       0x10
#define CMD_ID_WRITE8_NP  0x20 // 8-bit write (with response)
#define CMD_ID_WRITE16_NP 0x21 // 16-bit write (with response)
#define CMD_ID_WRITE_NP   0x22 // 32-bit write (with response)
#define CMD_ID_WRITE8     0x30 // 8-bit write
#define CMD_ID_WRITE16    0x31 // 16-bit write
#define CMD_ID_WRITE      0x32 // 32-bit write
#define CMD_ID_GPIO_WR    0x40
#define CMD_ID_GPIO_RD    0x41

#define MAX_POSTED_WR     4096

//-------------------------------------------------------------
// Constructor
//-------------------------------------------------------------
ftdi_axi_driver::ftdi_axi_driver(ftdi_driver_api *port)
{
    m_port    = port;
    m_seq_num = 1;  
}
//-------------------------------------------------------------
// fill_command: Fill command with optional data into a buffer
//-------------------------------------------------------------
int ftdi_axi_driver::fill_command(uint8_t *wr_buf, uint8_t cmd_id, uint32_t addr, uint8_t *data, int length)
{
    int            length4 = ((length + 3)/4) * 4;
    tCommandBlock *cmd     = (tCommandBlock*)wr_buf;
    int            wr_len;

    assert((length4 / 4) < 256);
    cmd->command = cmd_id;
    cmd->length  = length4 / 4;
    cmd->seq_num = m_seq_num;
    cmd->addr    = addr;

    if (data && length)
    {
        memcpy(&wr_buf[sizeof(tCommandBlock)], data, length);
        wr_len = sizeof(tCommandBlock) + length4;
    }
    else
        wr_len = sizeof(tCommandBlock);

    m_seq_num++;
    return wr_len;
}
//-------------------------------------------------------------
// send_command: Send command with optional data
//-------------------------------------------------------------
bool ftdi_axi_driver::send_command(uint8_t cmd_id, uint32_t addr, uint8_t *data, int length, int timeout_ms)
{
    int            length4 = ((length + 3)/4) * 4;
    uint8_t       *wr_buf  = new uint8_t[sizeof(tCommandBlock) + length4];
    tCommandBlock *cmd     = (tCommandBlock*)wr_buf;
    int            wr_len;

    assert((length4 / 4) < 256);
    cmd->command = cmd_id;
    cmd->length  = length4 / 4;
    cmd->seq_num = m_seq_num;
    cmd->addr    = addr;

    if (data && length)
    {
        memcpy(&wr_buf[sizeof(tCommandBlock)], data, length);
        wr_len = sizeof(tCommandBlock) + length4;
    }
    else
        wr_len = sizeof(tCommandBlock);

    int sent = m_port->write(wr_buf, wr_len, timeout_ms);
    delete []wr_buf;

    bool ok = true;
    if (sent != wr_len)
    {
        fprintf(stderr, "ERROR: Failed to send command\n");
        ok = false;
    }

    m_seq_num++;
    return ok;
}
//-------------------------------------------------------------
// recv_data: Wait on response data
//-------------------------------------------------------------
uint8_t* ftdi_axi_driver::recv_data(uint16_t seq_num, int length, int timeout_ms)
{
    bool     ok       = true;
    int      length4  = ((length + 3)/4) * 4;
    int      expected = sizeof(tStatusBlock) + length4;
    uint8_t *rd_buf   = new uint8_t[expected];
    int      rd_len   = m_port->read(rd_buf, expected, timeout_ms);
    if (rd_len == expected)
    {
        tStatusBlock *sts = (tStatusBlock *)&rd_buf[length4];
        if (sts->seq_num != seq_num)
        {
            fprintf(stderr, "ERROR: Sequence number: %04x != %04x\n", sts->seq_num, seq_num);
            ok = false;
        }   
    }
    else
    {
        fprintf(stderr, "ERROR: Failed to read data (got %d, expected %d)\n", rd_len, expected);
        ok = false;
    }

    if (!ok)
    {
        delete [] rd_buf;
        return NULL;
    }
    else
        return rd_buf;
}
//-------------------------------------------------------------
// send_drain: Send drain request
//-------------------------------------------------------------
bool ftdi_axi_driver::send_drain(int timeout_ms)
{
    uint8_t wr_buf[256];
    for (int i=0;i<sizeof(wr_buf);i++)
        wr_buf[i] = CMD_ID_DRAIN;
    m_port->write(wr_buf, sizeof(wr_buf), timeout_ms);

    return true;
}
//-------------------------------------------------------------
// send_echo: Send an echo request
//-------------------------------------------------------------
bool ftdi_axi_driver::send_echo(uint8_t *data, int length, int timeout_ms)
{
    int  length4 = ((length + 3)/4) * 4;
    bool ok      = true;

    if (!send_command(CMD_ID_ECHO, 0, data, length, timeout_ms))
    {
        fprintf(stderr, "ERROR: Failed to send echo data\n");
        ok = false;
    }
    else
    {
        uint8_t* rd_buf   = recv_data(m_seq_num - 1, length, timeout_ms);
        if (rd_buf)
        {
            for (int i=0;i<length;i++)
                if (data[i] != rd_buf[i])
                {
                    fprintf(stderr, "ERROR: ECHO mismatch %d: %02x != %02x\n", i, rd_buf[i], data[i]);
                    ok = false;
                }
            delete [] rd_buf;
        }
        else
            ok = false;
    }

    return ok;
}
//-------------------------------------------------------------
// gpio_write: Write Gpio
//-------------------------------------------------------------
bool ftdi_axi_driver::gpio_write(uint32_t value, int timeout_ms)
{
    bool ok = send_command(CMD_ID_GPIO_WR, 0, (uint8_t *)&value, 4, timeout_ms);
    if (ok)
    {
        uint8_t* rd_buf = recv_data(m_seq_num - 1, 0, timeout_ms);
        if (rd_buf)
            delete [] rd_buf;
        else
            ok = false;
    }
    return ok;
}
//-------------------------------------------------------------
// gpio_read: Read GPIO
//-------------------------------------------------------------
bool ftdi_axi_driver::gpio_read(uint32_t &value, int timeout_ms)
{
    bool ok = send_command(CMD_ID_GPIO_RD, 0, NULL, 4, timeout_ms);
    if (ok)
    {
        uint8_t* rd_buf   = recv_data(m_seq_num - 1, 4, timeout_ms);
        if (rd_buf)
        {
            value = *((uint32_t*)rd_buf);
            delete [] rd_buf;
        }
        else
            ok = false;
    }
    return ok;
}
//-------------------------------------------------------------
// write8: 8-bit write
//-------------------------------------------------------------
bool ftdi_axi_driver::write8(uint32_t addr, uint8_t data, int timeout_ms, bool posted)
{
    uint32_t wr_data = (uint32_t)data << (8 * (addr & 3));
    bool ok = send_command(posted ? CMD_ID_WRITE8 : CMD_ID_WRITE8_NP, addr, (uint8_t *)&wr_data, 4, timeout_ms);
    if (ok && !posted)
    {
        uint8_t* rd_buf = recv_data(m_seq_num - 1, 0, timeout_ms);
        if (rd_buf)
            delete [] rd_buf;
        else
            ok = false;
    }
    return ok;
}
//-------------------------------------------------------------
// write32: 32-bit write
//-------------------------------------------------------------
bool ftdi_axi_driver::write32(uint32_t addr, uint32_t data, int timeout_ms, bool posted)
{
    bool ok = send_command(posted ? CMD_ID_WRITE : CMD_ID_WRITE_NP, addr, (uint8_t *)&data, 4, timeout_ms);
    if (ok && !posted)
    {
        uint8_t* rd_buf = recv_data(m_seq_num - 1, 0, timeout_ms);
        if (rd_buf)
            delete [] rd_buf;
        else
            ok = false;
    }
    return ok;
}
//-------------------------------------------------------------
// read32: Blocking 32-bit read
//-------------------------------------------------------------
bool ftdi_axi_driver::read32(uint32_t addr, uint32_t &data, int timeout_ms)
{
    bool ok = send_command(CMD_ID_READ, addr, NULL, 4, timeout_ms);
    if (ok)
    {
        uint8_t* rd_buf   = recv_data(m_seq_num - 1, 4, timeout_ms);
        if (rd_buf)
        {
            data = *((uint32_t*)rd_buf);
            delete [] rd_buf;
        }
        else
            ok = false;
    }
    return ok;
}
//-------------------------------------------------------------
// write: Write a block of data
//-------------------------------------------------------------
bool ftdi_axi_driver::write(uint32_t addr, uint8_t *data, int length, int timeout_ms, bool posted)
{
    // Unaligned head
    while ((addr & 3) && length)
    {
        if (!write8(addr, *data, timeout_ms))
            return false;
        addr++;
        length--;
        data++;
    }

    uint8_t *wr_buf = m_write_buf;
    int chunks = 0;

    while (length >= 4)
    {
        int  size = (length < MAX_CHUNK_SIZE) ? (length & ~3) : MAX_CHUNK_SIZE;
        bool last = ((length - size) < MAX_CHUNK_SIZE) || (chunks >= (MAX_WR_CHUNKS-1));
        wr_buf += fill_command(wr_buf, last ? CMD_ID_WRITE_NP : CMD_ID_WRITE, addr, data, size);
        addr   += size;
        data   += size;
        length -= size;
        chunks += 1;

        if (last)
        {
            int sent = m_port->write(m_write_buf, wr_buf - m_write_buf, timeout_ms);

            uint8_t* rd_buf = recv_data(m_seq_num - 1, 0, timeout_ms);
            if (rd_buf)
                delete [] rd_buf;
            else
                return false;

            chunks = 0;
            wr_buf = m_write_buf;
        }
    }

    // Unaligned tail
    while (length)
    {
        if (!write8(addr, *data, timeout_ms))
            return false;
        addr++;
        length--;
        data++;
    }

    return true;
}
//-------------------------------------------------------------
// read: Read a block of data
//-------------------------------------------------------------
bool ftdi_axi_driver::read(uint32_t addr, uint8_t *data, int length, int timeout_ms)
{
    // Unaligned head
    if (addr & 3)
    {
        uint32_t dw;
        if (!read32(addr, dw, timeout_ms))
            return false;

        for (int b=(addr & 3);b<4;b++)
        {
            *data++ = (dw >> (8*b));
            addr++;
            length--;
        }
    }

    uint8_t *wr_buf = m_write_buf;
    int chunks = 0;
    int expected = 0;

    while (length >= 4)
    {
        int  size = (length < MAX_CHUNK_SIZE) ? (length & ~3) : MAX_CHUNK_SIZE;
        bool last = ((length - size) < MAX_CHUNK_SIZE) || (chunks >= (MAX_RD_CHUNKS-1));
        wr_buf += fill_command(wr_buf, CMD_ID_READ, addr, NULL, size);
        addr   += size;
        length -= size;
        chunks += 1;
        expected += size + 4;

        if (last)
        {
            int sent   = m_port->write(m_write_buf, wr_buf - m_write_buf, timeout_ms);
            int rd_len = m_port->read(m_read_buf, expected, timeout_ms);
            if (rd_len < 0)
                return false;

            // Wait for remaining data
            if (rd_len != expected)
            {
                int remain = expected - rd_len;
                int retry  = m_port->read(&m_read_buf[rd_len], remain, timeout_ms);
                if (retry != remain)
                {
                    fprintf(stderr, "ERROR: Data underflow\n");
                    return false;
                }
            }

            uint8_t *p = m_read_buf;
            int data_ready = expected - (chunks * 4);
            for (int i=0;i<chunks;i++)
            {
                int remain = (data_ready < MAX_CHUNK_SIZE) ? data_ready : MAX_CHUNK_SIZE;
                memcpy(data, p, remain);
                data += remain;
                p += remain;
                data_ready -= remain;

                // Skip status block
                p += 4;
            }

            chunks = 0;
            expected = 0;
            wr_buf = m_write_buf;
        }
    }

    // Unaligned tail
    if (length)
    {
        uint32_t dw;
        if (!read32(addr, dw, timeout_ms))
            return false;

        for (int b=0;b<length;b++)
            *data++ = (dw >> (8*b));
    }

    return true;
}
