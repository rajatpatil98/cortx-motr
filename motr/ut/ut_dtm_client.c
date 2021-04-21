/* -*- C -*- */
/*
 * Copyright (c) 2020 Seagate Technology LLC and/or its Affiliates
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For any questions about this software or licensing,
 * please email opensource@seagate.com or cortx-questions@seagate.com.
 *
 */



#define M0_TRACE_SUBSYSTEM M0_TRACE_SUBSYS_CLIENT
#include "lib/trace.h"

#include "lib/misc.h"               /* M0_SRC_PATH */
#include "lib/finject.h"
#include "ut/ut.h"
#include "ut/misc.h"                /* M0_UT_CONF_PROFILE */
#include "rpc/rpclib.h"             /* m0_rpc_server_ctx */
#include "fid/fid.h"
#include "motr/client.h"
#include "motr/client_internal.h"
#include "motr/idx.h"
#include "dix/layout.h"
#include "dix/client.h"
#include "dix/meta.h"
#include "fop/fom_simple.h"     /* m0_fom_simple */

#define WAIT_TIMEOUT               M0_TIME_NEVER
#define SERVER_LOG_FILE_NAME       "cas_server.log"

static struct m0_client        *ut_m0c;
static struct m0_config         ut_m0_config;
static struct m0_idx_dix_config ut_dix_config;

enum {
	MAX_RPCS_IN_FLIGHT = 10,
	CNT = 10,
	BATCH_SZ = 128,
};

static char *cas_startup_cmd[] = { "m0d", "-T", "linux",
                                "-D", "cs_sdb", "-S", "cs_stob",
                                "-A", "linuxstob:cs_addb_stob",
                                "-e", "lnet:0@lo:12345:34:1",
                                "-H", "0@lo:12345:34:1",
				"-w", "10", "-F",
				"-f", M0_UT_CONF_PROCESS,
				"-c", M0_SRC_PATH("motr/ut/dix_conf.xc")};

static const char         *local_ep_addr = "0@lo:12345:34:2";
static const char         *srv_ep_addr   = { "0@lo:12345:34:1" };
static const char         *process_fid   = M0_UT_CONF_PROCESS;
static struct m0_net_xprt *cs_xprts[]    = { &m0_net_lnet_xprt };

static struct m0_rpc_server_ctx dix_ut_sctx = {
		.rsx_xprts            = cs_xprts,
		.rsx_xprts_nr         = ARRAY_SIZE(cs_xprts),
		.rsx_argv             = cas_startup_cmd,
		.rsx_argc             = ARRAY_SIZE(cas_startup_cmd),
		.rsx_log_file_name    = SERVER_LOG_FILE_NAME
};

static void dix_config_init()
{
	struct m0_fid pver = M0_FID_TINIT('v', 1, 100);
	int           rc;
	struct m0_ext range[] = {{ .e_start = 0, .e_end = IMASK_INF }};

	/* Create meta indices (root, layout, layout-descr). */
	rc = m0_dix_ldesc_init(&ut_dix_config.kc_layout_ldesc, range,
			       ARRAY_SIZE(range), HASH_FNC_FNV1, &pver);
	M0_UT_ASSERT(rc == 0);
	rc = m0_dix_ldesc_init(&ut_dix_config.kc_ldescr_ldesc, range,
			       ARRAY_SIZE(range), HASH_FNC_FNV1, &pver);
	M0_UT_ASSERT(rc == 0);
	/*
	 * motr/setup.c creates meta indices now. Therefore, we must not
	 * create it twice or it will fail with -EEXIST error.
	 */
	ut_dix_config.kc_create_meta = false;
}

static void dix_config_fini()
{
	m0_dix_ldesc_fini(&ut_dix_config.kc_layout_ldesc);
	m0_dix_ldesc_fini(&ut_dix_config.kc_ldescr_ldesc);
}

