/* ui_study.c
 *
 * Part of amide - Amide's a Medical Image Dataset Examiner
 * Copyright (C) 2000-2003 Andy Loening
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
#include <gnome.h>
#include <string.h>
#include "image.h"
#include "ui_common.h"
#include "ui_study.h"
#include "ui_study_cb.h"
#include "ui_study_menus.h"
#include "ui_time_dialog.h"
#include "amitk_tree.h"
#include "amitk_canvas.h"
#include "amitk_threshold.h"
#include "amitk_progress_dialog.h"

/* external variables */

/* internal variables */
#define UI_STUDY_HELP_FONT "-*-helvetica-medium-r-normal-*-*-120-*-*-*-*-*-*"

#define HELP_INFO_LINE_HEIGHT 13

static gchar * help_info_legends[NUM_HELP_INFO_LINES] = {
  "1", "shift-1",
  "2", "shift-2",
  "3", "shift-3", "ctrl-3",
  "variable_place_holder"
};

enum {
  HELP_INFO_VARIABLE_LINE_CTRL_X,
  HELP_INFO_VARIABLE_LINE_SHIFT_CTRL_3,
  NUM_HELP_INFO_VARIABLE_LINES
};

static gchar * help_info_variable_legend[NUM_HELP_INFO_VARIABLE_LINES] = {
  "ctrl-x",
  "shift-ctrl-3"
};

static gchar * help_info_lines[][NUM_HELP_INFO_LINES] = {
  {"",  "", 
   "",  "",    
   "",  "", "",
   ""}, /* BLANK */
  {"move view", "shift data set",
   "move view, min. depth", "rotate", 
   "change depth", "",  "add fiducial mark",
   ""},
  {"shift", "", 
   "rotate", "", 
   "scale", "", "set data set inside roi to zero",
   "set data set outside roi to zero"}, /*CANVAS_ROI */
  {"shift",  "", 
   "", "", 
   "", "", "",
   ""}, /*CANVAS_FIDUCIAL_MARK */
  {"shift", "", 
   "erase isocontour point", "erase large point", 
   "start isocontour change", "", "set data set inside roi to zero",
   "set data set outside roi to zero"}, /*CANVAS_ISOCONTOUR_ROI */
  {"draw", "", 
   "", "", 
   "", "", "",
   ""}, /* CANVAS_NEW_ROI */
  {"pick isocontour value", "", 
   "", "", 
   "", "", "",
   ""}, /* CANVAS_NEW_ISOCONTOUR_ROI */
  {"cancel", "", 
   "cancel", "", 
   "shift", "", "",
   ""}, /*CANVAS SHIFT DATA SET*/
  {"cancel", "", 
   "cancel", "", 
   "rotate", "", "",
   ""}, /*CANVAS ROTATE DATA SET*/
  {"select data set", "", 
   "make active", "", 
   "pop up data set dialog", "add roi", "add fiducial mark",
   "delete data set"}, /* TREE_DATA_SET */
  {"select roi", "", 
   "center view on roi", "", 
   "pop up roi dialog", "", "",
   "delete roi"}, /* TREE_ROI */
  {"select point", "", 
   "center view on point", "", 
   "pop up point dialog", "", "",
   "delete mark"}, /* TREE_FIDUCIAL_MARK */
  {"", "", 
   "", "", 
   "pop up study dialog","", "",
   ""}, /* TREE_STUDY */
  {"", "", 
   "", "", 
   "add roi","", "",
   ""} /* TREE_NONE */
};

static gint next_study_num=1;


static void object_selection_changed_cb(AmitkObject * object, gpointer ui_study);
static void object_name_changed_cb(AmitkObject * object, gpointer ui_study);
static void object_add_child_cb(AmitkObject * parent, AmitkObject * child, gpointer ui_study);
static void object_remove_child_cb(AmitkObject * parent, AmitkObject * child, gpointer ui_study);
static void add_object(ui_study_t * ui_study, AmitkObject * object);
static void remove_object(ui_study_t * ui_study, AmitkObject * object);


static void object_selection_changed_cb(AmitkObject * object, gpointer data) {

  ui_study_t * ui_study = data;

  if (AMITK_IS_DATA_SET(object)) {
    if (ui_study->time_dialog != NULL)
      ui_time_dialog_set_times(ui_study->time_dialog);
  }

  return;
}

static void object_name_changed_cb(AmitkObject * object, gpointer data) {

  ui_study_t * ui_study = data;

  if (AMITK_IS_STUDY(object)) {
    ui_study_update_title(ui_study);
  }

  return;
}

static void object_add_child_cb(AmitkObject * parent, AmitkObject * child, gpointer data ) {

  ui_study_t * ui_study = data;

  g_return_if_fail(AMITK_IS_OBJECT(child));
  add_object(ui_study, child);

  return;
}

static void object_remove_child_cb(AmitkObject * parent, AmitkObject * child, gpointer data) {

  ui_study_t * ui_study = data;

  g_return_if_fail(AMITK_IS_OBJECT(child));
  remove_object(ui_study, child);

  return;
}
  

