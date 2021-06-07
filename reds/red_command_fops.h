#pragma once
#ifndef __MOTR_REDS_RED_COMMAND_FOPS_H__
#define __MOTR_REDS_RED_COMMAND_FOPS_H__

M0_INTERNAL void m0_red_command_fop_init(void);
M0_INTERNAL void m0_red_command_fop_fini(void);

extern struct m0_fop_type m0_red_command_req_fopt;
extern struct m0_fop_type m0_red_command_rep_fopt;

#endif
