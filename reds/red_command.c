#undef M0_TRACE_SUBSYSTEM
#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER

#include "lib/trace.h"
#include "reds/red_command.h"
#include "reds/red_command_fops.h"      /* m0_red_command_req_fopt */
#include "fop/fop.h"                    /* m0_fop */
#include "rpc/rpc.h"                    /* m0_rpc_session */
#include "rpc/rpclib.h"                 /* m0_rpc_post_sync */

M0_INTERNAL int m0_red_command_post_sync(struct m0_rpc_session  *sess,
                                        enum m0_red_disp        disp,
                                        uint32_t                num1,
                                        uint32_t                num2,
                                        int32_t                 *result)
{
        struct m0_fop                   *fop;
        struct m0_red_command_rep       *rep;
        struct m0_red_command_req       req = {
                .fcr_disp = disp,
                .fcr_num1 = num1,
                .fcr_num2 = num2,
        };
        int rc;

        fop = m0_fop_alloc(&m0_red_command_req_fopt, &req,
                session_machine(sess));
        if (fop == NULL)
                return M0_ERR(-ENOMEM);
        rc = m0_rpc_post_sync(fop, sess, NULL, 0);

        if (rc == 0) {
                rep = m0_fop_data(m0_rpc_item_to_fop(fop->f_item.ri_reply));
                rc = rep->fcp_rc;
                *result = rep->fcp_result;
        }
        fop->f_data.fd_data = NULL; /* protect local data from freeing */
        m0_fop_put_lock(fop);
        return M0_RC(rc);
}

#undef M0_TRACE_SUBSYSTEM