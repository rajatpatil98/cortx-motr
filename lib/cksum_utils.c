/* -*- C -*- */
/*
 * Copyright (c) 2021 Seagate Technology LLC and/or its Affiliates
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

#include "lib/buf.h"    /* m0_buf */
#include "lib/vec.h"    /* m0_indexvec */
#include "lib/ext.h"    /* m0_ext */
#include "lib/trace.h"  /* M0_RC */
#include "lib/cksum_utils.h"
#include "lib/cksum_data.h"


M0_INTERNAL m0_bcount_t m0_ext_get_num_unit_start(m0_bindex_t ext_start,
						  m0_bindex_t ext_len,
						  m0_bindex_t unit_sz)
{

	/* Compute how many unit starts in a given extent spans:
	 * Illustration below shows how extents can be received w.r.t
	 * unit size (4)
	 *    | Unit 0|| Unit 1|| Unit 2|| Unit 3 |
	      |0|1|2|3||4|5|6|7||8|9|a|b|
	 * 1. | e1|                            => 1 (0,2)(ext_start,ext_len)
	 * 2. |   e2  |                        => 1 (0,4) ending on unit 0
	 * 3. |   e2     |                     => 2 (0,5) end on unit 1's start
	 * 4.   |   e3   |                     => 1 (2,5)
	 * 5.   | e4 |                         => 0 (1,3) within unit 0
	 * 6.     | e5 |                       => 1 (3,5) ending on unit 1 end
	 * 7.       |   e6    |                => 2 (3,8) ending on unit 2
	 * To compute how many DU start we need to find the DU Index of
	 * start and end.
	 * Overall: compute unit start and end index, difference would give us
	 * number of unit boundaries in extent span, but it would miss start
	 * unit if it is on unit boundary, so we need to additionally check for
	 * same.
	 */

	m0_bcount_t cs_nob;
	M0_ASSERT(unit_sz);
	cs_nob = ((ext_start + ext_len - 1) / unit_sz - ext_start / unit_sz);

	/* Add handling for case 1 and 5 */
	if ((ext_start % unit_sz) == 0)
		cs_nob++;

	return cs_nob;
}


M0_INTERNAL m0_bcount_t m0_extent_get_unit_offset(m0_bindex_t off,
						  m0_bindex_t base_off,
						  m0_bindex_t unit_sz)
{
	M0_ASSERT(unit_sz);
	/* Unit size we get from layout id using
	 * m0_obj_layout_id_to_unit_size(lid)
	 */
	return (off - base_off) / unit_sz;
}

M0_INTERNAL void * m0_ext_get_cksum_addr(void *b_addr,
					 m0_bindex_t off,
					 m0_bindex_t base_off,
					 m0_bindex_t unit_sz,
					 m0_bcount_t cs_size)
{
	M0_ASSERT(unit_sz && cs_size);
	return (char *)b_addr +
	       m0_extent_get_unit_offset(off, base_off, unit_sz) *
	       cs_size;
}

M0_INTERNAL m0_bcount_t m0_ext_get_cksum_nob(m0_bindex_t ext_start,
					     m0_bindex_t ext_length,
					     m0_bindex_t unit_sz,
					     m0_bcount_t cs_size)
{
	M0_ASSERT(unit_sz && cs_size);
	return m0_ext_get_num_unit_start(ext_start, ext_length, unit_sz) *
	       cs_size;
}

/* This function will get checksum address for application provided checksum
 * buffer Checksum is corresponding to on offset (e.g gob offset) & its extent
 * and this function helps to locate exact address for the above.
 * Checksum is stored in contigious buffer: cksum_buf_vec, while COB extents may
 * not be contigious e.g.
 * Assuming each extent has two DU, so two checksum.
 *     | CS0 | CS1 | CS2 | CS3 | CS4 | CS5 | CS6 |
 *     | iv_index[0] |   | iv_index[1] | iv_index[2] |   | iv_index[3] |
 * Now if we have an offset for CS3 then after first travesal b_addr will point
 * to start of CS2 and then it will land in m0_ext_is_in and will compute
 * correct addr for CS3.
 */

M0_INTERNAL void * m0_extent_vec_get_checksum_addr(void *cksum_buf_vec,
						   m0_bindex_t off,
						   void *ivec,
						   m0_bindex_t unit_sz,
						   m0_bcount_t cs_sz)
{
	void                        *cksum_addr = NULL;
	struct m0_ext                ext;
	struct m0_indexvec          *vec = (struct m0_indexvec *)ivec;
	struct m0_bufvec            *cksum_vec;
	struct m0_bufvec_cursor      cksum_cursor;
	int                          attr_nob = 0;
	int                          i;

	cksum_vec = (struct m0_bufvec *)cksum_buf_vec;

	/* Get the checksum nobs consumed till reaching the off in given io */
	for (i = 0; i < vec->iv_vec.v_nr; i++) {
		ext.e_start = vec->iv_index[i];
		ext.e_end = vec->iv_index[i] + vec->iv_vec.v_count[i];

		/* We construct current extent e.g for iv_index[0] and check if
		 * offset is within the span of current extent
		 * | iv_index[0] || iv_index[1] | iv_index[2] || iv_index[3] |
		 */
		if (m0_ext_is_in(&ext, off)) {
			attr_nob += (m0_extent_get_unit_offset(off, ext.e_start,
							       unit_sz) *
							       cs_sz);
			break;
		}
		else {
			/* off is not in the current extent, so account
			 * increment the b_addr */
			attr_nob += m0_ext_get_cksum_nob(ext.e_start,
							 vec->iv_vec.v_count[i],
							 unit_sz, cs_sz);
		}
	}

	/* Assert to make sure the the offset is lying within the extent */
	M0_ASSERT(i < vec->iv_vec.v_nr);

	/* get the checksum_addr */
	m0_bufvec_cursor_init(&cksum_cursor, cksum_vec);

	if (attr_nob) {
		m0_bufvec_cursor_move(&cksum_cursor, attr_nob);
	}
	cksum_addr = m0_bufvec_cursor_addr(&cksum_cursor);
	return cksum_addr;
}

