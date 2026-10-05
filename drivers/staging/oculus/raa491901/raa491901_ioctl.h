// SPDX-License-Identifier: GPL-2.0+
// Copyright (c) Meta Platforms, Inc. and affiliates.

#ifndef __RAA491901_IOCTL_H__
#define __RAA491901_IOCTL_H__

struct raa491901_gain_cmd {
    uint16_t                red_gain;
    uint16_t                blue_gain;
    uint16_t                green_gain;
};

// Choosing IOCTL Magic Number 0xB7 since it is not used in https://www.kernel.org/doc/Documentation/ioctl/ioctl-number.txt
#define IOCTL_RAA491901_MAGIC               0xB7
#define IOCTL_RAA491901_MAXCMDS             2
#define IOCTL_RAA491901_IOCQGAIN            _IOR(IOCTL_RAA491901_MAGIC, 0, struct raa491901_gain_cmd)
#define IOCTL_RAA491901_IOCSGAIN            _IOW(IOCTL_RAA491901_MAGIC, 1, struct raa491901_gain_cmd)

#endif