static void add_object(ui_study_t * ui_study, AmitkObject * object) {

  GList * children;
  AmitkViewMode i_view_mode;
  AmitkView i_view;
  amide_real_t vox_size;


  g_object_ref(object); /* add a reference */

  if (AMITK_IS_STUDY(object)) { /* save a ref to a study object */
    if (ui_study->study != NULL) {
      remove_object(ui_study, AMITK_OBJECT(ui_study->study));
      ui_study->active_ds = NULL;
    }
    ui_study->study = AMITK_STUDY(object);
    ui_study->study_virgin=FALSE;

    /* set any settings we can */
    ui_study_update_thickness(ui_study, AMITK_STUDY_VIEW_THICKNESS(object));
    ui_study_update_zoom(ui_study);
    ui_study_update_title(ui_study);
    ui_study_update_time_button(ui_study->study, ui_study->time_button);
    ui_study_update_layout(ui_study);
    ui_study_update_canvas_visible_buttons(ui_study);
    ui_study_update_fuse_type(ui_study);
    ui_study_update_view_mode(ui_study);

    amitk_tree_set_study(AMITK_TREE(ui_study->tree), AMITK_STUDY(object));
    amitk_tree_expand_object(AMITK_TREE(ui_study->tree), object);
    for (i_view_mode = 0; i_view_mode <= AMITK_STUDY_VIEW_MODE(ui_study->study); i_view_mode++) 
      for (i_view=0; i_view< AMITK_VIEW_NUM; i_view++)
	if (AMITK_STUDY_CANVAS_VISIBLE(object, i_view))
	  amitk_canvas_set_study(AMITK_CANVAS(ui_study->canvas[i_view_mode][i_view]), AMITK_STUDY(object));

    g_signal_connect(G_OBJECT(object), "time_changed", G_CALLBACK(ui_study_cb_study_changed), ui_study);
    g_signal_connect(G_OBJECT(object), "thickness_changed",  G_CALLBACK(ui_study_cb_study_changed), ui_study);
    g_signal_connect(G_OBJECT(object), "object_name_changed", G_CALLBACK(object_name_changed_cb), ui_study);
    g_signal_connect(G_OBJECT(object), "canvas_visible_changed", G_CALLBACK(ui_study_cb_study_changed), ui_study);
    g_signal_connect(G_OBJECT(object), "view_mode_changed", G_CALLBACK(ui_study_cb_study_view_mode_changed), ui_study);

  } else if (AMITK_IS_DATA_SET(object)) {
    amitk_tree_expand_object(AMITK_TREE(ui_study->tree), AMITK_OBJECT_PARENT(object));
    vox_size = amitk_data_sets_get_min_voxel_size(AMITK_OBJECT_CHILDREN(ui_study->study));
    amitk_study_set_view_thickness(ui_study->study, vox_size);

    if (ui_study->study_altered != TRUE) {
      ui_study->study_altered=TRUE;
      ui_study->study_virgin=FALSE;
      ui_study_update_title(ui_study);
    }
  }

  g_signal_connect(G_OBJECT(object), "object_selection_changed", G_CALLBACK(object_selection_changed_cb), ui_study);
  g_signal_connect(G_OBJECT(object), "object_add_child", G_CALLBACK(object_add_child_cb), ui_study);
  g_signal_connect(G_OBJECT(object), "object_remove_child", G_CALLBACK(object_remove_child_cb), ui_study);


  /* add children */
  children = AMITK_OBJECT_CHILDREN(object);
  while (children != NULL) {
      add_object(ui_study, children->data);
      children = children->next;
  }

  return;
}

static void remove_object(ui_study_t * ui_study, AmitkObject * object) {

  GList * children;

  /* recursive remove children */
  children = AMITK_OBJECT_CHILDREN(object);
  while (children != NULL) {
    remove_object(ui_study, children->data);
    children = children->next;
  }

  /* disconnect the object's signals */
  if (AMITK_IS_STUDY(object)) {
    g_signal_handlers_disconnect_by_func(G_OBJECT(object), G_CALLBACK(ui_study_cb_study_changed), ui_study);
    g_signal_handlers_disconnect_by_func(G_OBJECT(object), G_CALLBACK(ui_study_cb_study_view_mode_changed), ui_study);
    g_signal_handlers_disconnect_by_func(G_OBJECT(object), G_CALLBACK(object_name_changed_cb), ui_study);
  }

  g_signal_handlers_disconnect_by_func(G_OBJECT(object), G_CALLBACK(object_selection_changed_cb), ui_study);
  g_signal_handlers_disconnect_by_func(G_OBJECT(object), G_CALLBACK(object_add_child_cb), ui_study);
  g_signal_handlers_disconnect_by_func(G_OBJECT(object), G_CALLBACK(object_remove_child_cb), ui_study);
  
  /* close down the object's dialog if it's up */
  if (object->dialog != NULL) {
    gtk_widget_destroy(GTK_WIDGET(object->dialog));
    object->dialog = NULL;
  }

  /* and unref */
  g_object_unref(object);

  return;
}


/* destroy a ui_study data structure */
ui_study_t * ui_study_free(ui_study_t * ui_study) {

  gboolean return_val;

  if (ui_study == NULL)
    return ui_study;

  /* sanity checks */
  g_return_val_if_fail(ui_study->reference_count > 0, NULL);

  /* remove a reference count */
  ui_study->reference_count--;

  /* if we've removed all reference's, free the structure */
  if (ui_study->reference_count == 0) {

    /* these two lines forces any remaining spin button updates, so that we
       don't call any spin button callbacks with invalid data */
    gtk_widget_grab_focus(ui_study->app);
    while (gtk_events_pending()) gtk_main_iteration();

#ifdef AMIDE_DEBUG
    g_print("freeing ui_study\n");
#endif
    if (ui_study->study != NULL) {
      remove_object(ui_study, AMITK_OBJECT(ui_study->study));
      ui_study->study = NULL;
    }

    if (ui_study->progress_dialog != NULL) {
      g_signal_emit_by_name(G_OBJECT(ui_study->progress_dialog), "delete_event", NULL, &return_val);
      ui_study->progress_dialog = NULL;
    }

    g_free(ui_study);
    ui_study = NULL;
  }
    
  return ui_study;
}

/* allocate and initialize a ui_study data structure */
ui_study_t * ui_study_init(void) {

  ui_study_t * ui_study;
  AmitkViewMode i_view_mode;
  AmitkView i_view;
  help_info_line_t i_line;

  /* alloc space for the data structure for passing ui info */
  ui_study = g_try_new(ui_study_t,1);
  g_return_val_if_fail(ui_study != NULL, NULL);

  ui_study->reference_count = 1;

  ui_study->study = NULL;
  ui_study->threshold_dialog = NULL;
  ui_study->preferences_dialog = NULL;
  ui_study->time_dialog = NULL;
  ui_study->thickness_spin = NULL;

  ui_study->active_ds = NULL;
  for (i_view_mode=0; i_view_mode < AMITK_VIEW_MODE_NUM; i_view_mode++) {
    ui_study->canvas_table[i_view_mode] = NULL;
    for (i_view=0; i_view < AMITK_VIEW_NUM; i_view++) {
      ui_study->canvas[i_view_mode][i_view] = NULL;
    }
  }

  ui_study->study_altered=FALSE;
  ui_study->study_virgin=TRUE;
  
  for (i_line=0 ;i_line < NUM_HELP_INFO_LINES;i_line++) {
    ui_study->help_line[i_line] = NULL;
    ui_study->help_legend[i_line] = NULL;
  }

  /* load in saved preferences */
  gnome_config_push_prefix("/"PACKAGE"/");
  ui_study->roi_width = 
    gnome_config_get_int("ROI/Width");
  if (ui_study->roi_width == 0) 
    ui_study->roi_width = 2; /* if no config file, put in sane value */
  ui_study->line_style = 
    gnome_config_get_int("ROI/LineStyle"); /* 0 is solid */
  ui_study->canvas_layout = 
    gnome_config_get_int("CANVAS/Layout"); /* 0 is AMITK_LAYOUT_LINEAR */
  ui_study->canvas_maintain_size = 
    !gnome_config_get_int("CANVAS/MinimizeSize"); /* 0 is FALSE */
  ui_study->canvas_leave_target = 
    gnome_config_get_int("CANVAS/LeaveTarget"); /* 0 is FALSE */
  ui_study->canvas_target_empty_area = 
    gnome_config_get_int("CANVAS/TargetEmptyArea"); /* in pixels */
  ui_study->dont_prompt_for_save_on_exit = 
    gnome_config_get_int("MISC/DontPromptForSaveOnExit"); /* 0 is FALSE, so we prompt */

  gnome_config_pop_prefix();

  return ui_study;
}


