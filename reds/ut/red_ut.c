/* -*- C -*- */
/*
 * COPYRIGHT 2017 XYRATEX TECHNOLOGY LIMITED
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF XYRATEX TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF XYRATEX TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF XYRATEX LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF XYRATEX'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A XYRATEX REPRESENTATIVE
 * http://www.xyratex.com/contact
 *
 * Original author: Igor Vartanov <igor.vartanov@seagate.com>
 * Original creation date: 08-Feb-2017
 */

#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_UT
#include "lib/trace.h"

#include "lib/finject.h"
#include "conf/ut/common.h"     /* m0_conf_ut_xprt */
#include "reds/red_command.h"
#include "rpc/rpclib.h"         /* m0_rpc_server_ctx, m0_rpc_client_ctx */
#include "ut/misc.h"            /* M0_UT_CONF_PROCESS */
#include "ut/ut.h"

enum {
	MAX_RPCS_IN_FLIGHT = 1,
};

//static struct m0_net_xprt       *xprt = &m0_net_lnet_xprt;
static struct m0_net_domain     client_net_dom;
static struct m0_rpc_client_ctx cctx = {
        .rcx_net_dom            = &client_net_dom,
        .rcx_local_addr         = CLIENT_ENDPOINT_ADDR,
        .rcx_remote_addr        = SERVER_ENDPOINT_ADDR,
        .rcx_max_rpcs_in_flight = MAX_RPCS_IN_FLIGHT,
        .rcx_fid                = &g_process_fid,
};

static void red_ut_motr_start(struct m0_rpc_server_ctx *rctx)
{
	int rc;
#define NAME(ext) "red_ut" ext
	char *argv[] = {
		NAME(""), "-T", "AD", "-D", NAME(".db"), "-j" /* red enabled */,
		"-S", NAME(".stob"), "-A", "linuxstob:"NAME("-addb.stob"),
		"-w", "10", "-e", SERVER_ENDPOINT, "-H", SERVER_ENDPOINT_ADDR,
		"-f", M0_UT_CONF_PROCESS,
		"-c", M0_SRC_PATH("reds/ut/red.xc")
	};
	*rctx = (struct m0_rpc_server_ctx) {
		.rsx_xprts         = m0_net_all_xprt_get(),
		.rsx_xprts_nr      = 1,
		.rsx_argv          = argv,
		.rsx_argc          = ARRAY_SIZE(argv),
		.rsx_log_file_name = NAME(".log")
	};
#undef NAME
	rc = m0_rpc_server_start(rctx);
	M0_UT_ASSERT(rc == 0);
}

static void red_ut_motr_stop(struct m0_rpc_server_ctx *rctx)
{
	m0_rpc_server_stop(rctx);
}

static void red_ut_client_start(void)
{
	int rc;

	rc = m0_net_domain_init(&client_net_dom, m0_net_xprt_default_get());
	M0_UT_ASSERT(rc == 0);
	rc = m0_rpc_client_start(&cctx);
	M0_UT_ASSERT(rc == 0);
}

static void red_ut_client_stop(void)
{
	int rc = m0_rpc_client_stop(&cctx);
	M0_UT_ASSERT(rc == 0);
	m0_net_domain_fini(&client_net_dom);
}

extern struct m0_fop_type m0_fic_req_fopt;

static void test_red_command_post(void)
{
	struct m0_rpc_server_ctx rctx;
	int     rc, num1 = 10, num2 = 20;
        int     result; 

	red_ut_motr_start(&rctx);
	red_ut_client_start();

	rc = m0_red_command_post_sync(&cctx.rcx_session, 
                                        ADD, num1, num2, &result);
        fprintf(stderr, "rc = %d, res = %d\n", rc, result);
	M0_UT_ASSERT(rc == 0);

	rc = m0_red_command_post_sync(&cctx.rcx_session,
                                        SUB, num1, num2, &result);
        fprintf(stderr, "rc = %d, res = %d\n", rc, result);
        M0_UT_ASSERT(rc == 0);

	rc = m0_red_command_post_sync(&cctx.rcx_session,
				     MUL, num1, num2, &result);
        fprintf(stderr, "rc = %d, res = %d\n", rc, result);
	M0_UT_ASSERT(rc == 0);

	rc = m0_red_command_post_sync(&cctx.rcx_session, DIV, num1, num2,
                                        &result);
        fprintf(stderr, "rc = %d, res = %d\n", rc, result);
        M0_UT_ASSERT(rc == 0);

	/* make sure unsupported disposition value not welcomed */
        rc = m0_red_command_post_sync(&cctx.rcx_session, 255, num1, num2,
                                        &result);
        M0_UT_ASSERT(rc == -EINVAL);

	red_ut_client_stop();
	red_ut_motr_stop(&rctx);
}


struct m0_ut_suite red_ut = {
	.ts_name  = "red-ut",
	.ts_tests = {
		{ "red-test",      test_red_command_post },
		{ NULL, NULL }
	},
	.ts_owners = "Rajat",
};
M0_EXPORTED(red_ut);
#undef M0_TRACE_SUBSYSTEM

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */