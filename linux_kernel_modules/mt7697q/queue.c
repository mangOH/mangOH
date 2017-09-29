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
#include <linux/spi/spi.h>
#include "bits.h"
#include "io.h"
#include "queue.h"
#include "spi.h"

static __inline ssize_t mt7697q_buf_diff(u32 size, u32 from, u32 to)
{
	if (from >= size) {
		pr_info("%s(): ERROR from(%u) >= size(%u)\n", 
			__func__, from, size);
    		WARN_ON(from >= size);
	}

	if (to >= size) {
		pr_info("%s(): ERROR to(%u) >= size(%u)\n", 
			__func__, to, size);
    		WARN_ON(to >= size);
	}

    	return (from <= to) ? (to - from):((size - from) + to);
}

static __inline size_t mt7697q_get_capacity(const struct mt7697q_spec *qs)
{
    	return BF_GET(qs->data.flags, MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET, 
		MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH) - 1;
}

static __inline size_t mt7697q_get_num_words(const struct mt7697q_spec *qs)
{
    	return mt7697q_buf_diff(BF_GET(qs->data.flags, 
			MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET, 
			MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH),
        	qs->data.rd_offset, qs->data.wr_offset);
}

static __inline size_t mt7697q_get_free_words(const struct mt7697q_spec *qs)
{
    	return mt7697q_get_capacity(qs) - mt7697q_get_num_words(qs);
}