/* if data set is NULL, it removes the active mark */
void ui_study_make_active_data_set(ui_study_t * ui_study, AmitkDataSet * ds) {

  AmitkView i_view;
  GList * current_objects;
  GList * temp_objects;
  AmitkViewMode i_view_mode;

  g_return_if_fail(ui_study->study != NULL);

  if (ui_study->active_ds != NULL) {
    g_signal_handlers_disconnect_by_func(G_OBJECT(ui_study->active_ds),
					 G_CALLBACK(ui_study_update_interpolation), ui_study);
  }

  ui_study->active_ds = ds;

  if (ds == NULL) {/* make a guess as to what should be the active data set */
    for (i_view_mode = 0; 
	 (i_view_mode <= AMITK_STUDY_VIEW_MODE(ui_study->study)) && (ui_study->active_ds == NULL); 
	 i_view_mode++) {

      current_objects = amitk_object_get_selected_children_of_type(AMITK_OBJECT(ui_study->study),
								   AMITK_OBJECT_TYPE_DATA_SET,
								   AMITK_VIEW_MODE_SINGLE+i_view_mode, 
								   TRUE);
      temp_objects = current_objects;
      while (temp_objects != NULL) {
	if (AMITK_IS_DATA_SET(temp_objects->data)) {
	  ui_study->active_ds = AMITK_DATA_SET(temp_objects->data);
	  temp_objects = NULL;
	} else {
	  temp_objects = temp_objects->next;
	}
      }
      amitk_objects_unref(current_objects);
    }
  }
								 
  /* indicate this is now the active object */
  if (ui_study->active_ds != NULL) {
    amitk_tree_set_active_object(AMITK_TREE(ui_study->tree), 
				 AMITK_OBJECT(ui_study->active_ds));
    /* connect any needed signals */
    g_signal_connect_swapped(G_OBJECT(ui_study->active_ds), "interpolation_changed", 
			     G_CALLBACK(ui_study_update_interpolation), ui_study);
  } else {
    amitk_tree_set_active_object(AMITK_TREE(ui_study->tree), NULL);
  }
  ui_study_update_interpolation(ui_study);

  

  for (i_view_mode = 0; i_view_mode <=  AMITK_STUDY_VIEW_MODE(ui_study->study); i_view_mode++) 
    for (i_view=0; i_view< AMITK_VIEW_NUM; i_view++)
      if (ui_study->canvas[i_view_mode][i_view] != NULL)
	amitk_canvas_set_active_data_set(AMITK_CANVAS(ui_study->canvas[i_view_mode][i_view]), 
					 ui_study->active_ds);
  
  /* reset the threshold widget based on the current data set */
  if (ui_study->threshold_dialog != NULL) {
    if (ds == NULL) {
      gtk_widget_destroy(ui_study->threshold_dialog);
      ui_study->threshold_dialog = NULL;
    } else {
      amitk_threshold_dialog_new_data_set(AMITK_THRESHOLD_DIALOG(ui_study->threshold_dialog), 
					  ui_study->active_ds);
    }
  }
}

/* function for adding a fiducial mark */
void ui_study_add_fiducial_mark(ui_study_t * ui_study, AmitkObject * parent_object,
				gboolean selected, AmitkPoint position) {

  GtkWidget * entry;
  gchar * pt_name = NULL;
  gint entry_return;
  AmitkFiducialMark * new_pt=NULL;
  gchar * temp_string;

  g_return_if_fail(AMITK_IS_OBJECT(parent_object));

  temp_string = g_strdup_printf("Adding fiducial mark for data set: %s\nEnter the mark's name:",
				AMITK_OBJECT_NAME(parent_object));
  entry = gnome_request_dialog(FALSE, temp_string, "", 256, 
			       ui_common_entry_name_cb,
			       &pt_name, GTK_WINDOW(ui_study->app));
  entry_return = gnome_dialog_run_and_close(GNOME_DIALOG(entry));
  g_free(temp_string);
  

  if (entry_return == 1) {

    new_pt = amitk_fiducial_mark_new();
    amitk_space_copy_in_place(AMITK_SPACE(new_pt), AMITK_SPACE(parent_object));
    amitk_fiducial_mark_set(new_pt, position);
    amitk_object_set_name(AMITK_OBJECT(new_pt), pt_name);
    amitk_object_add_child(AMITK_OBJECT(parent_object), AMITK_OBJECT(new_pt));
    amitk_tree_expand_object(AMITK_TREE(ui_study->tree), AMITK_OBJECT(parent_object));
    g_object_unref(new_pt); /* don't want an extra ref */

    if (selected)
      amitk_object_set_selected(AMITK_OBJECT(new_pt), TRUE, AMITK_SELECTION_SELECTED_0);

    if (ui_study->study_altered != TRUE) {
      ui_study->study_virgin=FALSE;
      ui_study->study_altered=TRUE;
      ui_study_update_title(ui_study);
    }
  }

  return;
}


