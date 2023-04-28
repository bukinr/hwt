#include <stdio.h>

#include <opencsd/c_api/ocsd_c_api_types.h>
#include <opencsd/c_api/opencsd_c_api.h>

#include "cs.h"

static dcd_tree_handle_t dcdtree_handle;

int
cs_init(void)
{

	ocsd_def_errlog_init(OCSD_ERR_SEV_INFO, 1);
	ocsd_def_errlog_init(0, 0);

	dcdtree_handle = ocsd_create_dcd_tree(OCSD_TRC_SRC_FRAME_FORMATTED,
	    OCSD_DFRMTR_FRAME_MEM_ALIGN);
	if (dcdtree_handle == C_API_INVALID_TREE_HANDLE) {
		printf("can't find dcd tree\n");
		return (-1);
	}

	return (0);
}
