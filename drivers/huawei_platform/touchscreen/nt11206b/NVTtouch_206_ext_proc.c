/* drivers/input/touchscreen/nt11206/NVTtouch_206_ext_proc.c
 *
 * Copyright (C) 2010 - 2016 Novatek, Inc.
 *
 * $Revision: 3239 $
 * $Date: 2016-01-27 10:55:00 +0800 (週三, 27 一月 2016) $
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */


#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/proc_fs.h>
#include <linux/unistd.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#include <linux/input/mt.h>
#include <linux/wakelock.h>

#include <linux/of_gpio.h>
#include <linux/of_irq.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


#include "NVTtouch_206.h"

#if NVT_TOUCH_EXT_PROC
#define NVT_FW_VERSION "nvt_fw_version"
#define NVT_BASELINE "nvt_baseline"
#define NVT_RAW "nvt_raw"
#define NVT_DIFF "nvt_diff"

#define I2C_TANSFER_LENGTH  64

#define NORMAL_MODE 0x00
#define TEST_MODE_1 0x21    //before algo.
#define TEST_MODE_2 0x22    //after algo.

#define RAW_PIPE0_ADDR  0x10528
#define RAW_PIPE1_ADDR  0x13528
#define BASELINE_ADDR   0x11054
#define DIFF_PIPE0_ADDR 0x10A50
#define DIFF_PIPE1_ADDR 0x13A50
#define XDATA_SECTOR_SIZE   256

static uint8_t xdata_tmp[2048] = {0};
static int32_t xdata[2048] = {0};
uint8_t fw_ver = 0;
uint8_t x_num = 0;
uint8_t y_num = 0;
uint8_t button_num = 0;

static struct proc_dir_entry *NVT_proc_fw_version_entry;
static struct proc_dir_entry *NVT_proc_baseline_entry;
static struct proc_dir_entry *NVT_proc_raw_entry;
static struct proc_dir_entry *NVT_proc_diff_entry;

extern struct nvt_ts_data *ts;
extern int32_t CTP_I2C_READ(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len);
extern int32_t CTP_I2C_READ_DUMMY(struct i2c_client *client, uint16_t address);
extern int32_t CTP_I2C_WRITE(struct i2c_client *client, uint16_t address, uint8_t *buf, uint16_t len);
extern int32_t nvt_clear_fw_status(void);
extern int32_t nvt_check_fw_status(void);


/*******************************************************
Description:
	Novatek touchscreen change mode function.

return:
	n.a.
*******************************************************/
void nvt_change_mode(uint8_t mode)
{
	uint8_t buf[8] = {0};

	//---dummy read to resume TP before writing command---
	CTP_I2C_READ_DUMMY(ts->client, I2C_FW_Address);

	//---set xdata index to 0x14700---
	buf[0] = 0xFF;
	buf[1] = 0x01;
	buf[2] = 0x47;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---set mode---
	buf[0] = 0x50;
	buf[1] = mode;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);

	if (mode == NORMAL_MODE) {
		buf[0] = 0x51;
		buf[1] = 0xBB;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 2);
	}
}

/*******************************************************
Description:
	Novatek touchscreen get firmware related information
	function.

return:
	n.a.
*******************************************************/
void nvt_get_fw_info(void)
{
	uint8_t buf[64] = {0};
	uint32_t retry_count = 0;

info_retry:
	//---dummy read to resume TP before writing command---
	CTP_I2C_READ_DUMMY(ts->client, I2C_FW_Address);

	//---set xdata index to 0x14700---
	buf[0] = 0xFF;
	buf[1] = 0x01;
	buf[2] = 0x47;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---read fw info---
	buf[0] = 0x78;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 17);
	fw_ver = buf[1];
	x_num = buf[3];
	y_num = buf[4];
	button_num = buf[11];

	//---clear x_num, y_num if fw info is broken---
	if ((buf[1] + buf[2]) != 0xFF) {
		tp_log_err("%s: FW info is broken! fw_ver=0x%02X, ~fw_ver=0x%02X\n", __func__, buf[1], buf[2]);
		x_num = 0;
		y_num = 0;
		button_num = 0;

		if(retry_count < 3) {
			retry_count++;
			tp_log_err("%s: retry_count=%d\n", __func__, retry_count);
			goto info_retry;
		}
	}

	return;
}

