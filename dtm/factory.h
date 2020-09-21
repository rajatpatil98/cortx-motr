/* -*- C -*- */
/*
 * Copyright (c) 2013-2020 Seagate Technology LLC and/or its Affiliates
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



#pragma once

#ifndef __MOTR_DTM_FACTORY_H__
#define __MOTR_DTM_FACTORY_H__


/**
 * @addtogroup dtm
 *
 * @{
 */

/* export */
struct m0_dtm_factory;
struct m0_dtm_factory_ops;
struct m0_dtm_factory_type;

/* import */
struct m0_dtm_dtx;
struct m0_dtm_oper;

struct m0_dtm_factory {
	const struct m0_dtm_factory_ops *fa_ops;
};

struct m0_dtm_factory_ops {
	const struct m0_dtm_factory_type *fao_type;

	int (*fao_dtx_make) (struct m0_dtm_factory *fa, struct m0_dtm_dtx *dt);
	int (*fao_oper_make)(struct m0_dtm_factory *fa,
			     struct m0_dtm_oper *oper);
};

struct m0_dtm_factory_type {
	const char *fat_name;
};

/** @} end of dtm group */

#endif /* __MOTR_DTM_FACTORY_H__ */


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
