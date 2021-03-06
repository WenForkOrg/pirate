/*
 * This work was authored by Two Six Labs, LLC and is sponsored by a subcontract
 * agreement with Galois, Inc.  This material is based upon work supported by
 * the Defense Advanced Research Projects Agency (DARPA) under Contract No.
 * HR0011-19-C-0103.
 *
 * The Government has unlimited rights to use, modify, reproduce, release,
 * perform, display, or disclose computer software or computer software
 * documentation marked with this legend. Any reproduction of technical data,
 * computer software, or portions thereof marked with this legend must also
 * reproduce this marking.
 *
 * Copyright 2019 Two Six Labs, LLC.  All rights reserved.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "libpirate.h"
#include "device.h"
#include "pipe.h"
#include "unix_socket.h"
#include "tcp_socket.h"
#include "udp_socket.h"
#include "shmem_interface.h"
#include "udp_shmem_interface.h"
#include "uio.h"
#include "serial.h"
#include "mercury.h"
#include "ge_eth.h"
#include "pirate_common.h"

typedef struct {
    union {
        device_ctx         device;
        pipe_ctx           pipe;
        unix_socket_ctx    unix_socket;
        tcp_socket_ctx     tcp_socket;
        udp_socket_ctx     udp_socket;
        shmem_ctx          shmem;
        udp_shmem_ctx      udp_shmem;
        uio_ctx            uio;
        serial_ctx         serial;
        mercury_ctx        mercury;
        ge_eth_ctx         ge_eth;
    } channel;
} pirate_channel_ctx_t;

typedef struct {
    pirate_channel_param_t param;
    pirate_channel_ctx_t ctx;
} pirate_channel_t;

static struct {
    pirate_channel_t reader;
    pirate_channel_t writer;
} gaps_channels[PIRATE_NUM_CHANNELS];

static inline pirate_channel_t *pirate_get_channel(int gd, int flags) {
    if ((gd < 0) || (gd >= PIRATE_NUM_CHANNELS)) {
        errno = EBADF;
        return NULL;
    }

    if (flags == O_RDONLY) {
        return &gaps_channels[gd].reader;
    } else if (flags == O_WRONLY) {
        return &gaps_channels[gd].writer;
    }

    errno = EINVAL;
    return NULL;
}


void pirate_init_channel_param(channel_enum_t channel_type, pirate_channel_param_t *param) {
    memset(param, 0, sizeof(*param));
    param->channel_type = channel_type;
}

int pirate_parse_channel_param(const char *str, pirate_channel_param_t *param) {

    // Channel configuration function is allowed to modify the string
    // while braking it into delimiter-separated tokens
    char opt[256];
    strncpy(opt, str, sizeof(opt));

    pirate_init_channel_param(INVALID, param);

    if (strncmp("device", opt, strlen("device")) == 0) {
        param->channel_type = DEVICE;
        return pirate_device_parse_param(opt, &param->channel.device);
    } else if (strncmp("pipe", opt, strlen("pipe")) == 0) {
        param->channel_type = PIPE;
        return pirate_pipe_parse_param(opt, &param->channel.pipe);
    } else if (strncmp("unix_socket", opt, strlen("unix_socket")) == 0) {
        param->channel_type = UNIX_SOCKET;
        return pirate_unix_socket_parse_param(opt, &param->channel.unix_socket);
    } else if (strncmp("tcp_socket", opt, strlen("tcp_socket")) == 0) {
        param->channel_type = TCP_SOCKET;
        return pirate_tcp_socket_parse_param(opt, &param->channel.tcp_socket);
    } else if (strncmp("udp_socket", opt, strlen("udp_socket")) == 0) {
        param->channel_type = UDP_SOCKET;
        return pirate_udp_socket_parse_param(opt, &param->channel.udp_socket);
    } else if (strncmp("shmem", opt, strlen("shmem")) == 0) {
        param->channel_type = SHMEM;
        return pirate_shmem_parse_param(opt, &param->channel.shmem);
    } else if (strncmp("udp_shmem", opt, strlen("udp_shmem")) == 0) {
        param->channel_type = UDP_SHMEM;
        return pirate_udp_shmem_parse_param(opt, &param->channel.udp_shmem);
    } else if (strncmp("uio", opt, strlen("uio")) == 0) {
        param->channel_type = UIO_DEVICE;
        return pirate_uio_parse_param(opt, &param->channel.uio);
    } else if (strncmp("serial", opt, strlen("serial")) == 0) {
        param->channel_type = SERIAL;
        return pirate_serial_parse_param(opt, &param->channel.serial);
    } else if (strncmp("mercury", opt, strlen("mercury")) == 0) {
        param->channel_type = MERCURY;
        return pirate_mercury_parse_param(opt, &param->channel.mercury);
    } else if (strncmp("ge_eth", opt, strlen("ge_eth")) == 0) {
        param->channel_type = GE_ETH;
        return pirate_ge_eth_parse_param(opt, &param->channel.ge_eth);
    }

    errno = EINVAL;
    return -1;
}

int pirate_set_channel_param(int gd, int flags,
                            const pirate_channel_param_t *param) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, flags)) == NULL) {
        return -1;
    }
    memcpy(&channel->param, param, sizeof(pirate_channel_param_t));
    return 0;
}


int pirate_get_channel_param(int gd, int flags,
                            pirate_channel_param_t *param) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, flags)) == NULL) {
        return -1;
    }
    memcpy(param, &channel->param, sizeof(pirate_channel_param_t));
    return 0;
}


// gaps descriptors must be opened from smallest to largest
int pirate_open(int gd, int flags) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, flags)) == NULL) {
        return -1;
    }

    pirate_channel_param_t *param = &channel->param;
    pirate_channel_ctx_t *ctx = &channel->ctx;

    switch (channel->param.channel_type) {

    case DEVICE:
        return pirate_device_open(gd, flags, &param->channel.device, &ctx->channel.device);

    case PIPE:
        return pirate_pipe_open(gd, flags, &param->channel.pipe, &ctx->channel.pipe);

    case UNIX_SOCKET:
        return pirate_unix_socket_open(gd, flags, &param->channel.unix_socket, &ctx->channel.unix_socket);

    case TCP_SOCKET:
        return pirate_tcp_socket_open(gd, flags, &param->channel.tcp_socket, &ctx->channel.tcp_socket);

    case UDP_SOCKET:
        return pirate_udp_socket_open(gd, flags, &param->channel.udp_socket, &ctx->channel.udp_socket);

    case SHMEM:
        return pirate_shmem_open(gd, flags, &param->channel.shmem, &ctx->channel.shmem);

    case UDP_SHMEM:
        return pirate_udp_shmem_open(gd, flags, &param->channel.udp_shmem, &ctx->channel.udp_shmem);

    case UIO_DEVICE:
        return pirate_uio_open(gd, flags, &param->channel.uio, &ctx->channel.uio);

    case SERIAL:
        return pirate_serial_open(gd, flags, &param->channel.serial, &ctx->channel.serial);

    case MERCURY:
        return pirate_mercury_open(gd, flags, &param->channel.mercury, &ctx->channel.mercury);

    case GE_ETH:
        return pirate_ge_eth_open(gd, flags, &param->channel.ge_eth, &ctx->channel.ge_eth);

    case INVALID:
    default:
        break;
    }

    errno = ENODEV;
    return -1;
}


int pirate_close(int gd, int flags) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, flags)) == NULL) {
        return -1;
    }

    pirate_channel_ctx_t *ctx = &channel->ctx;

    switch (channel->param.channel_type) {

    case DEVICE:
        return pirate_device_close(&ctx->channel.device);

    case PIPE:
        return pirate_pipe_close(&ctx->channel.pipe);

    case UNIX_SOCKET:
        return pirate_unix_socket_close(&ctx->channel.unix_socket);

    case TCP_SOCKET:
        return pirate_tcp_socket_close(&ctx->channel.tcp_socket);

    case UDP_SOCKET:
        return pirate_udp_socket_close(&ctx->channel.udp_socket);

    case SHMEM:
        return pirate_shmem_close(&ctx->channel.shmem);

    case UDP_SHMEM:
        return pirate_udp_shmem_close(&ctx->channel.udp_shmem);

    case UIO_DEVICE:
        return pirate_uio_close(&ctx->channel.uio);

    case SERIAL:
        return pirate_serial_close(&ctx->channel.serial);

    case MERCURY:
        return pirate_mercury_close(&ctx->channel.mercury);

    case GE_ETH:
        return pirate_ge_eth_close(&ctx->channel.ge_eth);

    case INVALID:
    default:
        errno = ENODEV;
        return -1;
    }
}

ssize_t pirate_read(int gd, void *buf, size_t count) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, O_RDONLY)) == NULL) {
        return -1;
    }

    pirate_channel_param_t *param = &channel->param;
    pirate_channel_ctx_t *ctx = &channel->ctx;

    switch (channel->param.channel_type) {

    case DEVICE:
        return pirate_device_read(&param->channel.device, &ctx->channel.device, buf, count);

    case PIPE:
        return pirate_pipe_read(&param->channel.pipe, &ctx->channel.pipe, buf, count);

    case UNIX_SOCKET:
        return pirate_unix_socket_read(&param->channel.unix_socket, &ctx->channel.unix_socket, buf, count);

    case TCP_SOCKET:
        return pirate_tcp_socket_read(&param->channel.tcp_socket, &ctx->channel.tcp_socket, buf, count);

    case UDP_SOCKET:
        return pirate_udp_socket_read(&param->channel.udp_socket, &ctx->channel.udp_socket, buf, count);

    case SHMEM:
        return pirate_shmem_read(&param->channel.shmem, &ctx->channel.shmem, buf, count);

    case UDP_SHMEM:
        return pirate_udp_shmem_read(&param->channel.udp_shmem, &ctx->channel.udp_shmem, buf, count);

    case UIO_DEVICE:
        return pirate_uio_read(&param->channel.uio, &ctx->channel.uio, buf, count);

    case SERIAL:
        return pirate_serial_read(&param->channel.serial, &ctx->channel.serial, buf, count);

    case MERCURY:
        return pirate_mercury_read(&param->channel.mercury, &ctx->channel.mercury, buf, count);

    case GE_ETH:
        return pirate_ge_eth_read(&param->channel.ge_eth, &ctx->channel.ge_eth, buf, count);

    case INVALID:
    default:
        errno = ENODEV;
        return -1;
    }
}

ssize_t pirate_write(int gd, const void *buf, size_t count) {
    pirate_channel_t *channel = NULL;

    if ((channel = pirate_get_channel(gd, O_WRONLY)) == NULL) {
        return -1;
    }

    pirate_channel_param_t *param = &channel->param;
    pirate_channel_ctx_t *ctx = &channel->ctx;

    switch (param->channel_type) {

    case DEVICE:
        return pirate_device_write(&param->channel.device, &ctx->channel.device, buf, count);

    case PIPE:
        return pirate_pipe_write(&param->channel.pipe, &ctx->channel.pipe, buf, count);

    case UNIX_SOCKET:
        return pirate_unix_socket_write(&param->channel.unix_socket, &ctx->channel.unix_socket, buf, count);

    case TCP_SOCKET:
        return pirate_tcp_socket_write(&param->channel.tcp_socket, &ctx->channel.tcp_socket, buf, count);

    case UDP_SOCKET:
        return pirate_udp_socket_write(&param->channel.udp_socket, &ctx->channel.udp_socket, buf, count);

    case SHMEM:
        return pirate_shmem_write(&param->channel.shmem, &ctx->channel.shmem, buf, count);

    case UDP_SHMEM:
        return pirate_udp_shmem_write(&param->channel.udp_shmem, &ctx->channel.udp_shmem, buf, count);

    case UIO_DEVICE:
        return pirate_uio_write(&param->channel.uio, &ctx->channel.uio, buf, count);

    case SERIAL:
        return pirate_serial_write(&param->channel.serial, &ctx->channel.serial, buf, count);

    case MERCURY:
        return pirate_mercury_write(&param->channel.mercury, &ctx->channel.mercury, buf, count);

    case GE_ETH:
        return pirate_ge_eth_write(&param->channel.ge_eth, &ctx->channel.ge_eth, buf, count);

    case INVALID:
    default:
        errno = ENODEV;
        return -1;
    }
}
