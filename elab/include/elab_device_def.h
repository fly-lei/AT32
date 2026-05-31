/*
 * eLab Project
 * Copyright (c) 2023, EventOS Team, <event-os@outlook.com>
 */

#ifndef ELAB_DEVICE_DEF_H
#define ELAB_DEVICE_DEF_H

/* includes ----------------------------------------------------------------- */
#include "elab_device.h"

#ifdef __cplusplus
extern "C" {
#endif

/* public define ------------------------------------------------------------ */
enum elab_device_user
{
    ELAB_DEVICE_MOTOR = ELAB_DEVICE_NORMAL_MAX,
    ELAB_DEVICE_IMU,

    ELAB_DEVICE_MAX,
};

#ifdef __cplusplus
}
#endif

#endif

/* ----------------------------- end of file -------------------------------- */
