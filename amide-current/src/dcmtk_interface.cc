/* dcmtk_interface.cc
 *
 * Part of amide - Amide's a Medical Image Dataset Examiner
 * Copyright (C) 2005-2007 Andy Loening
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

#include "amide_config.h"
#ifdef AMIDE_LIBDCMDATA_SUPPORT

#ifdef AMIDE_WIN32_HACKS
#include <unistd.h>
#endif
#include "dcmtk_interface.h"
#include <dirent.h>

/* dcmtk redefines a lot of things that they shouldn't... */
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#include <dctk.h>
//#include <dcostrmb.h>
const gchar * dcmtk_version = OFFIS_DCMTK_VERSION;


/* based on dcmftest.cc - part of dcmtk */
gboolean dcmtk_test_dicom(const gchar * filename) {

  FILE * file;
  gchar signature[4];
  gboolean is_dicom;

  g_return_val_if_fail(filename != NULL, FALSE);

  const char *fname = NULL;

  if ((file = fopen(filename, "rb")) == NULL)
    return FALSE;

  if ((fseek(file, DCM_PreambleLen, SEEK_SET) < 0) || 
      (fread(signature, 1, DCM_MagicLen, file) != DCM_MagicLen)) {
    is_dicom = FALSE;
  } else if (strncmp(signature, DCM_Magic, DCM_MagicLen) != 0) {
    is_dicom = FALSE;
  } else {
    is_dicom = TRUE;
  }
  fclose(file);

  return is_dicom;
}


