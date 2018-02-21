/*
 *    This file is part of acados.
 *
 *    acados is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    acados is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with acados; if not, write to the Free Software Foundation,
 *    Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

// external
#include <assert.h>
// hpipm
#include "hpipm/include/hpipm_d_ocp_qp.h"
#include "hpipm/include/hpipm_d_ocp_qp_sol.h"
#include "hpipm/include/hpipm_d_ocp_qp_ipm.h"
// acados
#include "acados/ocp_qp/ocp_qp_common.h"
#include "acados/ocp_qp/ocp_qp_hpipm.h"
#include "acados/utils/timing.h"
#include "acados/utils/types.h"
#include "acados/utils/mem.h"



int ocp_qp_hpipm_opts_calculate_size(void *config_, ocp_qp_dims *dims)
{
    int size = 0;
    size += sizeof(ocp_qp_hpipm_opts);
    size += sizeof(struct d_ocp_qp_ipm_arg);
    size += d_memsize_ocp_qp_ipm_arg(dims);

    return size;
}



void *ocp_qp_hpipm_opts_assign(void *config_, ocp_qp_dims *dims, void *raw_memory)
{
    ocp_qp_hpipm_opts *args;

    char *c_ptr = (char *) raw_memory;

    args = (ocp_qp_hpipm_opts *) c_ptr;
    c_ptr += sizeof(ocp_qp_hpipm_opts);

    args->hpipm_opts = (struct d_ocp_qp_ipm_arg *) c_ptr;
    c_ptr += sizeof(struct d_ocp_qp_ipm_arg);

    assert((size_t)c_ptr % 8 == 0 && "memory not 8-byte aligned!");

    d_create_ocp_qp_ipm_arg(dims, args->hpipm_opts, c_ptr);
    c_ptr += d_memsize_ocp_qp_ipm_arg(dims);

    assert((char*)raw_memory + ocp_qp_hpipm_opts_calculate_size(config_, dims) == c_ptr);

    return (void *)args;
}



void ocp_qp_hpipm_opts_initialize_default(void *config_, void *args_)
{
    ocp_qp_hpipm_opts *args = (ocp_qp_hpipm_opts *)args_;

    d_set_default_ocp_qp_ipm_arg(args->hpipm_opts);
	// overwrite some default options
    args->hpipm_opts->res_g_max = 1e-6;
    args->hpipm_opts->res_b_max = 1e-8;
    args->hpipm_opts->res_d_max = 1e-8;
    args->hpipm_opts->res_m_max = 1e-8;
    args->hpipm_opts->iter_max = 50;
    args->hpipm_opts->stat_max = 50;
    args->hpipm_opts->alpha_min = 1e-8;
    args->hpipm_opts->mu0 = 1;
}



int ocp_qp_hpipm_memory_calculate_size(void *config_, ocp_qp_dims *dims, void *args_)
{
    ocp_qp_hpipm_opts *args = (ocp_qp_hpipm_opts *)args_;

    int size = 0;
    size += sizeof(ocp_qp_hpipm_memory);

    size += sizeof(struct d_ocp_qp_ipm_workspace);

    size += d_memsize_ocp_qp_ipm(dims, args->hpipm_opts);

    return size;
}



void *ocp_qp_hpipm_memory_assign(void *config_, ocp_qp_dims *dims, void *args_, void *raw_memory)
{
    ocp_qp_hpipm_opts *args = (ocp_qp_hpipm_opts *)args_;
    ocp_qp_hpipm_memory *mem;

    // char pointer
    char *c_ptr = (char *)raw_memory;

    mem = (ocp_qp_hpipm_memory *) c_ptr;
    c_ptr += sizeof(ocp_qp_hpipm_memory);

    mem->hpipm_workspace = (struct d_ocp_qp_ipm_workspace *)c_ptr;
    c_ptr += sizeof(struct d_ocp_qp_ipm_workspace);

    struct d_ocp_qp_ipm_workspace *ipm_workspace = mem->hpipm_workspace;

    assert((size_t)c_ptr % 8 == 0 && "memory not 8-byte aligned!");

    // ipm workspace structure
    d_create_ocp_qp_ipm(dims, args->hpipm_opts, ipm_workspace, c_ptr);
    c_ptr += ipm_workspace->memsize;

    assert((char *)raw_memory + ocp_qp_hpipm_memory_calculate_size(config_, dims, args_) == c_ptr);

    return mem;
}



int ocp_qp_hpipm_workspace_calculate_size(void *config_, ocp_qp_dims *dims, void *args_)
{
    return 0;
}



int ocp_qp_hpipm(void *config_, ocp_qp_in *qp_in, ocp_qp_out *qp_out, void *args_, void *mem_, void *work_)
{
    ocp_qp_info *info = (ocp_qp_info *) qp_out->misc;
    acados_timer tot_timer, qp_timer;

     acados_tic(&tot_timer);
   // cast data structures
    ocp_qp_hpipm_opts *args = (ocp_qp_hpipm_opts *) args_;
    ocp_qp_hpipm_memory *memory = (ocp_qp_hpipm_memory *) mem_;

    // solve ipm
    acados_tic(&qp_timer);
    int hpipm_status = d_solve_ocp_qp_ipm(qp_in, qp_out, args->hpipm_opts, memory->hpipm_workspace);

    info->solve_QP_time = acados_toc(&qp_timer);
    info->interface_time = 0;  // there are no conversions for hpipm
    info->total_time = acados_toc(&tot_timer);
    info->num_iter = memory->hpipm_workspace->iter;

    // check exit conditions
    int acados_status = hpipm_status;
    if (hpipm_status == 0) acados_status = ACADOS_SUCCESS;
    if (hpipm_status == 1) acados_status = ACADOS_MAXITER;
    if (hpipm_status == 2) acados_status = ACADOS_MINSTEP;
    return acados_status;
}



void ocp_qp_hpipm_config_initialize_default(void *config_)
{

	ocp_qp_solver_config *config = config_;

	config->opts_calculate_size = &ocp_qp_hpipm_opts_calculate_size;
	config->opts_assign = &ocp_qp_hpipm_opts_assign;
	config->opts_initialize_default = &ocp_qp_hpipm_opts_initialize_default;
	config->memory_calculate_size = &ocp_qp_hpipm_memory_calculate_size;
	config->memory_assign = &ocp_qp_hpipm_memory_assign;
	config->workspace_calculate_size = &ocp_qp_hpipm_workspace_calculate_size;
	config->evaluate = &ocp_qp_hpipm;

	return;

}