static void idx_dix_ut_m0_client_init()
{
	int rc;

	ut_m0c = NULL;
	ut_m0_config.mc_is_oostore            = true;
	ut_m0_config.mc_is_read_verify        = false;
	ut_m0_config.mc_local_addr            = local_ep_addr;
	ut_m0_config.mc_ha_addr               = srv_ep_addr;
	ut_m0_config.mc_profile               = M0_UT_CONF_PROFILE;
	/* Use fake fid, see initlift_resource_manager(). */
	ut_m0_config.mc_process_fid           = process_fid;
	ut_m0_config.mc_tm_recv_queue_min_len = M0_NET_TM_RECV_QUEUE_DEF_LEN;
	ut_m0_config.mc_max_rpc_msg_size      = M0_RPC_DEF_MAX_RPC_MSG_SIZE;
	ut_m0_config.mc_idx_service_id        = M0_IDX_DIX;
	ut_m0_config.mc_idx_service_conf      = &ut_dix_config;

	m0_fi_enable_once("ha_init", "skip-ha-init");
	/* Skip HA finalisation in case of failure path. */
	m0_fi_enable("ha_fini", "skip-ha-fini");
	/*
	 * We can't use m0_fi_enable_once() here, because
	 * initlift_addb2() may be called twice in case of failure path.
	 */
	m0_fi_enable("initlift_addb2", "no-addb2");
	m0_fi_enable("ha_process_event", "no-link");
	rc = m0_client_init(&ut_m0c, &ut_m0_config, false);
	M0_UT_ASSERT(rc == 0);
	m0_fi_disable("ha_process_event", "no-link");
	m0_fi_disable("initlift_addb2", "no-addb2");
	m0_fi_disable("ha_fini", "skip-ha-fini");
	ut_m0c->m0c_motr = m0_get();
}

static void idx_dix_ut_init()
{
	int rc;

	M0_SET0(&dix_ut_sctx.rsx_motr_ctx);
	rc = m0_rpc_server_start(&dix_ut_sctx);
	M0_ASSERT(rc == 0);
	dix_config_init();
	idx_dix_ut_m0_client_init();
}

static void idx_dix_ut_fini()
{
	/*
	 * Meta-indices are destroyed automatically during m0_rpc_server_stop()
	 * along with the whole BE data.
	 */
	dix_config_fini();
	m0_fi_enable_once("ha_fini", "skip-ha-fini");
	m0_fi_enable_once("initlift_addb2", "no-addb2");
	m0_fi_enable("ha_process_event", "no-link");
	m0_client_fini(ut_m0c, false);
	m0_fi_disable("ha_process_event", "no-link");
	m0_rpc_server_stop(&dix_ut_sctx);
}


static uint8_t ifid_type(bool dist)
{
	return dist ? m0_dix_fid_type.ft_id : m0_cas_index_fid_type.ft_id;
}

static void general_ifid_fill(struct m0_fid *ifid, bool dist)
{
	*ifid = M0_FID_TINIT(ifid_type(dist), 2, 1);
}

static int ut_suite_mt_idx_dix_init(void)
{
	idx_dix_ut_init();
	return 0;
}

static int ut_suite_mt_idx_dix_fini(void)
{
	idx_dix_ut_fini();
	return 0;
}

extern void st_mt(void);
extern void st_lsfid(void);
extern void st_lsfid_cancel(void);


#include "dtm0/helper.h"
#include "dtm0/service.h"

struct dtm0_ut_ctx {
	struct m0_fid           duc_cli_svc_fid;
	struct m0_fid           duc_srv_svc_fid;
	struct m0_reqh_service *duc_srv_svc;
	struct m0_idx           duc_idx;
	struct m0_container     duc_realm;
	struct m0_fid           duc_ifid;
};

static struct dtm0_ut_ctx duc = {};

