/*
 * Copyright (c) 2017 Sierra Wireless Corporation
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include "interrupt.h"
#include "queue.h"
#include "io.h"
#include "spi.h"

static int mt7697q_irq_run(struct mt7697q_info *qinfo)
{
	int ret;
	u8 ch;
	u8 s2m_mbox;

	ret = mt7697q_get_s2m_mbx(qinfo, &s2m_mbox);
	if (ret < 0) {
		dev_err(qinfo->dev,
			"%s(): mt7697q_get_s2m_mbx() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

	if (s2m_mbox) {
		if (!queue_delayed_work(qinfo->irq_workq,
			&qinfo->irq_delayed_work, usecs_to_jiffies(10))) {
			dev_err(qinfo->dev,
				"%s(): queue_delayed_work() failed\n",
				__func__);
			ret = -EINVAL;
		}
	} else {
		enable_irq(qinfo->irq);
	}

	for (ch = 0; ch < MT7697_NUM_QUEUES; ch++) {
		struct mt7697q_spec *qs = &qinfo->queues[ch];
		u32 in_use = mt7697q_flags_get_in_use(qs->data.flags);
		u32 dir = mt7697q_flags_get_dir(qs->data.flags);

		if (in_use &&
		    (s2m_mbox & (0x01 << ch))) {
			if (dir == MT7697_QUEUE_DIR_S2M) {
				ret = mt7697q_proc_data(qs);
				if (ret < 0) {
					dev_err(qinfo->dev,
						"%s(): mt7697q_proc_data() failed(%d)\n",
						__func__, ret);
					goto cleanup;
				}
			} else if (mt7697q_blocked_writer(qs)) {
				WARN_ON(!qs->notify_tx_fcn);
				ret = qs->notify_tx_fcn(qs->priv,
							mt7697q_get_free_words(qs));
				if (ret < 0) {
					dev_err(qs->qinfo->dev,
						"%s(): notify_tx_fcn() failed(%d)\n",
						__func__, ret);
				}
			}
		}
	}

cleanup:
	return ret;
}

void mt7697q_irq_delayed_work(struct work_struct *irq_delayed_work)
{
	struct mt7697q_info *qinfo = container_of(irq_delayed_work,
		struct mt7697q_info, irq_delayed_work.work);
	int ret;

	dev_dbg(qinfo->dev, "%s(): process work\n", __func__);
	ret = mt7697q_irq_run(qinfo);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697q_irq_run() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return;
}

void mt7697q_irq_work(struct work_struct *irq_work)
{
	struct mt7697q_info *qinfo = container_of(irq_work,
		struct mt7697q_info, irq_work);
	int ret;

	dev_dbg(qinfo->dev, "%s(): process work\n", __func__);
	ret = mt7697q_irq_run(qinfo);
	if (ret < 0) {
		dev_err(qinfo->dev,
			"%s(): mt7697q_irq_run() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return;
}

irqreturn_t mt7697q_isr(int irq, void *arg)
{
	struct mt7697q_info *qinfo = (struct mt7697q_info*)arg;

	disable_irq_nosync(qinfo->irq);
	if (!queue_work(qinfo->irq_workq, &qinfo->irq_work)) {
		dev_err(qinfo->dev, "%s(): queue_work() failed\n", __func__);
	}

	return IRQ_HANDLED;
}