void ui_study_add_roi(ui_study_t * ui_study, AmitkObject * parent_object, AmitkRoiType roi_type) {

  GtkWidget * entry;
  gchar * roi_name = NULL;
  gint entry_return;
  AmitkRoi * roi;
  gchar * temp_string;

  g_return_if_fail(AMITK_IS_OBJECT(parent_object));

  temp_string = g_strdup_printf("Adding ROI to: %s\nEnter ROI Name:",
				AMITK_OBJECT_NAME(parent_object));
  entry = gnome_request_dialog(FALSE, temp_string, "", 256, 
			       ui_common_entry_name_cb,
			       &roi_name, GTK_WINDOW(ui_study->app));
  entry_return = gnome_dialog_run_and_close(GNOME_DIALOG(entry));
  g_free(temp_string);
  
  if (entry_return == 1) {

    roi = amitk_roi_new(roi_type);
    amitk_object_set_name(AMITK_OBJECT(roi), roi_name);
    amitk_object_add_child(parent_object, AMITK_OBJECT(roi));
    g_object_unref(roi); /* don't want an extra ref */

    if (AMITK_ROI_UNDRAWN(roi)) /* undrawn roi's selected to begin with*/
      amitk_object_set_selected(AMITK_OBJECT(roi), TRUE, AMITK_SELECTION_SELECTED_0);
  
    if (ui_study->study_altered != TRUE) {
      ui_study->study_altered=TRUE;
      ui_study->study_virgin=FALSE;
      ui_study_update_title(ui_study);
    }
  }

  return;
}


void ui_study_update_canvas_visible_buttons(ui_study_t * ui_study) {

  AmitkView i_view;

  for (i_view=0; i_view < AMITK_VIEW_NUM; i_view++) {
    g_signal_handlers_block_by_func(G_OBJECT(ui_study->canvas_visible_button[i_view]),
				    G_CALLBACK(ui_study_cb_canvas_visible), ui_study);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_study->canvas_visible_button[i_view]),
				 AMITK_STUDY_CANVAS_VISIBLE(ui_study->study, i_view));
    g_signal_handlers_unblock_by_func(G_OBJECT(ui_study->canvas_visible_button[i_view]),
				      G_CALLBACK(ui_study_cb_canvas_visible), ui_study);

  }

  return;
}


/* function to update the text in the time dialog popup widget */
void ui_study_update_time_button(AmitkStudy * study, GtkWidget * time_button) {

  gchar * temp_string;
  
  g_return_if_fail(AMITK_IS_STUDY(study));

  temp_string = g_strdup_printf("%5.1f-%5.1f s",
				AMITK_STUDY_VIEW_START_TIME(study),
				AMITK_STUDY_VIEW_START_TIME(study)+
				AMITK_STUDY_VIEW_DURATION(study));
  gtk_label_set_text(GTK_LABEL(GTK_BIN(time_button)->child),temp_string);
  g_free(temp_string);

  return;
}


/* This function updates the little info box which tells us what the different 
   mouse buttons will do */
void ui_study_update_help_info(ui_study_t * ui_study, AmitkHelpInfo which_info, 
			       AmitkPoint new_point, amide_data_t value) {

  help_info_line_t i_line;
  gchar * location_text[2];
  gchar * legend;
  AmitkPoint location_p;

  if (which_info == AMITK_HELP_INFO_UPDATE_LOCATION) {
    location_text[0] = g_strdup_printf("[x,y,z] = [% 5.2f,% 5.2f,% 5.2f]", 
				       new_point.x, new_point.y, new_point.z);
    location_text[1] = g_strdup_printf("value  = % 5.3g", value);
  } else if (which_info == AMITK_HELP_INFO_UPDATE_SHIFT) {
    location_text[0] = g_strdup_printf("shift (x,y,z) =");
    location_text[1] = g_strdup_printf("[% 5.2f,% 5.2f,% 5.2f]", 
				     new_point.x, new_point.y, new_point.z);
  } else if (which_info == AMITK_HELP_INFO_UPDATE_THETA) {
    location_text[0] = g_strdup("");
    location_text[1] = g_strdup_printf("theta = % 5.3f", value);

  } else {

    for (i_line=0; i_line < HELP_INFO_LINE_BLANK;i_line++) {

      /* the line's legend */
      if (strlen(help_info_lines[which_info][i_line]) > 0) {
	if (i_line == HELP_INFO_LINE_VARIABLE) {
	  if ((which_info == AMITK_HELP_INFO_CANVAS_ROI) ||
	      (which_info == AMITK_HELP_INFO_CANVAS_ISOCONTOUR_ROI)) {
	    legend = help_info_variable_legend[HELP_INFO_VARIABLE_LINE_SHIFT_CTRL_3];
	  } else {
	    legend = help_info_variable_legend[HELP_INFO_VARIABLE_LINE_CTRL_X];
	  }
	} else {
	  legend = help_info_legends[i_line];
	}
      } else {
	legend = "";
      }
      if (ui_study->help_legend[i_line] == NULL) 
	ui_study->help_legend[i_line] = 
	  gnome_canvas_item_new(gnome_canvas_root(ui_study->help_info),
				gnome_canvas_text_get_type(),
				"justification", GTK_JUSTIFY_RIGHT,
				"anchor", GTK_ANCHOR_NORTH_EAST,
				"text", legend,
				"x", (gdouble) 20.0,
				"y", (gdouble) (i_line*HELP_INFO_LINE_HEIGHT),
				"fill_color", "black", 
				"font", UI_STUDY_HELP_FONT, NULL);
      else /* just need to change the text */
	gnome_canvas_item_set(ui_study->help_legend[i_line], "text", legend, NULL);
      
      /* and the button info */
      if (ui_study->help_line[i_line] == NULL) 
	ui_study->help_line[i_line] = 
	  gnome_canvas_item_new(gnome_canvas_root(ui_study->help_info),
				gnome_canvas_text_get_type(),
				"justification", GTK_JUSTIFY_LEFT,
				"anchor", GTK_ANCHOR_NORTH_WEST,
				"text", help_info_lines[which_info][i_line],
				"x", (gdouble) 30.0,
				"y", (gdouble) (i_line*HELP_INFO_LINE_HEIGHT),
				"fill_color", "black", 
				"font", UI_STUDY_HELP_FONT, NULL);
      else /* just need to change the text */
	gnome_canvas_item_set(ui_study->help_line[i_line], "text", 
			      help_info_lines[which_info][i_line], NULL);
    }

    location_p = AMITK_STUDY_VIEW_CENTER(ui_study->study);
    location_text[0] = g_strdup_printf("view center (x,y,z) =");
    location_text[1] = g_strdup_printf("[% 5.2f,% 5.2f,% 5.2f]", 
				     new_point.x, new_point.y, new_point.z);
  }

  /* update the location display */
  for (i_line=HELP_INFO_LINE_LOCATION1; i_line <= HELP_INFO_LINE_LOCATION2;i_line++) {
    if (ui_study->help_line[i_line] == NULL) 
      ui_study->help_line[i_line] =
	gnome_canvas_item_new(gnome_canvas_root(ui_study->help_info),
			      gnome_canvas_text_get_type(),
			      "justification", GTK_JUSTIFY_LEFT,
			      "anchor", GTK_ANCHOR_NORTH_WEST,
			      "text", location_text[i_line-HELP_INFO_LINE_LOCATION1],
			      "x", (gdouble) 0.0,
			      "y", (gdouble) (i_line*HELP_INFO_LINE_HEIGHT),
			      "fill_color", "black",
			      "font", UI_STUDY_HELP_FONT, NULL);

    else /* just need to change the text */
      gnome_canvas_item_set(ui_study->help_line[i_line], "text", 
			    location_text[i_line-HELP_INFO_LINE_LOCATION1],  NULL);

    g_free(location_text[i_line-HELP_INFO_LINE_LOCATION1]);
  }

  return;
}


