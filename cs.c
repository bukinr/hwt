#include <stdio.h>
#include <string.h>

#include <opencsd/c_api/ocsd_c_api_types.h>
#include <opencsd/c_api/opencsd_c_api.h>

#include "hwt_var.h"
#include "cs.h"

#define	PMCTRACE_CS_DEBUG
#undef	PMCTRACE_CS_DEBUG

#ifdef	PMCTRACE_CS_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static dcd_tree_handle_t dcdtree_handle;
static int cs_flags = 0;
#define	FLAG_FORMAT			(1 << 0)
#define	FLAG_FRAME_RAW_UNPACKED		(1 << 1)
#define	FLAG_FRAME_RAW_PACKED		(1 << 2)
#define	FLAG_CALLBACK_MEM_ACC		(1 << 3)

#define	PACKET_STR_LEN	1024
static char packet_str[PACKET_STR_LEN];

static ocsd_err_t
attach_raw_printers(dcd_tree_handle_t dcd_tree_h)
{
	ocsd_err_t err;
	int flags;

	flags = 0;
	err = OCSD_OK;

	if (cs_flags & FLAG_FRAME_RAW_UNPACKED)
		flags |= OCSD_DFRMTR_UNPACKED_RAW_OUT;

	if (cs_flags & FLAG_FRAME_RAW_PACKED)
		flags |= OCSD_DFRMTR_PACKED_RAW_OUT;

	if (flags)
		err = ocsd_dt_set_raw_frame_printer(dcd_tree_h, flags);

	return err;
}

static int
print_data_array(const uint8_t *p_array, const int array_size,
    char *p_buffer, int buf_size)
{
	int bytes_processed;
	int chars_printed;

	chars_printed = 0;
	p_buffer[0] = 0;

	if (buf_size > 9) {
		strcat(p_buffer, "[ ");
		chars_printed += 2;

		for (bytes_processed = 0; bytes_processed < array_size;
		    bytes_processed++) {
			sprintf(p_buffer + chars_printed, "0x%02X ",
			    p_array[bytes_processed]);
			chars_printed += 5;
			if ((chars_printed + 5) > buf_size)
				break;
		}

		strcat(p_buffer, "];");
		chars_printed += 2;
	} else if (buf_size >= 4) {
		sprintf(p_buffer, "[];");
		chars_printed += 3;
	}

	return (chars_printed);
}

static void
packet_monitor(void *context __unused,
    const ocsd_datapath_op_t op,
    const ocsd_trc_index_t index_sop,
    const void *p_packet_in,
    const uint32_t size,
    const uint8_t *p_data)
{
	int offset;

	offset = 0;

	switch(op) {
	case OCSD_OP_DATA:
		sprintf(packet_str, "Idx:%"  OCSD_TRC_IDX_STR ";", index_sop);
		offset = strlen(packet_str);
		offset += print_data_array(p_data, size, packet_str + offset,
		    PACKET_STR_LEN - offset);

		/*
		 * Got a packet -- convert to string and use the libraries'
		 * message output to print to file and stdoout
		 */

		if (ocsd_pkt_str(OCSD_PROTOCOL_ETMV4I, p_packet_in,
		    packet_str + offset, PACKET_STR_LEN - offset) == OCSD_OK) {
			/* add in <CR> */
			if (strlen(packet_str) == PACKET_STR_LEN - 1)/*maxlen*/
				packet_str[PACKET_STR_LEN - 2] = '\n';
			else
				strcat(packet_str,"\n");

			/* print it using the library output logger. */
			ocsd_def_errlog_msgout(packet_str);
		}
		break;

	case OCSD_OP_EOT:
		sprintf(packet_str,"**** END OF TRACE ****\n");
		ocsd_def_errlog_msgout(packet_str);
		break;
	default:
		printf("%s: unknown op %d\n", __func__, op);
		break;
	}
}

static uint32_t
cs_cs_decoder__mem_access(const void *context __unused,
    const ocsd_vaddr_t address __unused,
    const ocsd_mem_space_acc_t mem_space __unused,
    const uint32_t req_size __unused, uint8_t *buffer __unused)
{

	/* TODO */

	return (0);
}

