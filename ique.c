#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h> // for htons

#include <libusb.h>

/* magic numbers */
#define IQUE_VID 0x1527
#define IQUE_PID 0xbbdb

#define IQUE_EP_SEND (2 | LIBUSB_ENDPOINT_OUT)
#define IQUE_EP_RECV (2 | LIBUSB_ENDPOINT_IN)

static struct libusb_device_handle *devh = NULL;
int claimed = 0;

static int find_ique_device(void) {
    devh = libusb_open_device_with_vid_pid(NULL, IQUE_VID, IQUE_PID);
    return devh ? 0 : -EIO;
}

int ique_init(void) {
    int r;

    void ique_deinit(void);

    // call the cleanup when we're done
    atexit(ique_deinit);

    r = libusb_init(NULL);
    if (r < 0) {
        fprintf(stderr, "failed to initialize libusb\n");
        exit(1); // pretty much hosed
    }

    r = find_ique_device();
    if (r < 0) {
        fprintf(stderr, "Could not find/open device, is it plugged in?\n");
        return r;
    }

    r = libusb_claim_interface(devh, 0);
    if (r < 0) {
        fprintf(stderr, "usb_claim_interface error %d\n", r);
        return r;
    }

    return 0;
}

void ique_deinit(void) {
    if (claimed)
        libusb_release_interface(devh, 0);

    libusb_close(devh);
    libusb_exit(NULL);
}

static void dump_buf(unsigned char *buf, int len) {
    int i;
    for (i = 0; i < len; ++i)
        printf("%02x ", buf[i]);
    printf("\n");
    fflush(stdout);
}

int ique_get_led(void) {
    int r, ret, transferred;
    unsigned char buf[128] =
        { 0x43, 0x00, 0x00, 0x00,
          0x43, 0x17, 0x00, 0x00,
          0x42, 0x00, 0x00,       };

    r = libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 11, &transferred, 0);
    if (r < 0)
        return r;

    // read the data
    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 0);
    if (r < 0)
        return r;
    printf("received %d\n", transferred);
    dump_buf(buf, transferred);

    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 0);
    if (r < 0)
        return r;
    printf("received %d\n", transferred);
    dump_buf(buf, transferred);

    // get the result
    ret = buf[5];

    buf[0] = 0x44;
    r = libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 1, &transferred, 0);
    if (r < 0)
        return r;

    return ret;
}

int set_led(int on) {
    int r, ret, transferred;
    unsigned char buf[128] =
        { 0x43, 0x00, 0x00, 0x00,
          0x43, 0x1d, 0x00, 0x00,
          0x42, 0x00, 0x00,       };

    if (on)
        buf[0xa] = 2;

    r = libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 11, &transferred, 0);
    if (r < 0)
        return r;

    // read the data
    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 0);
    if (r < 0)
        return r;
    printf("received %d\n", transferred);
    dump_buf(buf, transferred);

    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 0);
    if (r < 0)
        return r;
    printf("received %d\n", transferred);
    dump_buf(buf, transferred);

    // get the result
    ret = buf[5];

    buf[0] = 0x44;
    r = libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 1, &transferred, 0);
    if (r < 0)
        return r;

    return ret;
}

void blink_led(void) {
    set_led(1);
    sleep(1);
    set_led(0);
    sleep(1);
    set_led(1);
    sleep(1);
    set_led(0);
}

int ique_flush(void) {
    int r, transferred;
    unsigned char reply = 0x44;
    unsigned char buf[0x1000];
    libusb_bulk_transfer(devh, IQUE_EP_SEND, &reply, 1, &transferred, 0);
    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 500);
    /*
    libusb_bulk_transfer(devh, IQUE_EP_SEND, &reply, 1, &transferred, 0);
    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 1);
    */

    return 0;
}

/**
 * Issue a command to the iQue.
 *
 * Returns:
 *  < 0 on error
 *  0 on success
 */
