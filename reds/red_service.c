#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_OTHER

#include "lib/trace.h"
#include "lib/errno.h"
#include "lib/memory.h"           /* M0_ALLOC_PTR */
#include "reds/red_command_fops.h"
#include "reds/red_service.h"

static int red_allocate(struct m0_reqh_service **service,
                         const struct m0_reqh_service_type *stype);

static int red_start(struct m0_reqh_service *service)
{
        M0_ENTRY();
        M0_PRE(service != NULL);
        return M0_RC(0);
}

static void red_stop(struct m0_reqh_service *service)
{
        M0_ENTRY();
        M0_PRE(service != NULL);
        M0_LEAVE();
}

static void red_fini(struct m0_reqh_service *service)
{
        struct m0_reqh_red_service *red_svc;
        M0_PRE(service != NULL);
        red_svc = M0_AMB(red_svc, service, red_svc);
        m0_free(red_svc);
}

static const struct m0_reqh_service_type_ops red_type_ops = {
        .rsto_service_allocate = red_allocate
};

static const struct m0_reqh_service_ops red_ops = {
        .rso_start           = red_start,
        .rso_start_async     = m0_reqh_service_async_start_simple,
        .rso_stop            = red_stop,
        .rso_fini            = red_fini
};

struct m0_reqh_service_type m0_red_type = {
        .rst_name     = RED_SERVICE_NAME,
        .rst_ops      = &red_type_ops,
        .rst_level    = M0_RPC_SVC_LEVEL,
        .rst_typecode = M0_CST_RED,
};

static int red_allocate(struct m0_reqh_service **service,
                         const struct m0_reqh_service_type *stype)
{
        struct m0_reqh_red_service *red;
        
        /* Various preconditions to be held before proceeding. */
        M0_PRE(service != NULL && stype != NULL);

        M0_ALLOC_PTR(red);
        if (red == NULL)
                return M0_ERR(-ENOMEM);
        
        red->red_magic = M0_RED_SERVICE_MAGIC;
        
        *service = &red->red_svc;
        (*service)->rs_ops = &red_ops;
        
        return M0_RC(0);
}

M0_INTERNAL int m0_red_register(void)
{
        int rc = 0;
        M0_ENTRY();
        m0_red_command_fop_init();
        m0_reqh_service_type_register(&m0_red_type);
        return M0_RC(rc);
}

M0_INTERNAL void m0_red_unregister(void)
{
        M0_ENTRY();
        m0_reqh_service_type_unregister(&m0_red_type);
        m0_red_command_fop_fini();
        M0_LEAVE();
}
#undef M0_TRACE_SUBSYSTEM