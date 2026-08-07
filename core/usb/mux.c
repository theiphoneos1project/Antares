#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifndef _WIN32
#include <pthread.h>
#endif

#include "mux.h"
#include "platform.h"

typedef struct {
    mux_session_t *session;
    int tcp_fd;
    volatile int stop;
} fwd_ctx_t;

static void flag_str(uint8_t flags, char *out, size_t outsz) {
    out[0] = '\0';

    size_t written = 0;

    if (flags & TCP_SYN) {
        written += snprintf(out + written, outsz - written, "SYN|");
    }

    if (flags & TCP_ACK) {
        written += snprintf(out + written, outsz - written, "ACK|");
    }

    if (flags & TCP_FIN) {
        written += snprintf(out + written, outsz - written, "FIN|");
    }

    if (flags & TCP_RST) {
        written += snprintf(out + written, outsz - written, "RST|");
    }

    if (written > 0 && written < outsz && out[written - 1] == '|') {
        out[written - 1] = '\0';
    } else if (written == 0) {
        snprintf(out, outsz, "NONE");
    }
}

static int usb_pipe_write(usb_pipe_t *pipe, const uint8_t *data, int len) {
    int offset = 0;
    while (offset < len) {
        int chunk = len - offset;
        if (chunk > MAX_TRANSFER_SIZE) {
            chunk = MAX_TRANSFER_SIZE;
        }

        int transferred = 0;
        int r = libusb_bulk_transfer(pipe->handle, pipe->ep_out, (uint8_t *)data + offset, chunk, &transferred, USB_TIMEOUT_MS);
        if (r < 0) {
            fprintf(stderr, "[usb write] error: %s\n", libusb_strerror(r));
            return -1;
        }

        offset += transferred;
    }

    if (len % MAX_PACKET_SIZE == 0) {
        int transferred = 0;
        libusb_bulk_transfer(pipe->handle, pipe->ep_out, NULL, 0, &transferred, USB_TIMEOUT_MS);
    }

    return 0;
}

static int usb_pipe_read(usb_pipe_t *pipe, uint8_t *dst, int need) {
    int timeout_count = 0;

    while (pipe->rx_len < need) {
        uint8_t tmp[MAX_TRANSFER_SIZE];
        int transferred = 0;
        int r = libusb_bulk_transfer(pipe->handle, pipe->ep_in, tmp, MAX_TRANSFER_SIZE, &transferred, USB_TIMEOUT_MS);
        if (r == LIBUSB_ERROR_TIMEOUT) {
            timeout_count += 1;
            
            if (timeout_count > 3) {
                libusb_clear_halt(pipe->handle, pipe->ep_in);
                pipe->rx_len = 0;
                fprintf(stderr, "[usb read] timed out waiting for device response\n");
                return -1;
            }

            continue;
        } else if (r < 0) {
            libusb_clear_halt(pipe->handle, pipe->ep_in);
            pipe->rx_len = 0;
            fprintf(stderr, "[usb read] error: %s\n", libusb_strerror(r));
            return -1;
        }

        timeout_count = 0;

        if (transferred > 0) {
            if (pipe->rx_len + transferred > (int)sizeof(pipe->rx_buf)) {
                fprintf(stderr, "[usb read] rx_buf overflow\n");
                return -1;
            }

            memcpy(pipe->rx_buf + pipe->rx_len, tmp, transferred);
            pipe->rx_len += transferred;
        }
    }

    memcpy(dst, pipe->rx_buf, need);
    memmove(pipe->rx_buf, pipe->rx_buf + need, pipe->rx_len - need);
    pipe->rx_len -= need;

    return 0;
}

static int usb_pipe_poll(usb_pipe_t *pipe, int timeout_ms) {
    uint8_t tmp[MAX_TRANSFER_SIZE];
    int transferred = 0;
    int r = libusb_bulk_transfer(pipe->handle, pipe->ep_in, tmp, MAX_TRANSFER_SIZE, &transferred, timeout_ms);
    if (r == LIBUSB_ERROR_TIMEOUT) {
        return 0;
    }

    if (r < 0) {
        return 0;
    }

    if (transferred > 0 && pipe->rx_len + transferred <= (int)sizeof(pipe->rx_buf)) {
        memcpy(pipe->rx_buf + pipe->rx_len, tmp, transferred);
        pipe->rx_len += transferred;
    }

    return transferred;
}