static AmitkDataSet * read_dicom_file(const gchar * filename,
				      AmitkPreferences * preferences,
				      gint *pnum_frames,
				      gint *pnum_gates,
				      AmitkUpdateFunc update_func,
				      gpointer update_data,
				      gchar **perror_buf) {

  DcmFileFormat dcm_format;
  //  DcmMetaInfo * dcm_metainfo;
  DcmDataset * dcm_dataset;
  OFCondition result;
  Uint16 return_uint16=0;
  Sint32 return_sint32=0;
  Float64 return_float64=0.0;
  Uint16 bits_allocated;
  Uint16 pixel_representation;
  const char * return_str=NULL;

  AmitkPoint voxel_size = one_point;
  AmitkDataSet * ds=NULL;
  AmitkModality modality;
  AmitkVoxel dim;
  AmitkVoxel i;
  AmitkFormat format;
  gboolean continue_work=TRUE;
  gboolean found_value;
  const void * buffer=NULL;
  void * ds_pointer;
  long unsigned num_bytes;
  gint format_size;
  AmitkPoint new_offset;
  AmitkAxes new_axes;
  amide_time_t acquisition_start_time=-1.0;
  amide_time_t dose_time=-1.0;
  amide_time_t decay_time=-1.0;
  gdouble decay_value;
  gdouble half_life=-1.0;
  gint hours, minutes, seconds;
  gchar * object_name;

  /* note - dcmtk always uses POSIX locale - look to setlocale stuff in libmdc_interface.c if this ever comes up*/

  result = dcm_format.loadFile(filename);
  if (result.bad()) {
    g_warning("could not read DICOM file %s, dcmtk returned %s",filename, result.text());
    goto error;
  }

  //  dcm_metainfo = dcm_format.getMetaInfo();
  //  if (dcm_metainfo == NULL) {
  //    g_warning("could not find metainfo in DICOM file %s\n", filename);
  //  }

  dcm_dataset = dcm_format.getDataset();
  //  dcm_dataset = &(dcm_dir.getDataset());
  if (dcm_dataset == NULL) {
    g_warning(_("could not find dataset in DICOM file %s\n"), filename);
    goto error;
  }

  modality = AMITK_MODALITY_OTHER;
  if (dcm_dataset->findAndGetString(DCM_Modality, return_str).good()) {
    if (return_str != NULL) {
      
      /* first, process modalities we know we don't read */
      if ((g_strcasecmp(return_str, "PR") == 0)) {
	amitk_append_str_with_newline(perror_buf, _("Modality %s is not understood.  Ignoring File %s"), return_str, filename);
	goto error;
      }
      
      /* now to modalities we think we understand */
      if ((g_strcasecmp(return_str, "CR") == 0) ||
	  (g_strcasecmp(return_str, "CT") == 0) ||
	  (g_strcasecmp(return_str, "DF") == 0) ||
	  (g_strcasecmp(return_str, "DS") == 0) ||
	  (g_strcasecmp(return_str, "DX") == 0) ||
	  (g_strcasecmp(return_str, "MG") == 0) ||
	  (g_strcasecmp(return_str, "PX") == 0) ||
	  (g_strcasecmp(return_str, "RF") == 0) ||
	  (g_strcasecmp(return_str, "RG") == 0) ||
	  (g_strcasecmp(return_str, "XA") == 0) ||
	  (g_strcasecmp(return_str, "XA") == 0))
	modality = AMITK_MODALITY_CT;
      else if ((g_strcasecmp(return_str, "MA") == 0) ||
	       (g_strcasecmp(return_str, "MR") == 0) ||
	       (g_strcasecmp(return_str, "MS") == 0)) 
	modality = AMITK_MODALITY_MRI;
      else if ((g_strcasecmp(return_str, "ST") == 0) ||
	       (g_strcasecmp(return_str, "NM") == 0))
	modality = AMITK_MODALITY_SPECT;
      else if ((g_strcasecmp(return_str, "PT") == 0))
	modality = AMITK_MODALITY_PET;
    }
  }

  /* get basic data */
  if (dcm_dataset->findAndGetUint16(DCM_Columns, return_uint16).bad()) {
    g_warning(_("could not find # of columns - Failed to load file %s\n"), filename);
    goto error;
  }
  dim.x = return_uint16;

  if (dcm_dataset->findAndGetUint16(DCM_Rows, return_uint16).bad()) {
    g_warning(_("could not find # of rows - Failed to load file %s\n"), filename);
    goto error;
  }
  dim.y = return_uint16;

  if (dcm_dataset->findAndGetUint16(DCM_Planes, return_uint16).bad()) 
    dim.z = 1;
  else
    dim.z = return_uint16;

  //  if (dcm_dataset->findAndGetUint16(DCM_NumberOfFrames, return_uint16).bad()) 
  dim.t = 1; /* no support for multiple frames in a single file */

  dim.g = 1; /* no support for multiple gates in a single file */

  /* TimeSlices can also indicate # of bed positions, so only use for number of frames
     if this is explicitly a DYNAMIC data set */
  if (dcm_dataset->findAndGetString(DCM_SeriesType, return_str, OFTrue).good()) 
    if (return_str != NULL)
      if (strstr(return_str, "DYNAMIC") != NULL)
	if (dcm_dataset->findAndGetUint16(DCM_NumberOfTimeSlices, return_uint16).good()) 
	  *pnum_frames = return_uint16;

  if (dcm_dataset->findAndGetString(DCM_SeriesType, return_str, OFTrue).good()) 
    if (return_str != NULL) 
      if (strstr(return_str, "GATED\\IMAGE") != NULL)
	if (dcm_dataset->findAndGetUint16(DCM_NumberOfTimeSlots, return_uint16).good()) 
	  *pnum_gates = return_uint16;

  if (dcm_dataset->findAndGetUint16(DCM_BitsAllocated, return_uint16).bad()) {
    g_warning(_("could not find # of bits allocated - Failed to load file %s\n"), filename);
    goto error;
  }
  bits_allocated = return_uint16;

  if (dcm_dataset->findAndGetUint16(DCM_PixelRepresentation, return_uint16).bad()) {
    g_warning(_("could not find pixel representation - Failed to load file %s\n"), filename);
    goto error;
  }
  pixel_representation = return_uint16;

  switch(bits_allocated) {
  case 8:
    if (pixel_representation) format = AMITK_FORMAT_SBYTE;
    else format = AMITK_FORMAT_UBYTE;
    break;
  case 16:
    if (pixel_representation) format = AMITK_FORMAT_SSHORT;
    else format = AMITK_FORMAT_USHORT;
    break;
  case 32:
    if (pixel_representation) format = AMITK_FORMAT_SINT;
    else format = AMITK_FORMAT_UINT;
    break;
  default:
    g_warning(_("unsupported # of bits allocated (%d) - Failed to load file %s\n"), bits_allocated, filename);
    goto error;
  }

  /* malloc data set */
  ds = amitk_data_set_new_with_data(preferences, modality, format, dim, AMITK_SCALING_TYPE_0D_WITH_INTERCEPT);
  if (ds == NULL) {
    g_warning(_("Couldn't allocate space for the data set structure to hold DCMTK data - Failed to load file %s"), filename);
    goto error;
  }

  /* get the voxel size */
  if (dcm_dataset->findAndGetFloat64(DCM_PixelSpacing, return_float64,0).good()) {
    voxel_size.y = return_float64;
    if (dcm_dataset->findAndGetFloat64(DCM_PixelSpacing, return_float64,1).good())
      voxel_size.x = return_float64;
    else {
      voxel_size.x = voxel_size.y;
    }
  } else {
    amitk_append_str_with_newline(perror_buf, _("Could not find the pixel size, setting to 1 mm for File %s"), filename);
  }

  /* try to figure out the correct z thickness */
  voxel_size.z = -1;
  if (dcm_dataset->findAndGetFloat64(DCM_SpacingBetweenSlices, return_float64).good()) 
    voxel_size.z = return_float64;

  if (voxel_size.z <= 0.0) 
    if (dcm_dataset->findAndGetFloat64(DCM_SliceThickness, return_float64).good()) {
      voxel_size.z = return_float64;
    } else {
      amitk_append_str_with_newline(perror_buf, _("Could not find the slice thickness, setting to 1 mm for File %s"), filename);
      voxel_size.z = 1;
    }

  /* store this number, occasionally used in sorting */
  if (dcm_dataset->findAndGetSint32(DCM_InstanceNumber, return_sint32).good())
    ds->instance_number = return_sint32;

  /* DICOM coordinates are LPH+, meaning that x increases toward patient left, y increases towards
     patient posterior, and z increases toward patient head. AMIDE is LAF+ */
  /* ImageOrientationPatient contains a vector along the row direction, as well as the column direction,
     note though that the column direction in DICOM is in the wrong direction - it's from top to bottom,
     the z axis is off as well */
  new_axes[AMITK_AXIS_Z] = base_axes[AMITK_AXIS_Z];
  if (dcm_dataset->findAndGetFloat64(DCM_ImageOrientationPatient, return_float64, 0).good()) {
    new_axes[AMITK_AXIS_X].x = return_float64;
    if (dcm_dataset->findAndGetFloat64(DCM_ImageOrientationPatient, return_float64, 1).good()) {
      new_axes[AMITK_AXIS_X].y = return_float64;
      if (dcm_dataset->findAndGetFloat64(DCM_ImageOrientationPatient, return_float64, 2).good()) {
	new_axes[AMITK_AXIS_X].z = return_float64;
	if (dcm_dataset->findAndGetFloat64(DCM_ImageOrientationPatient, return_float64, 3).good()) {
	  new_axes[AMITK_AXIS_Y].x = return_float64;
	  if (dcm_dataset->findAndGetFloat64(DCM_ImageOrientationPatient, return_float64, 4).good()) {
	    new_axes[AMITK_AXIS_Y].y = return_float64;
	    if (dcm_dataset->findAndGetFloat64(DCM_ImageOrientationPatient, return_float64, 5).good()) {
	      new_axes[AMITK_AXIS_Y].z = return_float64;
	      
	      new_axes[AMITK_AXIS_X].y *= -1.0;
	      new_axes[AMITK_AXIS_X].z *= -1.0;
	      new_axes[AMITK_AXIS_Y].y *= -1.0;
	      new_axes[AMITK_AXIS_Y].z *= -1.0;
	      POINT_CROSS_PRODUCT(new_axes[AMITK_AXIS_X], new_axes[AMITK_AXIS_Y], new_axes[AMITK_AXIS_Z]);
	      amitk_space_set_axes(AMITK_SPACE(ds), new_axes, zero_point);
	    }
	  }
	}
      }
    }
  }

  /* note, ImagePositionPatient is defined as the center of the upper left hand voxel 
     in Patient space, not necessarily with respect to the Gantry */
  found_value=FALSE;
  if (dcm_dataset->findAndGetFloat64(DCM_ImagePositionPatient, return_float64, 0).good()) {
    new_offset.x = return_float64;
    if (dcm_dataset->findAndGetFloat64(DCM_ImagePositionPatient, return_float64, 1).good()) {
      new_offset.y = return_float64;
      if (dcm_dataset->findAndGetFloat64(DCM_ImagePositionPatient, return_float64, 2).good()) {
      	new_offset.z = return_float64;
	// not doing the half voxel correction... values seem more correct without, at least for GE 
	//      new_offset.x -= 0.5*voxel_size.x;
	//      new_offset.y -= 0.5*voxel_size.y;
	// new_offset.z -= 0.5*voxel_size.z;

	new_offset.y = -1.0*new_offset.y; /* DICOM specifies y axis in wrong direction */
	new_offset.z = -1.0*new_offset.z; /* DICOM specifies z axis in wrong direction */

	//	amitk_space_set_offset(AMITK_SPACE(ds), amitk_space_s2b(AMITK_SPACE(ds), new_offset));
	amitk_space_set_offset(AMITK_SPACE(ds), new_offset);
	found_value=TRUE;
      }
    }
  }
  if (!found_value) { /* see if we can get it otherwise */
    if (dcm_dataset->findAndGetFloat64(DCM_SliceLocation, return_float64).good()) {
      /* if no ImagePositionPatient, try using SliceLocation */
      new_offset.x = new_offset.y = 0.0;
      new_offset.z = -1.0*(return_float64+voxel_size.z*dim.z); /* DICOM specifies z axis in wrong direction */
      amitk_space_set_offset(AMITK_SPACE(ds), new_offset);
    }
  }


  amitk_data_set_set_voxel_size(ds, voxel_size);
  amitk_data_set_calc_far_corner(ds);

  if (dcm_dataset->findAndGetString(DCM_PatientID, return_str, OFTrue).good()) 
    amitk_data_set_set_subject_id(ds, return_str);

  if (dcm_dataset->findAndGetFloat64(DCM_PatientsWeight, return_float64).good()) {
    amitk_data_set_set_subject_weight(ds, return_float64);
    amitk_data_set_set_displayed_weight_unit(ds, AMITK_WEIGHT_UNIT_KILOGRAM);
  }

  /* store the gate time if possible */
  if (dcm_dataset->findAndGetFloat64(DCM_TriggerTime, return_float64).good()) 
    ds->gate_time = return_float64;
  else
    ds->gate_time = 0.0;

  /* find the most relevant start time */
  return_str = NULL;
  if (!dcm_dataset->findAndGetString(DCM_AcquisitionTime, return_str, OFTrue).good()) 
    if (!dcm_dataset->findAndGetString(DCM_SeriesTime, return_str, OFTrue).good()) 
      dcm_dataset->findAndGetString(DCM_StudyTime, return_str, OFTrue);
  if (return_str != NULL) {
    if (sscanf(return_str, "%2d%2d%2d\n", &hours, &minutes, &seconds)==3) {
      acquisition_start_time = ((hours*60)+minutes)*60+seconds;
    }
  }


  if (dcm_dataset->findAndGetString(DCM_RadiopharmaceuticalStartTime, return_str, OFTrue).good()) 
    if (return_str != NULL) 
      if (sscanf(return_str, "%2d%2d%2d\n", &hours, &minutes, &seconds)==3) 
	dose_time = ((hours*60)+minutes)*60+seconds;

  if ((dose_time >= 0.0) && (acquisition_start_time >= 0.0)) {
    /* correct for if the dose was measured the previous day... note, DICOM can't seem to handle
       dose measured more than 24hours in the past */
    if (acquisition_start_time > dose_time)
      decay_time = acquisition_start_time-dose_time;
    else
      decay_time = acquisition_start_time+24*60*60-dose_time; 
  }

  if (dcm_dataset->findAndGetFloat64(DCM_RadionuclideHalfLife, return_float64, OFTrue).good()) 
    half_life = return_float64;

  if (dcm_dataset->findAndGetFloat64(DCM_RadionuclideTotalDose, return_float64, OFTrue).good()) {
    /* note, for NM data sets, total dose is in MBq. For PET data sets total dose is
       suppose to be in Bq.  However, it seems that the value is sometimes in the 
       wrong units, at least for GE data. */
    decay_value = return_float64;
    if (decay_value > 10000.0) decay_value /= 1E6; /* if more than 10000, assume its in Bq, and convert */
    if ((decay_time > 0.0) && (half_life > 0.0))
      decay_value *= pow(0.5, decay_time/half_life);
    amitk_data_set_set_injected_dose(ds, decay_value);
    amitk_data_set_set_displayed_dose_unit(ds, AMITK_DOSE_UNIT_MEGABECQUEREL);
  }

  if (dcm_dataset->findAndGetString(DCM_Units, return_str, OFTrue).good()) {
    if (return_str != NULL) {
      /* BQML - voxel values are in Bq per ml */
      if (strcmp(return_str, "BQML")==0) {
	amitk_data_set_set_cylinder_factor(ds,1.0/1.0E6);
	amitk_data_set_set_displayed_cylinder_unit(ds, AMITK_CYLINDER_UNIT_MEGABECQUEREL_PER_CC_IMAGE_UNIT);
      }
    }
  }

  if (dcm_dataset->findAndGetString(DCM_StudyDescription, return_str, OFTrue).good()) {
    amitk_object_set_name(AMITK_OBJECT(ds), return_str);
  }

  if (dcm_dataset->findAndGetString(DCM_SeriesDescription, return_str, OFTrue).good()) {
    if (return_str != NULL) {
      if (AMITK_OBJECT_NAME(ds) != NULL)
	object_name = g_strdup_printf("%s - %s", AMITK_OBJECT_NAME(ds), return_str);
      else
	object_name = g_strdup(return_str);
      amitk_object_set_name(AMITK_OBJECT(ds), object_name);
      g_free(object_name);
    }
  }

  if (dcm_dataset->findAndGetString(DCM_PatientsName, return_str, OFTrue).good()) {
      if (AMITK_OBJECT_NAME(ds) == NULL)
	amitk_object_set_name(AMITK_OBJECT(ds), return_str);
      amitk_data_set_set_subject_name(ds, return_str);
  }

  if (dcm_dataset->findAndGetString(DCM_StudyDate, return_str, OFTrue).good()) 
    amitk_data_set_set_scan_date(ds, return_str);
  else if (dcm_dataset->findAndGetString(DCM_SeriesDate, return_str, OFTrue).good()) 
    amitk_data_set_set_scan_date(ds, return_str);
  else if (dcm_dataset->findAndGetString(DCM_AcquisitionDate, return_str, OFTrue).good()) 
    amitk_data_set_set_scan_date(ds, return_str);

  if (dcm_dataset->findAndGetString(DCM_PatientsBirthDate, return_str, OFTrue).good()) 
    amitk_data_set_set_subject_dob(ds, return_str);

  /* because of how the dicom coordinates are setup, after reading in the patient slices, 
     they should all be oriented as SUPINE_HEADFIRST in AMIDE */
  amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_SUPINE_HEADFIRST);
  //  if (dcm_dataset->findAndGetString(DCM_PatientPosition, return_str, OFTrue).good()) {
  //    if (return_str != NULL) {
  //      if (g_strcasecmp(return_str, "HFS")==0)
  //	amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_SUPINE_HEADFIRST);
  //      else if (g_strcasecmp(return_str, "FFS")==0)
  //	amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_SUPINE_FEETFIRST);
  //      else if (g_strcasecmp(return_str, "HFP")==0)
  //	amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_PRONE_HEADFIRST);
  //      else if (g_strcasecmp(return_str, "FFP")==0)
  //	amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_PRONE_FEETFIRST);
  //      else if (g_strcasecmp(return_str, "HFDR")==0)
  //	amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_RIGHT_DECUBITUS_HEADFIRST);
  //      else if (g_strcasecmp(return_str, "FFDR")==0)
  //	amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_RIGHT_DECUBITUS_FEETFIRST);
  //      else if (g_strcasecmp(return_str, "HFDL")==0)
  //	amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_LEFT_DECUBITUS_HEADFIRST);
  //      else if (g_strcasecmp(return_str, "FFDL")==0)
  //	amitk_data_set_set_subject_orientation(ds, AMITK_SUBJECT_ORIENTATION_LEFT_DECUBITUS_FEETFIRST);
  //    }
  //  }

  format_size = amitk_format_sizes[format];
  num_bytes = amitk_raw_format_calc_num_bytes(dim, amitk_format_to_raw_format(format));

  /* a "GetSint16Array" function is also provided, but for some reason I get an error
     when using it.  I'll just use GetUint16Array even for signed stuff */
  switch(format) {
  case AMITK_FORMAT_SBYTE:
  case AMITK_FORMAT_UBYTE:
    {
      const Uint8 * temp_buffer;
      result = dcm_dataset->findAndGetUint8Array(DCM_PixelData, temp_buffer);
      buffer = (void *) temp_buffer;
      break;
    }
  case AMITK_FORMAT_SSHORT:
  case AMITK_FORMAT_USHORT:
    {
      const Uint16 * temp_buffer;
      result = dcm_dataset->findAndGetUint16Array(DCM_PixelData, temp_buffer);
      buffer = (void *) temp_buffer;
      break;
    }
  case AMITK_FORMAT_SINT:
  case AMITK_FORMAT_UINT:
    {
      const Uint32 * temp_buffer;
      result = dcm_dataset->findAndGetUint32Array(DCM_PixelData, temp_buffer);
      buffer = (void *) temp_buffer;
      break;
    }
  default:
    g_warning("unsupported data format in %s at %d\n", __FILE__, __LINE__);
    goto error;
    break;
  }

  if (result.bad()) {
    g_warning(_("error reading in pixel data -  DCMTK error: %s - Failed to read file"),result.text(), filename);
    goto error;
  }

  i = zero_voxel;

  /* store the scaling factor... if there is one */
  if (dcm_dataset->findAndGetFloat64(DCM_RescaleSlope, return_float64,0).good()) 
    *AMITK_RAW_DATA_DOUBLE_2D_SCALING_POINTER(ds->internal_scaling_factor, i) = return_float64;

  /* same for the offset */
  if (dcm_dataset->findAndGetFloat64(DCM_RescaleIntercept, return_float64,0).good()) 
    *AMITK_RAW_DATA_DOUBLE_2D_SCALING_POINTER(ds->internal_scaling_intercept, i) = return_float64;

  if (dcm_dataset->findAndGetFloat64(DCM_FrameReferenceTime, return_float64).good()) 
    amitk_data_set_set_scan_start(ds,return_float64/1000.0);

  /* and load in the data - plane by plane */
  for (i.t = 0; (i.t < dim.t) && (continue_work); i.t++) {

    /* note ... doesn't seem to be a way to encode different frame durations within one dicom file */
    if (dcm_dataset->findAndGetSint32(DCM_ActualFrameDuration, return_sint32).good()) {
      amitk_data_set_set_frame_duration(ds,i.t, ((gdouble) return_sint32)/1000.0);
      /* make sure it's not zero */
      if (amitk_data_set_get_frame_duration(ds,i.t) < EPSILON) 
	amitk_data_set_set_frame_duration(ds,i.t, EPSILON);
    }

    for (i.g = 0; (i.g < ds->raw_data->dim.g) && (continue_work); i.g++) {

      /* dicom stores the slices in the opposite sequence from what we expect */
      for (i.z = 0 ; (i.z < ds->raw_data->dim.z) && (continue_work); i.z++) {

	/* transfer over the buffer */
	for (i.y = 0; i.y < ds->raw_data->dim.y; i.y++) {
	  ds_pointer = amitk_raw_data_get_pointer(AMITK_DATA_SET_RAW_DATA(ds), i);
	  /* note, we've already flipped the coordinate axis, so reading in the data straight is correct */
	  /* memcpy(ds_pointer, (guchar *) buffer + format_size*ds->raw_data->dim.x*(ds->raw_data->dim.y-i.y-1), format_size*ds->raw_data->dim.x); */
	  memcpy(ds_pointer, (guchar *) buffer + format_size*ds->raw_data->dim.x*i.y, format_size*ds->raw_data->dim.x);
	}
      }
    }
  }


  amitk_data_set_set_scale_factor(ds, 1.0); /* set the external scaling factor */
  amitk_data_set_calc_far_corner(ds); /* set the far corner of the volume */
  amitk_data_set_calc_max_min(ds, update_func, update_data);

  goto function_end;


 error:

  if (ds != NULL) {
    ds = AMITK_DATA_SET(amitk_object_unref(ds));
    ds = NULL;
  }


 function_end:
  return ds;
}