static int duc_setup(void)
{
	const char              *cl_ep_addr   = "0@lo:12345:34:2";
	struct m0_fid            cli_srv_fid  = M0_FID_INIT(0x7300000000000001,
							    0x1a);
	struct m0_fid            srv_dtm0_fid = M0_FID_INIT(0x7300000000000001,
							    0x1c);
	struct m0_reqh_service  *srv_srv = NULL;
	struct m0_reqh          *srv_reqh;
	int                      rc;
	struct m0_container     *realm = &duc.duc_realm;

	m0_fi_enable("m0_dtm0_in_ut", "ut");
	rc = ut_suite_mt_idx_dix_init();
	M0_UT_ASSERT(rc == 0);

	m0_container_init(realm, NULL, &M0_UBER_REALM, ut_m0c);

	if (ENABLE_DTM0) {
		/* Connect to the server */
		M0_UT_ASSERT(ut_m0c->m0c_dtms != NULL);
		srv_reqh = &dix_ut_sctx.rsx_motr_ctx.cc_reqh_ctx.rc_reqh;
		srv_srv = m0_reqh_service_lookup(srv_reqh, &srv_dtm0_fid);
		rc = m0_dtm0_service_process_connect(srv_srv, &cli_srv_fid,
						     cl_ep_addr, false);
		M0_UT_ASSERT(rc == 0);
	}

	general_ifid_fill(&duc.duc_ifid, true);

	/* Save the context */
	duc.duc_cli_svc_fid = cli_srv_fid;
	duc.duc_srv_svc_fid = srv_dtm0_fid;
	duc.duc_srv_svc = srv_srv;

	return 0;
}

