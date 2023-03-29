/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung EXYNOS SoC series DQE driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/fs.h>

#include "dqe.h"
#include "decon.h"
#if defined(CONFIG_SOC_EXYNOS9810)
#include "./cal_9810/regs-dqe.h"
#endif

struct dqe_device *dqe_drvdata;
struct class *dqe_class;

u32 gamma_lut[3][65];
u32 cgc_lut[8][3];
u32 hsc_lut[2][6];

int dqe_log_level = 6;
module_param(dqe_log_level, int, 0644);

static void dqe_load_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_info("%s\n", __func__);

	for (i = 0; i < DQECGCLUT_MAX; i++) {
		dqe->ctx.cgc[i].addr = DQECGC1_RGB_BASE + (i * 4);
		dqe->ctx.cgc[i].val = dqe_read(dqe->ctx.cgc[i].addr);
	}

	for (i = 0; i < DQEGAMMALUT_MAX; i++) {
		dqe->ctx.gamma[i].addr = DQEGAMMALUT_X_Y_BASE + (i * 4);
		dqe->ctx.gamma[i].val = dqe_read(dqe->ctx.gamma[i].addr);
	}

	for (i = 0; i < DQEHSCLUT_MAX - 1; i++) {
		dqe->ctx.hsc[i].addr = DQEHSCLUT_BASE + (i * 4);
		dqe->ctx.hsc[i].val = dqe_read(dqe->ctx.hsc[i].addr);
	}
	dqe->ctx.hsc[DQEHSCLUT_MAX - 1].addr = DQEHSC_SKIN_H;
	dqe->ctx.hsc[DQEHSCLUT_MAX - 1].val = dqe_read(dqe->ctx.hsc[DQEHSCLUT_MAX - 1].addr);

	for (i = 0; i < DQECGCLUT_MAX; i++) {
		dqe_dbg("0x%04x %d %d %d",
			dqe->ctx.cgc[i].addr,
			DQECGCLUT_R_GET(dqe->ctx.cgc[i].val),
			DQECGCLUT_G_GET(dqe->ctx.cgc[i].val),
			DQECGCLUT_B_GET(dqe->ctx.cgc[i].val));
	}

	for (i = 0; i < DQEGAMMALUT_MAX; i++) {
		dqe_dbg("0x%04x %d %d ",
			dqe->ctx.gamma[i].addr,
			DQEGAMMALUT_X_GET(dqe->ctx.gamma[i].val),
			DQEGAMMALUT_Y_GET(dqe->ctx.gamma[i].val));
	}

	for (i = 0; i < DQEHSCLUT_MAX; i++) {
		dqe_dbg("0x%04x %08x ",
			dqe->ctx.hsc[i].addr, dqe->ctx.hsc[i].val);
	}
}

static void dqe_init_context(void)
{
	int i, j, k, val;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_info("%s\n", __func__);

	dqe->ctx.cgc[0].val = 0x0ff00000; /* DQECGC1_RED */
	dqe->ctx.cgc[1].val = 0x0003fc00; /* DQECGC1_GREEN */
	dqe->ctx.cgc[2].val = 0x000000ff; /* DQECGC1_BLUE */
	dqe->ctx.cgc[3].val = 0x0003fcff; /* DQECGC1_CYAN */
	dqe->ctx.cgc[4].val = 0x0ff000ff; /* DQECGC1_MAGENTA */
	dqe->ctx.cgc[5].val = 0x0ff3fc00; /* DQECGC1_YELLOW */
	dqe->ctx.cgc[6].val = 0x0ff3fcff; /* DQECGC1_WHITE */
	dqe->ctx.cgc[7].val = 0x00000000; /* DQECGC1_BLACK */

	/* DQEGAMMALUT_R_01_00 -- DQEGAMMALUT_B_64 */
	for (j = 0, k = 0; j < 3; j++) {
		val = 0;
		for (i = 0; i < 64; i += 2) {
			dqe->ctx.gamma[k++].val = (DQEGAMMALUT_X(val) | DQEGAMMALUT_Y(val + 4));
			val += 8;
		}
		dqe->ctx.gamma[k++].val = DQEGAMMALUT_X(val);
	}

	dqe->ctx.hsc[0].val = 0x00000000; /* DQEHSC_PPSCGAIN_RGB */
	dqe->ctx.hsc[1].val = 0x00000000; /* DQEHSC_PPSCGAIN_CMY */
	dqe->ctx.hsc[2].val = 0x00007605; /* DQEHSC_ALPHASCALE_SHIF */
	dqe->ctx.hsc[3].val = 0x0aa0780a; /* DQEHSC_POLY_CURVE0 */
	dqe->ctx.hsc[4].val = 0x09b3c0e6; /* DQEHSC_POLY_CURVE1 */
	dqe->ctx.hsc[5].val = 0x00ce0030; /* DQEHSC_SKIN_S */
	dqe->ctx.hsc[6].val = 0x00000000; /* DQEHSC_PPHCGAIN_RGB */
	dqe->ctx.hsc[7].val = 0x00000000; /* DQEHSC_PPHCGAIN_CMY */
	dqe->ctx.hsc[8].val = 0x00007605; /* DQEHSC_TSC_YCOMP */
	dqe->ctx.hsc[9].val = 0x00008046; /* DQEHSC_POLY_CURVE2 */
	dqe->ctx.hsc[10].val = 0x0040000a; /* DQEHSC_SKIN_H */

	dqe->ctx.cgc_on = 0;
	dqe->ctx.gamma_on = 0;
	dqe->ctx.hsc_on = 0;
	dqe->ctx.hsc_control = 0;

	dqe->ctx.need_udpate = true;
}

