#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/version.h>
#include <linux/fb.h>
#include <linux/spi/spi.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/rmap.h>
#include <linux/pagemap.h>
#include <linux/device.h>
#include <linux/bitrev.h>

#include "fb_waveshare_eink.h"

#define WS_DRIVER_OUTPUT_CONTROL		0x01
#define WS_BOOSTER_SOFT_START_CONTROL		0x0C
#define WS_DEEP_SLEEP_MODE			0x10
#define WS_DATA_ENTRY_MODE_SETTING		0x11
#define WS_SW_RESET				0x12
#define WS_MASTER_ACTIVATION			0x20
#define WS_DISPLAY_UPDATE_CONTROL_1		0x21
#define WS_DISPLAY_UPDATE_CONTROL_2		0x22
#define WS_WRITE_RAM				0x24
#define WS_WRITE_VCOM_REGISTER			0x2C
#define WS_WRITE_LUT_REGISTER			0x32
#define WS_SET_DUMMY_LINE_PERIOD		0x3A
#define WS_SET_GATE_TIME			0x3B
#define WS_SET_RAM_X_ADDRESS_START_END_POSITION	0x44
#define WS_SET_RAM_Y_ADDRESS_START_END_POSITION	0x45
#define WS_SET_RAM_X_ADDRESS_COUNTER		0x4E
#define WS_SET_RAM_Y_ADDRESS_COUNTER		0x4F
#define WS_TERMINATE_FRAME_READ_WRITE		0xFF

 #define WS_FB_SET_SCREEN_DEEP_SLEEP		_IOW('M', 1, int8_t)
 #define WS_FB_SET_SCREEN_WAKEUP		_IOW('M', 2, int8_t)

struct waveshare_eink_device_properties {
	unsigned int width;
	unsigned int height;
	unsigned int bpp;
};

struct ws_eink_fb_par {
	struct spi_device *spi;
	struct fb_info *info;
	u8 *ssbuf;
	int rst;
	int dc;
	int busy;
	const struct waveshare_eink_device_properties *props;
};

