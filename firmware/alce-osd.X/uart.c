/*
    AlceOSD - Graphical OSD
    Copyright (C) 2015  Luis Alves

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "alce-osd.h"

#define UART_PROCESS_PRIO   50
#define UART_FIFO_MASK      0x3ff


const struct uart_regs {
    volatile unsigned int *BRG;
    unsigned int TXRP;
    volatile unsigned int *RXRP;
    volatile unsigned int *RX;
    volatile unsigned int *STA;
    volatile unsigned int *MODE;
} UARTS[] = {
    {
        .BRG = &U1BRG,
        .TXRP = 1,
        .RXRP = &RPINR18,
        .RX = &U1RXREG,
        .STA = &U1STA,
        .MODE = &U1MODE,
    },
    {
        .BRG = &U2BRG,
        .TXRP = 3,
        .RXRP = &RPINR19,
        .RX = &U2RXREG,
        .STA = &U2STA,
        .MODE = &U2MODE,
    },
    {
        .BRG = &U3BRG,
        .TXRP = 0x1b,
        .RXRP = &RPINR27,
        .RX = &U3RXREG,
        .STA = &U3STA,
        .MODE = &U3MODE,
    },
    {
        .BRG = &U4BRG,
        .TXRP = 0x1d,
        .RXRP = &RPINR28,
        .RX = &U4RXREG,
        .STA = &U4STA,
        .MODE = &U4MODE,
    },
};



const struct param_def params_uart12[] = {
    PARAM("SERIAL1_MODE", MAV_PARAM_TYPE_UINT8, &config.uart[0].mode, uart_set_config_clients),
    PARAM("SERIAL1_BAUD", MAV_PARAM_TYPE_UINT8, &config.uart[0].baudrate, uart_set_config_baudrates),
    PARAM("SERIAL2_MODE", MAV_PARAM_TYPE_UINT8, &config.uart[1].mode, uart_set_config_clients),
    PARAM("SERIAL2_BAUD", MAV_PARAM_TYPE_UINT8, &config.uart[1].baudrate, uart_set_config_baudrates),
    PARAM_END,
};

const struct param_def params_uart34[] = {
    PARAM("SERIAL3_MODE", MAV_PARAM_TYPE_UINT8, &config.uart[2].mode, uart_set_config_clients),
    PARAM("SERIAL3_BAUD", MAV_PARAM_TYPE_UINT8, &config.uart[2].baudrate, uart_set_config_baudrates),
    PARAM("SERIAL4_MODE", MAV_PARAM_TYPE_UINT8, &config.uart[3].mode, uart_set_config_clients),
    PARAM("SERIAL4_BAUD", MAV_PARAM_TYPE_UINT8, &config.uart[3].baudrate, uart_set_config_baudrates),
    PARAM_END,
};


const struct baudrate_tbl baudrates[] = {
    { .baudrate = 19200,  .brg = 227, .brgh = 0 },
    { .baudrate = 57600,  .brg = 76, .brgh = 0 },
    { .baudrate = 115200, .brg = 37, .brgh = 0 },
    { .baudrate = 921600, .brg = 18, .brgh = 1 },
};

static const struct hw_pin_map_table {
    unsigned int rx;
    unsigned int tx;
} hw_pin_map[5][4] = {
    /* telemetry, con2, icsp, con3 */
    /* hw0v1 */
    { { .rx = 38, .tx = 37 }, { .rx = 20, .tx = 41 }, { .rx = 0,  .tx = 0  }, { .rx = 0,  .tx = 0  } },
    /* hw0v2 */
    { { .rx = 38, .tx = 37 }, { .rx = 20, .tx = 36 }, { .rx = 0,  .tx = 0  }, { .rx = 34, .tx = 35 } },
    /* hw0v3 */
    { { .rx = 43, .tx = 42 }, { .rx = 38, .tx = 37 }, { .rx = 45, .tx = 39 }, { .rx = 34, .tx = 35 } },
    /* hw0v4 */
    { { .rx = 43, .tx = 42 }, { .rx = 38, .tx = 37 }, { .rx = 45, .tx = 39 }, { .rx = 34, .tx = 35 } },
    /* hw0v5 */
    { { .rx = 44, .tx = 43 }, { .rx = 47, .tx = 42 }, { .rx = 55, .tx = 54 }, { .rx = 39, .tx = 40 } },
};