/* updates the settings of the thickness spin button, will not change anything about the canvas */
void ui_study_update_thickness(ui_study_t * ui_study, amide_real_t thickness) {

  amide_real_t min_voxel_size, max_size;

  /* there's no spin button if we don't create the toolbar at this moment */
  if (ui_study->thickness_spin == NULL) return;

  min_voxel_size = amitk_data_sets_get_min_voxel_size(AMITK_OBJECT_CHILDREN(ui_study->study));
  max_size = amitk_volumes_get_max_size(AMITK_OBJECT_CHILDREN(ui_study->study));
    
  if ((min_voxel_size < 0)  || (max_size < 0)) return; /* no valid objects */
  
  /* block signals to the spin button, as we only want to
     change the value of the spin button, it's up to the caller of this
     function to change anything on the actual canvases... we'll 
     unblock at the end of this function */
  g_signal_handlers_block_by_func(G_OBJECT(ui_study->thickness_spin), 
				  G_CALLBACK(ui_study_cb_thickness), ui_study);
  
  /* set the current thickness if it hasn't already been set or if it's no longer valid*/
  if (thickness < min_voxel_size)
    thickness = min_voxel_size;
  
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui_study->thickness_spin), thickness);
  gtk_spin_button_set_increments(GTK_SPIN_BUTTON(ui_study->thickness_spin), min_voxel_size, min_voxel_size);
  gtk_spin_button_set_range(GTK_SPIN_BUTTON(ui_study->thickness_spin), min_voxel_size, max_size);
  gtk_spin_button_configure(GTK_SPIN_BUTTON(ui_study->thickness_spin),NULL, thickness, 
			    gtk_spin_button_get_digits(GTK_SPIN_BUTTON(ui_study->thickness_spin)));
  /* and now, reconnect the signal */
  g_signal_handlers_unblock_by_func(G_OBJECT(ui_study->thickness_spin),
				    G_CALLBACK(ui_study_cb_thickness), ui_study);

  return;
}

/* updates the settings of the zoom spinbutton, will not change anything about the canvas */
void ui_study_update_zoom(ui_study_t * ui_study) {

  /* there's no spin button if we don't create the toolbar at this moment */
  if (ui_study->zoom_spin == NULL) return;

  /* block signals to the zoom spin button, as we only want to
     change the value of the spin button, it's up to the caller of this
     function to change anything on the actual canvases... we'll 
     unblock at the end of this function */
  g_signal_handlers_block_by_func(G_OBJECT(ui_study->zoom_spin),
				  G_CALLBACK(ui_study_cb_zoom), ui_study);
  
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(ui_study->zoom_spin), 
			    AMITK_STUDY_ZOOM(ui_study->study));
  
  /* and now, reconnect the signal */
  g_signal_handlers_unblock_by_func(G_OBJECT(ui_study->zoom_spin),
				    G_CALLBACK(ui_study_cb_zoom), ui_study);

  return;
}

/* updates the settings of the interpolation radio button, will not change canvas */
void ui_study_update_interpolation(ui_study_t * ui_study) {

  AmitkInterpolation i_interpolation;
  AmitkInterpolation interpolation;

  if (ui_study->active_ds == NULL) {
    for (i_interpolation = 0; i_interpolation < AMITK_INTERPOLATION_NUM; i_interpolation++)
      gtk_widget_set_sensitive(ui_study->interpolation_button[i_interpolation], FALSE);
  } else {
    for (i_interpolation = 0; i_interpolation < AMITK_INTERPOLATION_NUM; i_interpolation++)
      gtk_widget_set_sensitive(ui_study->interpolation_button[i_interpolation], TRUE);

    interpolation = AMITK_DATA_SET_INTERPOLATION(ui_study->active_ds);

    /* block signals, as we only want to change the value, it's up to the caller of this
       function to change anything on the actual canvases... 
       we'll unblock at the end of this function */
    for (i_interpolation = 0; i_interpolation < AMITK_INTERPOLATION_NUM; i_interpolation++) 
      g_signal_handlers_block_by_func(G_OBJECT(ui_study->interpolation_button[i_interpolation]),
				      G_CALLBACK(ui_study_cb_interpolation), ui_study);

    /* need the button pressed to get the display to update correctly */
    gtk_button_pressed(GTK_BUTTON(ui_study->interpolation_button[interpolation]));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_study->interpolation_button[interpolation]),
				 TRUE);

    for (i_interpolation = 0; i_interpolation < AMITK_INTERPOLATION_NUM; i_interpolation++)
      g_signal_handlers_unblock_by_func(G_OBJECT(ui_study->interpolation_button[i_interpolation]),
					G_CALLBACK(ui_study_cb_interpolation),  ui_study);
  }

}

