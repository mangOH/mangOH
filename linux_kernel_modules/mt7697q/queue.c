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
    WARN_ON(from >= size || to >= size);
    return (from <= to) ? (to - from):((size - from) + to);
}

static int mt7697q_push_wr_ptr(struct mt7697q_spec *qs)
{
	const uint32_t write_addr = MT7697_IO_SLAVE_BUFFER_ADDRESS +
        	(qs->ch * sizeof(struct mt7697q_data)) +
        	offsetof(struct mt7697q_data, wr_offset);
	int ret;
    
	dev_dbg(qs->qinfo->dev, "%s(): ptr/offset(0x%08x/%u)\n", 
		__func__, write_addr, qs->data.wr_offset);

    	ret = mt7697io_wr(qs->qinfo, write_addr, &qs->data.wr_offset,
        	MT7697_QUEUE_LEN_TO_WORD(sizeof(qs->data.wr_offset)));
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

static int mt7697q_push_rd_ptr(struct mt7697q_spec *qs)
{
	const u32 write_addr = MT7697_IO_SLAVE_BUFFER_ADDRESS +
        	(qs->ch * sizeof(struct mt7697q_data)) +
        	offsetof(struct mt7697q_data, rd_offset);
	int ret;
    
	dev_dbg(qs->qinfo->dev, "%s(): ptr/offset(0x%08x/%u)\n", 
		__func__, write_addr, qs->data.rd_offset);

    	ret = mt7697io_wr(qs->qinfo, write_addr, &qs->data.rd_offset, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(qs->data.rd_offset)));
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

void* mt7697q_init(u8 ch, void *priv, rx_hndlr rx_fcn)
{
	char str[32];
	struct spi_master *master;
	struct device *dev;
	struct spi_device *spi;
	struct mt7697q_info *qinfo;
	struct mt7697q_spec *qs;
	int ret;

	master = spi_busnum_to_master(MT7697_SPI_BUS_NUM);
	if (!master) {
		pr_err(DRVNAME" spi_busnum_to_master(%d) failed\n",
			MT7697_SPI_BUS_NUM);
		goto cleanup;
	}

	snprintf(str, sizeof(str), "%s.%u", dev_name(&master->dev), MT7697_SPI_CS);
	dev_dbg(&master->dev, "%s(): find SPI device('%s')\n", __func__, str);
	dev = bus_find_device_by_name(&spi_bus_type, NULL, str);
	if (!dev) {
		dev_err(&master->dev, 
			"%s(): '%s' bus_find_device_by_name() failed\n", 
			__func__, str);
		goto cleanup;
	}

	spi = container_of(dev, struct spi_device, dev);
	if (!spi) {
		dev_err(&master->dev, "%s(): get SPI device failed\n", 
			__func__);
		goto cleanup;
	}

	qinfo = spi_get_drvdata(spi);
	if (!qinfo) {
		dev_dbg(&master->dev, "%s(): spi_get_drvdata() failed\n", 
			__func__);
		goto cleanup;
	}

	dev_dbg(&master->dev, "%s(): init queue(%u)\n", __func__, ch);

	if (ch >= MT7697_NUM_QUEUES) {
		dev_err(&master->dev, "%s():  invalid queue(%u)\n", 
			__func__, ch);
		goto cleanup;
	}
    	
	qs = &qinfo->queues[ch];
	qs->qinfo = qinfo;
	qs->ch = ch;
	qs->rx_fcn = rx_fcn;
	qs->priv = priv;

	mutex_lock(&qinfo->mutex);

	ret = mt7697io_rd(qinfo, 
		MT7697_IO_SLAVE_BUFFER_ADDRESS + ch * sizeof(struct mt7697q_data), 
		(u32*)&qs->data, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(struct mt7697q_data)));
	if (ret < 0) {
		dev_err(&master->dev, "%s(): mt7697io_rd() failed(%d)\n", 
			__func__, ret);
       		goto failed_rd;
    	}

	dev_dbg(&master->dev, "%s(): flags(0x%08x)\n", 
		__func__, qs->data.flags);
	dev_dbg(&master->dev, "%s(): base_addr(0x%08x)\n", 
		__func__, qs->data.base_addr);
	dev_dbg(&master->dev, "%s(): rd_offset(0x%08x)\n", 
		__func__, qs->data.rd_offset);
	dev_dbg(&master->dev, "%s(): wr_offset(0x%08x)\n", 
		__func__, qs->data.wr_offset);
	if (!qs->data.base_addr) {
		dev_err(&master->dev, "%s(): invalid base address(0x%08x)\n", 
			__func__, qs->data.base_addr);
       		goto failed_rd;
    	}
	else if (!qs->data.flags) {
		dev_err(&master->dev, "%s(): invalid flags(0x%08x)\n", 
			__func__, qs->data.flags);
       		goto failed_rd;
    	}
	else if (qs->data.rd_offset || qs->data.wr_offset) {
		dev_err(&master->dev, 
			"%s(): invalid rd/wr offset(0x%08x/0x%08x)\n", 
			__func__, qs->data.rd_offset, qs->data.wr_offset);
       		goto failed_rd;
    	}

	mutex_unlock(&qinfo->mutex);
	return qs;

failed_rd:
	mutex_unlock(&qinfo->mutex);
cleanup:
	memset(&qs, 0, sizeof(struct mt7697q_spec));
    	return NULL;
}

