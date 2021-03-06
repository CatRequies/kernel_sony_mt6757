/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     IMX300mipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx300mipi_Sensor.h"

/****************************Modify Following Strings for Debug****************************/
#define PFX "IMX300_camera_sensor"
#define LOG_1 LOG_INF("IMX300,MIPI 4LANE\n")
#define LOG_2 LOG_INF("preview 2760*2080@30fps; video 5520*4160@30fps; capture 5520*4160@24fps\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)	pr_debug(PFX "[%s] " format, __func__, ##args)

#define BYTE               unsigned char

/* static BOOL read_spc_flag = FALSE; */

static int isARNRtriggered = 0;

static DEFINE_SPINLOCK(imgsensor_drv_lock);

/* static BYTE imx300_SPC_data[352]={0}; */

/* extern void read_imx300_SPC( BYTE* data ); */

extern void imx300_DCC_conversion( kal_uint16 addr,BYTE* data, kal_uint32 size);

static imgsensor_info_struct imgsensor_info = {
	.sensor_id = IMX300_SENSOR_ID,	/* record sensor id defined in Kd_imgsensor.h */

	.checksum_value = 0xb76b5bc2,	/* checksum value for Camera Auto Test */

	.pre = {		/*data rate 1099.20 Mbps/lane */
		.pclk = 520000000,	/* record different mode's pclk */
		.linelength = 7416,	/* record different mode's linelength */
		.framelength = 2336,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 2760,	/* record different mode's width of grabwindow */
		.grabwindow_height = 2080,	/* record different mode's height of grabwindow */
		/*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 760000000,
		.linelength = 7616,
		.framelength = 4262,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5520,
		.grabwindow_height = 4160,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 230,
	},
	.cap1 = {
		.pclk = 760000000,
		.linelength = 7616,
		.framelength = 4262,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 5520,
		.grabwindow_height = 4160,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 240,
	},

	.normal_video = {
		.pclk = 520000000,
		.linelength = 7416,
		.framelength = 2336,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2760,
		.grabwindow_height = 2080,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 300,
	},
	.hs_video = {		/*data rate 600 Mbps/lane */
		.pclk = 760000000,
		.linelength = 7416,
		.framelength = 1706,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2152,
		.grabwindow_height = 1614,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 600,
	},
	.slim_video = {		/*data rate 792 Mbps/lane */
		.pclk = 760000000,
		.linelength = 7096,
		.framelength = 3570,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2992,
		.grabwindow_height = 1696,
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		.max_framerate = 300,
	},

	.custom1 = {		/*data rate 1499.20 Mbps/lane */
		.pclk = 760000000,	/* record different mode's pclk */
		.linelength = 8224,	/* record different mode's linelength */
		.framelength = 4262,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 5984,	/* record different mode's width of grabwindow */
		.grabwindow_height = 4160,	/* record different mode's height of grabwindow */
		/*         following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*         following for GetDefaultFramerateByScenario()        */
		.max_framerate = 210,
	},
	.custom2 = {		/*data rate 1099.20 Mbps/lane */
		.pclk = 760000000,	/* record different mode's pclk */
		.linelength = 8224,	/* record different mode's linelength */
		.framelength = 3576,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 5984,	/* record different mode's width of grabwindow */
		.grabwindow_height = 3392,	/* record different mode's height of grabwindow */
		/*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 250,
	},
#if 0
	.custom3 = {		/*data rate 1099.20 Mbps/lane */
		.pclk = 439680000,	/* record different mode's pclk */
		.linelength = 6024,	/* record different mode's linelength */
		.framelength = 2800,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 2672,	/* record different mode's width of grabwindow */
		.grabwindow_height = 2008,	/* record different mode's height of grabwindow */
		/*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 310,
	},
	.custom4 = {		/*data rate 1099.20 Mbps/lane */
		.pclk = 439680000,	/* record different mode's pclk */
		.linelength = 6024,	/* record different mode's linelength */
		.framelength = 2800,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 2672,	/* record different mode's width of grabwindow */
		.grabwindow_height = 2008,	/* record different mode's height of grabwindow */
		/*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 310,
	},
	.custom5 = {		/*data rate 1099.20 Mbps/lane */
		.pclk = 439680000,	/* record different mode's pclk */
		.linelength = 6024,	/* record different mode's linelength */
		.framelength = 2800,	/* record different mode's framelength */
		.startx = 0,	/* record different mode's startx of grabwindow */
		.starty = 0,	/* record different mode's starty of grabwindow */
		.grabwindow_width = 2672,	/* record different mode's width of grabwindow */
		.grabwindow_height = 2008,	/* record different mode's height of grabwindow */
		/*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
		.mipi_data_lp2hs_settle_dc = 85,	/* unit , ns */
		/*     following for GetDefaultFramerateByScenario()    */
		.max_framerate = 310,
	},

#endif
	.margin = 20,		/* sensor framelength & shutter margin */
	.min_shutter = 1,	/* min shutter */
	.max_frame_length = 0xffff,	/* max framelength by sensor register's limitation */
	.ae_shut_delay_frame = 0,	/* shutter delay frame for AE cycle,
					 * 2 frame with ispGain_delay-shut_delay=2-0=2
					 */
	.ae_sensor_gain_delay_frame = 0,	/* sensor gain delay frame for AE cycle,
						 * 2 frame with ispGain_delay-sensor_gain_delay=2-0=2
						 */
	.ae_ispGain_delay_frame = 2,	/* isp gain delay frame for AE cycle */
	.ihdr_support = 0,	/* 1, support; 0,not support */
	.ihdr_le_firstline = 0,	/* 1,le first ; 0, se first */
	.sensor_mode_num = 10,	/* support sensor mode num */

	.cap_delay_frame = 1,	/* enter capture delay frame num */
	.pre_delay_frame = 1,	/* enter preview delay frame num */
	.video_delay_frame = 1,	/* enter video delay frame num */
	.hs_video_delay_frame = 3,	/* enter high speed video  delay frame num */
	.slim_video_delay_frame = 3,	/* enter slim video delay frame num */

	.isp_driving_current = ISP_DRIVING_8MA,	/* mclk driving current */
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,	/* sensor_interface_type */
	.mipi_sensor_type = MIPI_OPHY_NCSI2,	/* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,	/* 0,MIPI_SETTLEDELAY_AUTO;
								 * 1,MIPI_SETTLEDELAY_MANNUAL
								 */
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,	/* sensor output first pixel color */
	.mclk = 24,		/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	.mipi_lane_num = SENSOR_MIPI_4_LANE,	/* mipi lane num */
	.i2c_addr_table = {0x20, 0xff},	/* record sensor support all write id addr, only supprt 4must end with 0xff */
	.i2c_speed = 400,	/* i2c read/write speed */
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,	/* mirrorflip information */
	.sensor_mode = IMGSENSOR_MODE_INIT,	/* IMGSENSOR_MODE enum value,record current sensor mode,such as:
						 * INIT, Preview, Capture, Video,High Speed Video, Slim Video
						 */
	.shutter = 0x3D0,	/* current shutter */
	.gain = 0x100,		/* current gain */
	.dummy_pixel = 0,	/* current dummypixel */
	.dummy_line = 0,	/* current dummyline */
	.current_fps = 300,	/* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
	.autoflicker_en = KAL_FALSE,	/* auto flicker enable: KAL_FALSE for disable auto flicker,
					 * KAL_TRUE for enable auto flicker
					 */
	.test_pattern = KAL_FALSE,	/* test pattern mode or not. KAL_FALSE for in test pattern mode,
					 * KAL_TRUE for normal output
					 */
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,	/* current scenario id */
	.ihdr_mode = 0,		/* sensor need support LE, SE with HDR feature */
	.i2c_write_id = 0x20,	/* record current sensor's i2c write id */
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] = {
	/* { 5984, 4160, 232, 0,  5520, 4160, 2760, 2080,  0000, 0000, 2760, 2080, 0,0, 2760, 2080},  Preview*/
	{5984, 4160, 232, 0, 5520, 4160, 2760, 2080, 0000, 0000, 2760, 2080, 0, 0, 2760, 2080},	/* Preview */
	{5984, 4160, 232, 0, 5520, 4160, 5520, 4160, 0000, 0000, 5520, 4160, 0, 0, 5520, 4160},	/* capture */
	{5984, 4160, 232, 0, 5520, 4160, 2760, 2080, 0000, 0000, 2760, 2080, 0, 0, 2760, 2080},	/* video */
	{5984, 4160, 0, 464, 5984, 3232, 2992, 1616, 420, 2, 2152, 1614, 0, 0, 2152, 1614},	/* hight speed video */
	{5984, 4160, 0, 384, 5984, 3392, 2992, 1696, 0000, 0000, 2992, 1696, 0, 0, 2992, 1696},	/* slim video */
	{5984, 4160, 0, 0, 5984, 4160, 5984, 4160, 0000, 0000, 5984, 4160, 0, 0, 5984, 4160},	/* full size for lsc */
	{5984, 4160, 0, 384, 5984, 3392, 5984, 3392, 0000, 0000, 5984, 3392, 0, 0, 5984, 3392},
#if 0
	{5344, 4016, 0, 0, 5344, 4016, 2672, 2008, 0000, 0000, 2672, 2008, 0, 0, 2672, 2008},
	{5344, 4016, 0, 0, 5344, 4016, 2672, 2008, 0000, 0000, 2672, 2008, 0, 0, 2672, 2008},
	{5344, 4016, 0, 0, 5344, 4016, 2672, 2008, 0000, 0000, 2672, 2008, 0, 0, 2672, 2008},
	{5344, 4016, 0, 0, 5344, 4016, 2672, 2008, 0000, 0000, 2672, 2008, 0, 0, 2672, 2008},
	{5344, 4016, 0, 0, 5344, 4016, 2672, 2008, 0000, 0000, 2672, 2008, 0, 0, 2672, 2008}
#endif
};