void transfer_slice(AmitkDataSet * ds, AmitkDataSet * slice_ds, AmitkVoxel i) {

  size_t num_bytes_per_slice;
  void * ds_pointer;
  void * slice_pointer;

  num_bytes_per_slice = amitk_raw_format_calc_num_bytes_per_slice(AMITK_DATA_SET_DIM(ds), AMITK_DATA_SET_FORMAT(ds));

  ds_pointer = amitk_raw_data_get_pointer(AMITK_DATA_SET_RAW_DATA(ds), i);
  slice_pointer = amitk_raw_data_get_pointer(AMITK_DATA_SET_RAW_DATA(slice_ds), zero_voxel);
  memcpy(ds_pointer, slice_pointer, num_bytes_per_slice);
	  
  /* copy the scaling factors */
  *AMITK_RAW_DATA_DOUBLE_2D_SCALING_POINTER(ds->internal_scaling_factor, i) = 
    amitk_data_set_get_internal_scaling_factor(slice_ds, zero_voxel);
  *AMITK_RAW_DATA_DOUBLE_2D_SCALING_POINTER(ds->internal_scaling_intercept, i) = 
    amitk_data_set_get_internal_scaling_intercept(slice_ds, zero_voxel);

  return;
}

/* sort by location */
static gint sort_slices_func(gconstpointer a, gconstpointer b) {
  AmitkDataSet * slice_a = (AmitkDataSet *) a;
  AmitkDataSet * slice_b = (AmitkDataSet *) b;
  AmitkPoint offset_b;

  g_return_val_if_fail(AMITK_IS_DATA_SET(slice_a), -1);
  g_return_val_if_fail(AMITK_IS_DATA_SET(slice_b), 1);

  offset_b = amitk_space_b2s(AMITK_SPACE(slice_a), AMITK_SPACE_OFFSET(slice_b));

  if (offset_b.z > 0.0)
    return -1;
  else if (offset_b.z < 0.0) 
    return 1;
  else
    return 0;
}