void ui_study_update_fuse_type(ui_study_t * ui_study) {

  AmitkFuseType i_fuse_type;
  
  g_return_if_fail(ui_study->study != NULL);

  for (i_fuse_type = 0; i_fuse_type < AMITK_FUSE_TYPE_NUM; i_fuse_type++) {
    g_signal_handlers_block_by_func(G_OBJECT(ui_study->fuse_type_button[i_fuse_type]),
				    G_CALLBACK(ui_study_cb_fuse_type), ui_study);
  }
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_study->fuse_type_button[AMITK_STUDY_FUSE_TYPE(ui_study->study)]),
			       TRUE);
  for (i_fuse_type = 0; i_fuse_type < AMITK_FUSE_TYPE_NUM; i_fuse_type++)
    g_signal_handlers_unblock_by_func(G_OBJECT(ui_study->fuse_type_button[i_fuse_type]),
				      G_CALLBACK(ui_study_cb_fuse_type),  ui_study);

}

void ui_study_update_view_mode(ui_study_t * ui_study) {

  AmitkViewMode i_view_mode;

  g_return_if_fail(ui_study->study != NULL);

  for (i_view_mode = 0; i_view_mode < AMITK_VIEW_MODE_NUM; i_view_mode++) 
    g_signal_handlers_block_by_func(G_OBJECT(ui_study->view_mode_button[i_view_mode]),
				   G_CALLBACK(ui_study_cb_view_mode), ui_study);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ui_study->view_mode_button[AMITK_STUDY_VIEW_MODE(ui_study->study)]),
			       TRUE);
  
  for (i_view_mode = 0; i_view_mode < AMITK_VIEW_MODE_NUM; i_view_mode++)
    g_signal_handlers_unblock_by_func(G_OBJECT(ui_study->view_mode_button[i_view_mode]),
				      G_CALLBACK(ui_study_cb_view_mode),ui_study);

}
 
void ui_study_update_title(ui_study_t * ui_study) {
  
  gchar * title;

  if (ui_study->study_altered) 
    title = g_strdup_printf("Study: %s *", AMITK_OBJECT_NAME(ui_study->study));
  else
    title = g_strdup_printf("Study: %s",AMITK_OBJECT_NAME(ui_study->study));
  gtk_window_set_title(GTK_WINDOW(ui_study->app), title);
  g_free(title);

}


