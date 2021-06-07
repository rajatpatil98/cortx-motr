#pragma once

#ifndef __MOTR_REDS_RED_SERVICE_H__
#define __MOTR_REDS_RED_SERVICE_H__

#include "reqh/reqh_service.h"

#define RED_SERVICE_NAME "M0_CST_RED"

struct m0_reqh_red_service
{
        struct m0_reqh_service  red_svc;
        uint64_t                red_magic;
};

M0_INTERNAL int m0_red_register(void);
M0_INTERNAL void m0_red_unregister(void);

#endif
