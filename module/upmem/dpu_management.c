/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright 2020 UPMEM. All rights reserved. */
#include <linux/list.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include <dpu_region_address_translation.h>
#include <dpu_rank.h>
#include <dpu_management.h>
#include <dpu_region.h>
#include <dpu_types.h>
#include <dpu_utils.h>

LIST_HEAD(rank_list);
DEFINE_SPINLOCK(rank_list_lock);

uint32_t dpu_rank_alloc(struct dpu_rank_t **rank)
{
	struct dpu_rank_t *rank_iterator;

	*rank = NULL;

	spin_lock(&rank_list_lock);
	list_for_each_entry (rank_iterator, &rank_list, list) {
		if (dpu_rank_get(rank_iterator) == DPU_OK) {
			*rank = rank_iterator;
			spin_unlock(&rank_list_lock);
			return DPU_OK;
		}
	}
	spin_unlock(&rank_list_lock);

	pr_warn("Failed to allocate rank, no available rank.\n");

	return DPU_ERR_DRIVER;
}
EXPORT_SYMBOL(dpu_rank_alloc);

uint32_t dpu_rank_free(struct dpu_rank_t *rank)
{
	dpu_rank_put(rank);

	return DPU_OK;
}
EXPORT_SYMBOL(dpu_rank_free);

/*
 * NOTE: Careful here this is inherently racy with the following allocations
 * since a rank might get allocated before this function returns.
 */
uint32_t dpu_get_number_of_available_ranks(void)
{
	struct dpu_rank_t *rank_iterator;
	uint32_t nr_available = 0;

	spin_lock(&rank_list_lock);
	list_for_each_entry (rank_iterator, &rank_list, list) {
		if (rank_iterator->owner.is_owned == 0)
			nr_available++;
	}
	spin_unlock(&rank_list_lock);

	return nr_available;
}
EXPORT_SYMBOL(dpu_get_number_of_available_ranks);

/*
 * NOTE: Careful here this is inherently racy with the following allocations
 * since a rank might get allocated before this function returns.
 */
bool dpu_is_dimm_used(struct dpu_rank_t *rank)
{
	struct dpu_rank_t *rank_iterator;
	const char *sn = rank->serial_number;
	int nr_ranks_used = 0;

	/* We cannot relate the rank to a DIMM */
	if (!strcmp(sn, "")) {
		return true;
	}

	spin_lock(&rank_list_lock);
	list_for_each_entry (rank_iterator, &rank_list, list) {
		if ((!strcmp(rank_iterator->serial_number, sn)) &&
		    (rank_iterator->owner.is_owned)) {
			nr_ranks_used++;
		}
	}
	spin_unlock(&rank_list_lock);

	return (nr_ranks_used == 0) ? false : true;
}
EXPORT_SYMBOL(dpu_is_dimm_used);

uint32_t dpu_get_number_of_dpus_for_rank(struct dpu_rank_t *rank)
{
	uint8_t nr_cis, nr_dpus_per_ci;

	nr_cis = rank->region->addr_translate.desc.topology
			 .nr_of_control_interfaces;
	nr_dpus_per_ci = rank->region->addr_translate.desc.topology
				 .nr_of_dpus_per_control_interface;

	return nr_cis * nr_dpus_per_ci;
}
EXPORT_SYMBOL(dpu_get_number_of_dpus_for_rank);

struct dpu_t *dpu_get(struct dpu_rank_t *rank, uint8_t ci_id, uint8_t dpu_id)
{
	uint8_t nr_cis, nr_dpus_per_ci;

	nr_cis = rank->region->addr_translate.desc.topology
			 .nr_of_control_interfaces;
	nr_dpus_per_ci = rank->region->addr_translate.desc.topology
				 .nr_of_dpus_per_control_interface;

	if ((ci_id >= nr_cis) || (dpu_id >> nr_dpus_per_ci))
		return NULL;

	return rank->dpus + DPU_INDEX(rank, ci_id, dpu_id);
}
EXPORT_SYMBOL(dpu_get);

uint8_t dpu_get_slice_id(struct dpu_t *dpu)
{
	return dpu->slice_id;
}
EXPORT_SYMBOL(dpu_get_slice_id);

uint8_t dpu_get_member_id(struct dpu_t *dpu)
{
	return dpu->dpu_id;
}
EXPORT_SYMBOL(dpu_get_member_id);