/* sort by time, then by location */
static gint sort_slices_func_with_time(gconstpointer a, gconstpointer b) {
  AmitkDataSet * slice_a = (AmitkDataSet *) a;
  AmitkDataSet * slice_b = (AmitkDataSet *) b;
  amide_time_t scan_start_a, scan_start_b;

  g_return_val_if_fail(AMITK_IS_DATA_SET(slice_a), -1);
  g_return_val_if_fail(AMITK_IS_DATA_SET(slice_b), 1);

  /* first sort by time */
  scan_start_a = AMITK_DATA_SET_SCAN_START(slice_a);
  scan_start_b = AMITK_DATA_SET_SCAN_START(slice_b);
  if (!REAL_EQUAL(scan_start_a, scan_start_b)) {
    if (scan_start_a < scan_start_b)
      return -1;
    else
      return 1;
  }

  /* then sort by location */
  return sort_slices_func(a,b);
}

/* sort by gate, then by location */
static gint sort_slices_func_with_gate(gconstpointer a, gconstpointer b) {
  AmitkDataSet * slice_a = (AmitkDataSet *) a;
  AmitkDataSet * slice_b = (AmitkDataSet *) b;
  gdouble gate_a, gate_b;

  g_return_val_if_fail(AMITK_IS_DATA_SET(slice_a), -1);
  g_return_val_if_fail(AMITK_IS_DATA_SET(slice_b), 1);

  /* first sort by time */
  gate_a = AMITK_DATA_SET(slice_a)->gate_time;
  gate_b = AMITK_DATA_SET(slice_b)->gate_time;
  if (!REAL_EQUAL(gate_a, gate_b)) {
    if (gate_a < gate_b)
      return -1;
    else
      return 1;
  }

  /* then sort by location */
  return sort_slices_func(a,b);
}

static GList * check_slices(GList * slices) {

  GList * current_slice;
  AmitkDataSet * slice;
  AmitkVoxel dim;
  AmitkPoint voxel_size;
  GList * discard_slices=NULL;

  current_slice = g_list_first(slices);
  slice = AMITK_DATA_SET(current_slice->data);

  dim = AMITK_DATA_SET_DIM(slice);
  voxel_size = AMITK_DATA_SET_VOXEL_SIZE(slice);

  current_slice = current_slice->next;
  while (current_slice != NULL) {
    slice = AMITK_DATA_SET(current_slice->data);
    if ((!VOXEL_EQUAL(AMITK_DATA_SET_DIM(slice), dim)) || 
	(!POINT_EQUAL(AMITK_DATA_SET_VOXEL_SIZE(slice), voxel_size)))
      discard_slices = g_list_append(discard_slices, slice);
    current_slice = current_slice->next;
  }

  while (discard_slices != NULL) {
    slice = AMITK_DATA_SET(discard_slices->data);
    slices = g_list_remove(slices, slice);
    discard_slices = g_list_remove(discard_slices, slice);
    amitk_object_unref(slice);
  }

  return slices;
}

static void separate_duplicate_slices(GList ** pslices, GList ** pdiscard_slices) {

  GList * current_slices;
  GList * discard_slices=NULL;
  AmitkDataSet * previous_slice;
  AmitkDataSet * slice;
  AmitkDataSet * discard_slice;
  gint discard=0;
  AmitkPoint offset1;
  AmitkPoint offset2;

  g_return_if_fail(pslices != NULL);
  g_return_if_fail(pdiscard_slices != NULL);

  current_slices = g_list_first(*pslices);
  previous_slice = AMITK_DATA_SET(current_slices->data);

  /* get a list of the duplicates */
  current_slices = current_slices->next;
  while (current_slices != NULL) {
    slice = AMITK_DATA_SET(current_slices->data);


    offset1 = AMITK_SPACE_OFFSET(previous_slice);
    offset2 = AMITK_SPACE_OFFSET(slice);
    if (POINT_EQUAL(offset1, offset2)) {
      if (slice->instance_number < previous_slice->instance_number) 
	discard_slice = previous_slice;	
      else
	discard_slice = slice;

      *pdiscard_slices = g_list_append(*pdiscard_slices, discard_slice);
    } 

    previous_slice = slice;
    current_slices = current_slices->next;
  }

  current_slices = *pdiscard_slices;
  while (current_slices != NULL) {
    *pslices = g_list_remove(*pslices, current_slices->data);
    current_slices = current_slices->next;
  }

  return;
}