/*VC1 for HDR(DT=0X35) , VC2 for PDAF(DT=0X36), unit : 10bit */
static SENSOR_VC_INFO_STRUCT SENSOR_VC_INFO[3] = {
	/* Preview/Video mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x0AC8, 0x0820, 0x00, 0x35, 0x0A00, 0x0001,
		0x00, 0x36, 0x0AC8, 0x0001, 0x03, 0x00, 0x0000, 0x0000
	},

	/* Capture mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1590, 0x1040, 0x00, 0x35, 0x0A00, 0x0001,
		0x00, 0x36, 0x1590, 0x0001, 0x03, 0x00, 0x0000, 0x0000
	},

	/* Custom2 mode setting */
	{
		0x03, 0x0a, 0x00, 0x08, 0x40, 0x00,
		0x00, 0x2b, 0x1760, 0x0D40, 0x00, 0x35, 0x0A00, 0x0001,
		0x00, 0x36, 0x1760, 0x0001, 0x03, 0x00, 0x0000, 0x0000
	}
};

typedef struct {
	MUINT16 DarkLimit_H;
	MUINT16 DarkLimit_L;
	MUINT16 OverExp_Min_H;
	MUINT16 OverExp_Min_L;
	MUINT16 OverExp_Max_H;
	MUINT16 OverExp_Max_L;
} SENSOR_ATR_INFO, *pSENSOR_ATR_INFO;
#if 0
static SENSOR_ATR_INFO sensorATR_Info[4] = {	/* Strength Range Min */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	/* Strength Range Std */
	{0x00, 0x32, 0x00, 0x3c, 0x03, 0xff},
	/* Strength Range Max */
	{0x3f, 0xff, 0x3f, 0xff, 0x3f, 0xff},
	/* Strength Range Custom */
	{0x3F, 0xFF, 0x00, 0x0, 0x3F, 0xFF}
};
#endif



static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;

	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *) &get_byte, 1, imgsensor.i2c_write_id);

	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
	char pu_send_cmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

#define MULTI_WRITE 1

#if MULTI_WRITE
#define I2C_BUFFER_LEN 225
#else
#define I2C_BUFFER_LEN 3

#endif
static kal_uint16 imx300_table_write_cmos_sensor(kal_uint16 *para, kal_uint32 len)
{
	char puSendCmd[I2C_BUFFER_LEN];
	kal_uint32 tosend, IDX;
	kal_uint16 addr = 0, addr_last = 0, data;

	tosend = 0;
	IDX = 0;
	/* kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor */

	while (len > IDX) {
		addr = para[IDX];

		if(tosend + 3 <= I2C_BUFFER_LEN)
		{
			puSendCmd[tosend++] = (char)(addr >> 8);
			puSendCmd[tosend++] = (char)(addr & 0xFF);
			data = para[IDX + 1];
			puSendCmd[tosend++] = (char)(data & 0xFF);
			IDX += 2;
			addr_last = addr;

		}
#if MULTI_WRITE

		if (tosend >= I2C_BUFFER_LEN || IDX == len || addr != addr_last) {
			iBurstWriteReg_multi(puSendCmd, tosend, imgsensor.i2c_write_id, 3,
					     imgsensor_info.i2c_speed);
			tosend = 0;
		}
#else
		iWriteRegI2C(puSendCmd, 3, imgsensor.i2c_write_id);
		tosend = 0;

#endif
	}
	return 0;
}

#if 0
static kal_uint32 imx300_ATR(UINT16 DarkLimit, UINT16 OverExp)
{
	/*
	 * write_cmos_sensor(0x6e50,sensorATR_Info[DarkLimit].DarkLimit_H);
	 * write_cmos_sensor(0x6e51,sensorATR_Info[DarkLimit].DarkLimit_L);
	 * write_cmos_sensor(0x9340,sensorATR_Info[OverExp].OverExp_Min_H);
	 * write_cmos_sensor(0x9341,sensorATR_Info[OverExp].OverExp_Min_L);
	 * write_cmos_sensor(0x9342,sensorATR_Info[OverExp].OverExp_Max_H);
	 * write_cmos_sensor(0x9343,sensorATR_Info[OverExp].OverExp_Max_L);
	 * write_cmos_sensor(0x9706,0x10);
	 * write_cmos_sensor(0x9707,0x03);
	 * write_cmos_sensor(0x9708,0x03);
	 * write_cmos_sensor(0x9e24,0x00);
	 * write_cmos_sensor(0x9e25,0x8c);
	 * write_cmos_sensor(0x9e26,0x00);
	 * write_cmos_sensor(0x9e27,0x94);
	 * write_cmos_sensor(0x9e28,0x00);
	 * write_cmos_sensor(0x9e29,0x96);
	 * LOG_INF("DarkLimit 0x6e50(0x%x), 0x6e51(0x%x)\n",sensorATR_Info[DarkLimit].DarkLimit_H,
	 * sensorATR_Info[DarkLimit].DarkLimit_L);
	 * LOG_INF("OverExpMin 0x9340(0x%x), 0x9341(0x%x)\n",sensorATR_Info[OverExp].OverExp_Min_H,
	 * sensorATR_Info[OverExp].OverExp_Min_L);
	 * LOG_INF("OverExpMin 0x9342(0x%x), 0x9343(0x%x)\n",sensorATR_Info[OverExp].OverExp_Max_H,
	 * sensorATR_Info[OverExp].OverExp_Max_L);
	 */
	return ERROR_NONE;
}
//[BY57/BY61/BY86] ==>
#else
//TC_OUT_NOISE_H, TC_OUT_NOISE_L, TC_OUT_MID_H, TC_OUT_MID_L, TC_OUT_HIGH_H, TC_OUT_HIGH_L
int ATR_parameters[][6] = {
				{-1, -1, -1, -1, -1, -1},
				{0x00, 0x64, 0x08, 0x00, -1, -1},
				{0x00, 0x56, 0x07, 0x00, -1, -1},
				{0x00, 0x40, 0x06, 0x00, -1, -1},
				{0x00, 0x32, 0x04, 0x00, -1, -1},
				{0x00, 0x16, 0x02, 0x00, -1, -1},
				{0x00, 0x08, 0x01, 0x00, -1, -1},
			};