/* This function calculates checksum for data read */
M0_INTERNAL int m0_prepare_checksum(uint8_t pi_type,
				    struct m0_uint128 obj_id,
				    struct m0_indexvec *ext,
				    struct m0_bufvec *data,
				    struct m0_bufvec *attr,
				    uint64_t usz)
{
	struct m0_pi_seed                  seed;
	struct m0_bufvec_cursor            datacur;
	struct m0_bufvec_cursor            tmp_datacur;
	struct m0_ivec_cursor              extcur;
	struct m0_bufvec                   user_data = {};
	unsigned char                      curr_context[sizeof(MD5_CTX)];
	int                                rc = 0;
	int                                i;
	int                                count;
	struct m0_md5_inc_context_pi       pi={};
	int                                attr_idx = 0;
	uint32_t                           nr_seg;
	m0_bcount_t                        bytes;
	enum m0_pi_calc_flag               flag = M0_PI_CALC_UNIT_ZERO;

	if(attr == NULL)
		return 0;

	m0_bufvec_cursor_init(&datacur, data);
	m0_bufvec_cursor_init(&tmp_datacur, data);
	m0_ivec_cursor_init(&extcur, ext);
	memset(&pi, 0, sizeof(struct m0_md5_inc_context_pi));

	while (!m0_bufvec_cursor_move(&datacur, 0) &&
		!m0_ivec_cursor_move(&extcur, 0)){
		nr_seg = 0;
		count = usz;
		/* calculate number of segments required for 1 data unit */
		while (count > 0 && !m0_bufvec_cursor_move(&tmp_datacur, 0)) {
			nr_seg++;
			bytes = m0_bufvec_cursor_step(&tmp_datacur);
			if (bytes < count) {
				m0_bufvec_cursor_move(&tmp_datacur, bytes);
				count -= bytes;
			} else {
				m0_bufvec_cursor_move(&tmp_datacur, count);
				count = 0;
			}
		}
		/* allocate an empty buf vec */
		rc = m0_bufvec_empty_alloc(&user_data, nr_seg);
		if (rc != 0) {
			// M0_LOG(M0_ERROR, "buffer allocation failed, rc %d", rc);
			return false;
		}
		/* populate the empty buf vec with data pointers
		 * and create 1 data unit worth of buf vec
		 */
		i = 0;
		count = usz;
		while (count > 0 && !m0_bufvec_cursor_move(&datacur, 0)) {
			bytes = m0_bufvec_cursor_step(&datacur);
			if (bytes < count) {
				user_data.ov_vec.v_count[i] = bytes;
				user_data.ov_buf[i] = m0_bufvec_cursor_addr(&datacur);
				m0_bufvec_cursor_move(&datacur, bytes);
				count -= bytes;
			}
			else {
				user_data.ov_vec.v_count[i] = count;
				user_data.ov_buf[i] = m0_bufvec_cursor_addr(&datacur);
				m0_bufvec_cursor_move(&datacur, count);
				count = 0;
			}
			i++;
		}
		M0_ASSERT(attr->ov_vec.v_nr != 0 && attr->ov_vec.v_count[attr_idx] != 0);
		if (attr_idx != 0 && pi_type == M0_PI_TYPE_MD5_INC_CONTEXT) {
			flag = M0_PI_NO_FLAG;
			memcpy(pi.pimd5c_prev_context, curr_context, sizeof(MD5_CTX));
		}
		seed.pis_data_unit_offset = m0_ivec_cursor_index(&extcur)/usz;
		seed.pis_obj_id.f_container = obj_id.u_hi;
		seed.pis_obj_id.f_key = obj_id.u_lo;
		pi.pimd5c_hdr.pih_type = pi_type;
		rc = m0_client_calculate_pi((struct m0_generic_pi *)&pi,
					    &seed, &user_data, flag,
					    curr_context, NULL);
		memcpy(attr->ov_buf[attr_idx], &pi,
		       m0_cksum_get_size(pi_type));
		attr_idx++;
		M0_ASSERT(attr_idx <= attr->ov_vec.v_nr);
		m0_ivec_cursor_move(&extcur, usz);
		m0_bufvec_free2(&user_data);
	}
	return rc;
}