static AmitkDataSet * import_slices_as_dataset(GList * slices, 
					       gint num_frames, 
					       gint num_gates,
					       AmitkUpdateFunc update_func,
					       gpointer update_data,
					       gchar **perror_buf) {

  AmitkDataSet * ds=NULL;
  guint num_images;
  gint image;
  AmitkDataSet * slice_ds=NULL;
  AmitkVoxel dim, scaling_dim;
  div_t x;
  AmitkVoxel i;
  amide_time_t last_scan_start;
  AmitkPoint offset, initial_offset, diff;
  gboolean screwed_up_timing;
  gboolean screwed_up_thickness;
  amide_real_t true_thickness, old_thickness;
  AmitkPoint voxel_size;

  g_return_val_if_fail(slices != NULL, NULL);

  screwed_up_timing=FALSE;
  screwed_up_thickness=FALSE;
  image=0;
  
  num_images = g_list_length(slices);

  while(image < num_images) {
    slice_ds = (AmitkDataSet *) g_list_nth_data(slices,image);

    /* special stuff for 1st slice */
    if (image==0) {
      
      dim = AMITK_DATA_SET_DIM(slice_ds);
      dim.z = num_images;
      if (num_frames > 1) {
	x = div(num_images, num_frames);
	if (x.rem != 0) {
 	  amitk_append_str_with_newline(perror_buf, _("Cannot evenly divide the number of slices by the number of frames for data set %s - ignoring dynamic data"), AMITK_OBJECT_NAME(slice_ds));
	  dim.t = num_frames=1;
	} else {
	  dim.t = num_frames;
	  dim.z = x.quot;
	}
      } else if (num_gates > 1) {
	x = div(num_images, num_gates);
	if (x.rem != 0) {
 	  amitk_append_str_with_newline(perror_buf, _("Cannot evenly divide the number of slices by the number of gates for data set %s - ignoring gated data"), AMITK_OBJECT_NAME(slice_ds));
	  dim.g = num_gates=1;
	} else {
	  dim.g = num_gates;
	  dim.z = x.quot;
	}
      }


      ds = AMITK_DATA_SET(amitk_object_copy(AMITK_OBJECT(slice_ds)));
      
      /* unref and remalloc what we need */
      if (ds->raw_data != NULL) g_object_unref(ds->raw_data);
      ds->raw_data = amitk_raw_data_new_with_data(AMITK_DATA_SET_FORMAT(slice_ds), dim);
      if (ds->distribution != NULL) g_object_unref(ds->distribution);
      ds->distribution = NULL;
      
      ds->scaling_type = AMITK_SCALING_TYPE_2D_WITH_INTERCEPT;
      scaling_dim=one_voxel;
      scaling_dim.t = dim.t;
      scaling_dim.g = dim.g;
      scaling_dim.z = dim.z;

      if (ds->internal_scaling_factor != NULL) g_object_unref(ds->internal_scaling_factor);
      ds->internal_scaling_factor = amitk_raw_data_new_with_data(AMITK_FORMAT_DOUBLE, scaling_dim);
      if (ds->internal_scaling_intercept != NULL) g_object_unref(ds->internal_scaling_intercept);
      ds->internal_scaling_intercept = amitk_raw_data_new_with_data(AMITK_FORMAT_DOUBLE, scaling_dim);
      if (ds->current_scaling_factor != NULL) g_object_unref(ds->current_scaling_factor);
      ds->current_scaling_factor=NULL;
      
      if (ds->frame_max != NULL)
	g_free(ds->frame_max);
      ds->frame_max = amitk_data_set_get_frame_max_min_mem(ds);
      if (ds->frame_min != NULL)
	g_free(ds->frame_min);
      ds->frame_min = amitk_data_set_get_frame_max_min_mem(ds);
      
      
      if (ds->frame_duration != NULL)
	g_free(ds->frame_duration);
      if ((ds->frame_duration = amitk_data_set_get_frame_duration_mem(ds)) == NULL) {
	g_warning(_("couldn't allocate space for the frame duration info"));
	goto error;
      }
      
      last_scan_start = AMITK_DATA_SET_SCAN_START(slice_ds);
      initial_offset = AMITK_SPACE_OFFSET(slice_ds);
    }

    x = div(image, dim.z);
    i=zero_voxel;
    if (num_gates > 1)
      i.g = x.quot;
    else
      i.t = x.quot;
    i.z = x.rem;
    transfer_slice(ds, slice_ds, i);

    /* record frame duration if needed */
    if (i.z == 0)
      amitk_data_set_set_frame_duration(ds, i.t, amitk_data_set_get_frame_duration(slice_ds, 0));


    if (image != 0) {
	  
      /* figure out which direction the slices are going */
      if (image == 1) {
	
	offset = AMITK_SPACE_OFFSET(slice_ds);

	POINT_SUB(offset, initial_offset, diff);
	true_thickness = POINT_MAGNITUDE(diff);
	old_thickness = AMITK_DATA_SET_VOXEL_SIZE_Z(ds);
	if (true_thickness > EPSILON)

	  /* correct for differences in thickness */
	  if (!REAL_EQUAL(true_thickness, old_thickness)) {
	    voxel_size = AMITK_DATA_SET_VOXEL_SIZE(ds);
	    voxel_size.z = true_thickness;
	    amitk_data_set_set_voxel_size(ds, voxel_size);

	    /* whether to bother complaining */
	    if (!REAL_CLOSE(true_thickness, old_thickness))
	      screwed_up_thickness = TRUE;
	  }
      }

      
      //	  if (image != 0) {
      
      /* check if we've crossed a frame boundary */
      //	    if (!REAL_EQUAL(last_scan_start, AMITK_DATA_SET_SCAN_START(slice_ds))) {
      
      //	      if (is_dynamic) {
      //		num_frames++;
      //	    if (num_frames > 1) {
      
      //	      if (!REAL_EQUAL(AMITK_DATA_SET_SCAN_START(slice_ds)-last_scan_start, duration)) 
      //		screwed_up_timing=TRUE;
      
      //		duration = amitk_data_set_get_frame_duration(slice_ds, 0);
      //		g_array_append_val(durations, duration);
      
      //		last_scan_start = AMITK_DATA_SET_SCAN_START(slice_ds);
      //	      }
      //	    }
      //	  }


      /* check if the curtains match the rug */
      if (i.z == 0) 
	if (!REAL_EQUAL(AMITK_DATA_SET_SCAN_START(slice_ds)-last_scan_start, amitk_data_set_get_frame_duration(slice_ds, i.t-1))) 
	  screwed_up_timing=TRUE;
    }

    //    last_offset = AMITK_SPACE_OFFSET(slice_ds);

    image++;
  } /* slice loop */

  if (screwed_up_timing) 
    amitk_append_str_with_newline(perror_buf, _("Detected discontinous frames in data set %s - frame start times will be incorrect"), AMITK_OBJECT_NAME(ds));
  
  if (screwed_up_thickness)
    amitk_append_str_with_newline(perror_buf, _("Slice thickness (%5.3f mm) not equal to slice spacing (%5.3f mm) in data set %s - will use slice spacing for thickness"), old_thickness, true_thickness, AMITK_OBJECT_NAME(ds));
  
  /* detected a dynamic data set, massage the data appropriately */
  //	if (num_frames > 1) {
  //	  x = div(dim.z, num_frames);
  //	  if (x.rem != 0) {
  //	    g_warning(_("Cannot evenly divide the number of slices by the number of frames for data set %s - ignoring dynamic data"), AMITK_OBJECT_NAME(ds));
  //	  } else {
	
  
  /* hack the data set sizes */
  //	    ds->raw_data->dim.t = num_frames;
  //	    ds->raw_data->dim.z = x.quot;
  //
  //	    ds->internal_scaling_factor->dim.t = num_frames;
  //	    ds->internal_scaling_factor->dim.z = x.quot;
  //
  //	    ds->internal_scaling_intercept->dim.t = num_frames;
  //	    ds->internal_scaling_intercept->dim.z = x.quot;
  
  /* free the frame max/min arrays - will be realloc's by set_calc_max_min */
  //	    g_free(ds->frame_max);
  //	    ds->frame_max=NULL;
  //	    g_free(ds->frame_min);
  //	    ds->frame_min=NULL;
  
  /* and set the frame durations */
  //	    if (ds->frame_duration != NULL)
  //	      g_free(ds->frame_duration);
  //	    if ((ds->frame_duration = amitk_data_set_get_frame_duration_mem(ds)) == NULL) {
  //	      g_warning(_("couldn't allocate space for the frame duration info"));
  //	      amitk_object_unref(ds);
  //	      return NULL;
  //	    }
  //
  //	    for (j=0; j < AMITK_DATA_SET_NUM_FRAMES(ds); j++) 
  //	      ds->frame_duration[j] = g_array_index(durations,amide_time_t, j);
  //	  }
  //	}
  //	g_array_free(durations, TRUE);
  
  /* correct for a datset where the z direction is reveresed */
  //	if (reversed_z) {
  //	  void * buffer;
  //	  amide_data_t temp_val;
  
  //	  buffer = g_try_malloc(num_bytes_per_slice);
  //	  if (buffer == NULL) {
  //	    g_warning("could not malloc temp buffer");
  //	    goto error;
  //	  }
  
  
  //	  k = zero_voxel;
  //	  for (i.t = 0,k.t=0; i.t < AMITK_DATA_SET_DIM_T(ds); i.t++,k.t++) {
  //	    for (i.g = 0,k.g=0; i.g < AMITK_DATA_SET_DIM_G(ds); i.g++,k.g++) {
  //	      for (i.z = 0,k.z=AMITK_DATA_SET_DIM_Z(ds)-1; i.z < AMITK_DATA_SET_DIM_Z(ds); i.z++,k.z--) {
  
  /* exchange the slices*/
  //		ds_pointer = amitk_raw_data_get_pointer(AMITK_DATA_SET_RAW_DATA(ds), i);
  //		slice_pointer = amitk_raw_data_get_pointer(AMITK_DATA_SET_RAW_DATA(ds), k);
  //		memcpy(buffer, ds_pointer, num_bytes_per_slice);
  //		memcpy(ds_pointer, slice_pointer, num_bytes_per_slice);
  //		memcpy(slice_pointer, buffer, num_bytes_per_slice);
  //
  /* exchange the scale factors */
  //		temp_val = AMITK_RAW_DATA_DOUBLE_CONTENT(ds->internal_scaling_factor, i);
  //		AMITK_RAW_DATA_DOUBLE_SET_CONTENT(ds->internal_scaling_factor, i) =
  //		  AMITK_RAW_DATA_DOUBLE_CONTENT(ds->internal_scaling_factor, k);
  //		AMITK_RAW_DATA_DOUBLE_SET_CONTENT(ds->internal_scaling_factor, k) =
  //		  temp_val;
  
  /* exchange the scale offsets */
  //		temp_val = AMITK_RAW_DATA_DOUBLE_CONTENT(ds->internal_scaling_intercept, i);
  //		AMITK_RAW_DATA_DOUBLE_SET_CONTENT(ds->internal_scaling_intercept, i) =
  //		  AMITK_RAW_DATA_DOUBLE_CONTENT(ds->internal_scaling_intercept, k);
  //		AMITK_RAW_DATA_DOUBLE_SET_CONTENT(ds->internal_scaling_intercept, k) =
  //		  temp_val;
  //		
  //	      }
  //	    }
  //	  }
  //
  //	  g_free(buffer);
  //	}
  
  
  /* make sure remaining values have been calculated */
  amitk_data_set_set_scale_factor(ds, 1.0); /* set the external scaling factor */
  amitk_data_set_calc_far_corner(ds); /* set the far corner of the volume */
  amitk_data_set_calc_max_min(ds, update_func, update_data);
  
  goto end;

 error:
  if (ds != NULL) {
    amitk_object_unref(ds);
    ds = NULL;
  }

 end:

  return ds;
}