static kal_uint32 imx300_ATR(UINT16 Lv)
{
	int LvIndex = 0;

	if(Lv > 100)
	{
		LvIndex = 1;
	}
	else if(Lv > 75)
	{
		LvIndex = 2;
	}
	else if(Lv > 60)
	{
		LvIndex = 3;
	}
	else if(Lv > 40)
	{
		LvIndex = 4;
	}
	else if(Lv > 20)
	{
		LvIndex = 5;
	}
	else
	{
		LvIndex = 6;
	}

	if(LvIndex > sizeof(ATR_parameters) / sizeof(int) / 6)
	{
		LOG_INF("Invalid LvIndex=%d", LvIndex);
	}
	else
	{
		if(ATR_parameters[LvIndex][0] > 0)
		{
			write_cmos_sensor(0xAA16, ATR_parameters[LvIndex][0] & 0xFF);
		}
		if(ATR_parameters[LvIndex][1] > 0)
		{
			write_cmos_sensor(0xAA17, ATR_parameters[LvIndex][1] & 0xFF);
		}
		if(ATR_parameters[LvIndex][2] > 0)
		{
			write_cmos_sensor(0xAA18, ATR_parameters[LvIndex][2] & 0xFF);
		}
		if(ATR_parameters[LvIndex][3] > 0)
		{
			write_cmos_sensor(0xAA19, ATR_parameters[LvIndex][3] & 0xFF);
		}
		if(ATR_parameters[LvIndex][4] > 0)
		{
			write_cmos_sensor(0xAA1A, ATR_parameters[LvIndex][4] & 0xFF);
		}
		if(ATR_parameters[LvIndex][5] > 0)
		{
			write_cmos_sensor(0xAA1B, ATR_parameters[LvIndex][5] & 0xFF);
		}

		LOG_INF("Lv=%u, LvIndex=%d, TC_OUT_NOISE=0x%02X%02X, TC_OUT_MID=0x%02X%02X, TC_OUT_HIGH=0x%02X%02X", Lv, LvIndex, ATR_parameters[LvIndex][0], ATR_parameters[LvIndex][1], ATR_parameters[LvIndex][2], ATR_parameters[LvIndex][3], ATR_parameters[LvIndex][4], ATR_parameters[LvIndex][5]);
	}
	/*LOG_INF("imx300 ATR read back: TC_OUT_NOISE=0x%02X%02X, TC_OUT_MID=0x%02X%02X, TC_OUT_HIGH=0x%02X%02X",
	*	read_cmos_sensor(0xAA16), read_cmos_sensor(0xAA17),
	*	read_cmos_sensor(0xAA18), read_cmos_sensor(0xAA19),
	*	read_cmos_sensor(0xAA1A), read_cmos_sensor(0xAA1B));
	*/

	return ERROR_NONE;
}
//[BY57/BY61/BY86] <==
#endif
#if 0
static MUINT32 cur_startpos;
static MUINT32 cur_size;
static void imx300_set_pd_focus_area(MUINT32 startpos, MUINT32 size)
{
	UINT16 start_x_pos, start_y_pos, end_x_pos, end_y_pos;
	UINT16 focus_width, focus_height;

	if ((cur_startpos == startpos) && (cur_size == size)) {
		LOG_INF("Not to need update focus area!\n");
		return;
	}

	cur_startpos = startpos;
	cur_size = size;

	start_x_pos = (startpos >> 16) & 0xFFFF;
	start_y_pos = startpos & 0xFFFF;
	focus_width = (size >> 16) & 0xFFFF;
	focus_height = size & 0xFFFF;

	end_x_pos = start_x_pos + focus_width;
	end_y_pos = start_y_pos + focus_height;

	if (imgsensor.pdaf_mode == 1) {
		LOG_INF("GC pre PDAF\n");
		/*PDAF*/
		/*PD_CAL_ENALBE */
		write_cmos_sensor(0x3121, 0x01);
		/*AREA MODE */
		write_cmos_sensor(0x31B0, 0x02);	/* 8x6 output */
		write_cmos_sensor(0x31B4, 0x01);	/* 8x6 output */
		/*PD_OUT_EN=1 */
		write_cmos_sensor(0x3123, 0x01);

		/*Fixed area mode */

		write_cmos_sensor(0x3158, (start_x_pos >> 8) & 0xFF);
		write_cmos_sensor(0x3159, start_x_pos & 0xFF);	/* X start */
		write_cmos_sensor(0x315a, (start_y_pos >> 8) & 0xFF);
		write_cmos_sensor(0x315b, start_y_pos & 0xFF);	/* Y start */
		write_cmos_sensor(0x315c, (end_x_pos >> 8) & 0xFF);
		write_cmos_sensor(0x315d, end_x_pos & 0xFF);	/* X end */
		write_cmos_sensor(0x315e, (end_y_pos >> 8) & 0xFF);
		write_cmos_sensor(0x315f, end_y_pos & 0xFF);	/* Y end */


	}


	LOG_INF("start_x_pos:%d, start_y_pos:%d, focus_width:%d, focus_height:%d, end_x_pos:%d, end_y_pos:%d\n",
		start_x_pos, start_y_pos, focus_width, focus_height, end_x_pos, end_y_pos);
}
#endif
#if 0
static void imx300_apply_SPC(void)
{
	unsigned int start_reg = 0x7c00;
	int i;

	if (read_spc_flag == FALSE) {
		/* read_imx300_SPC(imx300_SPC_data); */
		read_spc_flag = TRUE;
		return;
	}

	for (i = 0; i < 352; i++) {
		write_cmos_sensor(start_reg, imx300_SPC_data[i]);
		/* LOG_INF("SPC[%d]= %x\n", i , imx300_SPC_data[i]); */

		start_reg++;
	}

}
#endif
static void set_dummy(void)
{
	LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);
	/*
	 * you can set dummy by imgsensor.dummy_line and imgsensor.dummy_pixel,
	 * or you can set dummy by imgsensor.frame_length and imgsensor.line_length
	 */
	write_cmos_sensor(0x0104, 0x01);

	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);

	write_cmos_sensor(0x0104, 0x00);
}				/*    set_dummy  */

static kal_uint32 return_lot_id_from_otp(void)
{
	kal_uint16 val = 0;
	int i = 0;

	write_cmos_sensor(0x0a02, 0x1f);
	write_cmos_sensor(0x0a00, 0x01);

	for (i = 0; i < 3; i++) {
		val = read_cmos_sensor(0x0A01);
		if ((val & 0x01) == 0x01)
			break;
		mdelay(3);
	}
	if (i == 3) {
		LOG_INF("read otp fail Err!\n");	/* print log */
		return 0;
	}
	/* LOG_INF("0x0A28 0x%x 0x0A29 0x%x\n",read_cmos_sensor(0x0A28)<<4,read_cmos_sensor(0x0A29)>>4); */
	return ((read_cmos_sensor(0x0A28) << 4) | read_cmos_sensor(0x0A29) >> 4);
}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;
	/* unsigned long flags; */

	LOG_INF("framerate = %d, min framelength should enable %d\n", framerate,
		min_framelength_en);

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
		? frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	/* dummy_line = frame_length - imgsensor.min_frame_length; */
	/* if (dummy_line < 0) */
	/* imgsensor.dummy_line = 0; */
	/* else */
	/* imgsensor.dummy_line = dummy_line; */
	/* imgsensor.frame_length = frame_length + imgsensor.dummy_line; */
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}				/*    set_max_framerate  */



/*************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
//[BY57/BY61/BY86] ==>
#ifdef MTK_ORIGINAL
static void set_shutter(kal_uint16 shutter)
{
	unsigned long flags;
	kal_uint16 realtime_fps = 0;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

	/* write_shutter(shutter); */
	/* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
	/* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

	/* OV Recommend Solution */
	/* if shutter bigger than frame_length, should extend frame length first */
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
		? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			/* Extend frame length */
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0104, 0x00);
		}
	} else {
		/* Extend frame length */
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
	}

	/* Update Shutter */
	write_cmos_sensor(0x0104, 0x01);
	write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x0203, shutter & 0xFF);
	write_cmos_sensor(0x0104, 0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);
}				/*    set_shutter */
#else // #ifdef MTK_ORIGINAL
static void set_shutter(unsigned long long shutter)
{
	int longexposure_times = 0;
	unsigned long long count_shutter = shutter;
	kal_uint16 realtime_fps = 0;

	LOG_INF("set_shutter=%lld\n", shutter);

	while(count_shutter > 0xFFEB) //Max frame length - margin
	{
		count_shutter = count_shutter / 2;
		longexposure_times++;
	}

	if(longexposure_times > 0)
	{
		// Update Shutter
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0202, (count_shutter >> 8) & 0xFF);
		write_cmos_sensor(0x0203, count_shutter & 0xFF);

		write_cmos_sensor(0x0350, 0x01);
		write_cmos_sensor(0x3028, longexposure_times & 0x07);
		//write_cmos_sensor(0x0340, 0xFF);
		//write_cmos_sensor(0x0341, 0xFF);
		write_cmos_sensor(0x0104, 0x00);

		LOG_INF("set_shutter=%lld, longexposure_times=%d\n", shutter, longexposure_times);
	}
	else
	{
		//add for flicker without extend frame length
		if(imgsensor.autoflicker_en)
		{
			realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;

			if(realtime_fps >= 297 && realtime_fps <= 305)
				set_max_framerate(296, 0);
			else if(realtime_fps >= 247 && realtime_fps <= 250)
				set_max_framerate(246, 0);
			else if(realtime_fps >= 147 && realtime_fps <= 150)
				set_max_framerate(146, 0);
		}

		// Update Shutter
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0202, (count_shutter >> 8) & 0xFF);
		write_cmos_sensor(0x0203, count_shutter & 0xFF);

		write_cmos_sensor(0x0350, 0x01);
		write_cmos_sensor(0x3028, 0x00);
		write_cmos_sensor(0x0104, 0x00);

		LOG_INF("set_shutter=%lld, framelength=%d\n", shutter, imgsensor.frame_length);
	}
}				/*    set_shutter */
#endif // #ifdef MTK_ORIGINAL
//[BY57/BY61/BY86] <==






static kal_uint16 gain2reg(const kal_uint16 gain)
{
	kal_uint8 iI;

	LOG_INF("[IMX300MIPI]enter IMX300MIPIGain2Reg function\n");
	for (iI = 0; iI < IMX300MIPI_MaxGainIndex; iI++) {
		if (gain == IMX300MIPI_sensorGainMapping[iI][0])
			return IMX300MIPI_sensorGainMapping[iI][1];
		else if (gain < IMX300MIPI_sensorGainMapping[iI][0])
			break;

	}
	if (gain != IMX300MIPI_sensorGainMapping[iI][0]) {
		LOG_INF("Gain mapping don't correctly:%d %d\n", gain,
			IMX300MIPI_sensorGainMapping[iI][0]);
	}
	LOG_INF("exit IMX300MIPIGain2Reg function\n");
	return IMX300MIPI_sensorGainMapping[iI - 1][1];
}