const u8 lut_full_update[] = {
	0x22, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x11,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

const u8 lut_partial_update[] = {
	0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int ws_eink_send_cmd(struct ws_eink_fb_par *par, u8 cmd, const u8 *data,
				size_t data_size)
{
	int ret;
	struct spi_device *spi = par->spi;
	struct device *dev = &spi->dev;

	gpio_set_value(par->dc, 0);
	ret = spi_write(spi, &cmd, 1);
	if (ret) {
		dev_err(dev,
			"SPI write failure (%d) while sending command (%d)",
			ret, cmd);
		return ret;
	}

	gpio_set_value(par->dc, 1);
	ret = spi_write(spi, data, data_size);
	if (ret)
		dev_err(dev,
			"SPI write failure (%d) while sending data for command (%d)",
			ret, cmd);

	return ret;
}

static void wait_until_idle(struct ws_eink_fb_par *par)
{
	while (gpio_get_value(par->busy) != 0)
		mdelay(100);
}

static void ws_eink_reset(struct ws_eink_fb_par *par)
{
	gpio_set_value(par->rst, 0);
	mdelay(200);
	gpio_set_value(par->rst, 1);
	mdelay(200);
}

static int ws_eink_sleep(struct ws_eink_fb_par *par)
{
	int ret;
	ret = ws_eink_send_cmd(par, WS_DEEP_SLEEP_MODE, NULL, 0);
	if (ret)
		return ret;

	wait_until_idle(par);

	return 0;
}

static int set_lut(struct ws_eink_fb_par *par, const u8 *lut, size_t lut_size)
{
	return ws_eink_send_cmd(par, WS_WRITE_LUT_REGISTER, lut, lut_size);
}

static int int_lut(struct ws_eink_fb_par *par, const u8 *lut, size_t lut_size)
{
	int ret;
	const u8 data_driver_output_control[] = {
		(par->props->height - 1) & 0xFF,
		((par->props->height - 1) >> 8) & 0xFF, 0x00 };
	const u8 data_booster_soft_start_control[] = { 0xD7, 0xD6, 0x9D };
	const u8 data_write_vcom_register[] = { 0xA8 };
	const u8 data_set_dummy_line_period[] = { 0x1A };
	const u8 data_set_gate_time[] = { 0x08 };
	const u8 data_data_entry_mode_setting[] = { 0x03 };

	ws_eink_reset(par);

	ret = ws_eink_send_cmd(par, WS_DRIVER_OUTPUT_CONTROL,
			       data_driver_output_control,
			       sizeof(data_driver_output_control));
	if (ret != 0)
		return ret;

	ret = ws_eink_send_cmd(par, WS_BOOSTER_SOFT_START_CONTROL,
			       data_booster_soft_start_control,
			       sizeof(data_booster_soft_start_control));
	if (ret != 0)
		return ret;

	ret = ws_eink_send_cmd(par, WS_WRITE_VCOM_REGISTER,
			       data_write_vcom_register,
			       sizeof(data_write_vcom_register));
	if (ret != 0)
		return ret;

	ret = ws_eink_send_cmd(par, WS_SET_DUMMY_LINE_PERIOD,
			       data_set_dummy_line_period,
			       sizeof(data_set_dummy_line_period));
	if (ret != 0)
		return ret;

	ret = ws_eink_send_cmd(par, WS_SET_GATE_TIME, data_set_gate_time,
			       sizeof(data_set_gate_time));
	if (ret != 0)
		return ret;

	ret = ws_eink_send_cmd(par, WS_DATA_ENTRY_MODE_SETTING,
			       data_data_entry_mode_setting,
			       sizeof(data_data_entry_mode_setting));
	if (ret != 0)
		return ret;

	return set_lut(par, lut, lut_size);
}

static int set_memory_area(struct ws_eink_fb_par *par, int x_start, int y_start,
			   int x_end, int y_end)
{
	int ret;
	const u8 x_start_end[] = { (x_start >> 3) & 0xFF, (x_end >> 3) & 0xFF };
	const u8 y_start_end[] = {
		y_start & 0xFF, (y_start >> 8) & 0xFF,
		y_end & 0xFF, (y_end >> 8) & 0xFF,
	};

	ret = ws_eink_send_cmd(par, WS_SET_RAM_X_ADDRESS_START_END_POSITION,
			       x_start_end, sizeof(x_start_end));
	if (ret)
		return ret;

	ret = ws_eink_send_cmd(par, WS_SET_RAM_Y_ADDRESS_START_END_POSITION,
			       y_start_end, sizeof(y_start_end));

	return ret;
}

static int set_memory_pointer(struct ws_eink_fb_par *par, int x, int y)
{
	int ret;
	const u8 x_addr_counter[] = { (x >> 3) & 0xFF };
	const u8 y_addr_counter[] = { y & 0xFF, (y >> 8) & 0xFF };

	ret = ws_eink_send_cmd(par, WS_SET_RAM_X_ADDRESS_COUNTER,
			       x_addr_counter, sizeof(x_addr_counter));
	if (ret)
		return ret;

	ret = ws_eink_send_cmd(par, WS_SET_RAM_Y_ADDRESS_COUNTER,
			       y_addr_counter, sizeof(y_addr_counter));
	if (ret)
		return ret;

	wait_until_idle(par);

	return 0;
}

static int clear_frame_memory(struct ws_eink_fb_par *par, u8 color)
{
	int ret;
	int row;
	u8 solid_line[par->props->width / 8];
	memset(solid_line, color, ARRAY_SIZE(solid_line));
	ret = set_memory_area(par, 0, 0, par->props->width - 1,
			      par->props->height - 1);
	if (ret)
		return ret;
	for (row = 0; row < par->props->height; row++) {
		ret = set_memory_pointer(par, 0, row);
		if (ret)
			return ret;
		ret = ws_eink_send_cmd(par, WS_WRITE_RAM, solid_line,
				       sizeof(solid_line));
		if (ret)
			return ret;
	}

	return 0;
}

static int set_frame_memory(struct ws_eink_fb_par *par, u8 *image_buffer)
{
	int ret;
	int row;
	ret = set_memory_area(par, 0, 0, par->props->width - 1,
			      par->props->height - 1);
	if (ret)
		return ret;

	for (row = 0; row < par->props->height; row++) {
		const size_t row_bytes = par->props->width / 8;
		ret = set_memory_pointer(par, 0, row);
		if (ret)
			return ret;
		ret = ws_eink_send_cmd(par, WS_WRITE_RAM,
				       &image_buffer[row * row_bytes],
				       row_bytes);
		if (ret)
			return ret;
	}

	return 0;
}

static int display_frame(struct ws_eink_fb_par *par)
{
	int ret;
	const u8 data_display_update_control_2[] = { 0xC4 };
	ret = ws_eink_send_cmd(par, WS_DISPLAY_UPDATE_CONTROL_2,
			       data_display_update_control_2,
			       sizeof(data_display_update_control_2));
	if (ret)
		return ret;

	ret = ws_eink_send_cmd(par, WS_MASTER_ACTIVATION, NULL, 0);
	if (ret)
		return ret;

	ret = ws_eink_send_cmd(par, WS_TERMINATE_FRAME_READ_WRITE, NULL, 0);
	if (ret)
		return ret;

	wait_until_idle(par);

	return 0;
}

static int ws_eink_init_display(struct ws_eink_fb_par *par)
{
	int ret;
	struct device *dev = &par->spi->dev;

	ret = devm_gpio_request_one(&par->spi->dev, par->rst,
				    GPIOF_OUT_INIT_LOW, "ws_eink_rst");
	if (ret) {
		dev_err(dev, "Couldn't request reset GPIO\n");
		return ret;
	}

	ret = devm_gpio_request_one(&par->spi->dev, par->dc, GPIOF_OUT_INIT_LOW,
				    "ws_eink_dc");
	if (ret) {
		dev_err(dev, "Couldn't request data/command GPIO\n");
		return ret;
	}

	ret = devm_gpio_request_one(&par->spi->dev, par->busy, GPIOF_IN,
				    "ws_eink_busy");
	if (ret) {
		dev_err(dev, "Couldn't request busy GPIO\n");
		return ret;
	}

	ret = int_lut(par, lut_full_update, ARRAY_SIZE(lut_full_update));
	if (ret)
		return ret;

	ret = clear_frame_memory(par, 0xFF);
	if (ret)
		return ret;

	ret = display_frame(par);
	if (ret)
		return ret;

	ret = int_lut(par, lut_partial_update, ARRAY_SIZE(lut_partial_update));

	return ret;
}

static int ws_eink_update_display(struct ws_eink_fb_par *par)
{
	int ret = 0;
	int i;
	int frame_size;
	u8 *vmem = par->info->screen_base;
	u8 *ssbuf = par->ssbuf;
	const u8 *lut;
	size_t lut_size;
	static int update_count = 0;

	if(++update_count == 10) {
		update_count = 0;
		lut = lut_full_update;
		lut_size = ARRAY_SIZE(lut_full_update);
	} else {
		lut = lut_partial_update;
		lut_size = ARRAY_SIZE(lut_partial_update);
	}

	ret = int_lut(par, lut, lut_size);
	if (ret)
		return ret;

	frame_size = par->props->height * par->props->width * par->props->bpp / 8;

	memcpy(ssbuf, vmem, frame_size);

	for (i = 0; i < frame_size; i++) {
		ssbuf[i] = bitrev8(ssbuf[i]);
	}

	ret = set_frame_memory(par, ssbuf);
	if (ret)
		return ret;

	ret = display_frame(par);
	if (ret)
		return ret;

	ret = ws_eink_sleep(par);

	return ret;
}

void ws_eink_fb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	int ret;
	struct ws_eink_fb_par *par = info->par;
	sys_fillrect(info, rect);
	ret = ws_eink_update_display(par);
	if (ret)
		dev_err(info->device, "%s: failed to update display", __func__);
}

void ws_eink_fb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	int ret;
	struct ws_eink_fb_par *par = info->par;
	sys_copyarea(info, area);
	ret = ws_eink_update_display(par);
	if (ret)
		dev_err(info->device, "%s: failed to update display", __func__);
}