static GList * import_files_as_datasets(GList * image_files, 
					AmitkPreferences * preferences, 
					AmitkUpdateFunc update_func,
					gpointer update_data,
					gchar **perror_buf) {


  GList * returned_sets=NULL;
  AmitkDataSet * ds=NULL;
  AmitkDataSet * slice_ds=NULL;
  div_t x;
  gchar * slice_name;
  gint image;
  gint num_frames=1;
  gint num_gates=1;
  guint num_files;
  GList * slices=NULL;
  GList * discarded_slices=NULL;
  gboolean continue_work=TRUE;
  gint divider;

  num_files = g_list_length(image_files);
  g_return_val_if_fail(num_files != 0, NULL);

  if (update_func != NULL) 
    continue_work = (*update_func)(update_data, _("Importing File(s) Through DCMTK"), (gdouble) 0.0);
  divider = (num_files/AMIDE_UPDATE_DIVIDER < 1.0) ? 1 : (gint) rint(num_files/AMIDE_UPDATE_DIVIDER);

  for (image=0; (image < num_files) && (continue_work); image++) {
    
    if (update_func != NULL) {
      x = div(image,divider);
      if (x.rem == 0)
	continue_work = (*update_func)(update_data, NULL, ((gdouble) image)/((gdouble) num_files));
    }


    slice_name = (gchar *) g_list_nth_data(image_files,image);
    slice_ds = read_dicom_file(slice_name, preferences, &num_frames, &num_gates, NULL, NULL, perror_buf);
    if (slice_ds == NULL) {
      goto cleanup;
    } else if (AMITK_DATA_SET_DIM_Z(slice_ds) != 1) {
      g_warning(_("no support for multislice files within DICOM directory format"));
      amitk_object_unref(slice_ds);
      slice_ds = NULL;
      goto cleanup;
    } 
    slices = g_list_append(slices, slice_ds);
  }
  if (!continue_work) goto cleanup;

  if ((num_frames > 1) && (num_gates > 1)) 
    g_warning("Don't know how to deal with multi-gate and multi-frame data, results will be undefined");

  /* throw out any slices that don't fit in */
  slices = check_slices(slices);

  /* sort list based on the slice's time and z position */
  if (num_frames > 1)
    slices = g_list_sort(slices, sort_slices_func_with_time);
  else if (num_gates > 1)
    slices = g_list_sort(slices, sort_slices_func_with_gate);
  else
    slices = g_list_sort(slices, sort_slices_func);

  /* throw out any slices that are duplicated in terms of orientation */
  separate_duplicate_slices(&slices, &discarded_slices);


  /* load in the primary data set */
  ds = import_slices_as_dataset(slices, num_frames, num_gates, update_func, update_data, perror_buf);
  if (ds != NULL)
    returned_sets = g_list_append(returned_sets, ds);
  
  /* if we had left over slices, try loading them in as well */
  if (discarded_slices != NULL) {
    ds = import_slices_as_dataset(discarded_slices, num_frames, num_gates, update_func, update_data, perror_buf);
    if (ds != NULL)
      returned_sets = g_list_append(returned_sets, ds);
  }


 cleanup:
  if (update_func != NULL) /* remove progress bar */
    (*update_func) (update_data, NULL, (gdouble) 2.0); 

  slices = g_list_concat(slices, discarded_slices);
  while (slices != NULL) {
    slice_ds = (AmitkDataSet *) g_list_nth_data(slices,0);
    slices = g_list_remove(slices, slice_ds);
    amitk_object_unref(slice_ds);
  }

 
  return returned_sets;
}

typedef struct slice_info_t {
  gchar * filename;
  gchar * series_instance_uid;
  gchar * modality;
  gchar * series_number;
  gchar * series_description;
  gchar * patient_id;
  gchar * patient_name;
} slice_info_t;