/*************************************************************************
 * FUNCTION
 *    set_gain
 *
 * DESCRIPTION
 *    This function is to set global gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain, org_gain, enable_6db_mode = 0;

	/* 0x350A[0:1], 0x350B[0:7] AGC real gain */
	/* [0:3] = N meams N /16 X    */
	/* [4:9] = M meams M X         */
	/* Total gain = M + N /16 X   */

	/*  */
	if (gain < BASEGAIN || gain > 64 * BASEGAIN) {
		LOG_INF("Error gain setting");

		if (gain < BASEGAIN)
			gain = BASEGAIN;
		else if (gain > 64 * BASEGAIN)
			gain = 64 * BASEGAIN;
	}

	if (gain > 8 * BASEGAIN) {
		LOG_INF("+6db mode");

		org_gain = gain;
#ifdef ENABLE_DIGITAL_GAIN
		gain = 8 * BASEGAIN;
#else
		gain = gain / 2;
#endif
		enable_6db_mode = 1;
	}

	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	if (enable_6db_mode)
		imgsensor.gain = reg_gain + IMX300MIPI_MaxSensorGain;
	else
		imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);
	if (enable_6db_mode) {
#ifdef ENABLE_DIGITAL_GAIN
		LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
		gain = org_gain / 8;
		write_cmos_sensor(0x0104, 0x01);
//[BY57/BY61/BY86] ==> ARNR for capture mode
		if (isARNRtriggered > 3200 && isARNRtriggered <= 4800)
		{
			LOG_INF("ARNR is enabled and ISO is between 3200 & 4800\n");
			write_cmos_sensor(0x30A2, 0x01);
			write_cmos_sensor(0xA352, 0xFF);
			write_cmos_sensor(0xA353, 0x1F);
			write_cmos_sensor(0xA362, 0xFF);
			write_cmos_sensor(0xA363, 0x1F);
			write_cmos_sensor(0xA372, 0xFF);
			write_cmos_sensor(0xA373, 0x1F);
			write_cmos_sensor(0xA34E, 0x00);
			write_cmos_sensor(0xA34F, 0x13);
			write_cmos_sensor(0xA35E, 0x00);
			write_cmos_sensor(0xA35F, 0x13);
			write_cmos_sensor(0xA36E, 0x00);
			write_cmos_sensor(0xA36F, 0x13);
		}
		else if (isARNRtriggered > 4800 && isARNRtriggered <= 6400)
		{
			LOG_INF("ARNR is enabled and ISO is higher than 4800 but smaller than 6401\n");
			write_cmos_sensor(0x30A2, 0x01);
			write_cmos_sensor(0xA352, 0xFF);
			write_cmos_sensor(0xA353, 0x1F);
			write_cmos_sensor(0xA362, 0xFF);
			write_cmos_sensor(0xA363, 0x1F);
			write_cmos_sensor(0xA372, 0xFF);
			write_cmos_sensor(0xA373, 0x1F);
			write_cmos_sensor(0xA34E, 0x00);
			write_cmos_sensor(0xA34F, 0x20);
			write_cmos_sensor(0xA35E, 0x00);
			write_cmos_sensor(0xA35F, 0x20);
			write_cmos_sensor(0xA36E, 0x00);
			write_cmos_sensor(0xA36F, 0x20);
		}
		else if (isARNRtriggered > 6400 && isARNRtriggered <= 9600)
		{
			LOG_INF("ARNR is enabled and ISO is higher than 6400 but smaller than 9601\n");
			write_cmos_sensor(0x30A2, 0x01);
			write_cmos_sensor(0xA352, 0xFF);
			write_cmos_sensor(0xA353, 0x1F);
			write_cmos_sensor(0xA362, 0xFF);
			write_cmos_sensor(0xA363, 0x1F);
			write_cmos_sensor(0xA372, 0xFF);
			write_cmos_sensor(0xA373, 0x1F);
			write_cmos_sensor(0xA34E, 0x00);
			write_cmos_sensor(0xA34F, 0x30);
			write_cmos_sensor(0xA35E, 0x00);
			write_cmos_sensor(0xA35F, 0x30);
			write_cmos_sensor(0xA36E, 0x00);
			write_cmos_sensor(0xA36F, 0x30);
		}
		else if (isARNRtriggered > 9600 && isARNRtriggered <= 12800)
		{
			LOG_INF("ARNR is enabled and ISO is higher than 9600 but smaller than 12801\n");
			write_cmos_sensor(0x30A2, 0x01);
			write_cmos_sensor(0xA352, 0xFF);
			write_cmos_sensor(0xA353, 0x1F);
			write_cmos_sensor(0xA362, 0xFF);
			write_cmos_sensor(0xA363, 0x1F);
			write_cmos_sensor(0xA372, 0xFF);
			write_cmos_sensor(0xA373, 0x1F);
			write_cmos_sensor(0xA34E, 0x00);
			write_cmos_sensor(0xA34F, 0x40);
			write_cmos_sensor(0xA35E, 0x00);
			write_cmos_sensor(0xA35F, 0x40);
			write_cmos_sensor(0xA36E, 0x00);
			write_cmos_sensor(0xA36F, 0x40);
		}
		else
		{
			LOG_INF("ARNR is disabled\n");
			write_cmos_sensor(0x30A2, 0x00);
		}
//[BY57/BY61/BY86] <== ARNR for capture mode
		write_cmos_sensor(0x3220, 0x00);
		write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
		write_cmos_sensor(0x0205, reg_gain & 0xFF);
		/* digital gain */
		write_cmos_sensor(0x020e, (gain >> 6) & 0xFF);
		write_cmos_sensor(0x020f, ((gain & 0x3F) << 2) & 0xFF);
		write_cmos_sensor(0x0210, (gain >> 6) & 0xFF);
		write_cmos_sensor(0x0211, ((gain & 0x3F) << 2) & 0xFF);
		write_cmos_sensor(0x0212, (gain >> 6) & 0xFF);
		write_cmos_sensor(0x0213, ((gain & 0x3F) << 2) & 0xFF);
		write_cmos_sensor(0x0214, (gain >> 6) & 0xFF);
		write_cmos_sensor(0x0215, ((gain & 0x3F) << 2) & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
		LOG_INF("digital gain = %d , high_gain = %d , low_gain = 0x%x\n ", gain,
			(gain >> 6), ((gain & 0x3F) << 2));
		LOG_INF("digital mode gain = %d , reg_gain = 0x%x\n ", org_gain, imgsensor.gain);
		gain = org_gain;
#else
		LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
		LOG_INF("6db mode gain = %d , reg_gain = 0x%x\n ", org_gain, imgsensor.gain);

		gain = org_gain;
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x3220, 0x01);
		write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
		write_cmos_sensor(0x0205, reg_gain & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
#endif
	} else {
		LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

		write_cmos_sensor(0x0104, 0x01);
//[BY57/BY61/BY86] ==> ARNR for video mode
		if ( (imgsensor.sensor_mode == IMGSENSOR_MODE_VIDEO) && (imgsensor.ihdr_mode == 2) )
		{
			if (isARNRtriggered > 600 && isARNRtriggered <= 1200)
			{
				LOG_INF("ARNR is enabled and ISO is higher than 600 but smaller than 1201\n");
				write_cmos_sensor(0x30A2, 0x01);
				write_cmos_sensor(0xA352, 0xFF);
				write_cmos_sensor(0xA353, 0x1F);
				write_cmos_sensor(0xA362, 0xFF);
				write_cmos_sensor(0xA363, 0x1F);
				write_cmos_sensor(0xA372, 0xFF);
				write_cmos_sensor(0xA373, 0x1F);
				write_cmos_sensor(0xA34E, 0x00);
				write_cmos_sensor(0xA34F, 0x13);
				write_cmos_sensor(0xA35E, 0x00);
				write_cmos_sensor(0xA35F, 0x13);
				write_cmos_sensor(0xA36E, 0x00);
				write_cmos_sensor(0xA36F, 0x13);
				write_cmos_sensor(0x3004, 0x01);//CNR
			}
			else if (isARNRtriggered > 1200 && isARNRtriggered <= 1800)
			{
				LOG_INF("ARNR is enabled and ISO is higher than 1200 but smaller than 1801\n");
				write_cmos_sensor(0x30A2, 0x01);
				write_cmos_sensor(0xA352, 0xFF);
				write_cmos_sensor(0xA353, 0x1F);
				write_cmos_sensor(0xA362, 0xFF);
				write_cmos_sensor(0xA363, 0x1F);
				write_cmos_sensor(0xA372, 0xFF);
				write_cmos_sensor(0xA373, 0x1F);
				write_cmos_sensor(0xA34E, 0x00);
				write_cmos_sensor(0xA34F, 0x20);
				write_cmos_sensor(0xA35E, 0x00);
				write_cmos_sensor(0xA35F, 0x20);
				write_cmos_sensor(0xA36E, 0x00);
				write_cmos_sensor(0xA36F, 0x20);
				write_cmos_sensor(0x3004, 0x01);//CNR
			}
			else if (isARNRtriggered > 1800 && isARNRtriggered <= 2400)
			{
				LOG_INF("ARNR is enabled and ISO is higher than 1800 but smaller than 2401\n");
				write_cmos_sensor(0x30A2, 0x01);
				write_cmos_sensor(0xA352, 0xFF);
				write_cmos_sensor(0xA353, 0x1F);
				write_cmos_sensor(0xA362, 0xFF);
				write_cmos_sensor(0xA363, 0x1F);
				write_cmos_sensor(0xA372, 0xFF);
				write_cmos_sensor(0xA373, 0x1F);
				write_cmos_sensor(0xA34E, 0x00);
				write_cmos_sensor(0xA34F, 0x30);
				write_cmos_sensor(0xA35E, 0x00);
				write_cmos_sensor(0xA35F, 0x30);
				write_cmos_sensor(0xA36E, 0x00);
				write_cmos_sensor(0xA36F, 0x30);
				write_cmos_sensor(0x3004, 0x01);//CNR
			}
			else if (isARNRtriggered > 2400 && isARNRtriggered <= 3200)
			{
				LOG_INF("ARNR is enabled and ISO is higher than 2400 but smaller than 3201\n");
				write_cmos_sensor(0x30A2, 0x01);
				write_cmos_sensor(0xA352, 0xFF);
				write_cmos_sensor(0xA353, 0x1F);
				write_cmos_sensor(0xA362, 0xFF);
				write_cmos_sensor(0xA363, 0x1F);
				write_cmos_sensor(0xA372, 0xFF);
				write_cmos_sensor(0xA373, 0x1F);
				write_cmos_sensor(0xA34E, 0x00);
				write_cmos_sensor(0xA34F, 0x40);
				write_cmos_sensor(0xA35E, 0x00);
				write_cmos_sensor(0xA35F, 0x40);
				write_cmos_sensor(0xA36E, 0x00);
				write_cmos_sensor(0xA36F, 0x40);
				write_cmos_sensor(0x3004, 0x01);//CNR
			}
			else
			{
				LOG_INF("ARNR is disabled\n");
				write_cmos_sensor(0x30A2, 0x00);
				write_cmos_sensor(0x3004, 0x00);//CNR
			}
		}
//[BY57/BY61/BY86] <== ARNR for video mode
		write_cmos_sensor(0x3220, 0x00);
		write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
		write_cmos_sensor(0x0205, reg_gain & 0xFF);
#ifdef ENABLE_DIGITAL_GAIN
		/* 1x digital gain */
		write_cmos_sensor(0x020e, 0x01);
		write_cmos_sensor(0x020f, 0x00);
		write_cmos_sensor(0x0210, 0x01);
		write_cmos_sensor(0x0211, 0x00);
		write_cmos_sensor(0x0212, 0x01);
		write_cmos_sensor(0x0213, 0x00);
		write_cmos_sensor(0x0214, 0x01);
		write_cmos_sensor(0x0215, 0x00);
#endif
		write_cmos_sensor(0x0104, 0x00);
	}

	return gain;
}				/*    set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{

	kal_uint16 realtime_fps = 0;
	kal_uint16 reg_gain, org_gain, enable_6db_mode = 0;

	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = le + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (le < imgsensor_info.min_shutter)
		le = imgsensor_info.min_shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if (realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
//[BY57/BY61/BY86] ==>
		else if(realtime_fps >= 247 && realtime_fps <= 250)
			set_max_framerate(246, 0);
//[BY57/BY61/BY86] <==
		else if (realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0104, 0x00);
		}
	} else {
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
	}
	write_cmos_sensor(0x0104, 0x01);
	/* Long exposure */
	write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor(0x0203, le & 0xFF);
	/* Short exposure */
	write_cmos_sensor(0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor(0x0225, se & 0xFF);
	if (gain > 8 * BASEGAIN) {
		LOG_INF("+6db mode");

		org_gain = gain;
#ifdef ENABLE_DIGITAL_GAIN
		gain = 8 * BASEGAIN;
#else
		gain = gain / 2;
#endif
		enable_6db_mode = 1;
	}
	reg_gain = gain2reg(gain);
	spin_lock(&imgsensor_drv_lock);
	if (enable_6db_mode)
		imgsensor.gain = reg_gain + IMX300MIPI_MaxSensorGain;
	else
		imgsensor.gain = reg_gain;
	spin_unlock(&imgsensor_drv_lock);

	if (enable_6db_mode) {
#ifdef ENABLE_DIGITAL_GAIN
		LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
		gain = org_gain / 8;
		write_cmos_sensor(0x3220, 0x00);
		write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
		write_cmos_sensor(0x0205, reg_gain & 0xFF);
		/* digital gain */
		write_cmos_sensor(0x020e, (gain >> 6) & 0xFF);
		write_cmos_sensor(0x020f, ((gain & 0x3F) << 2) & 0xFF);
		write_cmos_sensor(0x0210, (gain >> 6) & 0xFF);
		write_cmos_sensor(0x0211, ((gain & 0x3F) << 2) & 0xFF);
		write_cmos_sensor(0x0212, (gain >> 6) & 0xFF);
		write_cmos_sensor(0x0213, ((gain & 0x3F) << 2) & 0xFF);
		write_cmos_sensor(0x0214, (gain >> 6) & 0xFF);
		write_cmos_sensor(0x0215, ((gain & 0x3F) << 2) & 0xFF);
		LOG_INF("digital gain = %d , high_gain = %d , low_gain = 0x%x\n ", gain,
			(gain >> 6), ((gain & 0x3F) << 2));
		LOG_INF("digital mode gain = %d , reg_gain = 0x%x\n ", org_gain, imgsensor.gain);
		gain = org_gain;
#else
		LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);
		LOG_INF("6db mode gain = %d , reg_gain = 0x%x\n ", org_gain, imgsensor.gain);

		gain = org_gain;
		write_cmos_sensor(0x3220, 0x01);
		/* Global analog Gain for Long expo */
		write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
		write_cmos_sensor(0x0205, reg_gain & 0xFF);
		/* Global analog Gain for Short expo */
		write_cmos_sensor(0x0216, (reg_gain >> 8) & 0xFF);
		write_cmos_sensor(0x0217, reg_gain & 0xFF);
#endif
	} else {
		LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

		write_cmos_sensor(0x3220, 0x00);
		/* Global analog Gain for Long expo */
		write_cmos_sensor(0x0204, (reg_gain >> 8) & 0xFF);
		write_cmos_sensor(0x0205, reg_gain & 0xFF);
		/* Global analog Gain for Short expo */
		write_cmos_sensor(0x0216, (reg_gain >> 8) & 0xFF);
		write_cmos_sensor(0x0217, reg_gain & 0xFF);
#ifdef ENABLE_DIGITAL_GAIN
		/* 1x digital gain */
		write_cmos_sensor(0x020e, 0x01);
		write_cmos_sensor(0x020f, 0x00);
		write_cmos_sensor(0x0210, 0x01);
		write_cmos_sensor(0x0211, 0x00);
		write_cmos_sensor(0x0212, 0x01);
		write_cmos_sensor(0x0213, 0x00);
		write_cmos_sensor(0x0214, 0x01);
		write_cmos_sensor(0x0215, 0x00);
#endif
	}
	write_cmos_sensor(0x0104, 0x00);

}