/*******************************************************
Description:
	Novatek touchscreen get firmware pipe function.

return:
	Executive outcomes. 0---pipe 0. 1---pipe 1.
*******************************************************/
static uint8_t nvt_get_fw_pipe(void)
{
	uint8_t buf[8]= {0};

	//---dummy read to resume TP before writing command---
	CTP_I2C_READ_DUMMY(ts->client, I2C_FW_Address);

	//---set xdata index to 0x14700---
	buf[0] = 0xFF;
	buf[1] = 0x01;
	buf[2] = 0x47;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

	//---read fw status---
	buf[0] = 0x51;
	buf[1] = 0x00;
	CTP_I2C_READ(ts->client, I2C_FW_Address, buf, 2);

	//tp_log_info("FW pipe=%d, buf[1]=0x%02X\n", (buf[1]&0x01), buf[1]);

	return (buf[1] & 0x01);
}

/*******************************************************
Description:
	Novatek touchscreen read meta data function.

return:
	n.a.
*******************************************************/
void nvt_read_mdata(uint32_t xdata_addr)
{
	int32_t i = 0;
	int32_t j = 0;
	int32_t k = 0;
	uint8_t buf[I2C_TANSFER_LENGTH + 1] = {0};
	uint32_t head_addr = 0;
	int32_t dummy_len = 0;
	int32_t data_len = 0;
	int32_t residual_len = 0;

	//---set xdata sector address & length---
	head_addr = xdata_addr - (xdata_addr % XDATA_SECTOR_SIZE);
	dummy_len = xdata_addr - head_addr;
	data_len = x_num * y_num * 2;
	residual_len = (head_addr + dummy_len + data_len) % XDATA_SECTOR_SIZE;

	//printk("head_addr=0x%05X, dummy_len=0x%05X, data_len=0x%05X, residual_len=0x%05X\n", head_addr, dummy_len, data_len, residual_len);

	//read xdata : step 1
	for (i = 0; i < ((dummy_len + data_len) / XDATA_SECTOR_SIZE); i++) {
		//---change xdata index---
		buf[0] = 0xFF;
		buf[1] = ((head_addr + XDATA_SECTOR_SIZE * i) >> 16);
		buf[2] = ((head_addr + XDATA_SECTOR_SIZE * i) >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---read xdata by I2C_TANSFER_LENGTH
		for (j = 0; j < (XDATA_SECTOR_SIZE / I2C_TANSFER_LENGTH); j++) {
			//---read data---
			buf[0] = I2C_TANSFER_LENGTH * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, I2C_TANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < I2C_TANSFER_LENGTH; k++) {
				xdata_tmp[XDATA_SECTOR_SIZE * i + I2C_TANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04X\n", buf[k+1], (XDATA_SECTOR_SIZE*i + I2C_TANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (head_addr+XDATA_SECTOR_SIZE*i));
	}

	//read xdata : step2
	if (residual_len != 0) {
		//---change xdata index---
		buf[0] = 0xFF;
		buf[1] = ((xdata_addr + data_len - residual_len) >> 16);
		buf[2] = ((xdata_addr + data_len - residual_len) >> 8) & 0xFF;
		CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);

		//---read xdata by I2C_TANSFER_LENGTH
		for (j = 0; j < (residual_len / I2C_TANSFER_LENGTH + 1); j++) {
			//---read data---
			buf[0] = I2C_TANSFER_LENGTH * j;
			CTP_I2C_READ(ts->client, I2C_FW_Address, buf, I2C_TANSFER_LENGTH + 1);

			//---copy buf to xdata_tmp---
			for (k = 0; k < I2C_TANSFER_LENGTH; k++) {
				xdata_tmp[(dummy_len + data_len - residual_len) + I2C_TANSFER_LENGTH * j + k] = buf[k + 1];
				//printk("0x%02X, 0x%04x\n", buf[k+1], ((dummy_len+data_len-residual_len) + I2C_TANSFER_LENGTH*j + k));
			}
		}
		//printk("addr=0x%05X\n", (xdata_addr+data_len-residual_len));
	}

	//---remove dummy data and 2bytes-to-1data---
	for (i = 0; i < (data_len / 2); i++) {
		xdata[i] = (xdata_tmp[dummy_len + i * 2] + 256 * xdata_tmp[dummy_len + i * 2 + 1]);
	}

	//---set xdata index to 0x14700---
	buf[0] = 0xFF;
	buf[1] = 0x01;
	buf[2] = 0x47;
	CTP_I2C_WRITE(ts->client, I2C_FW_Address, buf, 3);
}

/*******************************************************
Description:
    Novatek touchscreen get meta data function.

return:
    n.a.
*******************************************************/
void nvt_get_mdata(int32_t *buf, uint8_t *x, uint8_t *y)
{
    memcpy(buf, xdata, (x_num * y_num * 4));
	*x = x_num;
	*y = y_num;
}

void nvt_clear_mdata(void)
{
    int32_t i = 0;
	int32_t j = 0;

	for (i = 0; i < y_num; i++) {
		for (j = 0; j < x_num; j++) {
			xdata[i * x_num + j] = 0;
		}
	}
}


/*******************************************************
Description:
	Novatek touchscreen firmware version show function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_fw_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "fw_ver=%d, x_num=%d, y_num=%d, button_num=%d\n", fw_ver, x_num, y_num, button_num);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print show
	function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t c_show(struct seq_file *m, void *v)
{
	int32_t i = 0;
	int32_t j = 0;

	for (i = 0; i < y_num; i++) {
		for (j = 0; j < x_num; j++) {
			seq_printf(m, "%5d, ", (int16_t)xdata[i * x_num + j]);
		}
		seq_puts(m, "\n");
	}
	seq_printf(m, "\n\n");
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print start
	function.

return:
	Executive outcomes. 1---call next function.
	NULL---not call next function and sequence loop
	stop.
*******************************************************/
static void *c_start(struct seq_file *m, loff_t *pos)
{
	return *pos < 1 ? (void *)1 : NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print next
	function.

return:
	Executive outcomes. NULL---no next and call sequence
	stop function.
*******************************************************/
static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	++*pos;
	return NULL;
}

/*******************************************************
Description:
	Novatek touchscreen xdata sequence print stop
	function.

return:
	n.a.
*******************************************************/
static void c_stop(struct seq_file *m, void *v)
{
	return;
}

const struct seq_operations nvt_fw_version_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_fw_version_show
};

const struct seq_operations nvt_seq_ops = {
	.start  = c_start,
	.next   = c_next,
	.stop   = c_stop,
	.show   = c_show
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_fw_version open
	function.

return:
	n.a.
*******************************************************/
static int32_t nvt_fw_version_open(struct inode *inode, struct file *file)
{
	nvt_get_fw_info();
	return seq_open(file, &nvt_fw_version_seq_ops);
}

static const struct file_operations nvt_fw_version_fops = {
	.owner = THIS_MODULE,
	.open = nvt_fw_version_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_baseline open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_baseline_open(struct inode *inode, struct file *file)
{
	if (nvt_clear_fw_status() != 0)
		return -EAGAIN;

	nvt_change_mode(TEST_MODE_1);

	if (nvt_check_fw_status() != 0)
		return -EAGAIN;

	nvt_get_fw_info();

	nvt_read_mdata(BASELINE_ADDR);

	nvt_change_mode(NORMAL_MODE);

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_baseline_fops = {
	.owner = THIS_MODULE,
	.open = nvt_baseline_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_raw open function.

return:
	Executive outcomes. 0---succeed.
*******************************************************/
static int32_t nvt_raw_open(struct inode *inode, struct file *file)
{
	if (nvt_clear_fw_status() != 0)
		return -EAGAIN;

	nvt_change_mode(TEST_MODE_1);

	if (nvt_check_fw_status() != 0)
		return -EAGAIN;

	nvt_get_fw_info();

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(RAW_PIPE0_ADDR);
	else
		nvt_read_mdata(RAW_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_raw_fops = {
	.owner = THIS_MODULE,
	.open = nvt_raw_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/*******************************************************
Description:
	Novatek touchscreen /proc/nvt_diff open function.

return:
	Executive outcomes. 0---succeed. negative---failed.
*******************************************************/
static int32_t nvt_diff_open(struct inode *inode, struct file *file)
{
	if (nvt_clear_fw_status() != 0)
		return -EAGAIN;

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status() != 0)
		return -EAGAIN;

	nvt_get_fw_info();

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(DIFF_PIPE0_ADDR);
	else
		nvt_read_mdata(DIFF_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);

	return seq_open(file, &nvt_seq_ops);
}

static const struct file_operations nvt_diff_fops = {
	.owner = THIS_MODULE,
	.open = nvt_diff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int nvt_get_chip_info(char *buf)
{
	int len = 0;
	if(NULL == buf) {
		tp_log_err("%s,%d parameters invalid\n", __func__, __LINE__);
		return -1;
	}
	nvt_get_fw_info();
	len += snprintf(buf + len,PAGE_SIZE, "%s", "novatek_nt11206b_INX_");
	len += snprintf(buf + len , PAGE_SIZE, "%d\n", fw_ver);
	return len;
}

int nvt_get_rawdata(void)
{
	if (nvt_clear_fw_status() != 0)
		return -EAGAIN;

	nvt_change_mode(TEST_MODE_1);

	if (nvt_check_fw_status() != 0)
		return -EAGAIN;

	nvt_get_fw_info();

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(RAW_PIPE0_ADDR);
	else
		nvt_read_mdata(RAW_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);
		
	return 0;
}

int nvt_get_baseline(void)
{
	
	if (nvt_clear_fw_status() != 0)
		return -EAGAIN;

	nvt_change_mode(TEST_MODE_1);

	if (nvt_check_fw_status() != 0)
		return -EAGAIN;

	nvt_get_fw_info();

	nvt_read_mdata(BASELINE_ADDR);

	nvt_change_mode(NORMAL_MODE);
	
	return 0;
}

int nvt_get_diff(void)
{
	if (nvt_clear_fw_status() != 0)
		return -EAGAIN;

	nvt_change_mode(TEST_MODE_2);

	if (nvt_check_fw_status() != 0)
		return -EAGAIN;

	nvt_get_fw_info();

	if (nvt_get_fw_pipe() == 0)
		nvt_read_mdata(DIFF_PIPE0_ADDR);
	else
		nvt_read_mdata(DIFF_PIPE1_ADDR);

	nvt_change_mode(NORMAL_MODE);
	return 0;
}

/*******************************************************
Description:
	Novatek touchscreen extra function proc. file node
	initial function.

return:
	Executive outcomes. 0---succeed. -12---failed.
*******************************************************/
int32_t nvt_extra_proc_init(void)
{
	NVT_proc_fw_version_entry = proc_create(NVT_FW_VERSION, 0444, NULL,&nvt_fw_version_fops);
	if (NVT_proc_fw_version_entry == NULL) {
		tp_log_err("%s: create proc/nvt_fw_version Failed!\n", __func__);
		return -ENOMEM;
	} else {
		tp_log_info("%s: create proc/nvt_fw_version Succeeded!\n", __func__);
	}

	NVT_proc_baseline_entry = proc_create(NVT_BASELINE, 0444, NULL,&nvt_baseline_fops);
	if (NVT_proc_baseline_entry == NULL) {
		tp_log_err("%s: create proc/nvt_baseline Failed!\n", __func__);
		return -ENOMEM;
	} else {
		tp_log_info("%s: create proc/nvt_baseline Succeeded!\n", __func__);
	}

	NVT_proc_raw_entry = proc_create(NVT_RAW, 0444, NULL,&nvt_raw_fops);
	if (NVT_proc_raw_entry == NULL) {
		tp_log_err("%s: create proc/nvt_raw Failed!\n", __func__);
		return -ENOMEM;
	} else {
		tp_log_info("%s: create proc/nvt_raw Succeeded!\n", __func__);
	}

	NVT_proc_diff_entry = proc_create(NVT_DIFF, 0444, NULL,&nvt_diff_fops);
	if (NVT_proc_diff_entry == NULL) {
		tp_log_err("%s: create proc/nvt_diff Failed!\n", __func__);
		return -ENOMEM;
	} else {
		tp_log_info("%s: create proc/nvt_diff Succeeded!\n", __func__);
	}

	return 0;
}
#endif