int session_send_frame(mux_session_t *s, uint8_t flags, const uint8_t *data, int data_len) {
    int total_len = 28 + data_len;
    uint8_t frame[28 + MAX_TRANSFER_SIZE];

    w32be(frame + 0, MUX_PROTO_TCP);
    w32be(frame + 4, total_len);
    w16be(frame + 8, s->src_port);
    w16be(frame + 10, s->device_port);
    w32be(frame + 12, s->tx_seq);
    w32be(frame + 16, s->rx_ack);
    frame[20] = 0x50;
    frame[21] = flags;
    w16be(frame + 22, WINDOW);
    w16be(frame + 24, 0);
    w16be(frame + 26, 0);

    if (data && data_len > 0) {
        memcpy(frame + 28, data, data_len);
    }

    char fs[32];
    flag_str(flags, fs, sizeof(fs));
    if (usb_pipe_write(s->pipe, frame, total_len) < 0) {
        return -1;
    }

    if (flags & TCP_SYN || flags & TCP_FIN) {
        s->tx_seq += 1;
    }

    if (data_len > 0) {
        s->tx_seq += data_len;
    }

    return 0;
}

static int parse_frame(const uint8_t *buf, int buf_len, uint8_t *out_flags, uint32_t *out_seq, const uint8_t **out_payload, int *out_payload_len) {
    if (buf_len < 8) {
        return -1;
    }

    uint32_t total_len = r32be(buf + 4);
    if (total_len == 0 || total_len == 1) {
        return -1;
    }

    if ((int)total_len > buf_len || total_len < 28) {
        return -1;
    }

    uint32_t seq = r32be(buf + 12);
    uint8_t doff = buf[20];
    uint8_t flags = buf[21];

    int data_offset = (doff >> 4) * 4;
    int payload_len = (int)total_len - 8 - data_offset;
    if (payload_len < 0) {
        payload_len = 0;
    }

    *out_flags = flags;
    *out_seq = seq;
    *out_payload = buf + 8 + data_offset;
    *out_payload_len = payload_len;

    return (int)total_len;
}

static uint8_t recv_frame_buf[28 + MAX_TRANSFER_SIZE];

static int session_recv_frame(mux_session_t *s, uint8_t *out_flags, uint8_t **out_payload, int *out_payload_len) {
    uint8_t outer[8];
    if (usb_pipe_read(s->pipe, outer, 8) < 0) {
        return -1;
    }

    uint32_t total_len = r32be(outer + 4);
    if (total_len < 28 || total_len > sizeof(recv_frame_buf)) {
        fprintf(stderr, "[USB RX] bad total_len=%u\n", total_len);
        return -1;
    }

    memcpy(recv_frame_buf, outer, 8);
    if (usb_pipe_read(s->pipe, recv_frame_buf + 8, total_len - 8) < 0) {
        return -1;
    }

    uint8_t flags = 0;
    uint32_t seq = 0;
    const uint8_t *payload = NULL;
    int payload_len = 0;
    parse_frame(recv_frame_buf, total_len, &flags, &seq, &payload, &payload_len);

    char fs[32];
    flag_str(flags, fs, sizeof(fs));

    int consumed = payload_len;
    if (flags & TCP_SYN) {
        consumed++;
    }

    if (flags & TCP_FIN) {
        consumed++;
    }

    uint32_t new_ack = seq + consumed;
    if (consumed > 0 && new_ack > s->rx_ack) {
        s->rx_ack = new_ack;
    }

    *out_flags = flags;
    *out_payload = (uint8_t *)payload;
    *out_payload_len = payload_len;

    return 0;
}

int session_connect(mux_session_t *s) {
    while (usb_pipe_poll(s->pipe, 100) > 0) {
        // flush stale data
    }

    s->pipe->rx_len = 0;

    if (session_send_frame(s, TCP_SYN, NULL, 0) < 0) {
        return -1;
    }

    for (;;) {
        uint8_t flags;
        uint8_t *payload;
        int payload_len;
        if (session_recv_frame(s, &flags, &payload, &payload_len) < 0) {
            return -1;
        }

        if (flags & TCP_RST) {
            fprintf(stderr, "[handshake] Got RST\n");
            return -1;
        }

        if ((flags & TCP_SYN) && (flags & TCP_ACK)) {
            break;
        }
    }

    if (session_send_frame(s, TCP_ACK, NULL, 0) < 0) {
        return -1;
    }

#ifdef _WIN32
    ULONGLONG t0 = GetTickCount64();
    long deadline_ms = 500;
    for (;;) {
        ULONGLONG now = GetTickCount64();

        if ((long)(now - t0) >= deadline_ms) {
            break;
        }

        int got = usb_pipe_poll(s->pipe, DRAIN_TIMEOUT_MS);
        if (got > 0) {
            t0 = GetTickCount64();
            deadline_ms = 200;
        }
    }
#else
    struct timespec t0, now;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    long deadline_ms = 500;
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - t0.tv_sec) * 1000 + (now.tv_nsec - t0.tv_nsec) / 1000000;
        if (elapsed >= deadline_ms) {
            break;
        }

        int got = usb_pipe_poll(s->pipe, DRAIN_TIMEOUT_MS);
        if (got > 0) {
            clock_gettime(CLOCK_MONOTONIC, &t0);
            deadline_ms = 200;
        }
    }