#define UART_CLIENTS_MAX 10
static struct uart_client *uart_client_list[UART_CLIENTS_MAX] = { NULL };
static struct uart_client *port_clients[4] = {NULL, NULL, NULL, NULL};

struct uart_fifo_s {
    unsigned char buf[UART_FIFO_MASK+1];
    unsigned int rd, wr;
    unsigned int overflow, overrun, m, max;
    
    __eds__ unsigned char *tx_buf;
    unsigned int full;
    
    u32 rx, tx;
};

static struct uart_fifo_s uart_fifo[4];


/* tx dma buffers */
__eds__ unsigned char tx_buf1[UART_TX_BUF_SIZE] __attribute__((eds,space(dma),address(0x6000-UART_TX_BUF_SIZE)));
__eds__ unsigned char tx_buf2[UART_TX_BUF_SIZE] __attribute__((eds,space(dma),address(0x6000-(UART_TX_BUF_SIZE*2))));
__eds__ unsigned char tx_buf3[UART_TX_BUF_SIZE] __attribute__((eds,space(dma),address(0x6000-(UART_TX_BUF_SIZE*3))));
__eds__ unsigned char tx_buf4[UART_TX_BUF_SIZE] __attribute__((eds,space(dma),address(0x6000-(UART_TX_BUF_SIZE*4))));


static const char keywords[] = "I want to enter AlceOSD setup";
static const unsigned char answer[] = "AlceOSD setup starting";
static unsigned char key_idx[4] = {0, 0, 0, 0};


const char *UART_CLIENT_NAMES[] = {
    "off",
    "mavlink",
    "uavtalk",
    "shell",
    "frsky"
};
const char *UART_PIN_NAMES[] = {
    "telemetry",
    "con2",
    "con3",
    "icsp",
    "off"
};


void uart_set_client(unsigned char port, unsigned char client_id,
                        unsigned char force);

inline unsigned long uart_get_baudrate(unsigned char b)
{
    if (b < UART_BAUDRATES)
        return baudrates[b].baudrate;
    else
        return 0;
}

static inline void uart_set_baudrate(unsigned char port, unsigned char b)
{
    if (b < UART_BAUDRATES)
        *(UARTS[port].BRG) = baudrates[b].brg;
    
    if (baudrates[b].brgh)
        *(UARTS[port].MODE) |= (1 << 3);
    else
        *(UARTS[port].MODE) &= ~(1 << 3);
}

inline static void handle_uart_int(unsigned char port)
{
    unsigned int n_wr = uart_fifo[port].wr;
    unsigned char ch;
    u8 cnt = 0;

    /* if set, clear overflow bit */
    if (*(UARTS[port].STA) & 2) {
        *(UARTS[port].STA) &= ~2;
        uart_fifo[port].overflow++;
    }

    while (*(UARTS[port].STA) & 1) {
        n_wr = (n_wr + 1) & UART_FIFO_MASK;
        
        ch = *(UARTS[port].RX);
        if (keywords[key_idx[port]] == ch) {
            key_idx[port]++;
            if (key_idx[port] == (sizeof(keywords)-1)) {
                if (uart_get_client(port)->id == UART_CLIENT_SHELL)
                    return;
                uart_set_client(port, UART_CLIENT_SHELL, 1);
                uart_get_client(port)->write((unsigned char*) answer, sizeof(answer)-1);
                //while ( (*(UARTS[port].STA) & 0x0100) == 0);
                uart_fifo[port].rd = uart_fifo[port].wr;
                //uart_set_baudrate(port, UART_BAUD_115200);
                key_idx[port] = 0;
                return;
            }
        } else {
            key_idx[port] = 0;
        }
        
        if (port_clients[port] == NULL)
            return;
        
        if (n_wr != uart_fifo[port].rd) {
            uart_fifo[port].buf[uart_fifo[port].wr] = ch;
            uart_fifo[port].wr = n_wr;
        } else {
            uart_fifo[port].overrun++;
        }
        
        uart_fifo[port].max = max(uart_fifo[port].max, (uart_fifo[port].wr - uart_fifo[port].rd) & UART_FIFO_MASK);
        
        cnt++;
    }
    uart_fifo[port].m = max(uart_fifo[port].m, cnt);
    uart_fifo[port].rx += (u32) cnt;
}

