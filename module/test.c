#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/frontswap.h>
#include <linux/crypto.h>
#include <linux/types.h>

#include "upmem/dpu_description.h"

#include "upmem/uapi/dpu.h"
#include "upmem/uapi/dpu_management.h"
#include "upmem/uapi/dpu_memory.h"
#include "upmem/uapi/dpu_runner.h"

#define XFER_SIZE_BYTES 4096

dpu_description_t dpu_description;

struct dpu_transfer_matrix xfer_write_matrix = { 0 };
struct dpu_transfer_matrix xfer_read_matrix = { 0 };

static int dpu_alloc_all(struct dpu_rank_t ***ranks, uint32_t *nr_ranks) {
	int ret = 0;

	*nr_ranks = dpu_get_number_of_available_ranks();
	if (*nr_ranks == 0) {
		return 0;
	}

	*ranks = vmalloc(sizeof(dpu_rank_t *) * nr_ranks);
	if (*ranks == NULL) {
		pr_warn("ERROR: Failed to allocate rank buffer\n");
		return -1;
	}

	uint32_t r;
	for (r = 0; r < nr_ranks; r++) {
		uint32_t status = dpu_rank_alloc(ranks[r]);
		if (status != 0) {
			pr_warn("ERROR: Failed to allocate rank\n");
			*ranks[r] = NULL;
			ret = -1;
			goto err;
		}
		status = dpu_reset_rank(ranks[r]);
		if (status != 0) 	{
			pr_warn("ERROR: Failed to reset rank\n");
			ret = -1;
			goto err;
		}
	}

	pr_err("%s: Allocated %u ranks\n", __func__, *nr_ranks);

	return ret;

err:
	for (r = 0; r < nr_ranks; r++) {
		if (*ranks[r] != NULL) {
			dpu_rank_free(*ranks[r]);
		}
	}
	vfree(*ranks);
	return ret;
}

static int __init init_pim_swap(void)
{
	int ret = 0;
	uint32_t nr_ranks;
	struct dpu_rank_t **ranks;

	pr_err("%s: started\n", __func__);

	ret = dpu_alloc_all(&ranks, &nr_ranks)
	if (ret != 0) {
		return ret;
	}

	uint8_t in_buf[XFER_SIZE_BYTES];
	uint8_t out_buf[XFER_SIZE_BYTES];
	memset(in_buf, 0, sizeof(in_buf));
	memset(out_buf, 0, sizeof(out_buf));

	uint32_t r;
	for (r = 0; r < nr_ranks; r++) {
		// LOAD
		uint32_t nr_dpus = dpu_get_number_of_dpus_for_rank(ranks[r]);
		uint32_t d;
		for (d = 0; d < nr_dpus; d++) {
			uint8_t dpu_id = d % 8;
			uint8_t ci_id = d / 8;

			// ZERO, here for example
			uint32_t mram_byte_offset_in_objdump = 0x08000000;
    		mram_byte_offset_in_objdump &= ~0x08000000;

			struct dpu_t dpu = dpu_get(ranks[r], ci_id, dpu_id);

			status = dpu_copy_to_mram(dpu, mram_byte_offset_in_objdump, in_buf, sizeof(in_buf));
			if (status != 0) {
				pr_warn("ERROR: Failed to copy buffer to MRAM\n");
				ret = -1;
				goto cleanup;
			}
		}

		// START

		// load and execute the program
		/* elf should be located at /lib/firmware */
		status = dpu_load(ranks[r], "test.dpu");
		if (status != 0) {
			pr_warn("ERROR: Failed to load DPU program into rank %u\n", r);
			ret = -1;
			goto cleanup;
		}

		status = dpu_boot_rank(ranks[r]);
		if (status != 0) {
			pr_warn("ERROR: Failed to load DPU program into rank %u\n", r);
			ret = -1;
			goto put_rank;
		}

		uint8_t nb_dpu_running;
		do {
			status = dpu_poll_rank(ranks[r], &nb_dpu_running);
		} while ((status == 0) && (nb_dpu_running != 0));

		if (status != 0) {
			pr_warn("ERROR: Failed to poll rank %u\n", r);
			ret = -1;
			goto cleanup;
		}

		// COPY OUT

		for (d = 0; d < nb_dpus; d++) {
			uint8_t dpu_id = d % 8;
			uint8_t ci_id = d / 8;

			// ZERO, here for example
			uint32_t mram_byte_offset_in_objdump = 0x08000000;
    		mram_byte_offset_in_objdump &= ~0x08000000;

			struct dpu_t dpu = dpu_get(ranks[r], ci_id, dpu_id);
			status = dpu_copy_from_mram(dpu, out_buffer[dpu_id], mram_byte_offset, sizeof(out_buffer));
			if (status != 0) {
				pr_warn("ERROR: Failed to copy from MRAM for (CI%u; DPU%u)\n",
						ci_id, dpu_id);
				ret = -1;
				continue;
			}
		}

	}

	pr_err("%s: done\n", __func__);

cleanup:
	for (r = 0; r < nr_ranks; r++) {
		if (ranks[r] != NULL) {
			dpu_rank_free(ranks[r]);
		}
	}
	vfree(ranks);

	if (ret == 0) {
		pr_info("SUCCESS\n");
	}

	return ret;
}

static void __exit exit_pim_swap(void)
{
	pr_err("%s: exit\n", __func__);
}

module_init(init_pim_swap);
module_exit(exit_pim_swap);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Joel Nider <joel@nider.org>");
MODULE_DESCRIPTION("Compressed cache for swap pages using UPMEM processing-in-memory");