static void free_slice_info(slice_info_t * info) {

  if (info->filename != NULL)
    g_free(info->filename);

  if (info->series_instance_uid != NULL)
    g_free(info->series_instance_uid);

  if (info->modality != NULL)
    g_free(info->modality);
  
  if (info->series_number != NULL)
    g_free(info->series_number);

  if (info->series_description != NULL)
    g_free(info->series_description);

  if (info->patient_id != NULL)
    g_free(info->patient_id);

  if (info->patient_name != NULL)
    g_free(info->patient_name);

  g_free(info);

  return;
}

static slice_info_t * slice_info_new(void) {

  slice_info_t * info;

  info = g_try_new(slice_info_t, 1);
  info->filename=NULL;
  info->series_instance_uid=NULL;
  info->modality=NULL;
  info->series_number=NULL;
  info->series_description=NULL;
  info->patient_id=NULL;
  info->patient_name=NULL;

  return info;
}


static slice_info_t * get_slice_info(const gchar * filename) {

  OFCondition result;
  DcmFileFormat dcm_format;
  DcmDataset * dcm_dataset;
  const char * return_str=NULL;
  slice_info_t * info=NULL;

  result = dcm_format.loadFile(filename);
  if (result.bad()) return NULL;

  dcm_dataset = dcm_format.getDataset();
  if (dcm_dataset == NULL) return NULL;

  info = slice_info_new();
  info->filename = g_strdup(filename);

  dcm_dataset->findAndGetString(DCM_SeriesInstanceUID, return_str, OFTrue);
  if (return_str != NULL)
      info->series_instance_uid = g_strdup(return_str);

  dcm_dataset->findAndGetString(DCM_Modality, return_str, OFTrue);
  if (return_str != NULL)
      info->modality = g_strdup(return_str);

  dcm_dataset->findAndGetString(DCM_SeriesNumber, return_str, OFTrue);
  if (return_str != NULL)
      info->series_number = g_strdup(return_str);

  dcm_dataset->findAndGetString(DCM_SeriesDescription, return_str, OFTrue);
  if (return_str != NULL) 
      info->series_description = g_strdup(return_str);

  dcm_dataset->findAndGetString(DCM_PatientID, return_str, OFTrue);
  if (return_str != NULL) 
      info->patient_id = g_strdup(return_str);

  dcm_dataset->findAndGetString(DCM_PatientsName, return_str, OFTrue);
  if (return_str != NULL) 
      info->patient_name = g_strdup(return_str);

  return info;
}

static gboolean check_str(gchar * str1, gchar * str2) {

  if ((str1 == NULL) && (str2 == NULL))
    return TRUE;
  if ((str1 == NULL) || (str2 == NULL))
    return FALSE;
  if (strcmp(str1, str2) == 0)
    return TRUE;

  return FALSE;
}

static gboolean check_same(slice_info_t * slice1, slice_info_t * slice2) {

  gint j, count;
  gchar ** slice1_uid_split=NULL;
  gchar ** slice2_uid_split=NULL;

  gboolean same_dataset;


  if ((slice1->series_instance_uid == NULL) || (slice2->series_instance_uid == NULL)) 
    return FALSE;
  else if ((slice1->series_instance_uid != NULL) && (slice2->series_instance_uid != NULL)) {
    slice1_uid_split = g_strsplit(slice1->series_instance_uid, ".", -1);
    slice2_uid_split = g_strsplit(slice2->series_instance_uid, ".", -1); 

    count=0;
    while (slice1_uid_split[count] != NULL) count++;

    /* check all but last number in series instance uid */
    same_dataset=TRUE;
    for (j=0; (j<(count-1) && same_dataset); j++) {
      if (slice2_uid_split[j] == NULL)
	same_dataset=FALSE;
      else if (strcmp(slice1_uid_split[j], slice2_uid_split[j]) != 0)
	same_dataset=FALSE;
    }

    if (slice1_uid_split != NULL) g_strfreev(slice1_uid_split);
    if (slice2_uid_split != NULL) g_strfreev(slice2_uid_split);

    if (!same_dataset) return FALSE;
  }


  if (!check_str(slice1->series_description, slice2->series_description))
    return FALSE;

  if (!check_str(slice1->series_number, slice2->series_number))
    return FALSE;

  if (!check_str(slice1->modality, slice2->modality))
    return FALSE;

  if (!check_str(slice1->patient_id, slice2->patient_id)) {
    return FALSE;
  }

  if (!check_str(slice1->patient_name, slice2->patient_name)) {
    return FALSE;
  }

  return TRUE;
}

static GList * import_files(const gchar * filename,
			    AmitkPreferences * preferences,
			    AmitkUpdateFunc update_func,
			    gpointer update_data) {


  AmitkDataSet * ds;
  GList * data_sets = NULL;
  GList * returned_sets = NULL;
  GList * image_files=NULL;
  GList * raw_info=NULL;
  GList * all_slices=NULL; /* list of lists */
  GList * temp_all_slices;
  GList * sorted_slices=NULL; /* pointer to a list in all_slices */
  gchar * image_name;
  gchar * error_buf=NULL;
  slice_info_t * info=NULL;
  slice_info_t * current_info=NULL;
  gchar * dirname=NULL;
  gchar * basename=NULL;
  gchar * new_filename;
  DIR* dir;
  struct dirent* entry;
  gboolean continue_work=TRUE;
  gboolean all_datasets;
  gint count;
  GtkWidget * question;
  gint return_val;
  
  info = get_slice_info(filename);
  if (info == NULL) {
    g_warning(_("could not find dataset in DICOM file %s\n"), filename);
    return NULL;
  }
  raw_info = g_list_append(raw_info, info); 

  /* ------- find all dicom files in the directory ------------ */
  if (update_func != NULL) 
    continue_work = (*update_func)(update_data, _("Scanning Files to find additional DICOM Slices"), (gdouble) 0.0);
    
  /* open up the directory */
  dirname = g_path_get_dirname(filename);
  basename = g_path_get_basename(filename);
  if ((dir = opendir(dirname))!=NULL) {
    while (((entry = readdir(dir)) != NULL) && (continue_work)) {

      if (update_func != NULL)
	continue_work = (*update_func)(update_data, NULL, -1.0);

      if (dirname == NULL)
	new_filename = g_strdup_printf("%s", entry->d_name);
      else
	new_filename = g_strdup_printf("%s%s%s", dirname, G_DIR_SEPARATOR_S,entry->d_name);

      if (strcmp(basename, entry->d_name) != 0) { /* we've already got the initial filename */
	if (dcmtk_test_dicom(new_filename)) {
	  info = get_slice_info(new_filename);
	  if (info != NULL) 
	    raw_info = g_list_append(raw_info, info); /* we have a match */
	}
      }

      g_free(new_filename);
    }
    if (dir != NULL) closedir(dir);
  }
  if (dirname != NULL) g_free(dirname);
  if (basename != NULL) g_free(basename);

  if (update_func != NULL) /* remove progress bar */
    (*update_func) (update_data, NULL, (gdouble) 2.0); 




  /* ------------ sort the files ---------------- */
  while (raw_info != NULL) {
    current_info = (slice_info_t *) raw_info->data;
    raw_info = g_list_remove(raw_info, current_info);

    /* go through the list of slice lists, find one that matches */
    temp_all_slices = all_slices; 
    while ((temp_all_slices != NULL) && (current_info != NULL)){
      sorted_slices = (GList *) temp_all_slices->data;
      g_assert(sorted_slices->data != NULL);
      info = (slice_info_t *) sorted_slices->data;

      /* current slice matches the first slice in the current list, add it to this list */
      if (check_same(current_info, info)) {
	sorted_slices = g_list_append(sorted_slices, current_info);
	current_info = NULL;
      }

      temp_all_slices = temp_all_slices->next;
    }

    /* current info doesn't match anything, add it on as a new list */
    if (current_info != NULL) 
      all_slices = g_list_append(all_slices, g_list_append(NULL, current_info));
  }


  g_assert(all_slices != NULL);

  /* check if we want to load in everything or not */
  all_datasets=FALSE;
  if (g_list_length(all_slices) > 1) {
    /* make sure we really want to delete */
    question = gtk_message_dialog_new(NULL,
				      GTK_DIALOG_DESTROY_WITH_PARENT,
				      GTK_MESSAGE_QUESTION,
				      GTK_BUTTONS_YES_NO,
				      _("Multiple data sets were found in the same directory as the specified file. In addition to the dataset corresponding to the specified file, would you like to load in these additional datasets?"));
  
    /* and wait for the question to return */
    return_val = gtk_dialog_run(GTK_DIALOG(question));

    gtk_widget_destroy(question);
    if (return_val == GTK_RESPONSE_YES)
      all_datasets=TRUE;
  }

  count=0;
  while (all_slices != NULL) {
    sorted_slices = (GList *) all_slices->data;
    all_slices = g_list_remove(all_slices, sorted_slices);

    /* transfer file names */
    image_files = NULL;
    while (sorted_slices != NULL) {
      info = (slice_info_t *) sorted_slices->data;
      sorted_slices = g_list_remove(sorted_slices, info);
      image_files = g_list_append(image_files, info->filename);
      info->filename=NULL;
      free_slice_info(info);
    }

    if (all_datasets || (count==0)) {
      count++;
      returned_sets = import_files_as_datasets(image_files, preferences, update_func, update_data, &error_buf);

      while (returned_sets != NULL) {
	ds = AMITK_DATA_SET(returned_sets->data);
	data_sets = g_list_append(data_sets, ds);
	returned_sets = g_list_remove(returned_sets, ds);
      }
    }

    /* cleanup */
    while (image_files != NULL) {
      image_name = (gchar *) g_list_nth_data(image_files,0);
      image_files = g_list_remove(image_files, image_name); 
      g_free(image_name);
    }
  }



  /* cleanup */
  if (error_buf != NULL) {
    g_warning(error_buf);
    g_free(error_buf);
  }

  return data_sets;
}


