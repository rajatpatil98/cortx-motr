/* -*- C -*- */
/*
 * Copyright (c) 2012-2020 Seagate Technology LLC and/or its Affiliates
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

#ifndef __MOTR_FID_FID_LIST_H__
#define __MOTR_FID_FID_LIST_H__

/**
   @defgroup fid File identifier

   @{
 */

#include "fid/fid.h"
#include "lib/tlist.h"

/*
 * Item description FIDs list m0_tl
*/
struct m0_fid_item {
	struct m0_fid   i_fid;
	struct m0_tlink i_link;
	uint64_t        i_magic;
};

M0_TL_DESCR_DECLARE(m0_fids, M0_EXTERN);
M0_TL_DECLARE(m0_fids, M0_INTERNAL, struct m0_fid_item);


/** @} end of fid group */
#endif /* __MOTR_FID_FID_LIST_H__ */

/*
 *  Local variables:
 *  c-indentation-style: "K&R"
 *  c-basic-offset: 8
 *  tab-width: 8
 *  fill-column: 80
 *  scroll-step: 1
 *  End:
 */