int ique_issue_command(uint32_t command, uint32_t arg) {
    unsigned char cmd_buf[11];
    unsigned char *cmd_ptr, *arg_ptr;

    // network byte order
    command = htonl(command);
    arg = htonl(arg);

    cmd_ptr = (unsigned char *)&command;
    arg_ptr = (unsigned char *)&arg;

    cmd_buf[ 0] = 0x43;         // 3 bytes
    cmd_buf[ 1] = cmd_ptr[0];
    cmd_buf[ 2] = cmd_ptr[1];
    cmd_buf[ 3] = cmd_ptr[2];
    cmd_buf[ 4] = 0x43;         // 3 bytes
    cmd_buf[ 5] = cmd_ptr[3];
    cmd_buf[ 6] = arg_ptr[0];
    cmd_buf[ 7] = arg_ptr[1];
    cmd_buf[ 8] = 0x42;         // 2 bytes
    cmd_buf[ 9] = arg_ptr[2];
    cmd_buf[10] = arg_ptr[3];

    int transferred;
    int r = libusb_interrupt_transfer(devh, IQUE_EP_SEND, cmd_buf, 11, &transferred, 0);

    return r < 0 ? r : 0;
}

unsigned char recv_buf[0x1000];
int recv_offset = 0;
int recv_size = 0;

/**
 * Complete a bulk receive from the iQue Player.
 *
 * Returns:
 *  -1 on failure
 *  size on success
 *
 * This function looks rather nasty, but it's actually a very
 * straightforward state machine:
 *
 *  -> ( wait for size ) -> ( receive data ) -> ( wait for end ) -> ACK
 *
 * See the protocol documentation for full details.
 *
 * The receive buffer is saved between function calls. The iQue can send
 * the results of a pending transfer (typically chunk 2, 3, or 4 of a
 * block) at the end of the previous chunk. This function won't request
 * more data from USB until the first receive buffer has been drained.
 */
int ique_bulk_receive(unsigned char *out, int size) {

    int transferred;
    int offset = 0;
    int i;
    int chunk_size = 0;
    int done = 0;

#define STATE_WAIT_SIZE 0
#define STATE_RECV 1
#define STATE_WAIT_END 2

    int state = STATE_WAIT_SIZE;

    while (!done) {
        int r;

        // end of the buffer, fetch another chunk
        if (recv_offset >= recv_size) {

            do {
                r = libusb_bulk_transfer(devh, IQUE_EP_RECV, recv_buf, sizeof(recv_buf), &transferred, 1000);
                if (r < 0 && r != LIBUSB_ERROR_TIMEOUT) return r;

                unsigned char reply = 0x44;
                libusb_bulk_transfer(devh, IQUE_EP_SEND, &reply, 1, &transferred, 0);
                printf("sending 44\n");
            } while (r < 0);

            dump_buf(recv_buf, transferred);

            recv_offset = 0;
            recv_size = transferred;
        }

        for (i = recv_offset; i < recv_size && !done; i += 4) {
            switch (recv_buf[i]) {
                // size
                case 0x1b:
                    if (state == STATE_WAIT_SIZE) {
                        // XXX this ought to be 24 bits, but it shouldn't matter
                        chunk_size = ntohs(*(uint16_t *)&recv_buf[i+2]);
                        if (chunk_size == size)
                            state = STATE_RECV;
                        else
                            printf("chunk of size %d, expecting %d\n", chunk_size, size);
                    }
                    else // invalid state
                        printf("received size block in state %d\n", state);
                    break;

                // data
                case 0x1f:
                case 0x1e:
                case 0x1d:
                    if (state == STATE_RECV) {
                        int num_bytes = recv_buf[i] - 0x1c; // hack :P
                        if (offset + num_bytes <= size) {
                            memcpy(out + offset, recv_buf + i, num_bytes);
                            offset += num_bytes;

                            if (offset == size) {
                                state = STATE_WAIT_END;

                                unsigned char reply = 0x44;
                                libusb_interrupt_transfer(devh, IQUE_EP_SEND, &reply, 1, &transferred, 0);
                            }
                        }
                        else {
                            printf("overflow detected, giving up\n");
                            return -1;
                        }
                    }
                    else
                        printf("received data block in state %d\n", state);
                    break;

                // done
                case 0x15:
                    if (state == STATE_WAIT_END)
                        done = 1;
                    else
                        printf("received done block in state %d\n", state);
                    break;

                default:
                    printf("unknown TU type %02x\n", recv_buf[i]);
            }
        }

        recv_offset = i;
    }

    // ack the transfer
    unsigned char reply = 0x44;
    libusb_interrupt_transfer(devh, IQUE_EP_SEND, &reply, 1, &transferred, 0);

    return size;
}