static int dqe_save_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	if (dqe->ctx.need_udpate)
		return 0;

	dqe_dbg("%s\n", __func__);

	for (i = 0; i < DQECGCLUT_MAX; i++)
		dqe->ctx.cgc[i].val =
			dqe_read(dqe->ctx.cgc[i].addr);

	dqe->ctx.cgc_on = dqe_reg_get_cgc_on();

	for (i = 0; i < DQEGAMMALUT_MAX; i++)
		dqe->ctx.gamma[i].val =
			dqe_read(dqe->ctx.gamma[i].addr);

	dqe->ctx.gamma_on = dqe_reg_get_gamma_on();

	dqe->ctx.hsc[0].val = dqe_read(dqe->ctx.hsc[0].addr); /* DQEHSC_PPSCGAIN_RGB */
	dqe->ctx.hsc[1].val = dqe_read(dqe->ctx.hsc[1].addr); /* DQEHSC_PPSCGAIN_CMY */
	dqe->ctx.hsc[6].val = dqe_read(dqe->ctx.hsc[6].addr); /* DQEHSC_PPHCGAIN_RGB */
	dqe->ctx.hsc[7].val = dqe_read(dqe->ctx.hsc[7].addr); /* DQEHSC_PPHCGAIN_CMY */

	dqe->ctx.hsc_on = dqe_reg_get_hsc_on();
	dqe->ctx.hsc_control = dqe_reg_get_hsc_control();

	return 0;
}

static int dqe_restore_context(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	dqe_dbg("%s\n", __func__);

	for (i = 0; i < DQECGCLUT_MAX; i++) {
		dqe_write(dqe->ctx.cgc[i].addr,
				dqe->ctx.cgc[i].val);
		dqe_write(dqe->ctx.cgc[i].addr + 0x0400,
				dqe->ctx.cgc[i].val);
	}

	if (dqe->ctx.cgc_on)
		dqe_reg_set_cgc_on(1);

	for (i = 0; i < DQEGAMMALUT_MAX; i++)
		dqe_write(dqe->ctx.gamma[i].addr,
				dqe->ctx.gamma[i].val);

	if (dqe->ctx.gamma_on)
		dqe_reg_set_gamma_on(1);

	for (i = 0; i < DQEHSCLUT_MAX; i++)
		dqe_write(dqe->ctx.hsc[i].addr,
				dqe->ctx.hsc[i].val);

	if (dqe->ctx.hsc_on) {
		dqe_reg_set_hsc_control_all_reset();
		dqe_reg_set_hsc_on(1);
		if (dqe->ctx.hsc_control) {
			dqe_reg_set_hsc_pphc_on(1);
			dqe_reg_set_hsc_ppsc_on(1);
		}
	}

	dqe->ctx.need_udpate = false;

	return 0;
}

