/* Do not edit this file, it is generated automatically from variable_type.m4 */

	
	
	



	
	
	
/* roi_ISOCONTOUR_2D.c 
 *
 * generated from the following file:                 */

/* roi_variable_type.c - used to generate the different roi_*.c files
 *
 * Part of amide - Amide's a Medical Image Data Examiner
 * Copyright (C) 2001 Andy Loening
 *
 * Author: Andy Loening <loening@ucla.edu>
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



#include "config.h"
#define _GNU_SOURCE /* use GNU extensions, i.e. NaN */
#include <math.h>
#include <sys/resource.h>
#include <glib.h>
#include "roi_ISOCONTOUR_2D.h"


#define ROI_ISOCONTOUR_2D_TYPE




#if defined(ROI_CYLINDER_TYPE) || defined(ROI_ELLIPSOID_TYPE) || defined (ROI_BOX_TYPE)

static GSList * append_intersection_point(GSList * points_list, realpoint_t new_point) {
  realpoint_t * ppoint;

  ppoint = (realpoint_t * ) g_malloc(sizeof(realpoint_t));
  *ppoint = new_point;

  return  g_slist_append(points_list, ppoint);
}
static GSList * prepend_intersection_point(GSList * points_list, realpoint_t new_point) {
  realpoint_t * ppoint;

  ppoint = (realpoint_t * ) g_malloc(sizeof(realpoint_t));
  *ppoint = new_point;

  return  g_slist_prepend(points_list, ppoint);
}

/* returns a singly linked list of intersection points between the roi
   and the given slice.  returned points are in the slice's coord_frame */
GSList * roi_ISOCONTOUR_2D_get_intersection_line(const roi_t * roi, const volume_t * view_slice) {


  GSList * return_points = NULL;
  realpoint_t view_rp,temp_rp, slice_corner[2], roi_corner[2];
  realpoint_t * temp_rpp;
  voxelpoint_t i;
  gboolean voxel_in=FALSE, prev_voxel_intersection, saved=TRUE;
#if defined(ROI_ELLIPSOID_TYPE) || defined(ROI_CYLINDER_TYPE)
  realpoint_t center, radius;
#endif
#if defined(ROI_CYLINDER_TYPE)
  floatpoint_t height;
#endif

  
  g_assert(view_slice->data_set->dim.z == 1); /* sanity check */

  /* make sure we've already defined this guy */
  if (roi_undrawn(roi))  return NULL;

#ifdef AMIDE_DEBUG
  g_print("roi %s --------------------\n",roi->name);
  g_print("\t\tcorner\tx %5.3f\ty %5.3f\tz %5.3f\n",
	     roi->corner.x,roi->corner.y,roi->corner.z);
#endif

  /* get the roi corners in roi space */
  roi_corner[0] = realspace_base_coord_to_alt(rs_offset(roi->coord_frame), roi->coord_frame);
  roi_corner[1] = roi->corner;

#if defined(ROI_ELLIPSOID_TYPE) || defined(ROI_CYLINDER_TYPE)
  radius = rp_cmult(0.5, rp_diff(roi_corner[1],roi_corner[0]));
  center = rp_add(rp_cmult(0.5,roi_corner[1]), rp_cmult(0.5,roi_corner[0]));
#endif
#ifdef ROI_CYLINDER_TYPE
  height = fabs(roi_corner[1].z-roi_corner[0].z);
#endif

  /* get the corners of the view slice in view coordinate space */
  slice_corner[0] = realspace_base_coord_to_alt(rs_offset(view_slice->coord_frame), view_slice->coord_frame);
  slice_corner[1] = view_slice->corner;

  /* iterate through the slice, putting all edge points in the list */
  i.t = i.z=0;
  view_rp.z = (slice_corner[0].z+slice_corner[1].z)/2.0;
  view_rp.y = slice_corner[0].y+view_slice->voxel_size.y/2.0;

  for (i.y=0; i.y < view_slice->data_set->dim.y ; i.y++) {

    view_rp.x = slice_corner[0].x+view_slice->voxel_size.x/2.0;
    prev_voxel_intersection = FALSE;

    for (i.x=0; i.x < view_slice->data_set->dim.x ; i.x++) {
      temp_rp = realspace_alt_coord_to_alt(view_rp, view_slice->coord_frame, roi->coord_frame);
      
#ifdef ROI_BOX_TYPE
      voxel_in = rp_in_box(temp_rp, roi_corner[0],roi_corner[1]);
#endif
#ifdef ROI_CYLINDER_TYPE
      voxel_in = rp_in_elliptic_cylinder(temp_rp, center, height, radius);
#endif
#ifdef ROI_ELLIPSOID_TYPE
      voxel_in = rp_in_ellipsoid(temp_rp,center,radius);
#endif
	
      /* is it an edge */
      if (voxel_in != prev_voxel_intersection) {

	/* than save the point */
	saved = TRUE;

	temp_rp = view_rp;
	if (voxel_in ) {
	  return_points = prepend_intersection_point(return_points, temp_rp);
	} else { /* previous voxel should be returned */
	  temp_rp.x = view_rp.x - view_slice->voxel_size.x; /* backup one voxel */
	  return_points = append_intersection_point(return_points, temp_rp);
	}
      } else {
	saved = FALSE;
      }
      
      prev_voxel_intersection = voxel_in;
      view_rp.x += view_slice->voxel_size.x; /* advance one voxel */
    }
    
    /* check if the edge of this row is still in the roi, if it is, add it as
       a point */
    if ((voxel_in) && !(saved))
      return_points = append_intersection_point(return_points, view_rp);

    view_rp.y += view_slice->voxel_size.y; /* advance one row */
  }
  
  /* make sure the two ends meet */
  temp_rpp = g_slist_nth_data(return_points, 0);
  if (return_points != NULL)
    return_points = append_intersection_point(return_points, *temp_rpp);
					      
  
  return return_points;
}

