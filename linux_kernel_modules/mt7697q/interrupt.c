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
#include <linux/interrupt.h>
#include "interrupt.h"
#include "queue.h"
#include "io.h"
/*
static void mt7697_flush_deferred_work(struct mt7697_data *wl)
{
        struct sk_buff *skb;
*/
        /* Pass all received frames to the network stack */
/*        while ((skb = skb_dequeue(&wl->deferred_rx_queue)))
                ieee80211_rx_ni(wl->hw, skb);
*/
        /* Return sent skbs to the network stack */
/*        while ((skb = skb_dequeue(&wl->deferred_tx_queue)))
                ieee80211_tx_status_ni(wl->hw, skb);
}
*/
/*
static int mt7697_irq_locked(struct mt7697_data *wl)
{
        int ret = 0;
        int loopcount = MT7697_IRQ_MAX_LOOPS;
        bool done = false;
        unsigned int defer_count;
        unsigned long flags;

        if (unlikely(wl->state != MT7697_STATE_ON))
                goto out;

        while (!done && loopcount--) {
                clear_bit(MT7697_FLAG_IRQ_RUNNING, &wl->flags);

                ret = mt7697_rx(wl);
                if (ret < 0) {
			dev_err(wl->dev, "%s(): mt7697_rx() failed(%d)\n", __func__, ret);
                	goto out;
		}
*/
                /* Check if any tx blocks were freed */
/*                spin_lock_irqsave(&wl->lock, flags);
                if (!test_bit(MT7697_FLAG_FW_TX_BUSY, &wl->flags) &&
                    mt7697_tx_total_queue_count(wl) > 0) {
                	spin_unlock_irqrestore(&wl->lock, flags);*/
                        /*
                         * In order to avoid starvation of the TX path,
                         * call the work function directly.
                         */
/*                        ret = mt7697_tx_work_locked(wl);
                        if (ret < 0) {
				dev_err(wl->dev, "%s(): mt7697_tx_work_locked() failed(%d)\n", __func__, ret);
                        	goto out;
			}
                } else {
                	spin_unlock_irqrestore(&wl->lock, flags);
                }
*/
                /* check for tx results */
//                ret = mt7697_hw_tx_delayed_compl(wl);
/*                if (ret < 0) {
			dev_err(wl->dev, "%s(): mt7697_hw_tx_delayed_compl() failed(%d)\n", __func__, ret);
                	goto out;
		}
*/
                /* Make sure the deferred queues don't get too long */
/*                defer_count = skb_queue_len(&wl->deferred_tx_queue) +
                              skb_queue_len(&wl->deferred_rx_queue);
                if (defer_count > MT7697_DEFERRED_QUEUE_LIMIT)
                	mt7697_flush_deferred_work(wl);
	}

out:
        return ret;
}*/
/*
irqreturn_t mt7697_irq(int irq, void *cookie)
{
	struct mt7697_data *wl = cookie;
	int ret;
        unsigned long flags;

	spin_lock_irqsave(&wl->lock, flags);

        if (test_bit(MT7697_FLAG_SUSPENDED, &wl->flags)) {*/
                /* don't enqueue a work right now. mark it as pending */
/*                set_bit(MT7697_FLAG_PENDING_WORK, &wl->flags);
                dev_info(wl->dev, "should not enqueue work\n");
//                disable_irq_nosync(wl->irq);
//                pm_wakeup_event(wl->dev, 0);
                spin_unlock_irqrestore(&wl->lock, flags);
                return IRQ_HANDLED;
        }
        spin_unlock_irqrestore(&wl->lock, flags);
*/
        /* TX might be handled here, avoid redundant work */
/*        set_bit(MT7697_FLAG_TX_PENDING, &wl->flags);
        cancel_work_sync(&wl->tx_work);

        mutex_lock(&wl->mutex);

        ret = mt7697_irq_locked(wl);
        if (ret) {
		dev_warn(wl->dev, "%s(): mt7697_irq_locked() failed(%d)\n", __func__, ret);
//                mt7697_queue_recovery_work(wl);
	}

        spin_lock_irqsave(&wl->lock, flags);*/
        /* In case TX was not handled here, queue TX work */
/*        clear_bit(MT7697_FLAG_TX_PENDING, &wl->flags);
        if (!test_bit(MT7697_FLAG_FW_TX_BUSY, &wl->flags) &&
            mt7697_tx_total_queue_count(wl) > 0)
                ieee80211_queue_work(wl->hw, &wl->tx_work);

        spin_unlock_irqrestore(&wl->lock, flags);
        mutex_unlock(&wl->mutex);
        return IRQ_HANDLED;
}*/

void mt7697_irq_work(struct work_struct *irq_work)
{
	struct mt7697q_info *qinfo = container_of(irq_work, 
		struct mt7697q_info, irq_work);
	int ch;
	int ret;
	
	mutex_lock(&qinfo->mutex);

	ret = mt7697io_rd_s2m_mbx(qinfo);
    	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_rd_s2m_mbx() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	if (!qinfo->s2m_mbox) 
		goto cleanup;

    	ret = mt7697io_clr_s2m_mbx(qinfo);
    	if (ret < 0) {
		dev_err(qinfo->dev, 
			"%s(): mt7697io_clr_s2m_mbx() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	for (ch = 0; ch < MT7697_NUM_QUEUES; ch++) {
        	if (qinfo->s2m_mbox & (0x01 << ch)) {
			struct mt7697q_spec *qs = &qinfo->queues[ch];
			u32 in_use = mt7697q_flags_get_in_use(qs->data.flags);
			u32 dir = mt7697q_flags_get_dir(qs->data.flags);

            		if (in_use && 
			    (dir == MT7697_QUEUE_DIR_SLAVE_TO_MASTER)) {
				WARN_ON(!qs->rx_fcn);
				mutex_unlock(&qinfo->mutex);
				ret = qs->rx_fcn(qs->priv);
				if (ret < 0) {
					dev_err(qinfo->dev, 
						"%s(): rx_fcn() failed(%d)\n", 
						__func__, ret);
    				}

				mutex_lock(&qinfo->mutex);
            		}
            		else if (!in_use) {
				dev_warn(qinfo->dev, 
					"%s(): unused channel(%d)\n", 
					__func__, ch);
            		}
        	}
    	}

cleanup:
	ret = queue_delayed_work(qinfo->irq_workq, &qinfo->irq_work, 
		msecs_to_jiffies(100));
	if (ret < 0) {
		dev_err(qinfo->dev, 
			"%s(): queue_delayed_work() failed(%d)\n", 
			__func__, ret);
    	}

	mutex_unlock(&qinfo->mutex);
}

irqreturn_t mt7697_irq(int irq, void *cookie)
{
/*	struct mt7697q_info *qinfo = cookie;
	int err = queue_work(qinfo->irq_workq, &qinfo->irq_work);
	if (err < 0) {
		dev_err(qinfo->dev, "queue_work() failed(%d)\n", err);
    	}*/

	return IRQ_HANDLED;
}