#endif

    typedef struct {
        uint32_t seq;
        uint8_t flags;
    } seen_key_t;
    seen_key_t seen[64];
    int seen_n = 0;

    while (s->pipe->rx_len >= 8) {
        uint32_t total_len = r32be(s->pipe->rx_buf + 4);
        if ((int)total_len > s->pipe->rx_len) {
            break;
        }

        if (total_len < 28) {
            memmove(s->pipe->rx_buf, s->pipe->rx_buf + 1, --s->pipe->rx_len);
            continue;
        }

        uint8_t flags = 0;
        uint32_t seq = 0;
        const uint8_t *payload = NULL;
        int payload_len = 0;
        parse_frame(s->pipe->rx_buf, s->pipe->rx_len, &flags, &seq, &payload, &payload_len);

        char fs[32];
        flag_str(flags, fs, sizeof(fs));

        uint8_t fkey = flags & (TCP_SYN | TCP_FIN | TCP_ACK);
        int dup = 0;
        for (int i = 0; i < seen_n; i++) {
            if (seen[i].seq == seq && seen[i].flags == fkey) {
                dup = 1;
                break;
            }
        }
        
        if (!dup && seen_n < 64) {
            seen[seen_n].seq = seq;
            seen[seen_n].flags = fkey;
            seen_n++;
        }

        memmove(s->pipe->rx_buf, s->pipe->rx_buf + total_len, s->pipe->rx_len - total_len);
        s->pipe->rx_len -= total_len;

        if (dup) {
            continue;
        }

        int consumed = payload_len;
        if (flags & TCP_SYN) {
            consumed++;
        }

        if (flags & TCP_FIN) {
            consumed++;
        }

        uint32_t new_ack = seq + consumed;
        if (consumed > 0 && new_ack > s->rx_ack) {
            s->rx_ack = new_ack;
        }

        if (payload_len > 0) {
            if (s->prebuf_len + payload_len <= (int)sizeof(s->prebuf)) {
                memcpy(s->prebuf + s->prebuf_len, payload, payload_len);
                s->prebuf_len += payload_len;
            }

            session_send_frame(s, TCP_ACK, NULL, 0);
        }
    }

    return 0;
}

void session_close(mux_session_t *s) {
    session_send_frame(s, TCP_FIN | TCP_ACK, NULL, 0);
}

int session_recv(mux_session_t *s, uint8_t *buf, int bufsz) {
    if (s->prebuf_len > 0) {
        int n = s->prebuf_len;
        if (n > bufsz) {
            n = bufsz;
        }

        memcpy(buf, s->prebuf, n);
        memmove(s->prebuf, s->prebuf + n, s->prebuf_len - n);
        s->prebuf_len -= n;
        return n;
    }

    for (;;) {
        uint8_t flags;
        uint8_t *payload;
        int payload_len;
        if (session_recv_frame(s, &flags, &payload, &payload_len) < 0) {
            return -1;
        }

        if (flags & TCP_RST) {
            fprintf(stderr, "[recv] Device sent RST\n");
            return -1;
        }

        if (flags & TCP_FIN) {
            session_send_frame(s, TCP_FIN | TCP_ACK, NULL, 0);
            return -1;
        }

        if (payload_len > 0) {
            int n = payload_len;
            if (n > bufsz) {
                n = bufsz;
            }

            memcpy(buf, payload, n);
            session_send_frame(s, TCP_ACK, NULL, 0);
            return n;
        }
    }
}

static void *thread_tcp_to_usb(void *arg) {
    fwd_ctx_t *ctx = (fwd_ctx_t *)arg;
    uint8_t buf[4096];
    for (;;) {
        if (ctx->stop) {
            break;
        }

        ssize_t n = recv(ctx->tcp_fd, (char *)buf, sizeof(buf), 0);
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                usleep(10000);
                continue;
            }

            break;
        }

        if (session_send_frame(ctx->session, TCP_ACK, buf, (int)n) < 0) {
            break;
        }
    }

    ctx->stop = 1;
    return NULL;
}

static void *thread_usb_to_tcp(void *arg) {
    fwd_ctx_t *ctx = (fwd_ctx_t *)arg;
    uint8_t buf[MAX_TRANSFER_SIZE];
    for (;;) {
        if (ctx->stop) {
            break;
        }

        int n = session_recv(ctx->session, buf, sizeof(buf));
        if (n < 0) {
            break;
        }

        if (n > 0) {
            ssize_t sent = send(ctx->tcp_fd, (const char *)buf, n, 0);
            if (sent < 0) {
                char error_message[256] = {0};
#ifdef _WIN32
                int error_code = WSAGetLastError();

                FormatMessageA(
                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    error_message, sizeof(error_message), NULL
                );
#else
                int error_code = errno;
                (void)strerror_r(error_code, error_message, sizeof(error_message));
#endif
                fprintf(stderr, "[usb->tcp] send error (%d): %s\n", error_code, error_message);
                break;
            }
        }
    }

    ctx->stop = 1;
    return NULL;
}