void ws_eink_fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	int ret;
	struct ws_eink_fb_par *par = info->par;
	sys_imageblit(info, image);
	ret = ws_eink_update_display(par);
	if (ret)
		dev_err(info->device, "%s: failed to update display", __func__);
}

static ssize_t ws_eink_fb_write(struct fb_info *info, const char __user *buf,
				size_t count, loff_t *ppos)
{
	unsigned long p = *ppos;
	void *dst;
	int err = 0;
	unsigned long total_size;

	if (info->state != FBINFO_STATE_RUNNING)
		return -EPERM;

	total_size = info->fix.smem_len;

	if (p > total_size)
		return -EFBIG;

	if (count > total_size) {
		err = -EFBIG;
		count = total_size;
	}

	if (count + p > total_size) {
		if (!err)
			err = -ENOSPC;

		count = total_size - p;
	}

	dst = (void __force *)(info->screen_base + p);

	if (copy_from_user(dst, buf, count))
		err = -EFAULT;

	if (!err)
		*ppos += count;

	return (err) ? err : count;
}

static int ws_eink_fb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	struct ws_eink_fb_par *par = info->par;

	switch (cmd) {
		case WS_FB_SET_SCREEN_DEEP_SLEEP:
			ws_eink_sleep(par);
			break;
		case WS_FB_SET_SCREEN_WAKEUP:
			ws_eink_reset(par);
			break;
		default:
			return EINVAL;
	}

	return 0;
}