static void dqe_gamma_lut_set(void)
{
	int i, j, k;
	struct dqe_device *dqe = dqe_drvdata;

	for (j = 0, k = 0; j < 3; j++) {
		for (i = 0; i < 64; i += 2) {
			dqe->ctx.gamma[k++].val = (
				DQEGAMMALUT_X(gamma_lut[j][i]) |
				DQEGAMMALUT_Y(gamma_lut[j][i+1]));
		}
		dqe->ctx.gamma[k++].val =
			DQEGAMMALUT_X(gamma_lut[j][64]);
	}
}

static void dqe_cgc_lut_set(void)
{
	int i;
	struct dqe_device *dqe = dqe_drvdata;

	for (i = 0; i < 8; i++) {
		dqe->ctx.cgc[i].val = (
			DQECGCLUT_R(cgc_lut[i][0]) |
			DQECGCLUT_G(cgc_lut[i][1]) |
			DQECGCLUT_B(cgc_lut[i][2]));
	}
}

static void dqe_hsc_lut_set(void)
{
	struct dqe_device *dqe = dqe_drvdata;

	/* PPSCGAIN_RGB */
	dqe->ctx.hsc[0].val = (
		DQEHSCLUT_R(hsc_lut[0][0]) | DQEHSCLUT_G(hsc_lut[0][1]) | DQEHSCLUT_B(hsc_lut[0][2]));
	/* PPSCGAIN_CMY */
	dqe->ctx.hsc[1].val = (
		DQEHSCLUT_C(hsc_lut[0][3]) | DQEHSCLUT_M(hsc_lut[0][4]) | DQEHSCLUT_Y(hsc_lut[0][5]));
	/* PPHCGAIN_RGB */
	dqe->ctx.hsc[6].val = (
		DQEHSCLUT_R(hsc_lut[1][0]) | DQEHSCLUT_G(hsc_lut[1][1]) | DQEHSCLUT_B(hsc_lut[1][2]));
	/* PPHCGAIN_CMY */
	dqe->ctx.hsc[7].val = (
		DQEHSCLUT_C(hsc_lut[1][3]) | DQEHSCLUT_M(hsc_lut[1][4]) | DQEHSCLUT_Y(hsc_lut[1][5]));
}

static ssize_t decon_dqe_gamma_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	for (i = 0; i < DQEGAMMALUT_MAX; i++) {
		dqe_info("%d %d ",
			DQEGAMMALUT_X_GET(dqe->ctx.gamma[i].val),
			DQEGAMMALUT_Y_GET(dqe->ctx.gamma[i].val));
		dqe_dbg("0x%04x %08x ",
			dqe->ctx.gamma[i].addr, dqe->ctx.gamma[i].val);
	}

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "gamma_on = %d\n", dqe->ctx.gamma_on);

	return count;
}