#endif


#if defined(ROI_ISOCONTOUR_2D_TYPE) || defined(ROI_ISOCONTOUR_3D_TYPE)
/* we return the intersection data in the form of a volume slice because it's convienent */
volume_t * roi_ISOCONTOUR_2D_get_slice(const roi_t * roi, const volume_t * view_slice) {

  voxelpoint_t i_voxel;
  voxelpoint_t roi_vp;
  voxelpoint_t start, dim;
  realpoint_t view_rp;
  realpoint_t roi_rp;
  realpoint_t slice_corner[2];
  volume_t * intersection;
  realpoint_t temp_rp;
  data_set_UBYTE_t value;
#ifdef ROI_ISOCONTOUR_2D_TYPE
  gboolean y_edge;
#endif

  if (roi_undrawn(roi)) return NULL;

  /* figure out the intersections */
  roi_subset_of_volume(roi, view_slice, 0, &start, &dim);

  if (VOXELPOINT_EQUAL(dim, zero_vp)) return NULL;
  intersection = volume_init();
  intersection->data_set = data_set_UBYTE_2D_init(0, dim.y, dim.x);

  /* get the corners of the view slice in view coordinate space */
  slice_corner[0] = realspace_base_coord_to_alt(rs_offset(view_slice->coord_frame), 
						view_slice->coord_frame);
  slice_corner[1] = view_slice->corner;

  i_voxel.z = i_voxel.t = 0;
  view_rp.z = (slice_corner[0].z+slice_corner[1].z)/2.0;
  view_rp.y = slice_corner[0].y+((double) start.y + 0.5)*view_slice->voxel_size.y;

  for (i_voxel.y=0; i_voxel.y<dim.y; i_voxel.y++) {
    view_rp.x = slice_corner[0].x+((double)start.x+0.5)*view_slice->voxel_size.x;
#ifdef ROI_ISOCONTOUR_2D_TYPE
    if ((i_voxel.y == 0) || (i_voxel.y == (dim.y-1))) y_edge = TRUE;
    else y_edge = FALSE;
#endif

    for (i_voxel.x=0; i_voxel.x<dim.x; i_voxel.x++) {
      roi_rp = realspace_alt_coord_to_alt(view_rp, view_slice->coord_frame, roi->coord_frame);
      ROI_REALPOINT_TO_VOXEL(roi, roi_rp, roi_vp);

      if (data_set_includes_voxel(roi->isocontour, roi_vp)) {
	value = *DATA_SET_UBYTE_POINTER(roi->isocontour, roi_vp);
	if ( value == 1)
	  DATA_SET_UBYTE_SET_CONTENT(intersection->data_set, i_voxel) = 1;
#ifdef ROI_ISOCONTOUR_2D_TYPE
	else if ((value == 2) && ((y_edge) || (i_voxel.x == 0) || (i_voxel.x == (dim.x-1))))
	  DATA_SET_UBYTE_SET_CONTENT(intersection->data_set, i_voxel) = 1;
#endif
      }

      view_rp.x += view_slice->voxel_size.x;
    }
    view_rp.y += view_slice->voxel_size.y;
  }

  intersection->coord_frame = view_slice->coord_frame;
  intersection->voxel_size = view_slice->voxel_size;

  REALPOINT_MULT(start, intersection->voxel_size, temp_rp);
  rs_set_offset(&(intersection->coord_frame), 
		realspace_alt_coord_to_base(temp_rp, view_slice->coord_frame));

  temp_rp = rs_offset(intersection->coord_frame);

  REALPOINT_MULT(intersection->data_set->dim, intersection->voxel_size, 
		 intersection->corner);
  
  return intersection;
}