//[BY57/BY61/BY86] ==>
static void hdr_write_shutter(kal_uint16 le, kal_uint16 se, kal_uint16 lv)
{
	kal_uint16 realtime_fps = 0;
	kal_uint16 ratio;
	LOG_INF("le:0x%x, se:0x%x\n", le, se);
	spin_lock(&imgsensor_drv_lock);
	if (le > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = le + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
	if (imgsensor.autoflicker_en) {
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296, 0);
		else if(realtime_fps >= 247 && realtime_fps <= 250)
			set_max_framerate(246, 0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146, 0);
		else {
			write_cmos_sensor(0x0104, 0x01);
			write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
			write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
			write_cmos_sensor(0x0104, 0x00);
		}
	} else {
		write_cmos_sensor(0x0104, 0x01);
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		write_cmos_sensor(0x0104, 0x00);
	}
	write_cmos_sensor(0x0104, 0x01);

	/* Long exposure */
	write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
	write_cmos_sensor(0x0203, le & 0xFF);
	/* Short exposure */
	write_cmos_sensor(0x0224, (se >> 8) & 0xFF);
	write_cmos_sensor(0x0225, se & 0xFF);
	write_cmos_sensor(0x0104, 0x00);

	/* Ratio */
	if(se == 0)
		ratio = 2;
	else {
		ratio = (le+ (se >> 1)) / se;
		if(ratio > 16)
			ratio = 16;
		else if (ratio > 14)
			ratio = 16;
		else if (ratio >= 8)
			ratio = 8;
		else if (ratio >= 4)
			ratio = 4;
		else if (ratio >= 2)
			ratio = 2;
		else
			ratio = 1;
	}

	LOG_INF("le:%d, se:%d, ratio:%d\n", le, se, ratio);
	write_cmos_sensor(0x0222, ratio);
	imx300_ATR(lv);
}
//[BY57/BY61/BY86] <==