static ssize_t decon_dqe_gamma_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("gamma write count error\n");
		ret = -1;
		goto err;
	}

	if (decon) {
		if ((decon->state == DECON_STATE_OFF) ||
			(decon->state == DECON_STATE_INIT)) {
			dqe_err("decon is not enabled!(%d)\n", decon->state);
			ret = -1;
			goto err;
		}
	} else {
		dqe_err("decon is NULL!\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 3; i++) {
			k = (i == 2) ? 64 : 65;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtou32(head, 0, &gamma_lut[i][j]);
				if (ret) {
					dqe_err("strtou32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtou32(head, 0, &gamma_lut[i-1][j]);
		if (ret) {
			dqe_err("strtou32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}


	for (i = 0; i < 3; i++)
		for (j = 0; j < 65; j++)
			dqe_dbg("%d ", gamma_lut[i][j]);

	dqe_gamma_lut_set();

	dqe->ctx.gamma_on = DQE_GAMMA_ON_MASK;
	dqe->ctx.need_udpate = true;

	dqe_restore_context();
	decon_reg_update_req_dqe(decon->id);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(gamma, 0660,
	decon_dqe_gamma_show,
	decon_dqe_gamma_store);

static ssize_t decon_dqe_cgc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	for (i = 0; i < DQECGCLUT_MAX; i++) {
		dqe_info("%d %d %d ",
			DQECGCLUT_R_GET(dqe->ctx.cgc[i].val),
			DQECGCLUT_G_GET(dqe->ctx.cgc[i].val),
			DQECGCLUT_B_GET(dqe->ctx.cgc[i].val));
		dqe_dbg("0x%04x %08x",
			dqe->ctx.cgc[i].addr, dqe->ctx.cgc[i].val);
	}

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "cgc_on = %d\n", dqe->ctx.cgc_on);

	return count;
}

static ssize_t decon_dqe_cgc_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("cgc write count error\n");
		ret = -1;
		goto err;
	}

	if (decon) {
		if ((decon->state == DECON_STATE_OFF) ||
			(decon->state == DECON_STATE_INIT)) {
			dqe_err("decon is not enabled!(%d)\n", decon->state);
			ret = -1;
			goto err;
		}
	} else {
		dqe_err("decon is NULL!\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 8; i++) {
			k = (i == 7) ? 2 : 3;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtou32(head, 0, &cgc_lut[i][j]);
				if (ret) {
					dqe_err("strtou32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtou32(head, 0, &cgc_lut[i-1][j]);
		if (ret) {
			dqe_err("strtou32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (i = 0; i < 8; i++)
		for (j = 0; j < 3; j++)
			dqe_dbg("%d ", cgc_lut[i][j]);

	dqe_cgc_lut_set();

	dqe->ctx.cgc_on = DQE_CGC_ON_MASK;
	dqe->ctx.need_udpate = true;

	dqe_restore_context();
	decon_reg_update_req_dqe(decon->id);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(cgc, 0660,
	decon_dqe_cgc_show,
	decon_dqe_cgc_store);

static ssize_t decon_dqe_hsc_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	struct dqe_device *dqe = dev_get_drvdata(dev);

	dqe_info("%s\n", __func__);

	mutex_lock(&dqe->lock);

	dqe_info("%d %d %d %d %d %d",
		DQEHSCLUT_R_GET(dqe->ctx.hsc[0].val),
		DQEHSCLUT_G_GET(dqe->ctx.hsc[0].val),
		DQEHSCLUT_B_GET(dqe->ctx.hsc[0].val),
		DQEHSCLUT_C_GET(dqe->ctx.hsc[1].val),
		DQEHSCLUT_M_GET(dqe->ctx.hsc[1].val),
		DQEHSCLUT_Y_GET(dqe->ctx.hsc[1].val));

	dqe_info("%d %d %d %d %d %d",
		DQEHSCLUT_R_GET(dqe->ctx.hsc[6].val),
		DQEHSCLUT_G_GET(dqe->ctx.hsc[6].val),
		DQEHSCLUT_B_GET(dqe->ctx.hsc[6].val),
		DQEHSCLUT_C_GET(dqe->ctx.hsc[7].val),
		DQEHSCLUT_M_GET(dqe->ctx.hsc[7].val),
		DQEHSCLUT_Y_GET(dqe->ctx.hsc[7].val));

	mutex_unlock(&dqe->lock);

	count = snprintf(buf, PAGE_SIZE, "hsc_on = %d\n", dqe->ctx.hsc_on);

	return count;
}

static ssize_t decon_dqe_hsc_store(struct device *dev, struct device_attribute *attr,
		const char *buffer, size_t count)
{
	int i, j, k;
	int ret = 0;
	char *head = NULL;
	char *ptr = NULL;
	struct dqe_device *dqe = dev_get_drvdata(dev);
	struct decon_device *decon = get_decon_drvdata(0);

	dqe_info("%s +\n", __func__);

	mutex_lock(&dqe->lock);

	if (count <= 0) {
		dqe_err("hsc write count error\n");
		ret = -1;
		goto err;
	}

	if (decon) {
		if ((decon->state == DECON_STATE_OFF) ||
			(decon->state == DECON_STATE_INIT)) {
			dqe_err("decon is not enabled!(%d)\n", decon->state);
			ret = -1;
			goto err;
		}
	} else {
		dqe_err("decon is NULL!\n");
		ret = -1;
		goto err;
	}

	head = (char *)buffer;
	if  (*head != 0) {
		dqe_dbg("%s\n", head);
		for (i = 0; i < 2; i++) {
			k = (i == 1) ? 5 : 6;
			for (j = 0; j < k; j++) {
				ptr = strchr(head, ',');
				if (ptr == NULL) {
					dqe_err("not found comma.(%d, %d)\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				*ptr = 0;
				ret = kstrtou32(head, 0, &hsc_lut[i][j]);
				if (ret) {
					dqe_err("strtou32(%d, %d) error.\n", i, j);
					ret = -EINVAL;
					goto err;
				}
				head = ptr + 1;
			}
		}
		k = 0;
		while (*(head + k) >= '0' && *(head + k) <= '9')
			k++;
		*(head + k) = 0;
		ret = kstrtou32(head, 0, &hsc_lut[i-1][j]);
		if (ret) {
			dqe_err("strtou32(%d, %d) error.\n", i-1, j);
			ret = -EINVAL;
			goto err;
		}
	} else {
		dqe_err("buffer is null.\n");
		goto err;
	}

	for (i = 0; i < 2; i++)
		for (j = 0; j < 6; j++)
			dqe_dbg("%d ", hsc_lut[i][j]);

	dqe_hsc_lut_set();

	dqe->ctx.hsc_on = DQE_HSC_ON_MASK;
	dqe->ctx.hsc_control = (HSC_PPSC_ON_MASK | HSC_PPHC_ON_MASK);
	dqe->ctx.need_udpate = true;

	dqe_restore_context();
	decon_reg_update_req_dqe(decon->id);

	mutex_unlock(&dqe->lock);

	dqe_info("%s -\n", __func__);

	return count;
err:
	mutex_unlock(&dqe->lock);

	dqe_info("%s : err(%d)\n", __func__, ret);

	return ret;
}

static DEVICE_ATTR(hsc, 0660,
	decon_dqe_hsc_show,
	decon_dqe_hsc_store);

static struct attribute *dqe_attrs[] = {
	&dev_attr_gamma.attr,
	&dev_attr_cgc.attr,
	&dev_attr_hsc.attr,
	NULL,
};
ATTRIBUTE_GROUPS(dqe);

void decon_dqe_enable(struct decon_device *decon)
{
	u32 val;

	if (decon->id)
		return;

	dqe_dbg("%s\n", __func__);

	dqe_restore_context();
	dqe_reg_start(decon->id, decon->lcd_info);

	val = dqe_read(DQECON);
	dqe_info("dqe gamma:%d cgc:%d hsc:%d\n",
			DQE_GAMMA_ON_GET(val),
			DQE_CGC_ON_GET(val),
			DQE_HSC_ON_GET(val));
}

void decon_dqe_disable(struct decon_device *decon)
{
	if (decon->id)
		return;

	dqe_dbg("%s\n", __func__);

	dqe_save_context();
	dqe_reg_stop(decon->id);
}

int decon_dqe_create_interface(struct decon_device *decon)
{
	int ret = 0;
	struct dqe_device *dqe;

	if (decon->id || (decon->dt.out_type != DECON_OUT_DSI)) {
		dqe_info("decon%d doesn't need dqe interface\n", decon->id);
		return 0;
	}

	dqe = kzalloc(sizeof(struct dqe_device), GFP_KERNEL);
	if (!dqe) {
		ret = -ENOMEM;
		goto exit0;
	}

	dqe_drvdata = dqe;
	dqe->decon = decon;

	if (IS_ERR_OR_NULL(dqe_class)) {
		dqe_class = class_create(THIS_MODULE, "dqe");
		if (IS_ERR_OR_NULL(dqe_class)) {
			pr_err("failed to create dqe class\n");
			ret = -EINVAL;
			goto exit1;
		}

		dqe_class->dev_groups = dqe_groups;
	}

	dqe->dev = device_create(dqe_class, decon->dev, 0,
			&dqe, "dqe", 0);
	if (IS_ERR_OR_NULL(dqe->dev)) {
		pr_err("failed to create dqe device\n");
		ret = -EINVAL;
		goto exit2;
	}

	mutex_init(&dqe->lock);
	dev_set_drvdata(dqe->dev, dqe);

	dqe_load_context();

	dqe_init_context();

	dqe_info("decon_dqe_create_interface done.\n");

	return ret;
exit2:
	class_destroy(dqe_class);
exit1:
	kfree(dqe);
exit0:
	return ret;
}
