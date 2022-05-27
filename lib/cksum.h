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


#pragma once

#ifndef __MOTR_CKSUM_H__
#define __MOTR_CKSUM_H__

#include "lib/vec.h"
#include "fid/fid.h"
#include "xcode/xcode_attr.h"
#ifndef __KERNEL__
#include <openssl/md5.h>
#endif

#define M0_CKSUM_DATA_ROUNDOFF_BYTE (16)

/* Default checksum type, TODO_DI: Get from config */
#define M0_CKSUM_DEFAULT_PI M0_PI_TYPE_MD5

/* Max checksum size for all supported PIs */
#define M0_CKSUM_MAX_SIZE (sizeof(struct m0_md5_pi) > \
                           sizeof(struct m0_md5_inc_context_pi) ? \
                           sizeof(struct m0_md5_pi) : \
                           sizeof(struct m0_md5_inc_context_pi))


/*
 * Macro calculates the size of padding required in a struct
 * for byte alignment
 * size - size of all structure members
 * alignment - power of two, byte alignment
 */
#define M0_CALC_PAD(size, alignment) ( size%alignment ? (((size/alignment + 1 ) * alignment) - size) : 0)


/* Constants for protection info type, max types supported is 255 */
enum m0_pi_algo_type
{
        M0_PI_TYPE_RESERVED,
        M0_PI_TYPE_MD5 = 1,
        M0_PI_TYPE_MD5_INC_CONTEXT,
        M0_PI_TYPE_CRC,
        M0_PI_TYPE_MAX
};

enum m0_pi_calc_flag {
        /* NO PI FLAG */
        M0_PI_NO_FLAG = 0,
        /* PI calculation for data unit 0 */
        M0_PI_CALC_UNIT_ZERO = 1 << 0,
        /* Skip PI final value calculation */
        M0_PI_SKIP_CALC_FINAL = 1 << 1

};

M0_BASSERT(M0_PI_TYPE_MAX <= 255);

struct m0_pi_hdr {
        /* type of protection algorithm being used */
        uint8_t pih_type : 8;
        /*size of PI Structure in multiple of  32 bytes*/
        uint8_t pih_size : 8;
};
/*********************** MD5 Cksum Structure ***************************************/
/** Padding size for MD5 structure */
#define M0_CKSUM_PAD_MD5 (M0_CALC_PAD((sizeof(struct m0_pi_hdr)+MD5_DIGEST_LENGTH), \
			               M0_CKSUM_DATA_ROUNDOFF_BYTE))

/** MD5 checksum structure, the checksum value is in pimd5_value */
struct m0_md5_pi {
        /* header for protection info */
        struct m0_pi_hdr pimd5_hdr;
#ifndef __KERNEL__
        /* protection value computed for the current data*/
        unsigned char    pimd5_value[MD5_DIGEST_LENGTH];
        /* structure should be 32 byte aligned */
        char             pimd5_pad[M0_CKSUM_PAD_MD5];
#endif
};

/*********************** MD5 Including Context Checksum Structure ******************/
/** Padding size for MD5 Including Context structure */
#define M0_CKSUM_PAD_MD5_INC_CXT (M0_CALC_PAD((sizeof(struct m0_pi_hdr)+ \
				     sizeof(MD5_CTX)+MD5_DIGEST_LENGTH), M0_CKSUM_DATA_ROUNDOFF_BYTE))

/** MD5 checksum structure:
 *  - The computed checksum value will be in pimd5c_value.
 *  - Input context from previous MD5 computation in pimd5c_prev_context
 */
struct m0_md5_inc_context_pi {
        /* header for protection info */
        struct m0_pi_hdr pimd5c_hdr;
#ifndef __KERNEL__
        /*context of previous data unit, required for checksum computation */
        unsigned char    pimd5c_prev_context[sizeof(MD5_CTX)];
        /* protection value computed for the current data unit.
         * If seed is not provided then this checksum is
         * calculated without seed.
         */
        unsigned char    pimd5c_value[MD5_DIGEST_LENGTH];
        /* structure should be 32 byte aligned */
        char             pi_md5c_pad[M0_CKSUM_PAD_MD5_INC_CXT];
#endif
};

/*********************** Generic Protection Info Structure *****************/
struct m0_generic_pi {
        /* header for protection info */
        struct m0_pi_hdr pi_hdr;
        /*pointer to access specific pi structure fields*/
        void            *pi_t_pi;
};

/* seed values for calculating checksum */
struct m0_pi_seed {
        struct m0_fid pis_obj_id;
        /* offset within motr object */
        m0_bindex_t   pis_data_unit_offset;
};

/**
 * Get checksum size for the type of PI algorithm
 * @param pi_type Type of PI algorithm
 */
M0_INTERNAL uint32_t m0_cksum_get_size(enum m0_pi_algo_type pi_type);

/**
 * Return max cksum size possible
 */
M0_INTERNAL uint32_t m0_cksum_get_max_size(void);

/**
 * Calculate checksum/protection info for data/KV
 *
 * @param[IN/OUT] pi  Caller will pass Generic pi struct, which will be typecasted to
 *                    specific PI type struct. API will calculate the checksum and set
 *                    pi_value of PI type struct. In case of context, caller will send
 *                    data unit N-1 unit's context via prev_context field in PI type struct.
 *                    This api will calculate unit N's context and set value in curr_context.
 *                    IN values - pi_type, pi_size, prev_context
 *                    OUT values - pi_value, prev_context for first data unit.
 * @param[IN] seed seed value (pis_obj_id+pis_data_unit_offset) required to calculate
 *                 the checksum. If this pointer is NULL that means either
 *                 this checksum calculation is meant for KV or user does
 *                 not want seeding.
 *                 NOTE: seed is always NULL, non-null value sent at the last chunk of motr unit
 * @param[IN] m0_bufvec Set of buffers for which checksum is computed. Normally
 *                      this set of vectors will make one data unit. It can be NULL as well.
 * @param[IN] flag If flag is M0_PI_CALC_UNIT_ZERO, it means this api is called for
 *                 first data unit and init functionality should be invoked such as MD5_Init.
 * @param[OUT] curr_context context of data unit N, will be required to calculate checksum for
 *                         next data unit, N+1. This api will calculate and set value for this field.
 *                         NOTE: curr_context always have unseeded and non finalised context value
 *                         and sending this parameter is mandatory.
 * @param[OUT] pi_value_without_seed Caller may need checksum value without seed and with seed.
 *                                   With seed checksum is set in pi_value of PI type struct.
 *                                   Without seed checksum is set in this field.
 *                                   Caller has to allocate memory for this filed.
 */

int m0_client_calculate_pi(struct m0_generic_pi *pi,
                struct m0_pi_seed *seed,
                struct m0_bufvec *bvec,
                enum m0_pi_calc_flag flag,
                unsigned char *curr_context,
                unsigned char *pi_value_without_seed);


/**
 * Calculates checksum for data sent and compare it with checksum value sent.
 * If newly calculated checksum on data and checksum sent matches, return
 * true else return false.
 * @param[IN] pi This ia an already calculated checksum value structure.
 * @param[IN] seed This seed is required to calculate checksum
 * @param[IN] bvec buffer vector which contains pointers to data on which
 *                 checksum is to be calculated.
 */
bool m0_calc_verify_cksum_one_unit(struct m0_generic_pi *pi,
                                   struct m0_pi_seed *seed,
                                   struct m0_bufvec *bvec);

#endif /* __MOTR_CKSUM_H__ */
