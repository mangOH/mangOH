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
#include <linux/delay.h>
#include "bits.h"
#include "queue.h"
#include "io.h"
#include "spi.h"

static bool mt7697io_busy(u16 value)
{
	return BF_GET(value, MT7697_IO_STATUS_REG_BUSY_OFFSET,
		      MT7697_IO_STATUS_REG_BUSY_WIDTH) ==
		MT7697_IO_STATUS_REG_BUSY_VAL_BUSY;
}

static int mt7697io_write16(struct mt7697q_info *qinfo, u8 reg, u16 value)
{
	int ret;
	u8 txBuffer[] = {
		MT7697_IO_CMD_WRITE,
		reg,
		((value >> 8) & 0xFF),
		value & 0xFF,
	};

	WARN_ON(reg % sizeof(u16));
	ret = qinfo->hw_ops->write(qinfo->hw_priv, txBuffer, sizeof(txBuffer));
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): write() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return ret;
}

static int mt7697io_write32(struct mt7697q_info *qinfo, u8 reg, u32 value)
{
	int ret;
	WARN_ON(reg % sizeof(u32));

	ret = mt7697io_write16(qinfo, reg, BF_GET(value, 0, 16));
	if (ret < 0)
		goto fail;
	ret = mt7697io_write16(qinfo, reg + 2, BF_GET(value, 16, 16));
	if (ret < 0)
		goto fail;

	return ret;

fail:
	dev_err(qinfo->dev, "%s(): mt7697io_write16() failed(%d)\n", __func__,
		ret);
	return ret;
}

static int mt7697io_read16(struct mt7697q_info *qinfo, u8 reg, u16 *value)
{
	int ret;
	u8 spi_buffer[4] = {
		MT7697_IO_CMD_READ,
		reg,
	};

	WARN_ON(reg % sizeof(u16));
	ret = qinfo->hw_ops->write_then_read(qinfo->hw_priv, spi_buffer,
					     spi_buffer, sizeof(spi_buffer));
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): write_then_read() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

	*value = ((spi_buffer[2] << 8) | spi_buffer[3]);

cleanup:
	return ret;
}

static int mt7697io_read32(struct mt7697q_info *qinfo, u8 reg, u32 *value)
{
	int ret;
	u16 low;
	u16 high;
	WARN_ON(reg % sizeof(u32));

	ret = mt7697io_read16(qinfo, reg, &low);
	if (ret < 0)
		goto fail;
	ret = mt7697io_read16(qinfo, reg + sizeof(u16), &high);
	if (ret < 0)
		goto fail;
	*value = (low | (high << 16));

	return ret;

fail:
	dev_err(qinfo->dev, "%s(): mt7697io_read16() failed(%d)\n", __func__,
		ret);
	return ret;
}

static int mt7697io_chk_slave_busy(struct mt7697q_info *qinfo, bool *slave_busy)
{
	int ret;
	u16 value;

	ret = mt7697io_read16(qinfo, MT7697_IO_SLAVE_REG_STATUS, &value);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_read16() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

	*slave_busy = mt7697io_busy(value);

cleanup:
	return ret;
}

static int mt7697io_slave_wait(struct mt7697q_info *qinfo)
{
	int ret;
	bool slave_busy;
	do {
		ret = mt7697io_chk_slave_busy(qinfo, &slave_busy);
		if (ret < 0) {
			dev_err(qinfo->dev,
				"%s(): mt7697io_chk_slave_busy() failed(%d)\n",
				__func__, ret);
			goto cleanup;
		}
		udelay(1);
	} while (slave_busy);

cleanup:
	return ret;
}

static u8 mt7697io_get_s2m_mbox(u16 value)
{
	return BF_GET(value,
		MT7697_IO_S2M_MAILBOX_REG_MAILBOX_OFFSET,
		MT7697_IO_S2M_MAILBOX_REG_MAILBOX_WIDTH);
}

static u16 mt7697io_set_s2m_mbox(u8 value)
{
	return BF_DEFINE(value,
		MT7697_IO_S2M_MAILBOX_REG_MAILBOX_OFFSET,
		MT7697_IO_S2M_MAILBOX_REG_MAILBOX_WIDTH);
}

int mt7697io_wr_m2s_mbx(struct mt7697q_info *qinfo, u8 bits)
{
	int ret;
	const u16 value = BF_DEFINE(bits,
		MT7697_IO_M2S_MAILBOX_REG_MAILBOX_OFFSET,
		MT7697_IO_M2S_MAILBOX_REG_MAILBOX_WIDTH);

	dev_dbg(qinfo->dev, "%s(): m2s mbx(0x%02x)\n", __func__, bits);
	WARN_ON((GENMASK(MT7697_IO_M2S_MAILBOX_REG_MAILBOX_WIDTH, 0) & bits) !=
		bits);

	ret = mt7697io_write16(qinfo, MT7697_IO_SLAVE_REG_MAILBOX_M2S, value);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write16() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return ret;
}