void __attribute__((__interrupt__, auto_psv)) _U1RXInterrupt(void)
{
    handle_uart_int(0);
    IFS0bits.U1RXIF = 0;
}

void __attribute__((__interrupt__, auto_psv)) _U2RXInterrupt(void)
{
    handle_uart_int(1);
    IFS1bits.U2RXIF = 0;
}

void __attribute__((__interrupt__, auto_psv)) _U3RXInterrupt(void)
{
    handle_uart_int(2);
    IFS5bits.U3RXIF = 0;
}

void __attribute__((__interrupt__, auto_psv)) _U4RXInterrupt(void)
{
    handle_uart_int(3);
    IFS5bits.U4RXIF = 0;
}

void uart_set_direction(unsigned char port, unsigned char direction)
{
    unsigned char t;
    
    if ((hw_rev < 0x03) && (port > 1))
        return;
   
    if (direction == UART_DIR_TX) {
        /* disable rx */
        switch (port) {
            case 0:
                _U1RXIE = 0;
                break;
            case 1:
                _U2RXIE = 0;
                break;
            case 2:
                _U3RXIE = 0;
                break;
            case 3:
                _U4RXIE = 0;
                break;
            default:
                break;
        }
        
        /* enable tx */
        *(UARTS[port].STA) |= 0x0400;
    } else {
        /* disable tx */
        while ( (*(UARTS[port].STA) & 0x0100) == 0);
        *(UARTS[port].STA) &= ~0x0400;
        
        /* enable rx */
        while (*(UARTS[port].STA) & 1)
            t = *(UARTS[port].RX);
        if (*(UARTS[port].STA) & 2)
            *(UARTS[port].STA) &= ~2;

        switch (port) {
            case 0:
                _U1RXIF = 0;
                _U1RXIE = 1;
                break;
            case 1:
                _U2RXIF = 0;
                _U2RXIE = 1;
                break;
            case 2:
                _U3RXIF = 0;
                _U3RXIE = 1;
                break;
            case 3:
                _U4RXIF = 0;
                _U4RXIE = 1;
                break;
            default:
                break;
        }
    }
}


