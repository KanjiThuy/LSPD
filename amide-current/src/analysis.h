/* analysis.h
 *
 * Part of amide - Amide's a Medical Image Dataset Examiner
 * Copyright (C) 2001-2004 Andy Loening
 *
 * Author: Andy Loening <loening@alum.mit.edu>
 */

/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2, or (at your option)
  any later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
  02111-1307, USA.
*/

#ifndef __ANALYSIS_H__
#define __ANALYSIS_H__

/* header files that are always needed with this file */
#include "amitk_study.h"

/* defines */

/* typedefs, etc. */

typedef enum {
  ALL_VOXELS,
  HIGHEST_FRACTION_VOXELS,
  VOXELS_NEAR_MAX,
  NUM_CALCULATION_TYPES
} analysis_calculation_t;

typedef struct _analysis_gate_t analysis_gate_t;
typedef struct _analysis_frame_t analysis_frame_t;
typedef struct _analysis_volume_t analysis_volume_t;
typedef struct _analysis_roi_t analysis_roi_t;
typedef struct _analysis_study_t analysis_study_t;

struct _analysis_gate_t {

  /* stats */
  amide_data_t mean;
  amide_data_t median;
  guint voxels;
  amide_real_t fractional_voxels;
  amide_data_t var;
  amide_data_t min;
  amide_data_t max;
  amide_data_t total;

  /* info */
  amide_time_t duration;
  amide_time_t time_midpoint;

  /* internal */
  amide_data_t correction;
  analysis_gate_t * next_gate_analysis;
  guint ref_count;
};

struct _analysis_frame_t {
  analysis_gate_t * gate_analyses;
  guint ref_count;
  analysis_frame_t * next_frame_analysis;
};

struct _analysis_volume_t {
  AmitkDataSet * data_set;
  analysis_frame_t * frame_analyses;
  guint ref_count;
  analysis_volume_t * next_volume_analysis;
};

struct _analysis_roi_t {
  AmitkRoi * roi;
  AmitkStudy * study;
  analysis_calculation_t calculation_type;
  gdouble subfraction;
  gdouble threshold_value;
  analysis_volume_t * volume_analyses;
  guint ref_count;
  analysis_roi_t * next_roi_analysis;
};

/* external functions */
analysis_roi_t * analysis_roi_unref(analysis_roi_t *roi_analysis);

/* note, subfraction is only used for calculation_type == HIGHEST_FRACTION_VOXELS,
   threshold_value is only used for calculation_type == VOXELS_NEAR_MAX */
analysis_roi_t * analysis_roi_init(AmitkStudy * study, GList * rois, 
				   GList * volumes, analysis_calculation_t calculation_type,
				   gdouble subfraction, gdouble threshold_value);

#endif /* __ANALYSIS_H__ */



