void ui_study_update_layout(ui_study_t * ui_study) {

  AmitkView i_view;
  gboolean canvas_table_new;
  AmitkViewMode i_view_mode;
  gint row, column, table_column = 0, table_row = 0;
  gint max_width=0, max_height=0, width, height;
  
  g_return_if_fail(ui_study->study != NULL);

  /* get rid of visible canvases that are no longer visible */
  for (i_view_mode =  AMITK_STUDY_VIEW_MODE(ui_study->study)+1; 
       i_view_mode < AMITK_VIEW_MODE_NUM; 
       i_view_mode++) {
    if (ui_study->canvas_table[i_view_mode] != NULL) {
      gtk_widget_destroy(ui_study->canvas_table[i_view_mode]);
      ui_study->canvas_table[i_view_mode] = NULL;
      for (i_view=0; i_view < AMITK_VIEW_NUM; i_view++)
	ui_study->canvas[i_view_mode][i_view] = NULL;
    }
  }
  for (i_view_mode = 0; i_view_mode <= AMITK_STUDY_VIEW_MODE(ui_study->study); i_view_mode++) {
    for (i_view = 0; i_view < AMITK_VIEW_NUM; i_view++) {
      if ((!AMITK_STUDY_CANVAS_VISIBLE(ui_study->study, i_view)) &&
	  (ui_study->canvas[i_view_mode][i_view] != NULL)) {
	gtk_widget_destroy(ui_study->canvas[i_view_mode][i_view]);
	ui_study->canvas[i_view_mode][i_view] = NULL;
      }
    }
  }


  for (i_view_mode = 0; i_view_mode <= AMITK_STUDY_VIEW_MODE(ui_study->study); i_view_mode++) {

    if (ui_study->canvas_table[i_view_mode] == NULL) {
      ui_study->canvas_table[i_view_mode] = gtk_table_new(3, 2,FALSE);
      canvas_table_new = TRUE;
    } else {
      canvas_table_new = FALSE;
    }


    for (i_view = 0; i_view < AMITK_VIEW_NUM; i_view++) {

      if (AMITK_STUDY_CANVAS_VISIBLE(ui_study->study, i_view)) {
	if (ui_study->canvas[i_view_mode][i_view] == NULL) { /* new canvas */
	  ui_study->canvas[i_view_mode][i_view] = 
	    amitk_canvas_new(i_view, i_view_mode,
			     ui_study->canvas_layout,
			     ui_study->line_style,
			     ui_study->roi_width,
			     TRUE,
			     ui_study->canvas_maintain_size,
			     ui_study->canvas_leave_target,
			     ui_study->canvas_target_empty_area);
	  g_object_ref(G_OBJECT(ui_study->canvas[i_view_mode][i_view])); /* will be removed below */

	  amitk_canvas_set_study(AMITK_CANVAS(ui_study->canvas[i_view_mode][i_view]), 
				 ui_study->study);
	  amitk_canvas_set_active_data_set(AMITK_CANVAS(ui_study->canvas[i_view_mode][i_view]), 
					   ui_study->active_ds);
	
	  g_signal_connect(G_OBJECT(ui_study->canvas[i_view_mode][i_view]), "help_event",
			   G_CALLBACK(ui_study_cb_canvas_help_event), ui_study);
	  g_signal_connect(G_OBJECT(ui_study->canvas[i_view_mode][i_view]), "view_changing",
			   G_CALLBACK(ui_study_cb_canvas_view_changing), ui_study);
	  g_signal_connect(G_OBJECT(ui_study->canvas[i_view_mode][i_view]), "view_changed",
			   G_CALLBACK(ui_study_cb_canvas_view_changed), ui_study);
	  g_signal_connect(G_OBJECT(ui_study->canvas[i_view_mode][i_view]), "erase_volume",
			   G_CALLBACK(ui_study_cb_canvas_erase_volume), ui_study);
	  g_signal_connect(G_OBJECT(ui_study->canvas[i_view_mode][i_view]), "new_object",
			   G_CALLBACK(ui_study_cb_canvas_new_object), ui_study);
	
	} else { /* not a new canvas */
	  gtk_widget_hide(ui_study->canvas[i_view_mode][i_view]); /* hide, so we get more fluid moving */
	  /* add ref so it's not destroyed when we remove it from the container */
	  g_object_ref(G_OBJECT(ui_study->canvas[i_view_mode][i_view]));
	  gtk_container_remove(GTK_CONTAINER(ui_study->canvas_table[i_view_mode]), 
			       ui_study->canvas[i_view_mode][i_view]);
	}
      }
    }
	  

    g_object_ref(G_OBJECT(ui_study->canvas_table[i_view_mode]));
    if (!canvas_table_new)
      gtk_container_remove(GTK_CONTAINER(ui_study->center_table),
			   ui_study->canvas_table[i_view_mode]);

    for (i_view=0; i_view < AMITK_VIEW_NUM ;i_view++ )
      if (ui_study->canvas[i_view_mode][i_view] != NULL)
	amitk_canvas_set_layout(AMITK_CANVAS(ui_study->canvas[i_view_mode][i_view]), 
				ui_study->canvas_layout);

    /* keep an estimate as to how large each set of canvases is in max_width and max_height*/
    width = height = 0;
    switch(ui_study->canvas_layout) {
    case AMITK_LAYOUT_ORTHOGONAL:
      if (ui_study->canvas[i_view_mode][AMITK_VIEW_TRANSVERSE] != NULL) {
	width += amitk_canvas_get_width(AMITK_CANVAS(ui_study->canvas[i_view_mode][AMITK_VIEW_TRANSVERSE]));
	height += amitk_canvas_get_height(AMITK_CANVAS(ui_study->canvas[i_view_mode][AMITK_VIEW_TRANSVERSE]));
      } 

      if (ui_study->canvas[i_view_mode][AMITK_VIEW_CORONAL] != NULL) {
	height += amitk_canvas_get_height(AMITK_CANVAS(ui_study->canvas[i_view_mode][AMITK_VIEW_CORONAL]));
	if (ui_study->canvas[i_view_mode][AMITK_VIEW_TRANSVERSE] == NULL) {
	  width += amitk_canvas_get_width(AMITK_CANVAS(ui_study->canvas[i_view_mode][AMITK_VIEW_CORONAL]));
	}
      }

      if (ui_study->canvas[i_view_mode][AMITK_VIEW_SAGITTAL] != NULL) {
	width += amitk_canvas_get_width(AMITK_CANVAS(ui_study->canvas[i_view_mode][AMITK_VIEW_SAGITTAL]));
	if ((ui_study->canvas[i_view_mode][AMITK_VIEW_TRANSVERSE] == NULL)  &&
	    (ui_study->canvas[i_view_mode][AMITK_VIEW_CORONAL] == NULL)) {
	  height += amitk_canvas_get_height(AMITK_CANVAS(ui_study->canvas[i_view_mode][AMITK_VIEW_SAGITTAL]));
	}
      }

      if (width > max_width) max_width = width;
      if (height > max_height) max_height = height;

      break;
    case AMITK_LAYOUT_LINEAR:
    default:
      for (i_view=0; i_view < AMITK_VIEW_NUM ;i_view++ ) {
	if (ui_study->canvas[i_view_mode][i_view] != NULL) {
	  height = amitk_canvas_get_height(AMITK_CANVAS(ui_study->canvas[i_view_mode][i_view]));
	  if (height > max_height) max_height = height;
	  width += amitk_canvas_get_width(AMITK_CANVAS(ui_study->canvas[i_view_mode][i_view]));
	}
      }
      if (width > max_width) max_width = width;
      break;
    }
  }


  height = width = 0;
  for (i_view_mode = 0; i_view_mode <= AMITK_STUDY_VIEW_MODE(ui_study->study); i_view_mode++) {

    /* put the canvases in the table according to the desired layout */
    switch(ui_study->canvas_layout) {
    case AMITK_LAYOUT_ORTHOGONAL:
      row = column = 0;
      if (ui_study->canvas[i_view_mode][AMITK_VIEW_TRANSVERSE] != NULL) {
	gtk_table_attach(GTK_TABLE(ui_study->canvas_table[i_view_mode]), 
			 ui_study->canvas[i_view_mode][AMITK_VIEW_TRANSVERSE], 
			 column,column+1, row,row+1,FALSE,FALSE, X_PADDING, Y_PADDING);
	row++;
      }
      if (ui_study->canvas[i_view_mode][AMITK_VIEW_CORONAL] != NULL) {
	gtk_table_attach(GTK_TABLE(ui_study->canvas_table[i_view_mode]), 
			 ui_study->canvas[i_view_mode][AMITK_VIEW_CORONAL], 
			 column, column+1, row, row+1, FALSE,FALSE, X_PADDING, Y_PADDING);
      }
      row = 0;
      column++;
      if (ui_study->canvas[i_view_mode][AMITK_VIEW_SAGITTAL] != NULL)
	gtk_table_attach(GTK_TABLE(ui_study->canvas_table[i_view_mode]), 
			 ui_study->canvas[i_view_mode][AMITK_VIEW_SAGITTAL], 
			 column, column+1,row, row+1, FALSE,FALSE, X_PADDING, Y_PADDING);

      break;
    case AMITK_LAYOUT_LINEAR:
    default:
      for (i_view=0;i_view< AMITK_VIEW_NUM;i_view++)
	if (ui_study->canvas[i_view_mode][i_view] != NULL)
	  gtk_table_attach(GTK_TABLE(ui_study->canvas_table[i_view_mode]), 
			   ui_study->canvas[i_view_mode][i_view], 
			   i_view, i_view+1, 0,1, FALSE, FALSE, X_PADDING, Y_PADDING);

      break;
    }


    /* figure out where this group of canvases should go */
    /* this only works for up to 3 sets of canvases */
    if ((height == 0) && (width == 0)) {
      /* first iteration */
      height += max_height;
      width += max_width;
    } else {
      if (height > width/1.5) { /* the 1.5 is to favour horizontal placement */
	table_column++;
	table_row = 0;
	width += max_width;
      } else {
	height += max_height;
	table_row++;
	table_column = 0;
      }
    }

    /* and place them */
    gtk_table_attach(GTK_TABLE(ui_study->center_table), ui_study->canvas_table[i_view_mode],
		     table_column, table_column+1, table_row, table_row+1,
		     X_PACKING_OPTIONS | GTK_FILL, Y_PACKING_OPTIONS | GTK_FILL,  
		     X_PADDING, Y_PADDING);



    /* remove the additional reference */
    for (i_view = 0; i_view < AMITK_VIEW_NUM; i_view++)
      if (ui_study->canvas[i_view_mode][i_view] != NULL)
	g_object_unref(G_OBJECT(ui_study->canvas[i_view_mode][i_view]));
    g_object_unref(G_OBJECT(ui_study->canvas_table[i_view_mode]));
    gtk_widget_show_all(ui_study->canvas_table[i_view_mode]); /* and show */

      
  }


  return;
}