static ocsd_err_t
create_test_memory_acc(dcd_tree_handle_t handle, uintptr_t base,
    uintptr_t start, uintptr_t end)
{
	ocsd_vaddr_t address;
	uint8_t *p_mem_buffer;
	uint32_t mem_length;
	int ret;

	dprintf("%s: base %lx start %lx end %lx\n",
	    __func__, base, start, end);

	address = (ocsd_vaddr_t)base;
	p_mem_buffer = (uint8_t *)(base + start);
	mem_length = (end - start);

	if (cs_flags & FLAG_CALLBACK_MEM_ACC)
		ret = ocsd_dt_add_callback_mem_acc(handle, base + start,
			base + end - 1, OCSD_MEM_SPACE_ANY,
			cs_cs_decoder__mem_access, NULL);
	else
		ret = ocsd_dt_add_buffer_mem_acc(handle, address,
		    OCSD_MEM_SPACE_ANY, p_mem_buffer, mem_length);

	if (ret != OCSD_OK)
		printf("%s: can't create memory accessor: ret %d\n",
		    __func__, ret);

	return (ret);
}

static ocsd_err_t
create_generic_decoder(dcd_tree_handle_t handle, const char *p_name,
    const void *p_cfg, const void *p_context __unused, uint64_t base,
    uint64_t start, uint64_t end)
{
	ocsd_err_t ret;
	uint8_t CSID;

	CSID = 0;

	dprintf("%s\n", __func__);

	ret = ocsd_dt_create_decoder(handle, p_name,
	    OCSD_CREATE_FLG_FULL_DECODER, p_cfg, &CSID);
	if(ret != OCSD_OK)
		return (-1);

	if (cs_flags & FLAG_FORMAT) {
		ret = ocsd_dt_attach_packet_callback(handle, CSID,
		    OCSD_C_API_CB_PKT_MON, packet_monitor, p_context);
		if (ret != OCSD_OK)
			return (-1);
	}

	/* attach a memory accessor */
	ret = create_test_memory_acc(handle, base, start, end);
	if(ret != OCSD_OK)
		ocsd_dt_remove_decoder(handle,CSID);

	return (ret);
}

static ocsd_err_t
create_decoder_etmv4(dcd_tree_handle_t dcd_tree_h, uint64_t base,
    uint64_t start, uint64_t end)
{
	ocsd_etmv4_cfg trace_config;
	ocsd_err_t ret;

	trace_config.arch_ver = ARCH_V8;
	trace_config.core_prof = profile_CortexA;

	trace_config.reg_configr = 0x000000C1;
	trace_config.reg_traceidr = 0x00000010;   /* Trace ID */

	trace_config.reg_idr0   = 0x28000EA1;
	trace_config.reg_idr1   = 0x4100F403;
	trace_config.reg_idr2   = 0x00000488;
	trace_config.reg_idr8   = 0x0;
	trace_config.reg_idr9   = 0x0;
	trace_config.reg_idr10  = 0x0;
	trace_config.reg_idr11  = 0x0;
	trace_config.reg_idr12  = 0x0;
	trace_config.reg_idr13  = 0x0;

	ret = create_generic_decoder(dcd_tree_h, OCSD_BUILTIN_DCD_ETMV4I,
	    (void *)&trace_config, 0, base, start, end);

	return (ret);
}

int
cs_init(struct trace_context *tc)
{
	uintptr_t start, end;
	int error;

	ocsd_def_errlog_init(OCSD_ERR_SEV_INFO, 1);
	ocsd_def_errlog_init(0, 0);

	dcdtree_handle = ocsd_create_dcd_tree(OCSD_TRC_SRC_FRAME_FORMATTED,
	    OCSD_DFRMTR_FRAME_MEM_ALIGN);
	if (dcdtree_handle == C_API_INVALID_TREE_HANDLE) {
		printf("can't find dcd tree\n");
		return (-1);
	}

	start = (uintptr_t)tc->base;
	end = (uintptr_t)tc->base + tc->bufsize;

	error = create_decoder_etmv4(dcdtree_handle,
	    (uint64_t)tc->base, start, end);
	if (error != OCSD_OK) {
		printf("can't create decoder: base %lx start %lx end %lx\n",
		    (uint64_t)tc->base, start, end);
		return (-2);
	}

#ifdef PMCTRACE_CS_DEBUG
	ocsd_tl_log_mapped_mem_ranges(dcdtree_handle);
#endif

	if (cs_flags & FLAG_FORMAT)
		ocsd_dt_set_gen_elem_printer(dcdtree_handle);
#if 0
	else
		ocsd_dt_set_gen_elem_outfn(dcdtree_handle,
		    gen_trace_elem_print_lookup,
		    (const struct mtrace_data *)&tc->mdata);
#endif

	attach_raw_printers(dcdtree_handle);

	return (0);
}