static int mt7697q_wr_init(u8 tx_ch, u8 rx_ch, struct mt7697q_spec *qs)
{
	struct mt7697_queue_init_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_queue_init_req);
	req.cmd.grp = MT7697_CMD_GRP_QUEUE;
	req.cmd.type = MT7697_CMD_QUEUE_INIT;
	req.m2s_ch = tx_ch;
	req.s2m_ch = rx_ch;

 	dev_dbg(qs->qinfo->dev, "%s(): <-- QUEUE INIT channel(%u/%u)\n", 
		__func__, tx_ch, rx_ch);
	ret = mt7697q_write(qs, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697q_write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

static int mt7697q_pull_rd_ptr(struct mt7697q_spec *qs)
{
	const u32 read_addr = MT7697_IO_SLAVE_BUFFER_ADDRESS +
        	(qs->ch * sizeof(struct mt7697q_data)) +
        	offsetof(struct mt7697q_data, rd_offset);
	u32 rd_offset;
	int ret;

    	dev_dbg(qs->qinfo->dev, "%s(): queue(%u) addr(0x%08x)\n", 
		__func__, qs->ch, read_addr);
    	ret = mt7697io_rd(qs->qinfo, read_addr, &rd_offset, 
		LEN_TO_WORD(sizeof(rd_offset)));
    	if (ret < 0) {
		dev_err(qs->qinfo->dev, "%s(): mt7697io_rd() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
	}

	qs->data.rd_offset = rd_offset;
	dev_dbg(qs->qinfo->dev, "%s(): queue(%u) rd offset(%u)\n", 
		__func__, qs->ch, qs->data.rd_offset);

cleanup:
	return ret;
}

static int mt7697q_push_rd_ptr(struct mt7697q_spec *qs)
{
	const u32 write_addr = MT7697_IO_SLAVE_BUFFER_ADDRESS +
        	(qs->ch * sizeof(struct mt7697q_data)) +
        	offsetof(struct mt7697q_data, rd_offset);
	u32 rd_offset;
	int ret;

	mutex_lock(&qs->qinfo->mutex);    
	dev_dbg(qs->qinfo->dev, "%s(): rd ptr/offset(0x%08x/%u)\n", 
		__func__, write_addr, qs->data.rd_offset);

	rd_offset = qs->data.rd_offset;
    	ret = mt7697io_wr(qs->qinfo, write_addr, &rd_offset, 
		LEN_TO_WORD(sizeof(rd_offset)));
	if (ret < 0) {
		dev_err(qs->qinfo->dev, "%s(): mt7697io_wr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}    

    	ret = mt7697io_wr_m2s_mbx(qs->qinfo, 1 << qs->ch);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697io_wr_m2s_mbx() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

    	ret = mt7697io_trigger_intr(qs->qinfo);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697io_trigger_intr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

cleanup:
	mutex_unlock(&qs->qinfo->mutex);
	return ret;
}

static int mt7697q_pull_wr_ptr(struct mt7697q_spec *qs)
{
	const u32 read_addr = MT7697_IO_SLAVE_BUFFER_ADDRESS +
        	(qs->ch * sizeof(struct mt7697q_data)) +
        	offsetof(struct mt7697q_data, wr_offset);
	u32 wr_offset;
	int ret;

	mutex_lock(&qs->qinfo->mutex);

	dev_dbg(qs->qinfo->dev, "%s(): queue(%u) addr(0x%08x)\n", 
		__func__, qs->ch, read_addr);
    	ret = mt7697io_rd(qs->qinfo, read_addr, &wr_offset, 
		LEN_TO_WORD(sizeof(wr_offset)));
    	if (ret < 0) {
		dev_err(qs->qinfo->dev, "%s(): mt7697io_rd() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
	}

	qs->data.wr_offset = wr_offset;
	dev_dbg(qs->qinfo->dev, "%s(): queue(%u) wr offset(%u)\n", 
		__func__, qs->ch, qs->data.wr_offset);

cleanup:
	mutex_unlock(&qs->qinfo->mutex);
	return ret;
}

static int mt7697q_push_wr_ptr(struct mt7697q_spec *qs)
{
	const uint32_t write_addr = MT7697_IO_SLAVE_BUFFER_ADDRESS +
        	(qs->ch * sizeof(struct mt7697q_data)) +
        	offsetof(struct mt7697q_data, wr_offset);
	u32 wr_offset;
	int ret;
    
	dev_dbg(qs->qinfo->dev, "%s(): wr ptr/offset(0x%08x/%u)\n", 
		__func__, write_addr, qs->data.wr_offset);

	wr_offset = qs->data.wr_offset;
    	ret = mt7697io_wr(qs->qinfo, write_addr, &wr_offset,
        	LEN_TO_WORD(sizeof(wr_offset)));
    	if (ret < 0) {
		dev_err(qs->qinfo->dev, "%s(): mt7697io_wr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}    

    	ret = mt7697io_wr_m2s_mbx(qs->qinfo, 1 << qs->ch);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697io_wr_m2s_mbx() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

    	ret = mt7697io_trigger_intr(qs->qinfo);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697io_trigger_intr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

cleanup:
	return ret;
}

static int mt7697q_read_state(u8 ch, struct mt7697q_spec *qs)
{
	int ret;

	mutex_lock(&qs->qinfo->mutex);

	ret = mt7697io_rd(qs->qinfo, 
		MT7697_IO_SLAVE_BUFFER_ADDRESS + ch * sizeof(struct mt7697q_data), 
		(u32*)&qs->data, 
		LEN_TO_WORD(sizeof(struct mt7697q_data)));
	if (ret < 0) {
		dev_err(qs->qinfo->dev, "%s(): mt7697io_rd() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	dev_dbg(qs->qinfo->dev, "%s(): flags(0x%08x) base addr(0x%08x)\n", 
		__func__, qs->data.flags, qs->data.base_addr);
	dev_dbg(qs->qinfo->dev, "%s(): rd/wr offset(0x%08x/0x%08x)\n", 
		__func__, qs->data.rd_offset, qs->data.wr_offset);
	if (!qs->data.base_addr) {
		dev_err(qs->qinfo->dev, "%s(): invalid base address(0x%08x)\n", 
			__func__, qs->data.base_addr);
		ret = -EINVAL;
       		goto cleanup;
    	}
	else if (!qs->data.flags) {
		dev_err(qs->qinfo->dev, "%s(): invalid flags(0x%08x)\n", 
			__func__, qs->data.flags);
		ret = -EINVAL;
       		goto cleanup;
    	}
	else if ((qs->data.rd_offset > mt7697q_get_capacity(qs)) || 
		 (qs->data.wr_offset > mt7697q_get_capacity(qs))) {
		dev_err(qs->qinfo->dev, 
			"%s(): invalid rd/wr offset(0x%08x/0x%08x)\n", 
			__func__, qs->data.rd_offset, qs->data.wr_offset);
		ret = -EINVAL;
       		goto cleanup;
    	}

cleanup:
	mutex_unlock(&qs->qinfo->mutex);
	return ret;
}

static int mt7697q_proc_queue_rsp(struct mt7697q_spec *qs)
{
	int ret = 0;

	switch(qs->qinfo->rsp.cmd.type) {
	case MT7697_CMD_QUEUE_INIT_RSP:
		dev_dbg(qs->qinfo->dev, "%s(): --> QUEUE INIT RSP\n", 
			__func__);
		break;

	case MT7697_CMD_QUEUE_RESET_RSP:
		dev_dbg(qs->qinfo->dev, "%s(): --> QUEUE RESET RSP\n", 
			__func__);
		break;

	default:
		dev_err(qs->qinfo->dev, "%s(): unsupported cmd(%d)\n", 
			__func__, qs->qinfo->rsp.cmd.type);
		ret = -EINVAL;
		goto cleanup;
	}

cleanup:
	return ret;
}

int mt7697q_get_s2m_mbx(struct mt7697q_info *qinfo, u8* s2m_mbox)
{
	int ret;

	mutex_lock(&qinfo->mutex);

    	ret = mt7697io_rd_s2m_mbx(qinfo);
    	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_rd_s2m_mbx() failed(%d)\n", 
			__func__, ret);
		
       		goto cleanup;
    	}

    	*s2m_mbox = qinfo->s2m_mbox;

    	ret = mt7697io_clr_s2m_mbx(qinfo);
    	if (ret < 0) {
		dev_err(qinfo->dev, 
			"%s(): mt7697io_clr_s2m_mbx() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
    	}

cleanup:
	mutex_unlock(&qinfo->mutex);
    	return ret;
}

int mt7697q_proc_data(struct mt7697q_spec *qsS2M)
{
	size_t avail;
	u32 req = 0;
	int ret;

	ret = mt7697q_pull_wr_ptr(qsS2M);
	if (ret < 0) {
		dev_err(qsS2M->qinfo->dev, 
			"%s(): mt7697q_pull_wr_ptr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	avail = mt7697q_get_num_words(qsS2M);
	req = (qsS2M->qinfo->rsp.cmd.len > 0) ? 
		LEN_TO_WORD(qsS2M->qinfo->rsp.cmd.len - sizeof(struct mt7697q_rsp_hdr)) : 
		LEN_TO_WORD(sizeof(struct mt7697q_rsp_hdr));
	dev_dbg(qsS2M->qinfo->dev, "%s(): avail(%u) len(%u) req(%u)\n", 
		__func__, avail, qsS2M->qinfo->rsp.cmd.len, req);

	while (avail >= req) {
		if (!qsS2M->qinfo->rsp.cmd.len) {
			ret = mt7697q_read(qsS2M, (u32*)&qsS2M->qinfo->rsp, req);
			if (ret != req) {
				dev_err(qsS2M->qinfo->dev, 
					"%s(): mt7697q_read() failed(%d != %d)\n", 
					__func__, ret, req);
       				goto cleanup;
    			}

			avail -= LEN_TO_WORD(sizeof(struct mt7697q_rsp_hdr));
			req = LEN_TO_WORD(qsS2M->qinfo->rsp.cmd.len - sizeof(struct mt7697q_rsp_hdr));
			dev_dbg(qsS2M->qinfo->dev, "%s(): avail(%u) len(%u) req(%u)\n", 
				__func__, avail, qsS2M->qinfo->rsp.cmd.len, req);
		}

		if (qsS2M->qinfo->rsp.result < 0) {
			dev_warn(qsS2M->qinfo->dev, 
				"%s(): cmd(%u) result(%d)\n", 
				__func__, qsS2M->qinfo->rsp.cmd.type, 
				qsS2M->qinfo->rsp.result);
		}

		if (avail < req) {
			ret = mt7697q_pull_wr_ptr(qsS2M);
			if (ret < 0) {
				dev_err(qsS2M->qinfo->dev, 
					"%s(): mt7697q_pull_wr_ptr() failed(%d)\n", 
					__func__, ret);
       				goto cleanup;
    			}

			avail = mt7697q_get_num_words(qsS2M);
		}

		dev_dbg(qsS2M->qinfo->dev, "%s(): avail(%u) len(%u) req(%u)\n", 
			__func__, avail, qsS2M->qinfo->rsp.cmd.len, req);
		if (avail < req) {
			dev_dbg(qsS2M->qinfo->dev, 
				"%s(): queue need more data\n", __func__);
			goto cleanup;
		}

		if (qsS2M->qinfo->rsp.cmd.grp == MT7697_CMD_GRP_QUEUE) {
			ret = mt7697q_proc_queue_rsp(qsS2M);
			if (ret < 0) {
				dev_err(qsS2M->qinfo->dev, 
					"%s(): mt7697q_proc_queue_rsp() failed(%d)\n", 
					__func__, ret);
       				goto cleanup;
    			}
		}
		else {
			WARN_ON(!qsS2M->rx_fcn);			
			ret = qsS2M->rx_fcn((const struct mt7697q_rsp_hdr*)&qsS2M->qinfo->rsp, qsS2M->priv);
			if (ret < 0) {
				dev_err(qsS2M->qinfo->dev, 
					"%s(): rx_fcn() failed(%d)\n", 
					__func__, ret);
    			}
		}

		avail -= req;
		qsS2M->qinfo->rsp.cmd.len = 0;
		req = LEN_TO_WORD(sizeof(struct mt7697q_rsp_hdr));
		dev_dbg(qsS2M->qinfo->dev, "%s(): avail(%u) len(%u) req(%u)\n", 
			__func__, avail, qsS2M->qinfo->rsp.cmd.len, req);

		if (avail < req) {
			ret = mt7697q_pull_wr_ptr(qsS2M);
			if (ret < 0) {
				dev_err(qsS2M->qinfo->dev, 
					"%s(): mt7697q_pull_wr_ptr() failed(%d)\n", 
					__func__, ret);
       				goto cleanup;
    			}

			avail = mt7697q_get_num_words(qsS2M);
			dev_dbg(qsS2M->qinfo->dev, "%s(): avail(%u) len(%u) req(%u)\n", 
				__func__, avail, qsS2M->qinfo->rsp.cmd.len, req);
		}

		ret =  mt7697q_push_rd_ptr(qsS2M);
		if (ret < 0) {
			dev_err(qsS2M->qinfo->dev, 
				"%s(): mt7697q_push_rd_ptr() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
		}
	}

cleanup:
	return ret;
}

int mt7697q_wr_unused(void *tx_hndl, void* rx_hndl)
{
	struct mt7697q_spec *qsM2S = (struct mt7697q_spec*)tx_hndl;
	struct mt7697q_spec *qsS2M = (struct mt7697q_spec*)rx_hndl;
	struct mt7697_queue_reset_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_queue_reset_req);
	req.cmd.grp = MT7697_CMD_GRP_QUEUE;
	req.cmd.type = MT7697_CMD_QUEUE_UNUSED;
	req.m2s_ch = qsM2S->ch;
	req.s2m_ch = qsS2M->ch;

	qsM2S->data.flags &= ~BF_GET(qsM2S->data.flags, MT7697_QUEUE_FLAGS_IN_USE_OFFSET, 
		MT7697_QUEUE_FLAGS_IN_USE_WIDTH);
	qsS2M->data.flags &= ~BF_GET(qsS2M->data.flags, MT7697_QUEUE_FLAGS_IN_USE_OFFSET, 
		MT7697_QUEUE_FLAGS_IN_USE_WIDTH);

 	dev_dbg(qsM2S->qinfo->dev, "%s(): <-- QUEUE UNUSED(%u/%u)\n", 
		__func__, req.m2s_ch, req.s2m_ch);
	ret = mt7697q_write(qsM2S, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(qsM2S->qinfo->dev, 
			"%s(): mt7697q_write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

EXPORT_SYMBOL(mt7697q_wr_unused);

int mt7697q_wr_reset(void *tx_hndl, void* rx_hndl)
{
	struct mt7697q_spec *qsM2S = (struct mt7697q_spec*)tx_hndl;
	struct mt7697q_spec *qsS2M = (struct mt7697q_spec*)rx_hndl;
	struct mt7697_queue_reset_req req;
	int ret;

	req.cmd.len = sizeof(struct mt7697_queue_reset_req);
	req.cmd.grp = MT7697_CMD_GRP_QUEUE;
	req.cmd.type = MT7697_CMD_QUEUE_RESET;
	req.m2s_ch = qsM2S->ch;
	req.s2m_ch = qsS2M->ch;

 	dev_dbg(qsM2S->qinfo->dev, "%s(): <-- QUEUE RESET(%u/%u)\n", 
		__func__, req.m2s_ch, req.s2m_ch);
	ret = mt7697q_write(qsM2S, (const u32*)&req, 
		LEN_TO_WORD(req.cmd.len));
	if (ret != LEN_TO_WORD(req.cmd.len)) {
		dev_err(qsM2S->qinfo->dev, 
			"%s(): mt7697q_write() failed(%d != %d)\n", 
			__func__, ret, LEN_TO_WORD(req.cmd.len));
		ret = (ret < 0) ? ret:-EIO;
		goto cleanup;
	}

	ret = 0;

cleanup:
	return ret;
}

EXPORT_SYMBOL(mt7697q_wr_reset);

int mt7697q_init(u8 tx_ch, u8 rx_ch, void *priv, rx_hndlr rx_fcn, 
	         void** tx_hndl, void** rx_hndl)
{
	char str[32];
	struct spi_master *master = NULL;
	struct device *dev;
	struct spi_device *spi;
	struct mt7697q_info *qinfo;
	struct mt7697q_spec *qsTx, *qsRx;
	int bus_num = MT7697_SPI_BUS_NUM;
	int ret;

	pr_info(DRVNAME" %s(): initialize queue(%u/%u)\n", 
		__func__, tx_ch, rx_ch);

	while (!master && (bus_num >= 0)) {
		pr_info(DRVNAME" %s(): get SPI master bus(%u)\n", 
			__func__, bus_num);
		master = spi_busnum_to_master(bus_num);
		if (!master)
			bus_num--;
	}

	if (!master) {
		pr_err(DRVNAME" %s(): spi_busnum_to_master() failed\n",
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), MT7697_SPI_CS);
	dev_dbg(&master->dev, "%s(): find SPI device('%s')\n", __func__, str);
	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	if (!dev) {
		dev_err(&master->dev, 
			"%s(): '%s' bus_find_device_by_name() failed\n", 
			__func__, str);
		ret = -EINVAL;
		goto cleanup;
	}

	spi = container_of(dev, struct spi_device, dev);
	if (!spi) {
		dev_err(&master->dev, "%s(): get SPI device failed\n", 
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

	qinfo = spi_get_drvdata(spi);
	if (!qinfo) {
		dev_dbg(&master->dev, "%s(): spi_get_drvdata() failed\n", 
			__func__);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_dbg(qinfo->dev, "%s(): init queue spec(%u/%u)\n", 
		__func__, tx_ch, rx_ch);

	if ((tx_ch >= MT7697_NUM_QUEUES) ||
	    (rx_ch >= MT7697_NUM_QUEUES)) {
		dev_err(qinfo->dev, "%s():  invalid queue(%u/%u)\n", 
			__func__, tx_ch, rx_ch);
		ret = -EINVAL;
		goto cleanup;
	}
    	
	qsTx = &qinfo->queues[tx_ch];
	qsTx->qinfo = qinfo;
	qsTx->ch = tx_ch;
	qsTx->priv = priv;
	*tx_hndl = qsTx;

   	qsRx = &qinfo->queues[rx_ch];
	qsRx->qinfo = qinfo;
	qsRx->ch = rx_ch;
	qsRx->rx_fcn = rx_fcn;
	qsRx->priv = priv;
	
	*rx_hndl = qsRx;

	ret = mt7697q_read_state(tx_ch, qsTx);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697q_read_state() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}


	ret = mt7697q_read_state(rx_ch, qsRx);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697q_read_state() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	qsTx->data.flags |= BF_DEFINE(1, MT7697_QUEUE_FLAGS_IN_USE_OFFSET, 
		MT7697_QUEUE_FLAGS_IN_USE_WIDTH);
	qsRx->data.flags |= BF_DEFINE(1, MT7697_QUEUE_FLAGS_IN_USE_OFFSET, 
		MT7697_QUEUE_FLAGS_IN_USE_WIDTH);

	ret = mt7697q_wr_init(tx_ch, rx_ch, qsTx);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697q_wr_init() failed(%d)\n", 
			__func__, ret);
		goto cleanup;
	}

cleanup:
	if (ret < 0) {
		memset(&qsTx, 0, sizeof(struct mt7697q_spec));
		memset(&qsRx, 0, sizeof(struct mt7697q_spec));
	}

    	return ret;
}

EXPORT_SYMBOL(mt7697q_init);

size_t mt7697q_read(void *hndl, u32 *buf, size_t num)
{
    	struct mt7697q_spec *qs = (struct mt7697q_spec*)hndl;
	size_t rd_words = 0;
        u16 write_offset;
        u16 read_offset;
	u32 buff_words;
	int ret;

	mutex_lock(&qs->qinfo->mutex);

	buff_words = BF_GET(qs->data.flags, 
		MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET, 
		MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH);
    
	write_offset = qs->data.wr_offset;
        read_offset = qs->data.rd_offset;
	dev_dbg(qs->qinfo->dev, "%s(): rd(%u) queue(%d) rd/wr offset(%d/%d)", 
		__func__, num, qs->ch, read_offset, write_offset);
    	if (read_offset > write_offset) {
        	const size_t words_to_end =  buff_words - read_offset;
        	const u32 rd_addr = qs->data.base_addr + (read_offset * sizeof(u32));
        	const size_t rd_num = (num <= words_to_end) ? 
			num : words_to_end;

		dev_dbg(qs->qinfo->dev, 
			"%s(): rd(%u) queue(%u) rd offset(%u) addr(0x%08x)\n", 
			__func__, rd_num, qs->ch, read_offset, rd_addr);
        	ret = mt7697io_rd(qs->qinfo, rd_addr, &buf[rd_words], rd_num);
		if (ret < 0) {
			dev_err(qs->qinfo->dev, 
				"%s(): mt7697io_rd() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

        	rd_words += rd_num;
        	read_offset += rd_num;

        	if (read_offset == buff_words) {
            		/* 
			 * If we have read to the end, then 
		         * set the read pointer to the beginning
			 */
            		read_offset = 0;
        	}
    	}

    	if (rd_words < num) {
        	/* NOTE: rd_offset assumed to be <= wr_offset at this point */
        	const size_t words_avail = write_offset - read_offset;
        	const u32 rd_addr = qs->data.base_addr + (read_offset * sizeof(u32));
        	const size_t words_req = num - rd_words;
        	const size_t rd_num = (words_req <= words_avail) ? words_req : words_avail;

		dev_dbg(qs->qinfo->dev, 
			"%s(): rd(%u) queue(%u) rd offset(%u) addr(0x%08x)\n", 
			__func__, rd_num, qs->ch, read_offset, rd_addr);
        	ret = mt7697io_rd(qs->qinfo, rd_addr, &buf[rd_words], rd_num);
        	if (ret < 0) {
			dev_err(qs->qinfo->dev, 
				"%s(): mt7697io_rd() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

        	rd_words += rd_num;
        	read_offset += rd_num;
    	}

	if (read_offset >= buff_words) {
		dev_err(qs->qinfo->dev, "%s(): rd offset(%u) >= size(%u)\n",
			__func__, read_offset, buff_words);
		WARN_ON(read_offset >= buff_words);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_dbg(qs->qinfo->dev, "%s(): queue(%u) rd offset(%u) read(%u)\n", 
		__func__, qs->ch, read_offset, rd_words);
	qs->data.rd_offset = read_offset;

	ret = rd_words;

cleanup:
	mutex_unlock(&qs->qinfo->mutex);
	return ret;
}

EXPORT_SYMBOL(mt7697q_read);

size_t mt7697q_write(void *hndl, const u32 *buff, size_t num)
{
    	struct mt7697q_spec *qs = (struct mt7697q_spec*)hndl;
	size_t avail;
	size_t words_written = 0;
	u16 read_offset;
	u16 write_offset;        
	uint32_t buff_words;	
	int ret;

	mutex_lock(&qs->qinfo->mutex);

	avail = mt7697q_get_free_words(qs);
	dev_dbg(qs->qinfo->dev, "%s(): free words(%u)\n", __func__, avail);
	if (avail < num) {
		ret = mt7697q_pull_rd_ptr(qs);
		if (ret < 0) {
			dev_err(qs->qinfo->dev, 
				"%s(): mt7697q_pull_rd_ptr() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

		if (avail < num) {
			dev_warn(qs->qinfo->dev, "%s(): queue avail(%u < %u)\n", 
				__func__, avail, num);
			ret = -EAGAIN;
			goto cleanup;
		}
	}

	buff_words = BF_GET(qs->data.flags, MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET, 
		MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH);

	read_offset = qs->data.rd_offset;
	write_offset = qs->data.wr_offset; 
	dev_dbg(qs->qinfo->dev, "%s(): wr(%u) queue(%d) rd/wr offset(%d/%d)", 
		__func__, num, qs->ch, read_offset, write_offset);
    	if (write_offset >= read_offset) {
        	const size_t words_to_end = buff_words - write_offset;
        	const size_t words_avail = (read_offset == 0) ? 
			(words_to_end - 1) : words_to_end;
        	const size_t words_to_write =
            		(num <= words_avail) ? num : words_avail;
        	const u32 write_addr = qs->data.base_addr + 
			(write_offset * sizeof(u32));

		dev_dbg(qs->qinfo->dev, 
			"%s(): wr(%u) queue(%u) wr offset(%u) addr(0x%08x)\n", 
			__func__, words_to_write, qs->ch, write_offset, 
			write_addr);
        	ret = mt7697io_wr(qs->qinfo, write_addr, &buff[words_written], 
			words_to_write);
        	if (ret < 0) {
			dev_err(qs->qinfo->dev, 
				"%s(): mt7697io_wr() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

        	words_written += words_to_write;
		num -= words_to_write;
        	write_offset += words_to_write;
        	if (write_offset == buff_words) {
            		write_offset = 0;
        	}
    	}

    	if ((write_offset < read_offset) && (num > 0)) {
        	const size_t words_to_read = read_offset - write_offset - 1;
        	const size_t words_to_write = (num <= words_to_read) ? 
			num : words_to_read;
        	const uint32_t write_addr = qs->data.base_addr + 
			(write_offset * sizeof(u32));
        	
		dev_dbg(qs->qinfo->dev, 
			"%s(): wr(%u) queue(%u) wr offset(%u) addr(0x%08x)\n", 
			__func__, words_to_write, qs->ch, write_offset, 
			write_addr);
		ret = mt7697io_wr(qs->qinfo, write_addr, &buff[words_written], 
			words_to_write);
        	if (ret < 0) {
			dev_err(qs->qinfo->dev, 
				"%s(): mt7697io_wr() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

        	words_written += words_to_write;
		num -= words_to_write;
        	write_offset += words_to_write;
    	}

	if (write_offset >= buff_words) {
		dev_err(qs->qinfo->dev, "%s(): wr offset(%u) >= size(%u)\n",
			__func__, write_offset, buff_words);
		WARN_ON(write_offset >= buff_words);
		ret = -EINVAL;
		goto cleanup;
	}

	dev_dbg(qs->qinfo->dev, "%s(): queue(%u) wr offset(%u) write(%u)\n", 
		__func__, qs->ch, write_offset, words_written);
	qs->data.wr_offset = write_offset;

	ret = mt7697q_push_wr_ptr(qs);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697q_push_wr_ptr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	ret = words_written;

cleanup:
	mutex_unlock(&qs->qinfo->mutex);
    	return ret;
}

EXPORT_SYMBOL(mt7697q_write);

__inline u32 mt7697q_flags_get_in_use(u32 flags)
{
	return BF_GET(flags, MT7697_QUEUE_FLAGS_IN_USE_OFFSET, 
		MT7697_QUEUE_FLAGS_IN_USE_WIDTH);
}

__inline u32 mt7697q_flags_get_dir(u32 flags)
{
	return BF_GET(flags,MT7697_QUEUE_FLAGS_DIR_OFFSET,
		MT7697_QUEUE_FLAGS_DIR_WIDTH);
}

