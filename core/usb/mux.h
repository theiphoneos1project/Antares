//
// Huge thanks to EthanArbuckle!
//

#ifndef MUX_H
#define MUX_H

#include <libusb-1.0/libusb.h>
#include <string.h>
#include <stdint.h>
#include "platform.h"

#define APPLE_VENDOR_ID 0x05ac
#define MAX_PACKET_SIZE 512
#define MAX_TRANSFER_SIZE 16384
#define USB_TIMEOUT_MS 5000
#define DRAIN_TIMEOUT_MS 200
#define DRAIN_TOTAL_MS 500

#define MUX_PROTO_TCP 6
#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_ACK 0x10
#define WINDOW 0xFFFF

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    libusb_device_handle *handle;
    uint8_t ep_out;
    uint8_t ep_in;
    int interface_num;
    uint8_t rx_buf[MAX_TRANSFER_SIZE * 4];
    int rx_len;
} usb_pipe_t;

typedef struct {
    usb_pipe_t *pipe;
    uint16_t device_port;
    uint16_t src_port;
    uint32_t tx_seq;
    uint32_t rx_ack;
    uint8_t prebuf[MAX_TRANSFER_SIZE];
    int prebuf_len;
} mux_session_t;

static inline uint32_t r32be(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return __builtin_bswap32(v);
}

static inline void w32be(uint8_t *p, uint32_t v) {
    uint32_t t = __builtin_bswap32(v);
    memcpy(p, &t, sizeof(t));
}

static inline void w16be(uint8_t *p, uint16_t v) {
    uint16_t t = __builtin_bswap16(v);
    memcpy(p, &t, sizeof(t));
}

int find_and_claim(libusb_context *ctx, libusb_device_handle **out_handle, uint8_t *out_ep_out, uint8_t *out_ep_in, int *out_intf);

int session_connect(mux_session_t *s);
int session_recv(mux_session_t *s, uint8_t *buf, int bufsz);
int session_send_frame(mux_session_t *s, uint8_t flags, const uint8_t *data, int data_len);
void session_close(mux_session_t *s);

void forward_loop(int tcp_fd, mux_session_t *session);

#ifdef __cplusplus
}
#endif

#endif