#if 0
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

	/********************************************************
	 *
	 *   0x3820[2] ISP Vertical flip
	 *   0x3820[1] Sensor Vertical flip
	 *
	 *   0x3821[2] ISP Horizontal mirror
	 *   0x3821[1] Sensor Horizontal mirror
	 *
	 *   ISP and Sensor flip or mirror register bit should be the same!!
	 *
	 ********************************************************/

	switch (image_mirror) {
	case IMAGE_NORMAL:
		write_cmos_sensor(0x0101, 0x00);
		write_cmos_sensor(0x3A27, 0x00);
		write_cmos_sensor(0x3A28, 0x00);
		write_cmos_sensor(0x3A29, 0x01);
		write_cmos_sensor(0x3A2A, 0x00);
		write_cmos_sensor(0x3A2B, 0x00);
		write_cmos_sensor(0x3A2C, 0x00);
		write_cmos_sensor(0x3A2D, 0x01);
		write_cmos_sensor(0x3A2E, 0x01);
		break;
	case IMAGE_H_MIRROR:
		write_cmos_sensor(0x0101, 0x01);
		write_cmos_sensor(0x3A27, 0x01);
		write_cmos_sensor(0x3A28, 0x01);
		write_cmos_sensor(0x3A29, 0x00);
		write_cmos_sensor(0x3A2A, 0x00);
		write_cmos_sensor(0x3A2B, 0x01);
		write_cmos_sensor(0x3A2C, 0x00);
		write_cmos_sensor(0x3A2D, 0x00);
		write_cmos_sensor(0x3A2E, 0x01);
		break;
	case IMAGE_V_MIRROR:
		write_cmos_sensor(0x0101, 0x02);
		write_cmos_sensor(0x3A27, 0x10);
		write_cmos_sensor(0x3A28, 0x10);
		write_cmos_sensor(0x3A29, 0x01);
		write_cmos_sensor(0x3A2A, 0x01);
		write_cmos_sensor(0x3A2B, 0x00);
		write_cmos_sensor(0x3A2C, 0x01);
		write_cmos_sensor(0x3A2D, 0x01);
		write_cmos_sensor(0x3A2E, 0x00);
		break;
	case IMAGE_HV_MIRROR:
		write_cmos_sensor(0x0101, 0x03);
		write_cmos_sensor(0x3A27, 0x11);
		write_cmos_sensor(0x3A28, 0x11);
		write_cmos_sensor(0x3A29, 0x00);
		write_cmos_sensor(0x3A2A, 0x01);
		write_cmos_sensor(0x3A2B, 0x01);
		write_cmos_sensor(0x3A2C, 0x01);
		write_cmos_sensor(0x3A2D, 0x00);
		write_cmos_sensor(0x3A2E, 0x00);
		break;
	default:
		LOG_INF("Error image_mirror setting\n");
	}

}
#endif
/*************************************************************************
 * FUNCTION
 *    night_mode
 *
 * DESCRIPTION
 *    This function night mode of sensor.
 *
 * PARAMETERS
 *    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void night_mode(kal_bool enable)
{
	/*No Need to implement this function*/
}				/*    night_mode    */



static void sensor_init(void)
{
	LOG_INF("E\n");
	imx300_table_write_cmos_sensor(addr_data_pair_init_imx300,
				       sizeof(addr_data_pair_init_imx300) / sizeof(kal_uint16));
	LOG_INF("Exit\n");
}				/*    sensor_init  */

static void preview_setting(void)
{
	LOG_INF("E\n");

	write_cmos_sensor(0x0100, 0x00);
	imx300_table_write_cmos_sensor(addr_data_pair_preview_imx300,
				       sizeof(addr_data_pair_preview_imx300) / sizeof(kal_uint16));
	if (imgsensor.pdaf_mode == 1) {
		imx300_table_write_cmos_sensor(addr_data_pair_preview_imx300_pdaf,
					       sizeof(addr_data_pair_preview_imx300_pdaf) /
					       sizeof(kal_uint16));
	}
	write_cmos_sensor(0x0100, 0x01);

}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n", currefps);
	write_cmos_sensor(0x0100, 0x00);

	imx300_table_write_cmos_sensor(addr_data_pair_capture_imx300,
				       sizeof(addr_data_pair_capture_imx300) / sizeof(kal_uint16));
	if (imgsensor.pdaf_mode == 1) {
		imx300_table_write_cmos_sensor(addr_data_pair_capture_imx300_pdaf,
					       sizeof(addr_data_pair_capture_imx300_pdaf) /
					       sizeof(kal_uint16));
	}

	write_cmos_sensor(0x0100, 0x01);
}


static void normal_video_setting(kal_uint16 currefps)
{

	LOG_INF("E! currefps:%d\n", currefps);
//[BY57/BY61/BY86] ==>
	write_cmos_sensor(0x0100, 0x00);
//[BY57/BY61/BY86] <==
	imx300_table_write_cmos_sensor(addr_data_pair_video_imx300,
				       sizeof(addr_data_pair_video_imx300) / sizeof(kal_uint16));
//[BY57/BY61/BY86] ==>
	if(imgsensor.pdaf_mode == 1)
	{
		imx300_table_write_cmos_sensor(addr_data_pair_video_imx300_pdaf, sizeof(addr_data_pair_video_imx300_pdaf) / sizeof(kal_uint16));
	}
	write_cmos_sensor(0x0100, 0x01);
//[BY57/BY61/BY86] <==
}

static void hs_video_setting(void)
{
	LOG_INF("E\n");
	imx300_table_write_cmos_sensor(addr_data_pair_hs_video_imx300,
				       sizeof(addr_data_pair_hs_video_imx300) / sizeof(kal_uint16));
}

static void slim_video_setting(void)
{
	LOG_INF("E\n");
	imx300_table_write_cmos_sensor(addr_data_pair_slim_video_imx300,
				       sizeof(addr_data_pair_slim_video_imx300) /
				       sizeof(kal_uint16));
}

static void custom1_setting(void)
{
	imx300_table_write_cmos_sensor(addr_data_pair_custom1_imx300,
				       sizeof(addr_data_pair_custom1_imx300) / sizeof(kal_uint16));

}

static void custom2_setting(void)
{
//[BY57/BY61/BY86] ==>
	write_cmos_sensor(0x0100, 0x00);
//[BY57/BY61/BY86] <==
	imx300_table_write_cmos_sensor(addr_data_pair_custom2_imx300,
				       sizeof(addr_data_pair_custom2_imx300) / sizeof(kal_uint16));
//[BY57/BY61/BY86] ==>
	if(imgsensor.pdaf_mode == 1)
	{
		imx300_table_write_cmos_sensor(addr_data_pair_custom2_imx300_pdaf, sizeof(addr_data_pair_custom2_imx300_pdaf) / sizeof(kal_uint16));
	}
	write_cmos_sensor(0x0100, 0x01);
//[BY57/BY61/BY86] <==
}

static void custom3_setting(void)
{
	preview_setting();

}

static void custom4_setting(void)
{
	preview_setting();

}

static void custom5_setting(void)
{
	preview_setting();

}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable)
		write_cmos_sensor(0x0601, 0x02);
	else
		write_cmos_sensor(0x0601, 0x00);

	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

//[BY57/BY61/BY86] ==>
static kal_uint32 get_sensor_temperature(void)
{
    int temp = 0;
    UINT32 temperature;

    /*TEMP_SEN_CTL*/
    write_cmos_sensor(0x0138, 0x01);
    temp = read_cmos_sensor(0x013a);

    if(temp >= 0 && temp < 80)
        temperature = temp;
    else if(temp > 79 && temp < 128)
        temperature = 80;
    else if(temp > 128 && temp < 237)
        temperature = 0;
    else if(temp > 236 && temp < 256)
        temperature = 0;
    else
    {
        temperature = 100;
        LOG_INF("get_sensor_temperature failed\n");
        goto _exit;
    }
    LOG_INF("get_sensor_temperature=%d\n", temperature);

_exit:
    return temperature;
}
//[BY57/BY61/BY86] <==

/*************************************************************************
 * FUNCTION
 *    get_imgsensor_id
 *
 * DESCRIPTION
 *    This function get the sensor ID
 *
 * PARAMETERS
 *    *sensorID : return the sensor ID
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = return_lot_id_from_otp();
			if (*sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, *sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}

	if (*sensor_id != imgsensor_info.sensor_id) {
		/* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	/* imx300_apply_SPC(); */
	return ERROR_NONE;
}