/* returns 0 for something not in the roi, returns 1 for an edge, and 2 for something in the roi */
static data_set_UBYTE_t isocontour_edge(data_set_t * isocontour_ds, voxelpoint_t vp) {

  data_set_UBYTE_t edge_value=0;
  voxelpoint_t new_vp;
#ifdef ROI_ISOCONTOUR_3D_TYPE
  gint z;
#endif

#ifdef ROI_ISOCONTOUR_3D_TYPE
  for (z=-1; z<=1; z++) {
#endif

    new_vp = vp;
#ifdef ROI_ISOCONTOUR_3D_TYPE
    new_vp.z += z;
    if (z != 0) /* don't reconsider the original point */
      if (data_set_includes_voxel(isocontour_ds, new_vp))
	if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	  edge_value++;
#endif
    new_vp.x--;
    if (data_set_includes_voxel(isocontour_ds, new_vp))
      if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	edge_value++;

    new_vp.y--;
    if (data_set_includes_voxel(isocontour_ds, new_vp))
      if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	edge_value++;

    new_vp.x++;
    if (data_set_includes_voxel(isocontour_ds, new_vp))
      if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	edge_value++;

    new_vp.x++;
    if (data_set_includes_voxel(isocontour_ds, new_vp))
      if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	edge_value++;

    new_vp.y++;
    if (data_set_includes_voxel(isocontour_ds, new_vp))
      if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	edge_value++;

    new_vp.y++;
    if (data_set_includes_voxel(isocontour_ds, new_vp))
      if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	edge_value++;

    new_vp.x--;
    if (data_set_includes_voxel(isocontour_ds, new_vp))
      if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	edge_value++;

    new_vp.x--;
    if (data_set_includes_voxel(isocontour_ds, new_vp))
      if (*DATA_SET_UBYTE_POINTER(isocontour_ds,new_vp) != 0)
	edge_value++;

#ifdef ROI_ISOCONTOUR_3D_TYPE
  }
#endif

#ifdef ROI_ISOCONTOUR_3D_TYPE
  if (edge_value == 26 )
    edge_value = 2;
  else
    edge_value = 1;
#else /* ROI_ISOCONTOUR_2D_TYPE */
  if (edge_value == 8 )
    edge_value = 2;
  else
    edge_value = 1;
#endif

  return edge_value;
}

static void isocontour_consider(volume_t * volume, 
				data_set_t * temp_ds, 
				voxelpoint_t volume_vp, 
				amide_data_t iso_value) {
#ifdef ROI_ISOCONTOUR_3D_TYPE
  gint z;
#endif
  voxelpoint_t roi_vp;
  voxelpoint_t new_vp;

  roi_vp = volume_vp;
  roi_vp.t = 0;
  if (!data_set_includes_voxel(temp_ds, roi_vp))
    return; /* make sure we're still in the data set */

  if (*DATA_SET_UBYTE_POINTER(temp_ds,roi_vp) == 1)
    return; /* have we already considered this voxel */
  
  if (!(volume_value(volume, volume_vp) >= iso_value)) return;
  else DATA_SET_UBYTE_SET_CONTENT(temp_ds,roi_vp)=1; /* it's in */

  /* consider the 8 adjoining voxels, or 26 in the case of 3D */
#ifdef ROI_ISOCONTOUR_3D_TYPE
  for (z=-1; z<=1; z++) {
#endif
    new_vp = volume_vp;
#ifdef ROI_ISOCONTOUR_3D_TYPE
    new_vp.z += z;
    if (z != 0) /* don't reconsider the original poit */
      isocontour_consider(volume, temp_ds, new_vp, iso_value); 
#endif
    new_vp.x--;
    isocontour_consider(volume, temp_ds, new_vp, iso_value);
    new_vp.y--;
    isocontour_consider(volume, temp_ds, new_vp, iso_value);
    new_vp.x++;
    isocontour_consider(volume, temp_ds, new_vp, iso_value);
    new_vp.x++;
    isocontour_consider(volume, temp_ds, new_vp, iso_value);
    new_vp.y++;
    isocontour_consider(volume, temp_ds, new_vp, iso_value);
    new_vp.y++;
    isocontour_consider(volume, temp_ds, new_vp, iso_value);
    new_vp.x--;
    isocontour_consider(volume, temp_ds, new_vp, iso_value);
    new_vp.x--;
    isocontour_consider(volume, temp_ds, new_vp, iso_value);
#ifdef ROI_ISOCONTOUR_3D_TYPE
  }
#endif

  return;
}