/**
 * Poll the device for its status.
 */
int ique_status(void) {
    int r;
    unsigned char buf[8];

    ique_issue_command(0x1d, 0x00);
    ique_bulk_receive(buf, 8);

    r = ique_issue_command(0x17, 0x00);
    if (r < 0) return r;

    r = ique_bulk_receive(buf, 8);
    if (r < 0) return r;

    dump_buf(buf, 8);

    return 0;
}

int ique_get_block(uint32_t block, unsigned char *out) {
    int i;

    ique_issue_command(0x11, block);

    for (i = 0; i < 4; ++i)
        ique_bulk_receive(out + i * 0x1000, 0x1000);

    return 0;
}

int ique_get_block2(uint16_t block, unsigned char *out) {
    int r, ret, transferred;
    unsigned char buf[0x1000] =
        { 0x43, 0x00, 0x00, 0x00,
          0x43, 0x11, 0x00, 0x00,
          0x42,                   };

    buf[0] = 0x44;
    libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 1, &transferred, 0);

    buf[0] = 0x44;
    libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 1, &transferred, 0);

    buf[0] = 0x43;
    // ique expects big endian
    *(uint16_t *)&buf[9] = htons(block);

    r = libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 11, &transferred, 0);
    if (r < 0)
        return r;

    // read the data
    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 0);
    if (r < 0)
        return r;
    printf("received %d\n", transferred);
    // dump_buf(buf, transferred);

    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 0);
    if (r < 0)
        return r;
    printf("received %d\n", transferred);
    // dump_buf(buf, transferred);

    buf[0] = 0x44;
    r = libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 1, &transferred, 0);
    if (r < 0)
        return r;

    int total = 0;

    int size = 0;
    int offset = 0;
    while (offset < 0x4000) {
        // printf("total %04x, offset %04x, size %04x\n", total, offset, size);
        r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, 0x40, &transferred, 0);
        if (r < 0)
            return r;
        // printf("received %d\n", transferred);
        // dump_buf(buf, transferred);
        int i;
        for (i = 0; i < transferred; i += 4) {
            if (buf[i] == 0x1b) {
                buf[i] = 0;
                size = ntohl(*(uint32_t *)&buf[i]);
                printf("size: %x\n", size);
            }
            else if (buf[i] == 0x1f) {
                memcpy(out + offset, buf + i + 1, 3);
                offset += 3;
                total += 3;
                // printf("  offset %d total %d\n", offset, total);
            }
            else if (buf[i] == 0x1d) {
                if (total < size) {
                    memcpy(out + offset, buf + i + 1, size - total);
                    offset += size - total;
                }
                total = size; // done with this chunk, send an ack
                printf("  offset %d total %d\n", offset, total);

                total = 0;

                buf[0] = 0x44;
                r = libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 1, &transferred, 0);
            }
            else {
                printf("Don't know how to handle %02x, retrying\n", buf[i]);
                // buf[0] = 0x44;
                // libusb_bulk_transfer(devh, IQUE_EP_SEND, buf, 1, &transferred, 0);
                exit(1);
            }

            if (r < 0)
                return r;
        }
    }

    r = libusb_bulk_transfer(devh, IQUE_EP_RECV, buf, sizeof(buf), &transferred, 0);
    if (r < 0)
        return r;
    printf("received %d\n", transferred);
    dump_buf(buf, transferred);

    ret = 1;
    return ret;
}
