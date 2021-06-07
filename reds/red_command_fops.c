#include "reds/red_command_fom.h"    /* m0_red_command_fom_type_ops */
#include "reds/red_command_fops.h"
#include "reds/red_command_xc.h"     /* m0_red_command_req_xc, m0_red_command_rep_xc */
#include "fop/fom_generic.h"       /* m0_generic_conf */
#include "fop/fop.h"               /* M0_FOP_TYPE_INIT */
#include "rpc/rpc.h"               /* m0_rpc_service_type */
#include "rpc/rpc_opcodes.h"

struct m0_fop_type m0_red_command_req_fopt;
struct m0_fop_type m0_red_command_rep_fopt;

M0_INTERNAL void m0_red_command_fop_init(void)
{
        extern struct m0_reqh_service_type m0_rpc_service_type;

        M0_FOP_TYPE_INIT(&m0_red_command_req_fopt,
                         .name          = "RED Request",
                         .opcode        = M0_RED_REQ_OPCODE,
                         .xt            = m0_red_command_req_xc,
                         .sm            = &m0_generic_conf,
                         .fom_ops       = &m0_red_command_fom_type_ops,
                         .svc_type      = &m0_rpc_service_type,
                         .rpc_flags     = M0_RPC_ITEM_TYPE_REQUEST);
        M0_FOP_TYPE_INIT(&m0_red_command_rep_fopt,
                         .name          = "RED reply",
                         .opcode        = M0_RED_REP_OPCODE,
                         .xt            = m0_red_command_rep_xc,
                         .rpc_flags     = M0_RPC_ITEM_TYPE_REPLY,
                         .svc_type      = &m0_rpc_service_type);
}

M0_INTERNAL void m0_red_command_fop_fini(void)
{
        m0_fop_type_fini(&m0_red_command_req_fopt);
        m0_fop_type_fini(&m0_red_command_rep_fopt);
}