#include <opencsd/c_api/ocsd_c_api_types.h>
#include <opencsd/c_api/opencsd_c_api.h>

#include "cs.h"

int
cs_init(void)
{

	ocsd_def_errlog_init(OCSD_ERR_SEV_INFO, 1);
	ocsd_def_errlog_init(0, 0);

	return (0);
}