void roi_ISOCONTOUR_2D_set_isocontour(roi_t * roi, volume_t * vol, voxelpoint_t iso_vp) {

  data_set_t * temp_ds;
  realpoint_t temp_rp;
  voxelpoint_t min_vp, max_vp, i_voxel;
  rlim_t prev_stack_limit;
  struct rlimit rlim;

  g_return_if_fail(roi->type == ISOCONTOUR_2D);

  roi->isocontour_value = volume_value(vol, iso_vp); /* what we're setting the isocontour too */

  /* we first make a data set the size of the volume to record the in/out values */
#if defined(ROI_ISOCONTOUR_2D_TYPE)
  temp_ds = data_set_UBYTE_2D_init(0, vol->data_set->dim.y, vol->data_set->dim.x);
#elif defined(ROI_ISOCONTOUR_3D_TYPE)
  temp_ds = data_set_UBYTE_3D_init(0, vol->data_set->dim.z, vol->data_set->dim.y, vol->data_set->dim.x);
#endif

  /* remove any limitation to the stack size, this is so we can recurse deeply without
     seg faulting */
  getrlimit(RLIMIT_STACK, &rlim); 
  prev_stack_limit = rlim.rlim_cur;
  if (rlim.rlim_cur != rlim.rlim_max) {
    rlim.rlim_cur = rlim.rlim_max;
    setrlimit(RLIMIT_STACK, &rlim);
  }

  /* fill in the data set */
  isocontour_consider(vol, temp_ds, iso_vp, roi->isocontour_value-CLOSE);

  /* figure out the min and max dimensions */
  min_vp = max_vp = iso_vp;
  
  i_voxel.t = 0;
  for (i_voxel.z=0; i_voxel.z < temp_ds->dim.z; i_voxel.z++) {
    for (i_voxel.y=0; i_voxel.y < temp_ds->dim.y; i_voxel.y++) {
      for (i_voxel.x=0; i_voxel.x < temp_ds->dim.x; i_voxel.x++) {
	if (*DATA_SET_UBYTE_POINTER(temp_ds, i_voxel)==1) {
	  if (min_vp.x > i_voxel.x) min_vp.x = i_voxel.x;
	  if (max_vp.x < i_voxel.x) max_vp.x = i_voxel.x;
	  if (min_vp.y > i_voxel.y) min_vp.y = i_voxel.y;
	  if (max_vp.y < i_voxel.y) max_vp.y = i_voxel.y;
#ifdef ROI_ISOCONTOUR_3D_TYPE
	  if (min_vp.z > i_voxel.z) min_vp.z = i_voxel.z;
	  if (max_vp.z < i_voxel.z) max_vp.z = i_voxel.z;
#endif
	}
      }
    }
  }

  /* transfer the subset of the data set that contains positive information */
  roi->isocontour = data_set_free(roi->isocontour); 
  g_return_if_fail(roi->isocontour == NULL); /* sanity check */
#if defined(ROI_ISOCONTOUR_2D_TYPE)
  roi->isocontour = data_set_UBYTE_2D_init(0, max_vp.y-min_vp.y+1, max_vp.x-min_vp.x+1);
#elif defined(ROI_ISOCONTOUR_3D_TYPE)
  roi->isocontour = data_set_UBYTE_3D_init(0,max_vp.z-min_vp.z+1,
					   max_vp.y-min_vp.y+1, 
					   max_vp.x-min_vp.x+1);
#endif

  i_voxel.t = 0;
  for (i_voxel.z=0; i_voxel.z<roi->isocontour->dim.z; i_voxel.z++)
    for (i_voxel.y=0; i_voxel.y<roi->isocontour->dim.y; i_voxel.y++)
      for (i_voxel.x=0; i_voxel.x<roi->isocontour->dim.x; i_voxel.x++) {
#if defined(ROI_ISOCONTOUR_2D_TYPE)
	DATA_SET_UBYTE_SET_CONTENT(roi->isocontour, i_voxel) =
	  *DATA_SET_UBYTE_2D_POINTER(temp_ds, i_voxel.y+min_vp.y, i_voxel.x+min_vp.x);
#elif defined(ROI_ISOCONTOUR_3D_TYPE)
	DATA_SET_UBYTE_SET_CONTENT(roi->isocontour, i_voxel) =
	  *DATA_SET_UBYTE_3D_POINTER(temp_ds, i_voxel.z+min_vp.z,i_voxel.y+min_vp.y, i_voxel.x+min_vp.x);
#endif
      }

  temp_ds = data_set_free(temp_ds);

  /* mark the edges as such */
  i_voxel.t = 0;
  for (i_voxel.z=0; i_voxel.z<roi->isocontour->dim.z; i_voxel.z++)
    for (i_voxel.y=0; i_voxel.y<roi->isocontour->dim.y; i_voxel.y++) 
      for (i_voxel.x=0; i_voxel.x<roi->isocontour->dim.x; i_voxel.x++) 
	if (*DATA_SET_UBYTE_POINTER(roi->isocontour, i_voxel)) 
	  DATA_SET_UBYTE_SET_CONTENT(roi->isocontour, i_voxel) =
	    isocontour_edge(roi->isocontour, i_voxel);

  /* and set the rest of the important info for the data set */
  roi->coord_frame = vol->coord_frame;
  roi->voxel_size = vol->voxel_size;

  REALPOINT_MULT(min_vp, vol->voxel_size, temp_rp);
  temp_rp = realspace_alt_coord_to_base(temp_rp, vol->coord_frame);
  rs_set_offset(&(roi->coord_frame), temp_rp);

  REALPOINT_MULT(roi->isocontour->dim, roi->voxel_size, roi->corner);

  /* reset our previous stack limit */
  if (prev_stack_limit != rlim.rlim_cur) {
    rlim.rlim_cur = prev_stack_limit;
    setrlimit(RLIMIT_STACK, &rlim);
  }

  return;

}

