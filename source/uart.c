// uart.c — R6551 ACIA emulation. See uart.h for documentation.
//
// I do not claim this program as my own work; To be quite truthful, I 
// I took it off the internet, and cannot remember from where. I wanted
// to give the emulator a way to actually use the UART controllers 
// but honestly, I know almost nothing about networking and this
// specific peripheral in general. 

#include "uart.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#define ACIA_RX_BUF  256
#define ACIA_MAX_UNITS 4

typedef struct {
    int      listen_fd;
    int      client_fd;
    uint8_t  rx_buf[ACIA_RX_BUF];
    uint16_t rx_head;
    uint16_t rx_tail;
} UARTNet;

typedef struct {
    uint8_t  tdr, rdr;
    uint8_t  status;
    uint8_t  command;
    uint8_t  control;
    uint32_t tx_countdown;
    uint32_t rx_countdown;
    bool     tx_active;
    bool     rx_active;
    uint8_t  rx_next;
} ACIA;

static acia_config_t cfg;
static UARTNet       uartnet[ACIA_MAX_UNITS];
static ACIA          acia[ACIA_MAX_UNITS];
static uint32_t      periph_div_counter = 0;

// ------------------------------------------------------------------
// Transport layer (TCP). Replace these functions to change transport.
// ------------------------------------------------------------------
static int net_setup(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in a = {0};
    a.sin_family      = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port        = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
        perror("bind"); close(fd); return -1;
    }
    if (listen(fd, 1) < 0) { perror("listen"); close(fd); return -1; }
    fcntl(fd, F_SETFL, O_NONBLOCK);
    printf("ACIA: unit on 127.0.0.1:%d\n", port);
    return fd;
}

static void net_accept(int i) {
    if (uartnet[i].client_fd >= 0) return;
    int c = accept(uartnet[i].listen_fd, NULL, NULL);
    if (c < 0) return;
    fcntl(c, F_SETFL, O_NONBLOCK);
    uartnet[i].client_fd = c;
    uartnet[i].rx_head = uartnet[i].rx_tail = 0;
    printf("ACIA: unit %d client connected\n", i);
}

static void net_poll(int i) {
    if (uartnet[i].client_fd < 0) return;
    uint8_t tmp[64];
    ssize_t n = recv(uartnet[i].client_fd, tmp, sizeof(tmp), 0);
    if (n == 0) {
        printf("ACIA: unit %d client disconnected\n", i);
        close(uartnet[i].client_fd);
        uartnet[i].client_fd = -1;
        return;
    }
    if (n < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) return;
        close(uartnet[i].client_fd);
        uartnet[i].client_fd = -1;
        return;
    }
    for (ssize_t k = 0; k < n; k++) {
        uint16_t next = (uartnet[i].rx_tail + 1) % ACIA_RX_BUF;
        if (next == uartnet[i].rx_head) break;
        uartnet[i].rx_buf[uartnet[i].rx_tail] = tmp[k];
        uartnet[i].rx_tail = next;
    }
}

static int net_rx_available(int i) {
    return uartnet[i].rx_head != uartnet[i].rx_tail;
}

static uint8_t net_rx_pop(int i) {
    uint8_t b = uartnet[i].rx_buf[uartnet[i].rx_head];
    uartnet[i].rx_head = (uartnet[i].rx_head + 1) % ACIA_RX_BUF;
    return b;
}

static void net_send(int i, uint8_t b) {
    if (uartnet[i].client_fd < 0) return;
    send(uartnet[i].client_fd, &b, 1, MSG_NOSIGNAL);
}

// ------------------------------------------------------------------
// ACIA core
// ------------------------------------------------------------------
static uint32_t periph_ticks_per_byte(uint8_t control) {
    static const uint32_t baud[16] = {
        115200, 50, 75, 109, 134, 150, 300, 600,
        1200, 1800, 2400, 3600, 4800, 7200, 9600, 19200
    };
    uint32_t b    = baud[control & 0x0F];
    uint8_t  data = 8 - ((control >> 5) & 3);
    uint8_t  stop = 1 + ((control >> 7) & 1);
    uint32_t bits = 1 + data + stop;
    uint32_t periph_hz = cfg.cpu_hz / cfg.periph_div;
    return (uint32_t)(((uint64_t)bits * periph_hz) / b);
}

void acia_init(const acia_config_t *c) {
    cfg = *c;
    if (cfg.num_units < 0) cfg.num_units = 0;
    if (cfg.num_units > ACIA_MAX_UNITS) cfg.num_units = ACIA_MAX_UNITS;
    if (cfg.periph_div == 0) cfg.periph_div = 1;

    for (int i = 0; i < cfg.num_units; i++) {
        memset(&acia[i], 0, sizeof(acia[i]));
        acia[i].status = 0x10;   // TDRE set
        uartnet[i].listen_fd = net_setup(cfg.port_base + i);
        uartnet[i].client_fd = -1;
        uartnet[i].rx_head = uartnet[i].rx_tail = 0;
    }
    periph_div_counter = 0;
}

void acia_shutdown(void) {
    for (int i = 0; i < cfg.num_units; i++) {
        if (uartnet[i].client_fd >= 0) close(uartnet[i].client_fd);
        if (uartnet[i].listen_fd >= 0) close(uartnet[i].listen_fd);
        uartnet[i].client_fd = uartnet[i].listen_fd = -1;
    }
}

static void acia_tick_one(int i) {
    ACIA *a = &acia[i];
    if (a->tx_active) {
        if (a->tx_countdown > 0) a->tx_countdown--;
        if (a->tx_countdown == 0) { a->status |= 0x10; a->tx_active = false; }
    }
    if (a->rx_active) {
        if (a->rx_countdown > 0) a->rx_countdown--;
        if (a->rx_countdown == 0) {
            if (a->status & 0x08) a->status |= 0x04;
            else { a->rdr = a->rx_next; a->status |= 0x08; }
            a->rx_active = false;
        }
    }
    if (!a->rx_active && net_rx_available(i)) {
        a->rx_next = net_rx_pop(i);
        a->rx_active = true;
        a->rx_countdown = periph_ticks_per_byte(a->control);
    }
}

void acia_tick(void) {
    if (++periph_div_counter < cfg.periph_div) return;
    periph_div_counter = 0;
    for (int i = 0; i < cfg.num_units; i++) acia_tick_one(i);
}

void acia_poll(void) {
    for (int i = 0; i < cfg.num_units; i++) {
        net_accept(i);
        net_poll(i);
    }
}

uint8_t acia_read(int unit, int reg) {
    if (unit < 0 || unit >= cfg.num_units) return 0;
    ACIA *a = &acia[unit];
    switch (reg) {
        case 0: { uint8_t b = a->rdr; a->status &= ~0x0F; return b; }
        case 1: return a->status;
        case 2: return a->command;
        case 3: return a->control;
    }
    return 0;
}

void acia_write(int unit, int reg, uint8_t val) {
    if (unit < 0 || unit >= cfg.num_units) return;
    ACIA *a = &acia[unit];
    switch (reg) {
        case 0:
            a->tdr = val;
            a->status &= ~0x10;
            a->tx_countdown = periph_ticks_per_byte(a->control);
            a->tx_active = true;
            net_send(unit, val);
            return;
        case 1:
            a->command &= ~0x1F;
            a->status  &= ~0x04;
            return;
        case 2: a->command = val; return;
        case 3: a->control = val; return;
    }
}