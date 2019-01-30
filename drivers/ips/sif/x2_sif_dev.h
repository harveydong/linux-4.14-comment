/***************************************************************************
* COPYRIGHT NOTICE
* Copyright 2016 Horizon Robotics, Inc.
* All rights reserved.
***************************************************************************/
/**
 * @file     sif_dev.h
 * @brief    SIF Device head file
 * @author   tarryzhang (tianyu.zhang@hobot.cc)
 * @date     2017/7/6
 * @version  V1.0
 * @par      Horizon Robotics
 */
#ifndef __X2_SIF_DEV_H__
#define __X2_SIF_DEV_H__

#include "x2/x2_sif.h"

int32_t sif_dev_init(sif_init_t *sif_cfg);
int32_t sif_dev_update(sif_init_t *sif_cfg);
int32_t sif_dev_start(void);
int32_t sif_dev_stop(void);
void    sif_dev_get_status(sif_status_t *status);
void    sif_dev_get_info(sif_info_t *info);
int32_t sif_dev_frame_id_cfg(frame_id_t *cfg);
int32_t sif_dev_frame_id_get(frame_id_info_t *frameid);
int32_t sif_dev_mot_det_cfg(mot_det_t *cfg);

#endif //__X2_SIF_DEV_H__
