/***************************************************************************
* COPYRIGHT NOTICE
* Copyright 2016 Horizon Robotics, Inc.
* All rights reserved.
***************************************************************************/
/**
 * @file     mipi_host.h
 * @brief    MIPI HOST Common define
 * @author   tarryzhang (tianyu.zhang@hobot.cc)
 * @date     2017/7/6
 * @version  V1.0
 * @par      Horizon Robotics
 */
#ifndef __X2_MIPI_DPHY_H__
#define __X2_MIPI_DPHY_H__
#ifdef CONFIG_X2_MIPI_PHY

int32_t mipi_host_dphy_initialize(uint16_t mipiclk, uint16_t lane, uint16_t settle, void __iomem *iomem);
void    mipi_host_dphy_reset(void);

int32_t mipi_dev_dphy_initialize(uint16_t mipiclk, uint16_t lane, uint16_t settle, void __iomem *iomem);
void    mipi_dev_dphy_reset(void);
#endif
#endif /*__X2_MIPI_DPHY_H__*/
