/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _CDP_TXRX_PPE_H_
#define _CDP_TXRX_PPE_H_

/**
 * cdp_ppesds_entry_attach() - attach the ppe vp interface.
 * @soc: data path soc handle
 * @vdev_id: vdev id
 * @vpai: PPE VP opaque
 * @ppe_vp_num: Allocated VP Port number
 *
 * return: qdf_status where vp entry got allocated or not.
 */
static inline
QDF_STATUS cdp_ppesds_entry_attach(struct cdp_soc_t *soc, uint8_t vdev_id,
				   void *vpai, int32_t *ppe_vp_num)
{
	if (!soc || !soc->ops || !soc->ops->ppeds_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (soc->ops->ppeds_ops->ppeds_entry_attach)
		return soc->ops->ppeds_ops->ppeds_entry_attach(soc, vdev_id,
							       vpai,
							       ppe_vp_num);

	return QDF_STATUS_E_INVAL;
}

/**
 * cdp_ppesds_entry_detach() - Detach the PPE VP interface.
 * @soc: data path soc handle
 * @vdev_id: vdev ID
 *
 * return: void
 */
static inline
void cdp_ppesds_entry_detach(struct cdp_soc_t *soc, uint8_t vdev_id)
{
	if (!soc || !soc->ops || !soc->ops->ppeds_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return;
	}

	if (soc->ops->ppeds_ops->ppeds_entry_detach)
		return soc->ops->ppeds_ops->ppeds_entry_detach(soc, vdev_id);
}

/**
 * cdp_ppesds_set_int_pri2tid() - Set the INT_PRI to TID
 * @soc: data path soc handle
 * @pri2tid: PRI2TID table
 *
 * return: void
 */
static inline
void cdp_ppesds_set_int_pri2tid(struct cdp_soc_t *soc,
				uint8_t *pri2tid)
{
	if (!soc || !soc->ops || !soc->ops->ppeds_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return;
	}

	if (soc->ops->ppeds_ops->ppeds_set_int_pri2tid)
		return soc->ops->ppeds_ops->ppeds_set_int_pri2tid(soc, pri2tid);
}

/**
 * cdp_ppesds_update_int_pri2tid() - Update the INT_PRI to TID
 * @soc: data path soc handle
 * @pri: Priority index
 * @tid: TID mapped to the input priority
 *
 * return: void
 */
static inline
void cdp_ppesds_update_int_pri2tid(struct cdp_soc_t *soc,
				   uint8_t pri, uint8_t tid)
{
	if (!soc || !soc->ops || !soc->ops->ppeds_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
	}

	if (soc->ops->ppeds_ops->ppeds_update_int_pri2tid)
		return soc->ops->ppeds_ops->ppeds_update_int_pri2tid(soc, pri,
								     tid);
}

/**
 * cdp_ppesds_entry_dump() - Dump the PPE VP entries
 * @soc: data path soc handle
 *
 * return: void
 */
static inline
void cdp_ppesds_entry_dump(struct cdp_soc_t *soc)
{
	if (!soc || !soc->ops || !soc->ops->ppeds_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return;
	}

	if (soc->ops->ppeds_ops->ppeds_entry_dump)
		soc->ops->ppeds_ops->ppeds_entry_dump(soc);
}

/**
 * cdp_ppesds_enable_pri2tid() - Enable PPE VP PRI2TID table
 * @soc: data path soc handle
 * @vdev_id: vdev id
 * @val: Boolean value to enable/disable
 *
 * return: QDF_STATUS
 */
static inline
QDF_STATUS cdp_ppesds_enable_pri2tid(struct cdp_soc_t *soc,
				     uint8_t vdev_id, bool val)
{
	if (!soc || !soc->ops || !soc->ops->ppeds_ops) {
		QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_FATAL,
			  "%s invalid instance", __func__);
		return QDF_STATUS_E_INVAL;
	}

	if (soc->ops->ppeds_ops->ppeds_enable_pri2tid)
		return soc->ops->ppeds_ops->ppeds_enable_pri2tid(soc, vdev_id,
								 val);

	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* _CDP_TXRX_PPE_H_ */