/* function to setup the widgets inside of the GnomeApp study */
void ui_study_setup_widgets(ui_study_t * ui_study) {

  guint main_table_row=0; 
  guint main_table_column=0;
  GtkWidget * scrolled;

  /* make and add the main packing table */
  ui_study->main_table = gtk_table_new(3,2,FALSE);
  gnome_app_set_contents(GNOME_APP(ui_study->app), ui_study->main_table);

  /* connect the blank help signal */
  g_object_set_data(G_OBJECT(ui_study->app), "which_help", GINT_TO_POINTER(AMITK_HELP_INFO_BLANK));
  g_signal_connect(G_OBJECT(ui_study->app), "enter_notify_event",
		   G_CALLBACK(ui_study_cb_update_help_info), ui_study);

  ui_study->tree = amitk_tree_new();

  g_signal_connect(G_OBJECT(ui_study->tree), "help_event",
		   G_CALLBACK(ui_study_cb_tree_help_event), ui_study);
  g_signal_connect(G_OBJECT(ui_study->tree), "activate_object", 
		   G_CALLBACK(ui_study_cb_tree_activate_object), ui_study);
  g_signal_connect(G_OBJECT(ui_study->tree), "popup_object", 
		   G_CALLBACK(ui_study_cb_tree_popup_object), ui_study);
  g_signal_connect(G_OBJECT(ui_study->tree), "add_object", 
		   G_CALLBACK(ui_study_cb_tree_add_object), ui_study);
  g_signal_connect(G_OBJECT(ui_study->tree), "delete_object", 
		   G_CALLBACK(ui_study_cb_tree_delete_object), ui_study);
      
  /* make a scrolled area for the tree */
  scrolled = gtk_scrolled_window_new(NULL,NULL);  
  gtk_widget_set_size_request(scrolled,250,250);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), 
					ui_study->tree);
  gtk_table_attach(GTK_TABLE(ui_study->main_table), scrolled, 
		   main_table_column, main_table_column+1, main_table_row, main_table_row+1,
		   X_PACKING_OPTIONS | GTK_FILL, Y_PACKING_OPTIONS | GTK_FILL, 
		   X_PADDING, Y_PADDING);

  main_table_row++;


  /* the help information canvas */
  ui_study->help_info = GNOME_CANVAS(gnome_canvas_new_aa());
  gtk_table_attach(GTK_TABLE(ui_study->main_table), GTK_WIDGET(ui_study->help_info), 
		   main_table_column, main_table_column+1, main_table_row, main_table_row+1,
		   X_PACKING_OPTIONS | GTK_FILL, 0, X_PADDING, Y_PADDING);
  gtk_widget_set_size_request(GTK_WIDGET(ui_study->help_info), 150,
			      HELP_INFO_LINE_HEIGHT*NUM_HELP_INFO_LINES);
  gnome_canvas_set_scroll_region(ui_study->help_info, 0.0, 0.0, 150.0, 
				 HELP_INFO_LINE_HEIGHT*NUM_HELP_INFO_LINES);
  main_table_column++;
  main_table_row=0;

  /* make the stuff in the center */
  ui_study->center_table = gtk_table_new(2, 2,FALSE);
  gtk_table_attach(GTK_TABLE(ui_study->main_table), ui_study->center_table,
		   main_table_column, main_table_column+1, main_table_row, main_table_row+3,
  		   X_PACKING_OPTIONS | GTK_FILL, Y_PACKING_OPTIONS | GTK_FILL,  X_PADDING, Y_PADDING);
  main_table_column++;
  main_table_row=0;
  
  return;
}



/* replace what's currently in the ui_study with the specified study */
void ui_study_set_study(ui_study_t * ui_study, AmitkStudy * study) {

  add_object(ui_study, AMITK_OBJECT(study));
  ui_study->study_altered=FALSE;
  ui_study_update_title(ui_study);
  ui_study_make_active_data_set(ui_study, NULL);

}



/* procedure to set up the study window */
GtkWidget * ui_study_create(AmitkStudy * study) {

  ui_study_t * ui_study;
  gchar * temp_string;

  ui_study = ui_study_init();

  /* setup the study window */
  ui_study->app=gnome_app_new(PACKAGE, NULL);

  //  gtk_window_set_policy (GTK_WINDOW(ui_study->app), TRUE, TRUE, TRUE);

  /* disable user resizability, this allows the window to autoshrink */
  gtk_window_set_resizable(GTK_WINDOW(ui_study->app), FALSE);

  /* setup the callbacks for app */
  g_signal_connect(G_OBJECT(ui_study->app), "realize", 
		   G_CALLBACK(ui_common_window_realize_cb), NULL);
  g_signal_connect(G_OBJECT(ui_study->app), "delete_event",  
		   G_CALLBACK(ui_study_cb_delete_event), ui_study);

  ui_study->progress_dialog = amitk_progress_dialog_new(GTK_WINDOW(ui_study->app));

  /* setup the study menu */
  ui_study_menus_create(ui_study);

  /* setup the toolbar */
  ui_study_toolbar_create(ui_study);

  /* setup the rest of the study window */
  ui_study_setup_widgets(ui_study);


  /* add the study to the ui_study */
  if (study == NULL) {

    study = amitk_study_new();
    temp_string = g_strdup_printf("temp_%d",next_study_num++);
    amitk_object_set_name(AMITK_OBJECT(study), temp_string);
    g_free(temp_string);

    ui_study_set_study(ui_study, study);
    g_object_unref(study);
    ui_study->study_virgin=TRUE;

  } else {

    ui_study_set_study(ui_study, study);
    ui_study->study_virgin=FALSE;

  }
  ui_study_update_title(ui_study);

  /* get the study window running */
  gtk_widget_show_all(ui_study->app);
  amide_register_window((gpointer) ui_study->app);

  return ui_study->app;
}
 