/*************************************************************************
 * FUNCTION
 *    open
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint32 sensor_id = 0;

	LOG_1;
	LOG_2;

	/* sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address */
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = return_lot_id_from_otp();
			if (sensor_id == imgsensor_info.sensor_id) {
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n",
					imgsensor.i2c_write_id, sensor_id);
				break;
			}
			LOG_INF("Read sensor id fail, write id: 0x%x, id: 0x%x\n",
				imgsensor.i2c_write_id, sensor_id);
			retry--;
		} while (retry > 0);
		i++;
		if (sensor_id == imgsensor_info.sensor_id)
			break;
		retry = 2;
	}
	if (imgsensor_info.sensor_id != sensor_id)
		return ERROR_SENSOR_CONNECT_FAIL;
	/* initail sequence write in  */
	sensor_init();

	spin_lock(&imgsensor_drv_lock);

	imgsensor.autoflicker_en = KAL_FALSE;
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
	imgsensor.test_pattern = KAL_FALSE;
	imgsensor.current_fps = imgsensor_info.pre.max_framerate;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}				/*    open  */



/*************************************************************************
 * FUNCTION
 *    close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function */

	return ERROR_NONE;
}				/*    close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *    This function start the sensor preview.
 *
 * PARAMETERS
 *    *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	/* imgsensor.video_mode = KAL_FALSE; */
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	preview_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    preview   */

/*************************************************************************
 * FUNCTION
 *    capture
 *
 * DESCRIPTION
 *    This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
		/* PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M */
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
				imgsensor.current_fps, imgsensor_info.cap.max_framerate / 10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);	/*Full mode */

	return ERROR_NONE;
}				/* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	/* imgsensor.current_fps = 300; */
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	normal_video_setting(imgsensor.current_fps);
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	/* imgsensor.video_mode = KAL_TRUE; */
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */
	return ERROR_NONE;
}				/*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	/* set_mirror_flip(sensor_config_data->SensorImageMirror); */

	return ERROR_NONE;
}				/*    slim_video     */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;

	sensor_resolution->SensorCustom1Width = imgsensor_info.custom1.grabwindow_width;
	sensor_resolution->SensorCustom1Height = imgsensor_info.custom1.grabwindow_height;

	sensor_resolution->SensorCustom2Width = imgsensor_info.custom2.grabwindow_width;
	sensor_resolution->SensorCustom2Height = imgsensor_info.custom2.grabwindow_height;

	sensor_resolution->SensorCustom3Width = imgsensor_info.custom3.grabwindow_width;
	sensor_resolution->SensorCustom3Height = imgsensor_info.custom3.grabwindow_height;

	sensor_resolution->SensorCustom4Width = imgsensor_info.custom4.grabwindow_width;
	sensor_resolution->SensorCustom4Height = imgsensor_info.custom4.grabwindow_height;

	sensor_resolution->SensorCustom5Width = imgsensor_info.custom5.grabwindow_width;
	sensor_resolution->SensorCustom5Height = imgsensor_info.custom5.grabwindow_height;

	return ERROR_NONE;
}				/*    get_resolution    */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);


	/* sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; */ /* not use */
	/* sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; */ /* not use */
	/* imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; */ /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;	/* inverse with datasheet */
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;	/* not use */
	sensor_info->SensorResetActiveHigh = FALSE;	/* not use */
	sensor_info->SensorResetDelayCount = 5;	/* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;	/* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;	/* The frame of setting shutter
										 * default 0 for TG int
										 */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting
												 * sensor gain
												 */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 2;	/*0: NO PDAF, 1: PDAF Raw Data mode, 2:PDAF VC mode */
//[BY57/BY61/BY86] ==>
	sensor_info->HDR_Support = 2; /*0: NO HDR, 1: iHDR, 2:mvHDR, 3:zHDR*/
//[BY57/BY61/BY86] <==

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;	/* not use */
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;	/* not use */
	sensor_info->SensorPixelClockCount = 3;	/* not use */
	sensor_info->SensorDataLatchCount = 2;	/* not use */

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorHightSampling = 0;	/* 0 is default 1x */
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom2.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom3.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom4.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.custom5.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}				/*    get_info  */

static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
	imgsensor.pclk = imgsensor_info.custom1.pclk;
	imgsensor.line_length = imgsensor_info.custom1.linelength;
	imgsensor.frame_length = imgsensor_info.custom1.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom1_setting();
	return ERROR_NONE;
}

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
	imgsensor.pclk = imgsensor_info.custom2.pclk;
	imgsensor.line_length = imgsensor_info.custom2.linelength;
	imgsensor.frame_length = imgsensor_info.custom2.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom2_setting();
	return ERROR_NONE;
}

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
	imgsensor.pclk = imgsensor_info.custom3.pclk;
	imgsensor.line_length = imgsensor_info.custom3.linelength;
	imgsensor.frame_length = imgsensor_info.custom3.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom3_setting();
	return ERROR_NONE;
}

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
	imgsensor.pclk = imgsensor_info.custom4.pclk;
	imgsensor.line_length = imgsensor_info.custom4.linelength;
	imgsensor.frame_length = imgsensor_info.custom4.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom4_setting();
	return ERROR_NONE;
}				/*  Custom4   */


static kal_uint32 Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
	imgsensor.pclk = imgsensor_info.custom5.pclk;
	imgsensor.line_length = imgsensor_info.custom5.linelength;
	imgsensor.frame_length = imgsensor_info.custom5.framelength;
	imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	custom5_setting();
	return ERROR_NONE;
}				/*  Custom5   */

static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d\n", scenario_id);
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);
	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		Custom1(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		Custom2(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		Custom3(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		Custom4(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		Custom5(image_window, sensor_config_data);
		break;
	default:
		LOG_INF("Error ScenarioId setting");
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}				/* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{				/* This Function not used after ROME */
	LOG_INF("framerate = %d\n ", framerate);
	/* SetVideoMode Function should fix framerate */
	if (framerate == 0)
		/* Dynamic frame rate */
		return ERROR_NONE;
	spin_lock(&imgsensor_drv_lock);
	if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 296;
//[BY57/BY61/BY86] ==>
	else if ((framerate == 250) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 246;
//[BY57/BY61/BY86] <==
	else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
		imgsensor.current_fps = 146;
	else
		imgsensor.current_fps = framerate;
	spin_unlock(&imgsensor_drv_lock);
	set_max_framerate(imgsensor.current_fps, 1);

	return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
	LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable)		/* enable auto flicker */
		imgsensor.autoflicker_en = KAL_TRUE;
	else			/* Cancel Auto flick */
		imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id,
						MUINT32 framerate)
{
	kal_uint32 frame_length;

	LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CUSTOM1:
		frame_length = imgsensor_info.custom1.pclk / framerate * 10 /
			imgsensor_info.custom1.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength)
			? (frame_length - imgsensor_info.custom1.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		frame_length = imgsensor_info.custom2.pclk / framerate * 10 /
			imgsensor_info.custom2.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength)
			? (frame_length - imgsensor_info.custom2.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		if (framerate == 0)
			return ERROR_NONE;
		frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
			imgsensor_info.normal_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength)
			? (frame_length - imgsensor_info.normal_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength)
				? (frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		} else {
			if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
				LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
					framerate, imgsensor_info.cap.max_framerate / 10);
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
				? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
		}
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength)
			? (frame_length - imgsensor_info.hs_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		frame_length = imgsensor_info.slim_video.pclk / framerate * 10 /
			imgsensor_info.slim_video.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength)
			? (frame_length - imgsensor_info.slim_video.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		break;
	default:		/* coding with  preview scenario by default */
		frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
		spin_lock(&imgsensor_drv_lock);
		imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
			? (frame_length - imgsensor_info.pre.framelength) : 0;
		imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
		imgsensor.min_frame_length = imgsensor.frame_length;
		spin_unlock(&imgsensor_drv_lock);
		set_dummy();
		LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
		break;
	}
	return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id,
						    MUINT32 *framerate)
{
	LOG_INF("scenario_id = %d\n", scenario_id);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		*framerate = imgsensor_info.pre.max_framerate;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		*framerate = imgsensor_info.normal_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		*framerate = imgsensor_info.cap.max_framerate;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		*framerate = imgsensor_info.hs_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		*framerate = imgsensor_info.slim_video.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM1:
		*framerate = imgsensor_info.custom1.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM2:
		*framerate = imgsensor_info.custom2.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM3:
		*framerate = imgsensor_info.custom3.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM4:
		*framerate = imgsensor_info.custom4.max_framerate;
		break;
	case MSDK_SCENARIO_ID_CUSTOM5:
		*framerate = imgsensor_info.custom5.max_framerate;
		break;
	default:
		break;
	}

	return ERROR_NONE;
}