EXPORT_SYMBOL(mt7697q_init);

size_t mt7697q_read(void *hndl, u32 *buf, size_t num)
{
    	struct mt7697q_spec *qs = (struct mt7697q_spec*)hndl;
	size_t rd_words = 0;
	u32 buff_words;
	int ret;

	buff_words = BF_GET(qs->data.flags, 
		MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET, 
		MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH);
    
	dev_dbg(qs->qinfo->dev, "%s(): rd(%u) queue(%d) rd/wr offset(%d/%d)", 
		__func__, num, qs->ch, qs->data.rd_offset, qs->data.wr_offset);
    	if (qs->data.rd_offset > qs->data.wr_offset) {
        	const size_t words_to_end =  buff_words - 
			qs->data.rd_offset;
        	const u32 rd_addr = qs->data.base_addr + 
			(qs->data.rd_offset * sizeof(u32));
        	const size_t rd_num = (num <= words_to_end) ? 
			num : words_to_end;

		dev_dbg(qs->qinfo->dev, 
			"%s(): rd(%u) queue(%u) offset(%u) addr(0x%08x)\n", 
			__func__, rd_num, qs->ch, qs->data.rd_offset, rd_addr);
        	ret = mt7697io_rd(qs->qinfo, rd_addr, &buf[rd_words], rd_num);
		if (ret < 0) {
			dev_err(qs->qinfo->dev, 
				"%s(): mt7697io_rd() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

        	rd_words += rd_num;
        	qs->data.rd_offset += rd_num;

        	if (qs->data.rd_offset == buff_words) {
            		/* 
			 * If we have read to the end, then 
		         * set the read pointer to the beginning
			 */
            		qs->data.rd_offset = 0;
        	}
    	}

    	if (rd_words < num) {
        	/* NOTE: rd_offset assumed to be <= wr_offset at this point */
        	const size_t words_avail = qs->data.wr_offset - 
			qs->data.rd_offset;
        	const u32 rd_addr = qs->data.base_addr + 
			(qs->data.rd_offset * sizeof(u32));
        	const size_t words_req = num - rd_words;
        	const size_t rd_num = (words_req <= words_avail) ? 
			words_req : words_avail;

		dev_dbg(qs->qinfo->dev, 
			"%s(): rd(%u) queue(%u) offset(%u) addr(0x%08x)\n", 
			__func__, rd_num, qs->ch, qs->data.rd_offset, rd_addr);
        	ret = mt7697io_rd(qs->qinfo, rd_addr, &buf[rd_words], rd_num);
        	if (ret < 0) {
			dev_err(qs->qinfo->dev, 
				"%s(): mt7697io_rd() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

        	rd_words += rd_num;
        	qs->data.rd_offset += rd_num;
    	}

	ret =  mt7697q_push_rd_ptr(qs);
    	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697q_push_rd_ptr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	dev_dbg(qs->qinfo->dev, "%s(): queue(%u) offset(%u) read(%u)\n", 
		__func__, qs->ch, qs->data.rd_offset, rd_words);
	ret = rd_words;

cleanup:
    	return ret;
}

EXPORT_SYMBOL(mt7697q_read);

size_t mt7697q_write(void *hndl, const u32 *buff, size_t num)
{
    	struct mt7697q_spec *qs = (struct mt7697q_spec*)hndl;
	size_t avail;
	size_t words_written = 0;
	uint32_t wr_words;	
	int ret;

	mutex_lock(&qs->qinfo->mutex);

	ret = mt7697q_pull_rd_ptr(qs);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697q_pull_rd_ptr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	avail = mt7697q_get_free_words(qs);
	dev_dbg(qs->qinfo->dev, "%s(): free words(%u)\n", __func__, avail);
	if (avail < num)
	{
		dev_warn(qs->qinfo->dev, "%s(): queue avail(%u < %u)\n", 
			__func__, avail, num);
		ret = -EAGAIN;
		goto cleanup;
	}

	wr_words = BF_GET(qs->data.flags, MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET, 
		MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH);

	dev_dbg(qs->qinfo->dev, "%s(): wr(%u) queue(%d) rd/wr offset(%d/%d)", 
		__func__, num, qs->ch, qs->data.rd_offset, qs->data.wr_offset);
    	if (qs->data.wr_offset >= qs->data.rd_offset) {
        	const size_t words_to_end = wr_words - qs->data.wr_offset;
        	const size_t words_avail = (qs->data.rd_offset == 0) ? 
			(words_to_end - 1) : words_to_end;
        	const size_t words_to_write =
            		(num <= words_avail) ? num : words_avail;
        	const u32 write_addr = qs->data.base_addr + 
			(qs->data.wr_offset * sizeof(u32));

		dev_dbg(qs->qinfo->dev, 
			"%s(): wr(%u) queue(%u) offset(%u) addr(0x%08x)\n", 
			__func__, words_to_write, qs->ch, qs->data.wr_offset, 
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
        	qs->data.wr_offset += words_to_write;
        	if (qs->data.wr_offset == wr_words) {
            		qs->data.wr_offset = 0;
        	}
    	}

    	if ((qs->data.wr_offset < qs->data.rd_offset) && (num > 0)) {
        	const size_t words_to_read = qs->data.rd_offset - qs->data.wr_offset - 1;
        	const size_t words_to_write = (num <= words_to_read) ? 
			num : words_to_read;
        	const uint32_t write_addr = qs->data.base_addr + 
			(qs->data.wr_offset * sizeof(u32));
        	
		dev_dbg(qs->qinfo->dev, 
			"%s(): wr(%u) queue(%u) offset(%u) addr(0x%08x)\n", 
			__func__, words_to_write, qs->ch, qs->data.wr_offset, 
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
        	qs->data.wr_offset += words_to_write;
    	}

    	ret = mt7697q_push_wr_ptr(qs);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697q_push_wr_ptr() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	dev_dbg(qs->qinfo->dev, "%s(): queue(%u) offset(%u) write(%u)\n", 
		__func__, qs->ch, qs->data.wr_offset, words_written);
	ret = words_written;

cleanup:
	mutex_unlock(&qs->qinfo->mutex);
    	return ret;
}

EXPORT_SYMBOL(mt7697q_write);

int mt7697q_pull_wr_ptr(void *hndl)
{
	struct mt7697q_spec *qs = (struct mt7697q_spec*)hndl;
	const u32 read_addr = MT7697_IO_SLAVE_BUFFER_ADDRESS +
        	(qs->ch * sizeof(struct mt7697q_data)) +
        	offsetof(struct mt7697q_data, wr_offset);
	int ret;

	qs->qinfo->s2m_mbox = 1 << qs->ch;
    	ret = mt7697io_clr_s2m_mbx(qs->qinfo);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697io_clr_s2m_mbx() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
	}

	dev_dbg(qs->qinfo->dev, 
		"%s(): pull wr ptr queue(%u) rd addr(0x%08x)\n", 
		__func__, qs->ch, read_addr);
    	ret = mt7697io_rd(qs->qinfo, read_addr, &qs->data.wr_offset, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(qs->data.wr_offset)));
    	if (ret < 0) {
		dev_err(qs->qinfo->dev, "%s(): mt7697io_rd() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
	}

	dev_dbg(qs->qinfo->dev, "%s(): wr offset(%u)\n", 
		__func__, qs->data.wr_offset);

cleanup:
	return ret;
}

EXPORT_SYMBOL(mt7697q_pull_wr_ptr);

int mt7697q_pull_rd_ptr(void *hndl)
{
	struct mt7697q_spec *qs = (struct mt7697q_spec*)hndl;
	const u32 read_addr = MT7697_IO_SLAVE_BUFFER_ADDRESS +
        	(qs->ch * sizeof(struct mt7697q_data)) +
        	offsetof(struct mt7697q_data, rd_offset);
	int ret;

	qs->qinfo->s2m_mbox = 1 << qs->ch;
    	ret = mt7697io_clr_s2m_mbx(qs->qinfo);
	if (ret < 0) {
		dev_err(qs->qinfo->dev, 
			"%s(): mt7697io_clr_s2m_mbx() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
	}

    	dev_dbg(qs->qinfo->dev, 
		"%s(): pull rd ptr queue(%u) rd addr(0x%08x)\n", 
		__func__, qs->ch, read_addr);
    	ret = mt7697io_rd(qs->qinfo, read_addr, &qs->data.rd_offset, 
		MT7697_QUEUE_LEN_TO_WORD(sizeof(qs->data.rd_offset)));
    	if (ret < 0) {
		dev_err(qs->qinfo->dev, "%s(): mt7697io_rd() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
	}

	dev_dbg(qs->qinfo->dev, "%s(): rd offset(%u)\n", 
		__func__, qs->data.rd_offset);

cleanup:
	return ret;
}

EXPORT_SYMBOL(mt7697q_pull_rd_ptr);

__inline u8 mt7697q_busy(u16 value)
{
	return BF_GET(value, 
		MT7697_IO_STATUS_REG_BUSY_OFFSET, 
		MT7697_IO_STATUS_REG_BUSY_WIDTH);
}

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

__inline size_t mt7697q_get_capacity(const void *hndl)
{
    	const struct mt7697q_spec *qs = (const struct mt7697q_spec*)hndl;
	return BF_GET(qs->data.flags, MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET, 
		MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH) - 1;
}

EXPORT_SYMBOL(mt7697q_get_capacity);

__inline size_t mt7697q_get_num_words(const void *hndl)
{
    	const struct mt7697q_spec *qs = (const struct mt7697q_spec*)hndl;
	return mt7697q_buf_diff(BF_GET(qs->data.flags, 
			MT7697_QUEUE_FLAGS_NUM_WORDS_OFFSET, 
			MT7697_QUEUE_FLAGS_NUM_WORDS_WIDTH),
        	qs->data.rd_offset, qs->data.wr_offset);
}

EXPORT_SYMBOL(mt7697q_get_num_words);

__inline size_t mt7697q_get_free_words(const void *hndl)
{
    	return mt7697q_get_capacity(hndl) - 
		mt7697q_get_num_words(hndl);
}

EXPORT_SYMBOL(mt7697q_get_free_words);