static struct fb_ops ws_eink_ops = {
	.owner		= THIS_MODULE,
	.fb_read	= fb_sys_read,
	.fb_write	= ws_eink_fb_write,
	.fb_fillrect	= ws_eink_fb_fillrect,
	.fb_copyarea	= ws_eink_fb_copyarea,
	.fb_imageblit	= ws_eink_fb_imageblit,
	.fb_ioctl 	= ws_eink_fb_ioctl,
};

enum waveshare_devices {
	DEV_WS_213,
};

static struct waveshare_eink_device_properties devices[] =
{
	[DEV_WS_213] = {.width = 128, .height = 250, .bpp = 1},
};

static struct spi_device_id waveshare_eink_tbl[] = {
	{"waveshare_213", (kernel_ulong_t)&devices[DEV_WS_213]},
	{},
};
MODULE_DEVICE_TABLE(spi, waveshare_eink_tbl);

static void ws_eink_deferred_io(struct fb_info *info,
				struct list_head *pagelist)
{
	int ret = ws_eink_update_display(info->par);
	if (ret)
		dev_err(info->device, "%s: failed to update display", __func__);
}

static struct fb_deferred_io ws_eink_defio = {
	.delay		= HZ,
	.deferred_io	= ws_eink_deferred_io,
};

static int ws_eink_spi_probe(struct spi_device *spi)
{
	struct fb_info *info;
	int retval = 0;
	struct waveshare_eink_platform_data *pdata;
	const struct spi_device_id *spi_id;
	const struct waveshare_eink_device_properties *dev_props;
	struct ws_eink_fb_par *par;
	int vmem_size;

	pdata = spi->dev.platform_data;
	if (!pdata) {
		dev_err(&spi->dev, "Required platform data was not provided");
		return -EINVAL;
	}

	spi_id = spi_get_device_id(spi);
	if (!spi_id) {
		dev_err(&spi->dev, "device id not supported!\n");
		return -EINVAL;
	}

	dev_props = (const struct waveshare_eink_device_properties *)
		spi_id->driver_data;
	if (!dev_props) {
		dev_err(&spi->dev, "device definition lacks driver_data\n");
		return -EINVAL;
	}

	info = framebuffer_alloc(sizeof(struct ws_eink_fb_par), &spi->dev);
	if (!info)
		return -ENOMEM;

	vmem_size = dev_props->width * dev_props->height * dev_props->bpp / 8;
	info->screen_base = vzalloc(vmem_size);
	if (!info->screen_base) {
		retval = -ENOMEM;
		goto screen_base_fail;
	}

	info->fbops = &ws_eink_ops;

	WARN_ON(strlcpy(info->fix.id, "waveshare_eink", sizeof(info->fix.id)) >=
		sizeof(info->fix.id));
	info->fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fix.visual	= FB_VISUAL_PSEUDOCOLOR;
	info->fix.smem_len	= vmem_size;
	info->fix.xpanstep	= 0;
	info->fix.ypanstep	= 0;
	info->fix.ywrapstep	= 0;
	info->fix.line_length	= dev_props->width * dev_props->bpp / 8;

	info->var.xres			= dev_props->width;
	info->var.yres			= dev_props->height;
	info->var.xres_virtual		= dev_props->width;
	info->var.yres_virtual		= dev_props->height;
	info->var.bits_per_pixel	= dev_props->bpp;

	info->flags = FBINFO_FLAG_DEFAULT | FBINFO_VIRTFB;

	info->fbdefio = &ws_eink_defio;
	fb_deferred_io_init(info);

	par		= info->par;
	par->info	= info;
	par->spi	= spi;
	par->props	= dev_props;
	par->rst	= pdata->rst_gpio;
	par->dc		= pdata->dc_gpio;
	par->busy	= pdata->busy_gpio;
	par->ssbuf	= vzalloc(vmem_size);
	if (!par->ssbuf) {
		retval = -ENOMEM;
		goto ssbuf_alloc_fail;
	}

	retval = register_framebuffer(info);
	if (retval < 0) {
		dev_err(&spi->dev, "framebuffer registration failed");
		goto fbreg_fail;
	}

	spi_set_drvdata(spi, info);

	retval = ws_eink_init_display(par);
	if (retval) {
		dev_err(&spi->dev, "display initialization failed");
		goto disp_init_fail;
	}

	dev_dbg(&spi->dev,
		"fb%d: %s frame buffer device,\n\tusing %d KiB of video memory\n",
		info->node, info->fix.id, vmem_size);

	return 0;

disp_init_fail:
	framebuffer_release(info);
fbreg_fail:
	vfree(par->ssbuf);
ssbuf_alloc_fail:
	vfree(info->screen_base);
screen_base_fail:
	vfree(info->screen_base);
	return retval;
}

static int ws_eink_spi_remove(struct spi_device *spi)
{
	struct fb_info *p = spi_get_drvdata(spi);
	struct ws_eink_fb_par *par = p->par;
	fb_deferred_io_cleanup(p);
	unregister_framebuffer(p);
	vfree(p->screen_base);
	vfree(par->ssbuf);
	framebuffer_release(p);

	return 0;
}

static struct spi_driver ws_eink_driver = {
	.driver = {
		.name	= "waveshare_eink",
		.owner	= THIS_MODULE,
	},

	.id_table	= waveshare_eink_tbl,
	.probe		= ws_eink_spi_probe,
	.remove 	= ws_eink_spi_remove,
};
module_spi_driver(ws_eink_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thong Nguyen");
MODULE_DESCRIPTION("FB driver for Waveshare eink displays");
MODULE_VERSION("0.1");
