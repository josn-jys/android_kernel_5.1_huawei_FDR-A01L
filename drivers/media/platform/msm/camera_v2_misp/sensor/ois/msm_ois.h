/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef MSM_OIS_H
#define MSM_OIS_H

#include <linux/i2c.h>
#include <linux/gpio.h>
/*< DTS2015012606635 tangying/205982 20150126 begin*/
/*add camera ois driver*/
#include <linux/wait.h>
#include <linux/workqueue.h>
/*DTS2015012606635 tangying/205982 20150126 end >*/
#include <soc/qcom/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"
#include "msm_camera_dt_util.h"
#include "msm_camera_io_util.h"
/*< DTS2015012606635 tangying/205982 20150126 begin*/
/*add camera ois driver*/
#include "mini_isp_ois_interface.h"
/*DTS2015012606635 tangying/205982 20150126 end >*/

/*< DTS2015031803765 ganfan/206212 20150318 begin*/
/*mini-isp do the ois init*/
#define USE_MINI_ISP_INIT_OIS
/*DTS2015031803765 ganfan/206212 20150318 end >*/

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define	MSM_OIS_MAX_VREGS (10)

struct msm_ois_ctrl_t;

enum msm_ois_state_t {
	OIS_POWER_UP,
	OIS_POWER_DOWN,
};

/*< DTS2015041705933 tangying/205982 20150418 begin*/
/*fix ois running test fail issue*/
/*< DTS2015012606635 tangying/205982 20150126 begin*/
/*add camera ois driver*/
enum msm_ois_work_type_t {
	OIS_INIT_WORK,
	OIS_RUNNING_TEST_WORK,
	OIS_TURN_ON_WORK,
	OIS_TURN_OFF_WORK,
};
/*DTS2015012606635 tangying/205982 20150126 end >*/
/*DTS2015041705933 tangying/205982 20150418 end >*/

struct msm_ois_vreg {
	struct camera_vreg_t *cam_vreg;
	void *data[MSM_OIS_MAX_VREGS];
	int num_vreg;
};

struct msm_ois_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client i2c_client;
	enum msm_camera_device_type_t ois_device_type;
	struct msm_sd_subdev msm_sd;
	struct mutex *ois_mutex;
	enum msm_camera_i2c_data_type i2c_data_type;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *ois_v4l2_subdev_ops;
	void *user_data;
	uint16_t i2c_tbl_index;
	enum cci_i2c_master_t cci_master;
	uint32_t subdev_id;
	enum msm_ois_state_t ois_state;
	struct msm_ois_vreg vreg_cfg;

/*< DTS2015012606635 tangying/205982 20150126 begin*/
/*add camera ois driver*/
    enum msm_ois_work_type_t ois_work_type;
    int32_t runningtest_parm;

    //for mmi test
    int32_t x_width;
    int32_t y_height;
    /*< DTS2015042909943 ganfan/206212 20150505 begin*/
    int32_t turn_on_type;
    /*DTS2015042909943 ganfan/206212 20150505 end >*/
    struct delayed_work ois_init_work;
    /*< DTS2015042702485 ganfan/206212 20150427 begin*/
    /*mini-isp do the ois init*/
    /*delete invalid codes*/
    /*DTS2015042702485 ganfan/206212 20150427 end >*/
    /*< DTS2015020511088 tangying/205982 20150205 begin*/
    /*optimize ois turn on/off interface in driver*/
    int32_t     ois_onoff;
    /*DTS2015020511088 tangying/205982 20150205 end >*/
    
    wait_queue_head_t	wait_check_ois;
    int32_t     check_ois;
    int32_t     ois_wait_end;
    int32_t module_id;
	/* <DTS2015073106553 xurenjie xwx208548 20150731 begin */
    int32_t gyro_xgain;
    int32_t gyro_ygain;
    int32_t towrite_gyro_xgain;
    int32_t towrite_gyro_ygain;
    int32_t delay_write_gain;
	/* DTS2015073106553 xurenjie xwx208548 20150731 end > */
/*DTS2015012606635 tangying/205982 20150126 end >*/
};

#endif
