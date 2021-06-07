#pragma once

#ifndef __MOTR_REDS_RED_COMMAND_H__
#define __MOTR_REDS_RED_COMMAND_H__

#include "xcode/xcode.h"
#include "lib/buf_xc.h"
#include "rpc/session.h"

/** RED command disposition types */
enum m0_red_disp {
        ADD,            /* Add operation */
        SUB,            /* Subtract operation */
        MUL,            /* Multiply operation */
        DIV             /* Division operation */
};

/** RED command request. */
struct m0_red_command_req {
        uint8_t       fcr_disp;     /**< disposition, @ref m0_fi_disp */
        uint32_t      fcr_num1;     /**< 1st numeric */
        uint32_t      fcr_num2;     /**< 2nd numeric */
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/** RED command reply. */
struct m0_red_command_rep {
        int32_t fcp_rc;
        int32_t fcp_result;
} M0_XCA_RECORD M0_XCA_DOMAIN(rpc);

/**
 * Posts reduction command over already existing rpc session.
 * The command is executed synchronously.
 *
 * @param sess - a valid rpc session
 * @param disp - operation
 * @param num1 - 1st numerical
 * @param num2 - 2nd numerical
 */
M0_INTERNAL int m0_red_command_post_sync(struct m0_rpc_session  *sess,
                                        enum m0_red_disp        disp,
                                        uint32_t                num1,
                                        uint32_t                num2,
                                        int                     *result);
#endif  /* __MERO_REDS_RED_COMMAND_H__ */