static void uart_init_(unsigned char port)
{
    uart_set_baudrate(port, UART_BAUD_115200);
    
    switch (port) {
        case UART_PORT1:
            _U1RXIP = 1;
            _U1RXIF = 0;
            _U1RXIE = 1;
            DMA0CON = 0x6001; /* one-shot p-p disabled, ram to per, byte mode */
            DMA0REQ = 0x0c; // U1TX interrupt requests transfer
            DMA0PAD = (volatile unsigned int) &U1TXREG; // Transfer to U1TXREG
            DMA0STAL = __builtin_dmaoffset(&tx_buf1);
            DMA0STAH = __builtin_dmapage(&tx_buf1);
            uart_fifo[0].tx_buf = tx_buf1;
            break;
        case UART_PORT2:
            _U2RXIP = 1;
            _U2RXIF = 0;
            _U2RXIE = 1;
            DMA1CON = 0x6001; /* one-shot p-p disabled, ram to per, byte mode */
            DMA1REQ = 0x1f; // U2TX interrupt requests transfer
            DMA1PAD = (volatile unsigned int) &U2TXREG; // Transfer to U2TXREG
            DMA1STAL = __builtin_dmaoffset(&tx_buf2);
            DMA1STAH = __builtin_dmapage(&tx_buf2);
            uart_fifo[1].tx_buf = tx_buf2;
            break;
        case UART_PORT3:
            _U3RXIP = 1;
            _U3RXIF = 0;
            _U3RXIE = 1;
            DMA2CON = 0x6001; /* one-shot p-p disabled, ram to per, byte mode */
            DMA2REQ = 0x53; // U3TX interrupt requests transfer
            DMA2PAD = (volatile unsigned int) &U3TXREG; // Transfer to U3TXREG
            DMA2STAL = __builtin_dmaoffset(&tx_buf3);
            DMA2STAH = __builtin_dmapage(&tx_buf3);
            uart_fifo[2].tx_buf = tx_buf3;
            break;
        case UART_PORT4:
            _U4RXIP = 1;
            _U4RXIF = 0;
            _U4RXIE = 1;
            DMA3CON = 0x6001; /* one-shot p-p disabled, ram to per, byte mode */
            DMA3REQ = 0x59; // U4TX interrupt requests transfer
            DMA3PAD = (volatile unsigned int) &U4TXREG; // Transfer to U4TXREG
            DMA3STAL = __builtin_dmaoffset(&tx_buf4);
            DMA3STAH = __builtin_dmapage(&tx_buf4);
            uart_fifo[3].tx_buf = tx_buf4;
            break;
        default:
            break;
    }
    
    /* enable uart */
    *(UARTS[port].MODE) = 0x8000;
    /* enable tx */
    *(UARTS[port].STA)  = 0x0400;
}

static inline unsigned int uart_count(unsigned char port)
{
    return (uart_fifo[port].wr - uart_fifo[port].rd) & UART_FIFO_MASK;
}

static inline void uart_discard(unsigned char port, unsigned int count)
{
    uart_fifo[port].rd = (uart_fifo[port].rd + count) & UART_FIFO_MASK;
}

static unsigned int uart_read(u8 port, u8 *buf, u16 len)
{
    u8 *rdbuf = &uart_fifo[port].buf[uart_fifo[port].rd];
    u16 half = UART_FIFO_MASK + 1 - uart_fifo[port].rd;
    u16 size;

    if (len == 0)
        size = uart_count(port);
    else
        size = len;
    
    if (len > half) {
        //shell_printf("\n\nHALF: wr=%u rd=%u len=%u half=%u\n\n", uart_fifo[port].wr, uart_fifo[port].rd, half, len);
        memcpy(buf, rdbuf, half);
        buf += half;
        len -= half;
        rdbuf = uart_fifo[port].buf;
    }
    memcpy(buf, rdbuf, len);
    
    uart_discard(port, size);
    return size;
}

static inline unsigned int uart1_read(unsigned char *buf, unsigned int len)
{
    return uart_read(0, buf, len);
}
static inline unsigned int uart2_read(unsigned char *buf, unsigned int len)
{
    return uart_read(1, buf, len);
}
static inline unsigned int uart3_read(unsigned char *buf, unsigned int len)
{
    return uart_read(2, buf, len);
}
static inline unsigned int uart4_read(unsigned char *buf, unsigned int len)
{
    return uart_read(3, buf, len);
}

static inline unsigned int uart1_avail() { return uart_count(0); }
static inline unsigned int uart2_avail() { return uart_count(1); }
static inline unsigned int uart3_avail() { return uart_count(2); }
static inline unsigned int uart4_avail() { return uart_count(3); }