/* read in a DICOMDIR type file */
static GList * import_dir(const gchar * filename, 
			  AmitkPreferences * preferences,
			  AmitkUpdateFunc update_func,
			  gpointer update_data) {

  GList * data_sets=NULL;
  GList * returned_sets=NULL;
  DcmDirectoryRecord * dcm_root_record;
  DcmDirectoryRecord * patient_record=NULL;
  DcmDirectoryRecord * study_record=NULL;
  DcmDirectoryRecord * series_record=NULL;
  DcmDirectoryRecord * image_record=NULL;
  OFCondition result;

  AmitkDataSet * ds=NULL;
  const char * return_str=NULL;
  const char * record_name[3];
  const char * patient_id;
  gchar * dirname=NULL;
  gchar * object_name=NULL;
  gchar * temp_name;
  gchar * temp_name2;
  GList * image_files=NULL;
  gint j;

  gchar * error_buf=NULL;
  gchar * image_name1;
  gchar * image_name2;


  /* first try loading it as a DIRFILE */
  DcmDicomDir dcm_dir(filename);
  /*  dcm_dir.print(cout); */
  dirname = g_path_get_dirname(filename);

  dcm_root_record = &(dcm_dir.getRootRecord());
  /*  dcm_root_record->print(cout);*/

  /* go through the whole directory */
  while ((patient_record = dcm_root_record->nextSub(patient_record)) != NULL) {
    patient_record->findAndGetString(DCM_PatientsName, record_name[0]);
    patient_record->findAndGetString(DCM_PatientID, patient_id, OFTrue);


    while ((study_record= patient_record->nextSub(study_record)) != NULL) {
      study_record->findAndGetString(DCM_StudyDescription, record_name[1]);

      while ((series_record = study_record->nextSub(series_record)) != NULL) {
	series_record->findAndGetString(DCM_SeriesDescription, record_name[2]);

	/* concat the names that aren't NULL */
	for (j=0; j<3; j++) {
	  if (record_name[j] != NULL) {
	    temp_name2 = g_strdup(record_name[j]);
	    g_strstrip(temp_name2);
	    if (object_name != NULL) {
	      temp_name = object_name;
	      object_name = g_strdup_printf("%s - %s", temp_name, temp_name2);
	      g_free(temp_name);
	    } else
	      object_name = g_strdup(temp_name2);
	    g_free(temp_name2);
	  }
	}

	while ((image_record=series_record->nextSub(image_record)) != NULL) {
	  if (image_record->findAndGetString(DCM_ReferencedFileID, return_str).good()) {
	    image_name1 = g_strdup(return_str);
	    g_strdelimit(image_name1, "\\", G_DIR_SEPARATOR); /* remove separators */
	    image_name2 = g_strdup_printf("%s%s%s", dirname, G_DIR_SEPARATOR_S,image_name1);
	    image_files = g_list_append(image_files, image_name2);
	    g_free(image_name1);
	  }
	}

#if AMIDE_DEBUG
	g_print("object name: %s    with %d files (images)\n", object_name, g_list_length(image_files));
#endif

	/* read in slices as dataset(s) */
	returned_sets = import_files_as_datasets(image_files, preferences, update_func, update_data, &error_buf);
	while (returned_sets != NULL) {
	  ds = AMITK_DATA_SET(returned_sets->data);
	  /* use the DICOMDIR entries if available */
	  /* Checking for record_name[2] - if we have a series description in the DICOMDIR file, 
	     use that, otherwise use the name directly from the dicom file if available, otherwise
	     use object_name */
	  if ((record_name[2] != NULL) || (AMITK_OBJECT_NAME(ds) == NULL)) 
	    amitk_object_set_name(AMITK_OBJECT(ds), object_name);
	  if (record_name[0] != NULL)
	    amitk_data_set_set_subject_name(ds, record_name[0]);
	  if (patient_id != NULL)
	    amitk_data_set_set_subject_id(ds, patient_id);

	  data_sets = g_list_append(data_sets, ds);
	  returned_sets = g_list_remove(returned_sets, ds);
	}


	/* cleanup */
	while (image_files != NULL) {
	  image_name2 = (gchar *) g_list_nth_data(image_files,0);
	  image_files = g_list_remove(image_files, image_name2);
	  g_free(image_name2);
	}

	g_free(object_name);
	object_name=NULL;

      }
    }
  }

  /* more cleanup */
  if (dirname != NULL)
    g_free(dirname);

  if (error_buf != NULL) {
    g_warning(error_buf);
    g_free(error_buf);
  }

  return data_sets;
}


GList * dcmtk_import(const gchar * filename,
		     AmitkPreferences * preferences,
		     AmitkUpdateFunc update_func,
		     gpointer update_data) {

  GList * data_sets=NULL;
  AmitkDataSet * ds;
  gboolean is_dir=FALSE;
  DcmFileFormat dcm_format;
  DcmMetaInfo * dcm_metainfo;
  OFCondition result;
  const char * return_str=NULL;

  if (!dcmDataDict.isDictionaryLoaded()) {
    g_warning(_("Could not find a DICOM Data Dictionary.  Reading may fail.  Consider defining the environmental variable DCMDICTPATH to the dicom.dic file."));
  }

  result = dcm_format.loadFile(filename);
  if (result.bad()) {
    g_warning(_("could not read DICOM file %s, dcmtk returned %s"),filename, result.text());
    return NULL;
  }

  dcm_metainfo = dcm_format.getMetaInfo();
  if (dcm_metainfo == NULL) {
    g_warning("could not find metainfo for DICOM file %s\n", filename);
    return NULL;
  }

  dcm_metainfo->findAndGetString(DCM_MediaStorageSOPClassUID, return_str, OFTrue);
  if (return_str != NULL) {
    if (strcmp(return_str, UID_MediaStorageDirectoryStorage) == 0)
      is_dir= TRUE; /* we've got a DICOMDIR file */
  }


  if (is_dir) {
    data_sets = import_dir(filename, preferences, update_func, update_data);
  } else {
    data_sets = import_files(filename, preferences, update_func, update_data);
  }

  return data_sets;
}



#endif /* AMIDE_LIBDCMDATA_SUPPORT */





