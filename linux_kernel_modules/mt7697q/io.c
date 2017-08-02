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

static __inline u8 mt7697io_busy(u16 value)
{
	return BF_GET(value, MT7697_IO_STATUS_REG_BUSY_OFFSET, 
		MT7697_IO_STATUS_REG_BUSY_WIDTH);
}

static int mt7697io_write16(struct mt7697q_info *qinfo, u8 reg, u16 value)
{
	int ret;

    	WARN_ON(reg % sizeof(u16));

	qinfo->txBuffer[0] = MT7697_IO_CMD_WRITE;
    	qinfo->txBuffer[1] = reg;
	qinfo->txBuffer[2] = ((value >> 8) & 0xFF);
	qinfo->txBuffer[3] = value & 0xFF;

	ret = qinfo->hw_ops->write(qinfo->hw_priv, qinfo->txBuffer, 
		sizeof(qinfo->txBuffer));
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
    	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write16() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

    	ret = mt7697io_write16(qinfo, reg + 2, BF_GET(value, 16, 16));
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write16() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

cleanup:
    	return ret;
}

static int mt7697io_read16(struct mt7697q_info *qinfo, u8 reg, u16 *value)
{
    	int ret;

	WARN_ON(reg % sizeof(u16));
    
	qinfo->txBuffer[0] = MT7697_IO_CMD_READ;
	qinfo->txBuffer[1] = reg;

	ret = qinfo->hw_ops->write_then_read(qinfo->hw_priv, 
		qinfo->txBuffer, sizeof(u16), 
		qinfo->rxBuffer, sizeof(qinfo->rxBuffer));
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): write_then_read() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	*value = ((qinfo->rxBuffer[0] << 8) | qinfo->rxBuffer[1]);

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
    	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_read16() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}  
    
    	ret = mt7697io_read16(qinfo, reg + sizeof(u16), &high);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_read16() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

    	*value = (low | (high << 16));

cleanup:
    return ret;
}

static int mt7697io_chk_slave_busy(struct mt7697q_info *qinfo)
{
	int ret;
    	u16 value;
    
	ret = mt7697io_read16(qinfo, MT7697_IO_SLAVE_REG_STATUS, &value);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_read16() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	qinfo->slave_busy = mt7697io_busy(value) == MT7697_IO_STATUS_REG_BUSY_VAL_BUSY;

cleanup:
    	return ret;
}

static int mt7697io_slave_wait(struct mt7697q_info *qinfo)
{
	int ret;

	qinfo->slave_busy = true;
	while (qinfo->slave_busy) {
            	ret = mt7697io_chk_slave_busy(qinfo);
		if (ret < 0) {
			dev_err(qinfo->dev, 
				"%s(): mt7697io_chk_slave_busy() failed(%d)\n", 
				__func__, ret);
       			goto cleanup;
    		}

		udelay(1);
        };

cleanup:
    	return ret;
}

static __inline u8 mt7697io_get_s2m_mbox(u16 value)
{
	return BF_GET(value, 
		MT7697_IO_S2M_MAILBOX_REG_MAILBOX_OFFSET, 
		MT7697_IO_S2M_MAILBOX_REG_MAILBOX_WIDTH);
}

static __inline u16 mt7697io_set_s2m_mbox(u8 value)
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
    	WARN_ON((GENMASK(MT7697_IO_M2S_MAILBOX_REG_MAILBOX_WIDTH, 0) & bits) != bits);
   
    	ret = mt7697io_write16(qinfo, MT7697_IO_SLAVE_REG_MAILBOX_M2S, value);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write16() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

cleanup:
    	return ret;
}

int mt7697io_rd_s2m_mbx(struct mt7697q_info *qinfo)
{
	int ret;
    	u16 value;

    	ret = mt7697io_read16(qinfo, MT7697_IO_SLAVE_REG_MAILBOX_S2M, &value);
    	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_read16() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

	qinfo->s2m_mbox = mt7697io_get_s2m_mbox(value);
	dev_dbg(qinfo->dev, "%s(): s2m mbx(0x%02x)\n", 
		__func__, qinfo->s2m_mbox);

cleanup:
    	return ret;
}

int mt7697io_clr_s2m_mbx(struct mt7697q_info *qinfo)
{
	const u16 value = mt7697io_set_s2m_mbox(qinfo->s2m_mbox);
	int ret;

    	ret = mt7697io_write16(qinfo, MT7697_IO_SLAVE_REG_MAILBOX_S2M, value);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write16() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

cleanup:
    	return ret;
}

int mt7697io_wr(struct mt7697q_info *qinfo, u32 addr, const u32 *data, size_t num)
{
	size_t i;
	int ret;

    	WARN_ON(num == 0);

	ret = mt7697io_slave_wait(qinfo);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_slave_wait() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

    	ret = mt7697io_write32(qinfo, MT7697_IO_SLAVE_REG_BUS_ADDR_LOW, addr);
    	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write32() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

    	for (i = 0; i < num; i++) {
        	ret = mt7697io_write32(qinfo, MT7697_IO_SLAVE_REG_WRITE_DATA_LOW, data[i]);
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

	ret = mt7697io_slave_wait(qinfo);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_slave_wait() failed(%d)\n", 
			__func__, ret);
       		goto cleanup;
    	}

    	ret = mt7697io_write32(qinfo, MT7697_IO_SLAVE_REG_BUS_ADDR_LOW, addr);
	if (ret < 0) {
		dev_err(qinfo->dev, "%s(): mt7697io_write32() failed(%d)\n", 
			__func__, ret);
   	    	goto cleanup;
   	}

    	for (i = 0; i < num; i++) {
		ret = mt7697io_write16(qinfo, MT7697_IO_SLAVE_REG_COMMAND, (
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

        	ret = mt7697io_read32(qinfo, MT7697_IO_SLAVE_REG_READ_DATA_LOW, &data[i]);
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
	int ret = mt7697io_write16(qinfo, MT7697_IO_SLAVE_REG_IRQ, 
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
