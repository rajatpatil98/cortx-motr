#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER

#include "lib/trace.h"
#include "lib/memory.h"                 /* M0_ALLOC_PTR, m0_free */
#include "lib/buf.h"                    /* m0_buf_strdup */
#include "lib/finject.h"                /* M0_FI_ENABLED */

#include "reds/red_command.h"
#include "reds/red_command_fops.h"
#include "fop/fom_generic.h"
#include "fop/fop.h"
#include "rpc/rpc.h"
#include "sm/sm.h"

struct m0_red_command_fom {
        /** Generic m0_fom object. */
        struct m0_fom  fcf_gen;
        /** FOP associated with this FOM. */
        struct m0_fop *fcf_fop;
};

extern const struct m0_fom_ops red_command_fom_ops;

static int red_command_fom_create(struct m0_fop *fop,
                                struct m0_fom **out,
                                struct m0_reqh *reqh)
{
        struct m0_fom                   *fom;
        struct m0_red_command_fom       *fom_obj;
        struct m0_fop                   *fop_rep;

        M0_ENTRY();
        M0_PRE(fop != NULL);
        M0_PRE(out != NULL);

        fop_rep = M0_FI_ENABLED("no_mem") ? NULL :
                        m0_fop_reply_alloc(fop, &m0_red_command_rep_fopt);
        M0_ALLOC_PTR(fom_obj);
        if (fop_rep == NULL || fom_obj == NULL)
                goto no_mem;
        fom = &fom_obj->fcf_gen;
        m0_fom_init(fom, &fop->f_type->ft_fom_type, &red_command_fom_ops, fop,
                    fop_rep, reqh);
        fom_obj->fcf_fop = fop;
        *out = fom;
        return M0_RC(0);
no_mem:
        m0_free(fop_rep);
        m0_free(fom_obj);
        return M0_ERR(-ENOMEM);
}

static void red_command_fom_fini(struct m0_fom *fom)
{
        struct m0_red_command_fom *fom_obj = M0_AMB(fom_obj, fom, fcf_gen);

        m0_fom_fini(fom);
        m0_free(fom_obj);
}

static int red_command_execute(const struct m0_red_command_req *req,
                                int *result)
{
        int   rc   = 0;

        switch (req->fcr_disp) {
        case ADD:
                *result = req->fcr_num1 + req->fcr_num2;
                break;
        case SUB:
                *result = req->fcr_num1 - req->fcr_num2;
                break;
        case MUL:
                *result = req->fcr_num1 * req->fcr_num2; 
                break;
        case DIV:
                if (req->fcr_num2) {
                        *result = req->fcr_num1 / req->fcr_num2;
                } else {
                        rc = -EINVAL;
                }
                break;
        default:
                rc = -EINVAL;
                break;
        }
        return rc == 0 ? M0_RC(rc) : M0_ERR(rc);
}

static int red_command_fom_tick(struct m0_fom *fom)
{
        struct m0_red_command_fom *fcf = M0_AMB(fcf, fom, fcf_gen);
        struct m0_red_command_req *req = m0_fop_data(fcf->fcf_fop);
        struct m0_fop   *fop = fom->fo_rep_fop;
        struct m0_red_command_rep *rep = m0_fop_data(fop);

        rep->fcp_rc = red_command_execute(req, &(rep->fcp_result));
        m0_rpc_reply_post(&fcf->fcf_fop->f_item, m0_fop_to_rpc_item(fop));
        m0_fom_phase_set(fom, M0_FOPH_FINISH);
        return M0_FSO_WAIT;
}

static size_t red_command_fom_locality(const struct m0_fom *fom)
{
        static size_t locality = 0;

        return locality++;
}

const struct m0_fom_type_ops m0_red_command_fom_type_ops = {
        .fto_create = red_command_fom_create
};

const struct m0_fom_ops red_command_fom_ops = {
        .fo_fini          = red_command_fom_fini,
        .fo_tick          = red_command_fom_tick,
        .fo_home_locality = red_command_fom_locality
};

#undef M0_TRACE_SUBSYSTEM