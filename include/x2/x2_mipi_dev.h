/***************************************************************************
* COPYRIGHT NOTICE
* Copyright 2016 Horizon Robotics, Inc.
* All rights reserved.
***************************************************************************/
/**
 * @file     mipi_dev.h
 * @brief    MIPI DEV Common define
 * @author   tarryzhang (tianyu.zhang@hobot.cc)
 * @date     2017/7/6
 * @version  V1.0
 * @par      Horizon Robotics
 */
#ifndef __X2_MIPI_DEV_H__
#define __X2_MIPI_DEV_H__

#include <linux/types.h>

typedef struct _mipi_dev_cfg_t {
	uint16_t lane;
	uint16_t datatype;
	uint16_t fps;
	uint16_t mclk;
	uint16_t mipiclk;
	uint16_t width;
	uint16_t height;
	uint16_t linelenth;
	uint16_t framelenth;
	uint16_t settle;
	uint16_t vpg;
	uint16_t ipi_lines;
} mipi_dev_cfg_t;

#define MIPIDEVIOC_MAGIC 'v'
#define MIPIDEVIOC_INIT             _IOW(MIPIDEVIOC_MAGIC, 0, mipi_dev_cfg_t)
#define MIPIDEVIOC_DEINIT           _IO(MIPIDEVIOC_MAGIC,  1)
#define MIPIDEVIOC_START            _IO(MIPIDEVIOC_MAGIC,  2)
#define MIPIDEVIOC_STOP             _IO(MIPIDEVIOC_MAGIC,  3)

#endif /*__X2_MIPI_DEV_H__*/