static void idx_setup(void)
{
	struct m0_op            *op = NULL;
	struct m0_idx           *idx = &duc.duc_idx;
	struct m0_container     *realm = &duc.duc_realm;
	int                      rc;
	struct m0_fid           *ifid = &duc.duc_ifid;

	m0_idx_init(idx, &realm->co_realm, (struct m0_uint128 *) ifid);

	/* Create the index */
	rc = m0_entity_create(NULL, &idx->in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	m0_op_launch(&op, 1);
	rc = m0_op_wait(op, M0_BITS(M0_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	m0_op_fini(op);
	m0_op_free(op);
	op = NULL;
}

static void idx_teardown(void)
{
	struct m0_op            *op = NULL;
	int                      rc;

	/* Delete the index */
	rc = m0_entity_delete(&duc.duc_idx.in_entity, &op);
	M0_UT_ASSERT(rc == 0);
	m0_op_launch(&op, 1);
	rc = m0_op_wait(op, M0_BITS(M0_OS_STABLE), WAIT_TIMEOUT);
	M0_UT_ASSERT(rc == 0);
	m0_op_fini(op);
	m0_op_free(op);
	m0_idx_fini(&duc.duc_idx);
	M0_SET0(&duc.duc_idx);
}


static int duc_teardown(void)
{
	struct m0_reqh_service  *srv_srv = duc.duc_srv_svc;
	struct m0_fid            cli_srv_fid  = duc.duc_cli_svc_fid;
	int                      rc;

	if (ENABLE_DTM0) {
		/* Disconnect from the server */
		rc = m0_dtm0_service_process_disconnect(srv_srv, &cli_srv_fid);
		M0_UT_ASSERT(rc == 0);
	}

	rc = ut_suite_mt_idx_dix_fini();
	m0_fi_disable("m0_dtm0_in_ut", "ut");
	return rc;
}

/* Submits multiple M0 client (PUT|DEL) operations and then waits on "phase1"
 * states, and then waits on "phase2" states.
 */
static void run_m0ops(uint64_t nr, enum m0_idx_opcode opcode,
		      uint64_t phase1wait,
		      uint64_t phase2wait)
{
	struct m0_idx      *idx = &duc.duc_idx;
	struct m0_op      **ops;
	int                *rcs;
	struct m0_bufvec   *key_vecs;
	char               *val = NULL;
	struct m0_bufvec    vals = {};
	m0_bcount_t         len = 1;
	int                 flags = 0;
	uint64_t            i;
	int                 rc;

	M0_PRE(M0_IN(opcode, (M0_IC_PUT, M0_IC_DEL)));
	M0_ALLOC_ARR(ops, nr);
	M0_UT_ASSERT(ops != NULL);
	M0_ALLOC_ARR(rcs, nr);
	M0_UT_ASSERT(rcs != NULL);
	M0_ALLOC_ARR(key_vecs, nr);
	M0_UT_ASSERT(key_vecs != NULL);

	if (opcode == M0_IC_PUT) {
		val = m0_strdup("ItIsAValue");
		M0_UT_ASSERT(val != NULL);
		vals = M0_BUFVEC_INIT_BUF((void **) &val, &len);
	}

	/* Execute the ops */
	for (i = 0; i < nr; ++i) {
		rc = m0_bufvec_alloc(&key_vecs[i], 1, sizeof(i));
		M0_UT_ASSERT(key_vecs[i].ov_vec.v_count[0] == sizeof(i));
		memcpy(key_vecs[i].ov_buf[0], &i, sizeof(i));

		rc = m0_idx_op(idx, opcode, &key_vecs[i],
			       opcode == M0_IC_DEL ? NULL : &vals,
			       &rcs[i], flags, &ops[i]);
		M0_UT_ASSERT(rc == 0);
		m0_op_launch(&ops[i], 1);

		if (phase1wait != 0)
			rc = m0_op_wait(ops[i], phase1wait, WAIT_TIMEOUT);
		else
			rc = 0;
		M0_LOG(M0_DEBUG, "Got phase1 %" PRIu64, i);
		if (rc == -ESRCH)
			M0_UT_ASSERT(ops[i]->op_sm.sm_state == M0_OS_STABLE);
	}

	/* Wait until they get stable */
	for (i = 0; i < nr; ++i) {
		if (phase2wait != 0)
			rc = m0_op_wait(ops[i], phase2wait, WAIT_TIMEOUT);
		else
			rc = 0;
		M0_LOG(M0_DEBUG, "Got phase2 %" PRIu64, i);
		M0_UT_ASSERT(rc == 0);
		M0_UT_ASSERT(ops[i]->op_rc == 0);
		M0_UT_ASSERT(rcs[0] == 0);
		m0_op_fini(ops[i]);
		m0_op_free(ops[i]);
		ops[i] = NULL;
		m0_bufvec_free(&key_vecs[i]);
	}

	M0_UT_ASSERT(M0_IS0(ops));
	m0_free(key_vecs);
	m0_free(ops);
	m0_free(val);
}

/** Launch an operation, wait until it gets executed and launch another one.
 * When all operations are executed, wait until all of them get stable.
 */
static void exec_then_stable(uint64_t nr, enum m0_idx_opcode opcode)
{
	run_m0ops(nr, opcode, M0_BITS(M0_OS_EXECUTED), M0_BITS(M0_OS_STABLE));
}

/** Launch an operation and wait until it gets stable. Then launch another one.
 */
static void exec_one_by_one(uint64_t nr, enum m0_idx_opcode opcode)
{
	run_m0ops(nr, opcode, M0_BITS(M0_OS_STABLE), 0);
}

/** Launch an operation then launch another one. When all opearations are
 * launched, wait until they get stable.
 */
static void exec_concurrent(uint64_t nr, enum m0_idx_opcode opcode)
{
	run_m0ops(nr, opcode, 0, M0_BITS(M0_OS_STABLE));
}

static void st_dtm0(void)
{
	idx_setup();
	exec_one_by_one(1, M0_IC_PUT);
	idx_teardown();
}

static void st_dtm0_putdel(void)
{
	idx_setup();
	exec_one_by_one(1, M0_IC_PUT);
	exec_one_by_one(1, M0_IC_DEL);
	idx_teardown();

	idx_setup();
	exec_one_by_one(100, M0_IC_PUT);
	exec_one_by_one(100, M0_IC_DEL);
	idx_teardown();
}

static void st_dtm0_e_then_s(void)
{
	idx_setup();
	exec_then_stable(100, M0_IC_PUT);
	exec_then_stable(100, M0_IC_DEL);
	idx_teardown();
}

static void st_dtm0_c(void)
{
	idx_setup();
	exec_concurrent(100, M0_IC_PUT);
	exec_concurrent(100, M0_IC_DEL);
	idx_teardown();
}


struct m0_ut_suite ut_suite_mt_dtm = {
	.ts_name   = "dtm0-mt",
	.ts_owners = "Anatoliy",
	.ts_init   = duc_setup,
	.ts_fini   = duc_teardown,
	.ts_tests  = {
		{ "dtm0",           st_dtm0,          "Anatoliy" },
		{ "dtm0_putdel",    st_dtm0_putdel,   "Ivan"     },
		{ "dtm0_e_then_s",  st_dtm0_e_then_s, "Ivan"     },
		{ "dtm0_c",         st_dtm0_c,        "Ivan"     },
		{ NULL, NULL }
	}
};

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
/*
 * vim: tabstop=8 shiftwidth=8 noexpandtab textwidth=80 nowrap
 */