void roi_ISOCONTOUR_2D_erase_area(roi_t * roi, voxelpoint_t erase_vp, gint area_size) {

  voxelpoint_t i_voxel;

#ifdef ROI_ISOCONTOUR_3D_TYPE
  for (i_voxel.z = erase_vp.z-area_size; i_voxel.z <= erase_vp.z+area_size; i_voxel.z++) 
#endif
    for (i_voxel.y = erase_vp.y-area_size; i_voxel.y <= erase_vp.y+area_size; i_voxel.y++) 
      for (i_voxel.x = erase_vp.x-area_size; i_voxel.x <= erase_vp.x+area_size; i_voxel.x++) 
	if (data_set_includes_voxel(roi->isocontour, i_voxel))
	  DATA_SET_UBYTE_SET_CONTENT(roi->isocontour,i_voxel)=0;

  /* re edge the neighboring points */
  i_voxel.t = i_voxel.z = 0;
#ifdef ROI_ISOCONTOUR_3D_TYPE
  for (i_voxel.z = erase_vp.z-1-area_size; i_voxel.z <= erase_vp.z+1+area_size; i_voxel.z++) 
#endif
    for (i_voxel.y = erase_vp.y-1-area_size; i_voxel.y <= erase_vp.y+1+area_size; i_voxel.y++) 
      for (i_voxel.x = erase_vp.x-1-area_size; i_voxel.x <= erase_vp.x+1+area_size; i_voxel.x++) 
	if (data_set_includes_voxel(roi->isocontour, i_voxel)) 
	  if (*DATA_SET_UBYTE_POINTER(roi->isocontour, i_voxel)) 
	    DATA_SET_UBYTE_SET_CONTENT(roi->isocontour, i_voxel) =
	      isocontour_edge(roi->isocontour, i_voxel);

  return;
}
#endif