static int uart1_write(unsigned char *buf, unsigned int len)
{
    __eds__ unsigned char *b = tx_buf1;
    
    if (len == 0)
        return 0;
    
    if (U1STAbits.TRMT == 0) {
        uart_fifo[0].full++;
        return 1;
        //while (U1STAbits.TRMT == 0);
    }

    len = min(len, UART_TX_BUF_SIZE);
    uart_fifo[0].tx += (u32) len;
    DMA0CNT = len - 1;
    while (len-- > 0)
        *b++ = *buf++;

    DMA0CONbits.CHEN = 1;
    DMA0REQbits.FORCE = 1;
    return 0;
}

static int uart2_write(unsigned char *buf, unsigned int len)
{
    __eds__ unsigned char *b = tx_buf2;
    
    if (len == 0)
        return 0;
    
    if (U2STAbits.TRMT == 0) {
        uart_fifo[1].full++;
        return 1;
        //while (U2STAbits.TRMT == 0);
    }

    len = min(len, UART_TX_BUF_SIZE);
    uart_fifo[1].tx += (u32) len;
    DMA1CNT = len - 1;
    while (len-- > 0)
        *b++ = *buf++;

    DMA1CONbits.CHEN = 1;
    DMA1REQbits.FORCE = 1;
    return 0;
}

static int uart3_write(unsigned char *buf, unsigned int len)
{
    __eds__ unsigned char *b = tx_buf3;
    
    if (len == 0)
        return 0;
    
    if (U3STAbits.TRMT == 0) {
        uart_fifo[2].full++;
        return 1;
        //while (U3STAbits.TRMT == 0);
    }

    len = min(len, UART_TX_BUF_SIZE);
    uart_fifo[2].tx += (u32) len;
    DMA2CNT = len - 1;
    while (len-- > 0)
        *b++ = *buf++;

    DMA2CONbits.CHEN = 1;
    DMA2REQbits.FORCE = 1;
    return 0;
}

static int uart4_write(unsigned char *buf, unsigned int len)
{
    __eds__ unsigned char *b = tx_buf4;
    
    if (len == 0)
        return 0;
    
    if (U4STAbits.TRMT == 0) {
        uart_fifo[3].full++;
        return 1;
        //while (U4STAbits.TRMT == 0);
    }

    len = min(len, UART_TX_BUF_SIZE);
    uart_fifo[3].tx += (u32) len;
    DMA3CNT = len - 1;
    while (len-- > 0)
        *b++ = *buf++;

    DMA3CONbits.CHEN = 1;
    DMA3REQbits.FORCE = 1;
    return 0;
}

unsigned char uart_getc(unsigned char port, char *c)
{
    unsigned char ret = (uart_fifo[port].rd != uart_fifo[port].wr);
    if (ret) {
        *c = uart_fifo[port].buf[uart_fifo[port].rd++];
        uart_fifo[port].rd &= UART_FIFO_MASK;
    }
    return ret;
}


#ifdef REDIRECT_LIBC_WRITE
int __attribute__((__weak__, __section__(".libc"))) write(int handle, void *buf, unsigned int len)
{
    switch (handle) {
        case 0:
        case 1:
        case 2:
            console_printn((char *) buf, len);
            break;
    }
    return len;
}
#endif