void forward_loop(int tcp_fd, mux_session_t *session) {
    fwd_ctx_t ctx = { .session = session, .tcp_fd = tcp_fd, .stop = 0 };

#ifdef _WIN32
    HANDLE t1, t2;
    t1 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_tcp_to_usb, &ctx, 0, NULL);
    t2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)thread_tcp_to_usb, &ctx, 0, NULL);

    WaitForSingleObject(t1, INFINITE);
    WaitForSingleObject(t1, INFINITE);

    CloseHandle(t1);
    CloseHandle(t2);
#else
    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread_tcp_to_usb, &ctx);
    pthread_create(&t2, NULL, thread_usb_to_tcp, &ctx);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
#endif
}

int find_and_claim(libusb_context *ctx, libusb_device_handle **out_handle, uint8_t *out_ep_out, uint8_t *out_ep_in, int *out_intf) {
    libusb_device **list;
    ssize_t cnt = libusb_get_device_list(ctx, &list);
    if (cnt < 0) {
        fprintf(stderr, "libusb_get_device_list: %s\n", libusb_strerror((int)cnt));
        return -1;
    }

    int found = 0;
    for (ssize_t i = 0; i < cnt && !found; i++) {
        libusb_device *dev = list[i];
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(dev, &desc) < 0) {
            continue;
        }

        if (desc.idVendor != APPLE_VENDOR_ID) {
            continue;
        }

        int mux_cfg = -1;
        int mux_intf = -1;
        uint8_t ep_out = 0;
        uint8_t ep_in = 0;
        for (int c = 0; c < desc.bNumConfigurations && mux_cfg < 0; c++) {
            struct libusb_config_descriptor *cfg;
            if (libusb_get_config_descriptor(dev, (uint8_t)c, &cfg) < 0) {
                continue;
            }

            for (int ii = 0; ii < cfg->bNumInterfaces && mux_cfg < 0; ii++) {
                const struct libusb_interface *iface = &cfg->interface[ii];
                for (int a = 0; a < iface->num_altsetting && mux_cfg < 0; a++) {
                    const struct libusb_interface_descriptor *alt = &iface->altsetting[a];
                    if (alt->bInterfaceClass != 0xff || alt->bInterfaceSubClass != 0xfe || alt->bInterfaceProtocol != 0x02) {
                        continue;
                    }

                    uint8_t tmp_out = 0, tmp_in = 0;
                    for (int e = 0; e < alt->bNumEndpoints; e++) {
                        const struct libusb_endpoint_descriptor *ep = &alt->endpoint[e];
                        if ((ep->bmAttributes & 0x3) != LIBUSB_TRANSFER_TYPE_BULK) {
                            continue;
                        }

                        if ((ep->bEndpointAddress & 0x80) == LIBUSB_ENDPOINT_IN) {
                            tmp_in  = ep->bEndpointAddress;
                        }
                        else {
                            tmp_out = ep->bEndpointAddress;
                        }
                    }

                    if (!tmp_in || !tmp_out) {
                        continue;
                    }

                    mux_cfg = cfg->bConfigurationValue;
                    mux_intf = alt->bInterfaceNumber;
                    ep_out = tmp_out;
                    ep_in = tmp_in;
                }
            }

            libusb_free_config_descriptor(cfg);
        }

        if (mux_cfg < 0) {
            continue;
        }

        libusb_device_handle *handle;
        int r = libusb_open(dev, &handle);
        if (r < 0) {
            fprintf(stderr, "libusb_open failed: %s\n", libusb_strerror(r));
            continue;
        }

        if (libusb_has_capability(LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER)) {
            libusb_set_auto_detach_kernel_driver(handle, 1);
        }

        int cur_cfg = 0;
        libusb_get_configuration(handle, &cur_cfg);

        if (cur_cfg != mux_cfg) {
            if (libusb_set_configuration(handle, mux_cfg) < 0) {
                fprintf(stderr, "set_configuration failed\n");
                libusb_close(handle);
                continue;
            }
        }
        
        int cr = libusb_claim_interface(handle, mux_intf);
        if (cr < 0) {
            fprintf(stderr, "claim_interface %d failed: %s\n", mux_intf, libusb_strerror(cr));
            libusb_close(handle);
            continue;
        }
        
        *out_handle = handle;
        *out_ep_out = ep_out;
        *out_ep_in = ep_in;
        *out_intf = mux_intf;
        found = 1;
    }

    libusb_free_device_list(list, 1);
    return found ? 0 : -1;
}