int mt7697io_rd_s2m_mbx(struct mt7697q_info *qinfo, u8 *s2m_mbx)
{
	int ret;
	u16 value;

	ret = mt7697io_read16(qinfo, MT7697_IO_SLAVE_REG_MAILBOX_S2M, &value);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_read16() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

	*s2m_mbx = mt7697io_get_s2m_mbox(value);
	dev_dbg(qinfo->dev, "%s(): s2m mbx(0x%02x)\n",
		__func__, *s2m_mbx);

cleanup:
	return ret;
}

int mt7697io_clr_s2m_mbx(struct mt7697q_info *qinfo, u8 s2m_mbx)
{
	const u16 value = mt7697io_set_s2m_mbox(s2m_mbx);
	int ret;

	ret = mt7697io_write16(qinfo, MT7697_IO_SLAVE_REG_MAILBOX_S2M, value);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write16() failed(%d)\n",
			__func__, ret);
	}

	return ret;
}

int mt7697io_wr(struct mt7697q_info *qinfo, u32 addr, const u32 *data,
		size_t num)
{
	size_t i;
	int ret;

	WARN_ON(num == 0);

	ret = mt7697io_write32(qinfo, MT7697_IO_SLAVE_REG_BUS_ADDR_LOW, addr);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write32() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

	for (i = 0; i < num; i++) {
		ret = mt7697io_write32(
			qinfo, MT7697_IO_SLAVE_REG_WRITE_DATA_LOW, data[i]);
		if (ret < 0) {
			dev_err(qinfo->dev,
				"%s(): mt7697io_write32() failed(%d)\n",
				__func__, ret);
			goto cleanup;
		}

		ret = mt7697io_write16(qinfo, MT7697_IO_SLAVE_REG_COMMAND, (
	                BF_DEFINE(MT7697_IO_COMMAND_REG_BUS_SIZE_VAL_WORD,
	                          MT7697_IO_COMMAND_REG_BUS_SIZE_OFFSET,
	                          MT7697_IO_COMMAND_REG_BUS_SIZE_WIDTH) |
	                BF_DEFINE(MT7697_IO_COMMAND_REG_RW_VAL_WRITE,
				  MT7697_IO_COMMAND_REG_RW_OFFSET,
				  MT7697_IO_COMMAND_REG_RW_WIDTH)));
		if (ret < 0) {
			dev_err(qinfo->dev,
				"%s(): mt7697io_write16() failed(%d)\n",
				__func__, ret);
			goto cleanup;
		}

		ret = mt7697io_slave_wait(qinfo);
		if (ret < 0) {
			dev_err(qinfo->dev,
				"%s(): mt7697io_slave_wait() failed(%d)\n",
				__func__, ret);
			goto cleanup;
		}
	}

cleanup:
	return ret;
}

int mt7697io_rd(struct mt7697q_info *qinfo, u32 addr, u32 *data, size_t num)
{
	size_t i;
	int ret;

	WARN_ON(num == 0);

	ret = mt7697io_write32(qinfo, MT7697_IO_SLAVE_REG_BUS_ADDR_LOW, addr);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write32() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

	for (i = 0; i < num; i++) {
		ret = mt7697io_write16(
			qinfo, MT7697_IO_SLAVE_REG_COMMAND, (
				BF_DEFINE(MT7697_IO_COMMAND_REG_BUS_SIZE_VAL_WORD,
				          MT7697_IO_COMMAND_REG_BUS_SIZE_OFFSET,
				          MT7697_IO_COMMAND_REG_BUS_SIZE_WIDTH) |
				BF_DEFINE(MT7697_IO_COMMAND_REG_RW_VAL_READ,
				          MT7697_IO_COMMAND_REG_RW_OFFSET,
				          MT7697_IO_COMMAND_REG_RW_WIDTH)));
		if (ret < 0) {
			dev_err(qinfo->dev,
				"%s(): mt7697io_write16() failed(%d)\n",
				__func__, ret);
			goto cleanup;
		}


		ret = mt7697io_slave_wait(qinfo);
		if (ret < 0) {
			dev_err(qinfo->dev,
				"%s(): mt7697io_slave_wait() failed(%d)\n",
				__func__, ret);
			goto cleanup;
		}

		ret = mt7697io_read32(
			qinfo, MT7697_IO_SLAVE_REG_READ_DATA_LOW, &data[i]);
		if (ret < 0) {
			dev_err(qinfo->dev,
				"%s(): mt7697io_read32() failed(%d)\n",
				__func__, ret);
			goto cleanup;
		}
	}

cleanup:
	return ret;
}

int mt7697io_trigger_intr(struct mt7697q_info *qinfo)
{
	int ret = mt7697io_write16(
		qinfo, MT7697_IO_SLAVE_REG_IRQ,
		BF_DEFINE(MT7697_IO_IRQ_REG_IRQ_STATUS_VAL_ACTIVE,
		          MT7697_IO_IRQ_REG_IRQ_STATUS_OFFSET,
		          MT7697_IO_IRQ_REG_IRQ_STATUS_WIDTH));
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write16() failed(%d)\n",
			__func__, ret);
		goto cleanup;
	}

cleanup:
	return ret;
}