static void uart_set_pins(unsigned char port, unsigned char pins)
{
    unsigned char i;
    
    if (((hw_rev < 0x03) && (port > 1)) || (pins >= UART_PINS))
        return;
    
    if (pins == UART_PINS_OFF) {
        /* disable rx pins*/
        *(UARTS[port].RXRP) = 0;
    } else {
        /* check if pins are already taken by another port */
        for (i = 0; i < 4; i++) {
            if ((config.uart[i].pins == pins) && (i != port)) {
                return;
            }
        }
        /* setup rx pins */
        *(UARTS[port].RXRP) = hw_pin_map[hw_rev-1][pins].rx;
    }
    
    /* setup tx pins */
    switch (pins) {
        case UART_PINS_TELEMETRY:
            if (hw_rev >= 0x05) {
                _RP43R = UARTS[port].TXRP;
            } else if (hw_rev >= 0x03) {
                _RP42R = UARTS[port].TXRP;
            } else {
                _RP37R = UARTS[port].TXRP;
            }
            break;
        case UART_PINS_CON2:
            if (hw_rev >= 0x05) {
                _RP42R = UARTS[port].TXRP;
            } else if (hw_rev >= 0x03) {
                _RP37R = UARTS[port].TXRP;
            } else if (hw_rev >= 0x02) {
                _RP36R = UARTS[port].TXRP;
            } else {
                _RP41R = UARTS[port].TXRP;
            }
            break;
        case UART_PINS_CON3:
            if (hw_rev >= 0x05) {
                _RP54R = UARTS[port].TXRP;
            } else if (hw_rev >= 0x03) {
                _RP39R = UARTS[port].TXRP;
            }
            break;
        case UART_PINS_ICSP:
            if (hw_rev >= 0x05) {
                _RP40R = UARTS[port].TXRP;
            } else if (hw_rev >= 0x02) {
                _RP35R = UARTS[port].TXRP;
            }
            break;
        default:
            break;
    }
}

void uart_set_client(unsigned char port, unsigned char client_id,
                        unsigned char force)
{
    struct uart_client **clist = uart_client_list;
    struct uart_client **c = &port_clients[port];
    unsigned char i;

    if (port > 3)
        return;
    
    /* detach client */
    if ((*c) != NULL) {
        /* client_id is already on this port */
        if ((*c)->id == client_id)
            return;
        if ((*c)->close != NULL)
            (*c)->close(*c);
        (*c)->write = NULL;
    }
    
    if (client_id == UART_CLIENT_NONE)
        return;
    
    while (*clist != NULL) {
        if (((*clist)->id == client_id) &&
                    (((*clist)->write == NULL) || (force == 1))) {
            
            if ((*clist)->write != NULL) {
                for (i = 0; i < 4; i++) {
                    if (port_clients[i] == (*clist)) {
                        if (port_clients[i]->close != NULL)
                            port_clients[i]->close(port_clients[i]);
                        port_clients[i] = NULL;
                        break;
                    }
                }
            }
            
            *c = *clist;
            switch (port) {
                case UART_PORT1:
                    (*c)->write = uart1_write;
                    (*c)->read = uart1_read;
                    (*c)->avail = uart1_avail;
                    break;
                case UART_PORT2:
                    (*c)->write = uart2_write;
                    (*c)->read = uart2_read;
                    (*c)->avail = uart2_avail;
                    break;
                case UART_PORT3:
                    (*c)->write = uart3_write;
                    (*c)->read = uart3_read;
                    (*c)->avail = uart3_avail;
                    break;
                case UART_PORT4:
                    (*c)->write = uart4_write;
                    (*c)->read = uart4_read;
                    (*c)->avail = uart4_avail;
                    break;
                default:
                    break;
            }

            (*c)->port = port;
            if ((*c)->open != NULL)
                (*c)->open(*c);
            
            //*(UARTS[port].STA) &= ~0x00c0;
            //if (client_id == UART_CLIENT_MAVLINK)
            //    *(UARTS[port].STA) |= 0x00c0;

            break;
        }
        clist++;
    }

    /* client not found or no clients */
    if (*clist == NULL)
        *c = NULL;
}

struct uart_client* uart_get_client(unsigned char port)
{
    struct uart_client **c = &port_clients[port];
    if (port > 3)
        return NULL;
    return *c;
}

void uart_set_props(unsigned char port, unsigned int props)
{
    if ((hw_rev < 0x03) && (port > 1))
        return;

    if (props & UART_PROP_TX_INVERTED)
        *(UARTS[port].STA) |= 0x4000;
    else
        *(UARTS[port].STA) &= ~0x4000;

    if (props & UART_PROP_RX_INVERTED)
        *(UARTS[port].MODE) |= 0x0010;
    else
        *(UARTS[port].MODE) &= ~0x0010;
    
    if (props & UART_PROP_HALF_DUPLEX) {
        *(UARTS[port].RXRP) = hw_pin_map[hw_rev-1][config.uart[port].pins].tx;
        uart_set_direction(port, UART_DIR_RX);
    }
}