#if 1
static kal_uint32 imx300_awb_gain(SET_SENSOR_AWB_GAIN *pSetSensorAWB)
{
	UINT32 rgain_32, grgain_32, gbgain_32, bgain_32;

	LOG_INF("imx300_awb_gain\n");

	grgain_32 = (pSetSensorAWB->ABS_GAIN_GR << 8) >> 9;
	rgain_32 = (pSetSensorAWB->ABS_GAIN_R << 8) >> 9;
	bgain_32 = (pSetSensorAWB->ABS_GAIN_B << 8) >> 9;
	gbgain_32 = (pSetSensorAWB->ABS_GAIN_GB << 8) >> 9;

	LOG_INF("[imx300_awb_gain] ABS_GAIN_GR:%d, grgain_32:%d\n", pSetSensorAWB->ABS_GAIN_GR,
		grgain_32);
	LOG_INF("[imx300_awb_gain] ABS_GAIN_R:%d, rgain_32:%d\n", pSetSensorAWB->ABS_GAIN_R,
		rgain_32);
	LOG_INF("[imx300_awb_gain] ABS_GAIN_B:%d, bgain_32:%d\n", pSetSensorAWB->ABS_GAIN_B,
		bgain_32);
	LOG_INF("[imx300_awb_gain] ABS_GAIN_GB:%d, gbgain_32:%d\n", pSetSensorAWB->ABS_GAIN_GB,
		gbgain_32);

	write_cmos_sensor(0x0b8e, (grgain_32 >> 8) & 0xFF);
	write_cmos_sensor(0x0b8f, grgain_32 & 0xFF);
	write_cmos_sensor(0x0b90, (rgain_32 >> 8) & 0xFF);
	write_cmos_sensor(0x0b91, rgain_32 & 0xFF);
	write_cmos_sensor(0x0b92, (bgain_32 >> 8) & 0xFF);
	write_cmos_sensor(0x0b93, bgain_32 & 0xFF);
	write_cmos_sensor(0x0b94, (gbgain_32 >> 8) & 0xFF);
	write_cmos_sensor(0x0b95, gbgain_32 & 0xFF);
	return ERROR_NONE;
}
#endif

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
	UINT16 *feature_data_16 = (UINT16 *) feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
	UINT32 *feature_data_32 = (UINT32 *) feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	/* unsigned long long *feature_return_data = (unsigned long long*)feature_para; */

	SENSOR_WINSIZE_INFO_STRUCT *wininfo;
	SENSOR_VC_INFO_STRUCT *pvcinfo;
//[BY57/BY61/BY86] ==>
	SET_SENSOR_AWB_GAIN *pSetSensorAWB=(SET_SENSOR_AWB_GAIN *)feature_para;
//[BY57/BY61/BY86] <==
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

	LOG_INF("feature_id = %d\n", feature_id);
	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
#ifdef MTK_ORIGINAL
		set_shutter((UINT16) *feature_data);
#else // #ifdef MTK_ORIGINAL
		set_shutter(*feature_data);
#endif // #ifdef MTK_ORIGINAL
		break;
	case SENSOR_FEATURE_SET_NIGHTMODE:
		night_mode((BOOL) *feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16) *feature_data);
		break;
	case SENSOR_FEATURE_SET_FLASHLIGHT:
		break;
	case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
		break;
	case SENSOR_FEATURE_SET_REGISTER:
		write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
		break;
	case SENSOR_FEATURE_GET_REGISTER:
		sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
		break;
	case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
		/* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
		/* if EEPROM does not exist in camera module. */
		*feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_VIDEO_MODE:
		set_video_mode(*feature_data);
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		set_auto_flicker_mode((BOOL) *feature_data_16, *(feature_data_16 + 1));
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM) *feature_data,
					      *(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM) *feature_data,
						  (MUINT32 *) (uintptr_t) (*(feature_data + 1)));
		break;
	case SENSOR_FEATURE_GET_PDAF_DATA:
		{
			LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
			imx300_DCC_conversion((kal_uint16) (*feature_data),
					      (char *)(uintptr_t) (*(feature_data + 1)),
					      (kal_uint32) (*(feature_data + 2)));
			break;
		}
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		set_test_pattern_mode((BOOL) *feature_data);
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:	/* for factory mode auto testing */
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		LOG_INF("current fps :%d\n", (UINT32) *feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32) *feature_data);
		wininfo = (SENSOR_WINSIZE_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));

		switch (*feature_data_32) {
		case MSDK_SCENARIO_ID_CUSTOM1:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[5],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CUSTOM2:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[6],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[1],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[2],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[3],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[4],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy((void *)wininfo, (void *)&imgsensor_winsize_info[0],
			       sizeof(SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
		/*HDR CMD */
//[BY57/BY61/BY86] ==>
		case SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY:
		{
			LOG_INF("SENSOR_FEATURE_GET_SENSOR_HDR_CAPACITY scenarioId:%llu\n", *feature_data);
			//HDR capacity enable or not,  only video mode support HDR
			/*
			SENSOR_VHDR_MODE_NONE  = 0x0,
			SENSOR_VHDR_MODE_IVHDR = 0x01,
			SENSOR_VHDR_MODE_MVHDR = 0x02,
			SENSOR_VHDR_MODE_ZVHDR = 0x09
			*/
			switch (*feature_data) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0x0;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0x02;
					break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0x0;
					break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0x0;
					break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0x0;
					break;
				case MSDK_SCENARIO_ID_CUSTOM2:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0x0;
					break;
				default:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0x0;
					break;
			}
			break;
		}
//[BY57/BY61/BY86] <==
	case SENSOR_FEATURE_SET_HDR:
		LOG_INF("ihdr enable :%d\n", (BOOL) *feature_data);
		spin_lock(&imgsensor_drv_lock);
		imgsensor.ihdr_mode = *feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n", (UINT16) *feature_data,
			(UINT16) *(feature_data + 1), (UINT16) *(feature_data + 2));
		ihdr_write_shutter_gain(*feature_data, *(feature_data + 1), *(feature_data + 2));
		break;
//[BY57/BY61/BY86] ==>
	case SENSOR_FEATURE_SET_HDR_SHUTTER:
		LOG_INF("SENSOR_FEATURE_SET_HDR_SHUTTER LE=%d, SE=%d\n", (UINT16)*feature_data, (UINT16)*(feature_data + 1));
		hdr_write_shutter((UINT16)*feature_data, (UINT16)*(feature_data + 1), (UINT16)*(feature_data + 2));
		break;
//[BY57/BY61/BY86] <==
	case SENSOR_FEATURE_GET_VC_INFO:
		{
			LOG_INF("SENSOR_FEATURE_GET_VC_INFO %d\n", (UINT16) *feature_data);
			pvcinfo = (SENSOR_VC_INFO_STRUCT *) (uintptr_t) (*(feature_data + 1));
			switch (*feature_data_32) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[1],
				       sizeof(SENSOR_VC_INFO_STRUCT));
				break;
//[BY57/BY61/BY86] ==>
//			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			case MSDK_SCENARIO_ID_CUSTOM2:
//[BY57/BY61/BY86] <==
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[2],
				       sizeof(SENSOR_VC_INFO_STRUCT));
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
//[BY57/BY61/BY86] ==>
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
//[BY57/BY61/BY86] <==
			default:
				memcpy((void *)pvcinfo, (void *)&SENSOR_VC_INFO[0],
				       sizeof(SENSOR_VC_INFO_STRUCT));
				break;
			}
			break;
		}
	case SENSOR_FEATURE_SET_AWB_GAIN:
//[BY57/BY61/BY86] ==>
		imx300_awb_gain(pSetSensorAWB);
//[BY57/BY61/BY86] <==
		break;
		/*END OF HDR CMD */
		/*PDAF CMD */
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		{
			LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%llu\n",
				*feature_data);
			/* PDAF capacity enable or not, 2p8 only full size support PDAF */
			switch (*feature_data) {
			case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
				break;
			case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
//[BY57/BY61/BY86] ==>
//				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
//[BY57/BY61/BY86] <==
				break;
			case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
				break;
			case MSDK_SCENARIO_ID_SLIM_VIDEO:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
				break;
			case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
//[BY57/BY61/BY86] ==>
//				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
//[BY57/BY61/BY86] <==
				break;
//[BY57/BY61/BY86] ==>
			case MSDK_SCENARIO_ID_CUSTOM2:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 1;
				break;
//[BY57/BY61/BY86] <==
			default:
				*(MUINT32 *) (uintptr_t) (*(feature_data + 1)) = 0;
				break;
			}
			break;
		}
	case SENSOR_FEATURE_SET_PDAF:
		{
			LOG_INF("PDAF mode :%d\n", *feature_data_16);
			imgsensor.pdaf_mode = *feature_data_16;
			break;
		}
		/*End of PDAF */
		/* case SENSOR_FEATURE_SET_PDFOCUS_AREA: */
		/* LOG_INF("SENSOR_FEATURE_SET_IMX300_PDFOCUS_AREA Start Pos=%d, Size=%d\n",
		 * (UINT32)*feature_data,(UINT32)*(feature_data+1));
		 */
		/* imx300_set_pd_focus_area(*feature_data,*(feature_data+1)); */
		/* break; */
//[BY57/BY61/BY86] ==>
	case SENSOR_FEATURE_GET_TEMPERATURE_VALUE:
		*feature_return_para_32 = get_sensor_temperature();
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_SENSOR_ARNR:
		isARNRtriggered = (int)(*feature_data_16);
		LOG_INF("ARNR ISO:%d\n", isARNRtriggered);
		break;
//[BY57/BY61/BY86] <==
	default:
		break;
	}

	return ERROR_NONE;
}				/*    feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 IMX300_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}				/*    IMX230_MIPI_RAW_SensorInit    */