void uart_set_config_clients(void)
{
    uart_set_client(0, config.uart[0].mode, 0);
    uart_set_client(1, config.uart[1].mode, 0);
    if (hw_rev > 0x02) {
        uart_set_client(2, config.uart[2].mode, 0);
        uart_set_client(3, config.uart[3].mode, 0);
    }
}

void uart_set_config_baudrates(void)
{
    uart_set_baudrate(0, config.uart[0].baudrate);
    uart_set_baudrate(1, config.uart[1].baudrate);
    if (hw_rev > 0x02) {
        uart_set_baudrate(2, config.uart[2].baudrate);
        uart_set_baudrate(3, config.uart[3].baudrate);
    }
}

void uart_set_config_pins(void)
{
    uart_set_pins(0, config.uart[0].pins);
    uart_set_pins(1, config.uart[1].pins);
    if (hw_rev > 0x02) {
        uart_set_pins(2, config.uart[2].pins);
        uart_set_pins(3, config.uart[3].pins);
    }
}

void uart_add_client(struct uart_client *c)
{
    struct uart_client **clist = uart_client_list;
    while (*clist != NULL)
        clist++;

    *(clist++) = c;
    *clist = NULL;
}

void uart_init(void)
{
    extern int __C30_UART;
    __C30_UART = 1;

    memset(uart_fifo, 0, sizeof(struct uart_fifo_s) * 4);
    
    uart_set_config_pins();

    uart_init_(0);
    uart_init_(1);
    params_add(params_uart12);
    
    if (hw_rev > 0x02) {
        uart_init_(2);
        uart_init_(3);
        params_add(params_uart34);
    }
}


static void shell_cmd_stats(char *args, void *data)
{
    unsigned char i;

    shell_printf("\nStats:\n");
    for (i = 0; i < 4; i++) {
        shell_printf(" port%d: tx=%lu rx=%lu", i, uart_fifo[i].tx, uart_fifo[i].rx);
        shell_printf(" orun=%u oflow=%u txfull=%u (rxloop=%u max=%u)\n",
                uart_fifo[i].overrun, uart_fifo[i].overflow, uart_fifo[i].full,
                uart_fifo[i].m, uart_fifo[i].max);
        uart_fifo[i].m = 0;
        uart_fifo[i].max = 0;
        uart_fifo[i].overflow = 0;
        uart_fifo[i].full = 0;
    }
}

#define SHELL_CMD_CONFIG_ARGS   5
static void shell_cmd_config(char *args, void *data)
{
    struct shell_argval argval[SHELL_CMD_CONFIG_ARGS+1], *p, *now;
    struct uart_client **cli;
    unsigned char t, i, total = 0;
    int port;
    long baud;
    
    t = shell_arg_parser(args, argval, SHELL_CMD_CONFIG_ARGS);
    p = shell_get_argval(argval, 'p');

    if ((t < 2) || (p == NULL)) {
        shell_printf("Port settings (config):\n");
        for (i = 0; i < 4; i++) {
            shell_printf(" port%d: %6lubps pins=%-9s client=%s\n",
                    i, baudrates[config.uart[i].baudrate].baudrate,
                    UART_PIN_NAMES[config.uart[i].pins],
                    UART_CLIENT_NAMES[config.uart[i].mode]);
        }

        shell_printf("\nRegistered clients:\n");
        cli = uart_client_list;
        while (*cli != NULL) {
            shell_printf(" %-7s : ch=%d init=%04p close=%04p read=%04p write=%04p\n",
                    UART_CLIENT_NAMES[(*cli)->id], (*cli)->ch,
                    (*cli)->open, (*cli)->close, (*cli)->read, (*cli)->write);
            total++;
            cli++;
        }
        shell_printf("total=%d max=%d\n", total, UART_CLIENTS_MAX);

        shell_printf("\nActive clients:\n");
        cli = port_clients;
        for (i = 0; i < 4; i++) {
            if (cli[i] == NULL)
                continue;
            shell_printf(" port%d: client=%-7s ch=%d init=%04p close=%04p read=%04p write=%04p\n",
                    cli[i]->port, UART_CLIENT_NAMES[cli[i]->id], cli[i]->ch,
                    cli[i]->open, cli[i]->close, cli[i]->read, cli[i]->write);
        }

        shell_printf("syntax: -p <port> [-n] [-b <baudrate>] [-c <client>] [-i <pins>]\n");
        shell_printf(" -p <port>      uart port number: 0 to 3\n");
        shell_printf(" -n             apply changes now\n");
        shell_printf(" -b <baudrate>  baudrate:");
        for (i = 0; i < UART_BAUDRATES; i++)
            shell_printf(" %lu", baudrates[i].baudrate);
        shell_printf("\n -c <client>    client name:");
        for (i = 0; i < UART_CLIENTS; i++)
            shell_printf(" %s", UART_CLIENT_NAMES[i]);
        shell_printf("\n -i <pins>      connector:");
        for (i = 0; i < UART_PINS; i++)
            shell_printf(" %s", UART_PIN_NAMES[i]);
        shell_printf("\n");
    } else {
        now = shell_get_argval(argval, 'n');
        port = atoi(p->val);
        if ((port < 0) || (port >= UART_PORTS)) {
            shell_printf("error: invalid port '%d'\n", port);
            return;
        }
        p = shell_get_argval(argval, 'b');
        if (p != NULL) {
            baud = atol(p->val);
            for (i = 0; i < UART_BAUDRATES; i++) {
                if (baud == baudrates[i].baudrate) {
                    config.uart[port].baudrate = i;
                    if (now)
                        uart_set_baudrate(port, i);
                    break;
                }
            }
            if (i == UART_BAUDRATES) {
                shell_printf("error: invalid baudrate '%lu'\n", baud);
                return;
            }
        }
        p = shell_get_argval(argval, 'c');
        if (p != NULL) {
            for (i = 0; i < UART_CLIENTS; i++) {
                if (strcmp(p->val, UART_CLIENT_NAMES[i]) == 0) {
                    config.uart[port].mode = i;
                    if (now)
                        uart_set_client(port, i, 0);
                    break;
                }
            }
            if (i == UART_CLIENTS) {
                shell_printf("error: invalid client '%s'\n", p->val);
                return;
            }
        }
        p = shell_get_argval(argval, 'i');
        if (p != NULL) {
            for (i = 0; i < UART_PINS; i++) {
                if (strcmp(p->val, UART_PIN_NAMES[i]) == 0) {
                    config.uart[port].pins = i;
                    if (now)
                        uart_set_pins(port, i);
                    break;
                }
            }
            if (i == UART_PINS) {
                shell_printf("error: invalid pins '%s'\n", p->val);
                return;
            }
        }
        p = shell_get_argval(argval, 'x');
        if (p != NULL) {
            u16 v = atoi(p->val) & 3;
            *(UARTS[port].STA) &= ~0x00c0;
            *(UARTS[port].STA) |= (v << 6);
        }
    }
}

static const struct shell_cmdmap_s uart_cmdmap[] = {
    {"config", shell_cmd_config, "args: -p <port_nr> -b <baudrate> -c <client> -i <pins>", SHELL_CMD_SIMPLE},
    {"stats", shell_cmd_stats, "Display statistics", SHELL_CMD_SIMPLE},
    {"", NULL, ""},
};

void shell_cmd_uart(char *args, void *data)
{
    shell_exec(args, uart_cmdmap, data);
}
