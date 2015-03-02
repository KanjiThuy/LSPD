/* raw_data_import.c
 *
 * Part of amide - Amide's a Medical Image Dataset Examiner
 * Copyright (C) 2001-2006 Andy Loening
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
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <string.h>
#include "raw_data_import.h"
#include "amitk_progress_dialog.h"

#ifndef AMIDE_WIN32_HACKS
#include <libgnome/libgnome.h>
#else
static AmitkModality last_modality = AMITK_MODALITY_PET;
static AmitkRawFormat last_raw_format = AMITK_RAW_FORMAT_UBYTE_8_NE;
static AmitkVoxel last_data_dim=ONE_VOXEL;
static AmitkPoint last_voxel_size = ONE_POINT;
static guint last_offset = 0;
static amide_data_t last_scale_factor=1.0;
#endif



/* raw_data information structure */
typedef struct raw_data_info_t {
  gchar * filename;
  gchar * name;
  guint total_file_size;
  AmitkRawFormat raw_format;
  AmitkVoxel data_dim;
  AmitkPoint voxel_size;
  AmitkModality modality;
  amide_data_t scale_factor;
  guint offset;

  GtkWidget * num_bytes_label1;
  GtkWidget * num_bytes_label2;
  GtkWidget * read_offset_label;
  GtkWidget * ok_button;

} raw_data_info_t;



static void change_name_cb(GtkWidget * widget, gpointer data);
static void change_scaling_cb(GtkWidget * widget, gpointer data);
static void change_entry_cb(GtkWidget * widget, gpointer data);
static void change_modality_cb(GtkWidget * widget, gpointer data);
static void change_raw_format_cb(GtkWidget * widget, gpointer data);


static guint update_num_bytes(raw_data_info_t * raw_data_info);
static void read_last_values(AmitkModality * plast_modality,
			     AmitkRawFormat * plast_raw_format,
			     AmitkVoxel * plast_data_dim,
			     AmitkPoint * plast_voxel_size,
			     guint * plast_offset,
			     amide_data_t *plast_scale_factor);
static GtkWidget * import_dialog(raw_data_info_t * raw_data_info);
static void update_offset_label(raw_data_info_t * raw_data_info);



/* function called when the name of the data set has been changed */
static void change_name_cb(GtkWidget * widget, gpointer data) {

  raw_data_info_t * raw_data_info = data;
  gchar * new_name;

  /* get the contents of the name entry box and save it */
  new_name = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
  g_free(raw_data_info->name);
  raw_data_info->name = new_name;

  return;
}

/* function called to change the scaling factor */
static void change_scaling_cb(GtkWidget * widget, gpointer data) {

  gchar * str;
  gint error;
  gdouble temp_real;
  raw_data_info_t * raw_data_info = data;

  /* get the contents of the name entry box */
  str = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);

  /* convert to the correct number */
  error = sscanf(str, "%lf", &temp_real);
  if (error == EOF)
    return; /* make sure it's a valid number */
  g_free(str);

  /* and save the value */
  raw_data_info->scale_factor = temp_real;

#ifndef AMIDE_WIN32_HACKS
  gnome_config_push_prefix("/"PACKAGE"/");
  gnome_config_set_float("RAWDATAIMPORT/LastScaleFactor", raw_data_info->scale_factor);
  gnome_config_pop_prefix();
  gnome_config_sync();
#else
  last_scale_factor = raw_data_info->scale_factor;
#endif
  return;
}
      

/* function called when a numerical entry of the data set has been changed, 
   used for the dimensions and voxel_size */
static void change_entry_cb(GtkWidget * widget, gpointer data) {

  gchar * str;
  gint error;
  gdouble temp_real=0.0;
  gint temp_int=0;
  guint which_widget;
  raw_data_info_t * raw_data_info = data;

  /* figure out which widget this is */
  which_widget = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "type")); 

  /* get the contents of the name entry box */
  str = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);


  /* convert to the correct number */
  if ((which_widget < AMITK_DIM_NUM) || (which_widget == AMITK_DIM_NUM+AMITK_AXIS_NUM)) {
    error = sscanf(str, "%d", &temp_int);
    if (error == EOF)
      return; /* make sure it's a valid number */
    if (temp_int < 0)
      return;
  } else { /* one of the voxel_size widgets */
    error = sscanf(str, "%lf", &temp_real);
    if (error == EOF) /* make sure it's a valid number */
      return;
    if (temp_real < 0.0)
      return;
  }
  g_free(str);

  /* and save the value in our temporary data set structure */
  switch(which_widget) {
  case AMITK_DIM_X:
    raw_data_info->data_dim.x = temp_int;
    break;
  case AMITK_DIM_Y:
    raw_data_info->data_dim.y = temp_int;
    break;
  case AMITK_DIM_Z:
    raw_data_info->data_dim.z = temp_int;
    break;
  case AMITK_DIM_G:
    raw_data_info->data_dim.g = temp_int;
    break;
  case AMITK_DIM_T:
    raw_data_info->data_dim.t = temp_int;
    break;
  case (AMITK_DIM_NUM+AMITK_AXIS_X):
    raw_data_info->voxel_size.x = temp_real;
    break;
  case (AMITK_DIM_NUM+AMITK_AXIS_Y):
    raw_data_info->voxel_size.y = temp_real;
    break;
  case (AMITK_DIM_NUM+AMITK_AXIS_Z):
    raw_data_info->voxel_size.z = temp_real;
    break;
  case (AMITK_DIM_NUM+AMITK_AXIS_NUM):
    raw_data_info->offset = temp_int;
    break;
  default:
    break; /* do nothing */
  }
  
  /* recalculate the total number of bytes to be read and have it displayed */
  update_num_bytes(raw_data_info);

#ifndef AMIDE_WIN32_HACKS
  gnome_config_push_prefix("/"PACKAGE"/");
  gnome_config_set_int("RAWDATAIMPORT/LastDataDimG", raw_data_info->data_dim.g);
  gnome_config_set_int("RAWDATAIMPORT/LastDataDimT", raw_data_info->data_dim.t);
  gnome_config_set_int("RAWDATAIMPORT/LastDataDimZ", raw_data_info->data_dim.z);
  gnome_config_set_int("RAWDATAIMPORT/LastDataDimY", raw_data_info->data_dim.y);
  gnome_config_set_int("RAWDATAIMPORT/LastDataDimX", raw_data_info->data_dim.x);
  gnome_config_set_float("RAWDATAIMPORT/LastVoxelSizeZ", raw_data_info->voxel_size.z);
  gnome_config_set_float("RAWDATAIMPORT/LastVoxelSizeY", raw_data_info->voxel_size.y);
  gnome_config_set_float("RAWDATAIMPORT/LastVoxelSizeX", raw_data_info->voxel_size.x);
  gnome_config_set_int("RAWDATAIMPORT/LastOffset", raw_data_info->offset);
  gnome_config_pop_prefix();
  gnome_config_sync();
#else
  last_data_dim = raw_data_info->data_dim;
  last_voxel_size = raw_data_info->voxel_size;
  last_offset = raw_data_info->offset;
#endif

  return;
}
      
      

/* function to change the modality */
static void change_modality_cb(GtkWidget * widget, gpointer data) {
  raw_data_info_t * raw_data_info = data;

#if 1
  raw_data_info->modality = gtk_option_menu_get_history(GTK_OPTION_MENU(widget));
#else
  raw_data_info->modality = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
#endif

#ifndef AMIDE_WIN32_HACKS
  gnome_config_push_prefix("/"PACKAGE"/");
  gnome_config_set_int("RAWDATAIMPORT/LastModality", raw_data_info->modality);
  gnome_config_pop_prefix();
  gnome_config_sync();
#else
  last_modality = raw_data_info->modality;
#endif

  return;
}

/* function to change the raw data file's data format */
static void change_raw_format_cb(GtkWidget * widget, gpointer data) {
  raw_data_info_t * raw_data_info = data;

#if 1
  /* figure out which menu item called me */
  raw_data_info->raw_format = 
    GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),"raw_format"));
#else
  raw_data_info->raw_format = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
#endif

  /* recalculate the total number of bytes to be read and have it displayed*/
  update_num_bytes(raw_data_info);

  /* update the offset label so it makes sense */
  update_offset_label(raw_data_info);

#ifndef AMIDE_WIN32_HACKS
  gnome_config_push_prefix("/"PACKAGE"/");
  gnome_config_set_int("RAWDATAIMPORT/LastRawFormat", raw_data_info->raw_format);
  gnome_config_pop_prefix();
  gnome_config_sync();
#else
  last_raw_format = raw_data_info->raw_format;
#endif
  return;
}








/* reset the label for the offset entry */
static void update_offset_label(raw_data_info_t * raw_data_info) {

  if (raw_data_info->raw_format == AMITK_RAW_FORMAT_ASCII_8_NE)
    gtk_label_set_text(GTK_LABEL(raw_data_info->read_offset_label), 
		       _("read offset (entries):"));
  else
    gtk_label_set_text(GTK_LABEL(raw_data_info->read_offset_label), 
		       _("read offset (bytes):"));
}

/* calculate the total amount of the file that will be read through */
static guint update_num_bytes(raw_data_info_t * raw_data_info) {

  guint num_bytes, num_entries;
  gchar * temp_string;
  
  /* how many bytes we're currently reading from the file */
  if (raw_data_info->raw_format == AMITK_RAW_FORMAT_ASCII_8_NE) {
    num_entries = raw_data_info->offset + 
      raw_data_info->data_dim.x*raw_data_info->data_dim.y*raw_data_info->data_dim.z*raw_data_info->data_dim.g*raw_data_info->data_dim.t;
    gtk_label_set_text(GTK_LABEL(raw_data_info->num_bytes_label1), 
		       _("total entries to read through:"));
    temp_string = g_strdup_printf("%d",num_entries);
    gtk_label_set_text(GTK_LABEL(raw_data_info->num_bytes_label2), temp_string);
    g_free(temp_string);

    /* and figure out a bare minimum for how many bytes might be in the file,
       this would correspond to all single digit numbers, seperated by spaces */
    num_bytes = 2*num_entries; 

  } else {
    num_bytes = 
      raw_data_info->offset +
      amitk_raw_format_calc_num_bytes(raw_data_info->data_dim, 
				      raw_data_info->raw_format);
    gtk_label_set_text(GTK_LABEL(raw_data_info->num_bytes_label1), 
		       _("total bytes to read through:"));
    temp_string = g_strdup_printf("%d",num_bytes);
    gtk_label_set_text(GTK_LABEL(raw_data_info->num_bytes_label2), temp_string);
    g_free(temp_string);
  }

  /* if we think we can load in the file, desensitise the "ok" button */
  gtk_widget_set_sensitive(raw_data_info->ok_button, 
			   (num_bytes <= raw_data_info->total_file_size) && (num_bytes > 0));

  return num_bytes;
}


static void read_last_values(AmitkModality * plast_modality,
			     AmitkRawFormat * plast_raw_format,
			     AmitkVoxel * plast_data_dim,
			     AmitkPoint * plast_voxel_size,
			     guint * plast_offset,
			     amide_data_t *plast_scale_factor) {
#ifdef AMIDE_WIN32_HACKS
  *plast_modality = last_modality;
  *plast_raw_format = last_raw_format;
  *plast_data_dim = last_data_dim;
  *plast_voxel_size = last_voxel_size;
  *plast_offset = last_offset;
  *plast_scale_factor = last_scale_factor;
#else
  gint default_value;
  gint temp_int;
  gfloat temp_float;

  gnome_config_push_prefix("/"PACKAGE"/");

  *plast_modality = gnome_config_get_int("RAWDATAIMPORT/LastModality");
  *plast_raw_format = gnome_config_get_int("RAWDATAIMPORT/LastRawFormat");

  temp_int = gnome_config_get_int_with_default("RAWDATAIMPORT/LastDataDimG",&default_value);
  (*plast_data_dim).g = default_value ? 1 : temp_int;
  temp_int = gnome_config_get_int_with_default("RAWDATAIMPORT/LastDataDimT",&default_value);
  (*plast_data_dim).t = default_value ? 1 : temp_int;
  temp_int = gnome_config_get_int_with_default("RAWDATAIMPORT/LastDataDimZ",&default_value);
  (*plast_data_dim).z = default_value ? 1 : temp_int;
  temp_int = gnome_config_get_int_with_default("RAWDATAIMPORT/LastDataDimY",&default_value);
  (*plast_data_dim).y = default_value ? 1 : temp_int; 
  temp_int = gnome_config_get_int_with_default("RAWDATAIMPORT/LastDataDimX",&default_value);
  (*plast_data_dim).x = default_value ? 1 : temp_int; 


  temp_float = gnome_config_get_float_with_default("RAWDATAIMPORT/LastVoxelSizeZ",&default_value);
  (*plast_voxel_size).z =  default_value ? 1.0 : temp_float;
  temp_float = gnome_config_get_float_with_default("RAWDATAIMPORT/LastVoxelSizeY",&default_value);
  (*plast_voxel_size).y =  default_value ? 1.0 : temp_float;
  temp_float = gnome_config_get_float_with_default("RAWDATAIMPORT/LastVoxelSizeX",&default_value);
  (*plast_voxel_size).x =  default_value ? 1.0 : temp_float;

  *plast_offset = gnome_config_get_int("RAWDATAIMPORT/LastOffset");

  temp_float = gnome_config_get_float_with_default("RAWDATAIMPORT/LastScaleFactor",&default_value);
  *plast_scale_factor =  default_value ? 1.0 : temp_float;

  gnome_config_pop_prefix();
#endif

  return;
}

/* function to bring up the dialog widget to direct our importing of raw data */
static GtkWidget * import_dialog(raw_data_info_t * raw_data_info) {

  AmitkModality i_modality;
  AmitkDim i_dim;
  AmitkRawFormat i_raw_format;
  AmitkAxis i_axis;
  gchar * temp_string = NULL;
  gchar ** frags;
  GtkWidget * dialog;
  GtkWidget * packing_table;
  GtkWidget * label;
  GtkWidget * entry;
#if 1
  GtkWidget * option_menu;
  GtkWidget * menuitem;
#endif
  GtkWidget * menu;
  guint table_row = 0;




  /* start making the import dialog */
  dialog = gtk_dialog_new();

  temp_string = g_strdup_printf(_("%s: Raw Data Import Dialog\n"), PACKAGE);
  gtk_window_set_title(GTK_WINDOW(dialog), temp_string);
  g_free(temp_string);

  raw_data_info->ok_button = gtk_dialog_add_button(GTK_DIALOG(dialog), 
						   GTK_STOCK_OK, GTK_RESPONSE_YES);
  gtk_dialog_add_button(GTK_DIALOG(dialog),
			GTK_STOCK_CANCEL, GTK_RESPONSE_CLOSE);

  /* make the packing table */
  packing_table = gtk_table_new(10,6,FALSE);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox),packing_table);

  /* widgets to change the roi's name */
  label = gtk_label_new(_("name:"));
  gtk_table_attach(GTK_TABLE(packing_table),
		   GTK_WIDGET(label), 0,1,
		   table_row, table_row+1,
		   0, 0,
		   X_PADDING, Y_PADDING);

  /* figure out an initial name for the data */
  temp_string = g_path_get_basename(raw_data_info->filename);
  /* remove the extension of the file */
  g_strreverse(temp_string);
  frags = g_strsplit(temp_string, ".", 2);
  g_free(temp_string);
  if (frags[1] != NULL)
    raw_data_info->name = strdup(frags[1]);
  else /* no extension on filename */
    raw_data_info->name = strdup(frags[0]);
  g_strfreev(frags); 
  g_strreverse(raw_data_info->name);

  entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry), raw_data_info->name);

  gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
  g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(change_name_cb), raw_data_info);
  gtk_table_attach(GTK_TABLE(packing_table),
		   GTK_WIDGET(entry),1,2,
		   table_row, table_row+1,
		   X_PACKING_OPTIONS, 0,
		   X_PADDING, Y_PADDING);
 
  table_row++;




  /* widgets to change the object's modality */
  label = gtk_label_new(_("modality:"));
  gtk_table_attach(GTK_TABLE(packing_table),
  		   GTK_WIDGET(label), 0,1,
  		   table_row, table_row+1,
  		   0, 0,
  		   X_PADDING, Y_PADDING);


#if 1
  option_menu = gtk_option_menu_new();
  menu = gtk_menu_new();

  for (i_modality=0; i_modality<AMITK_MODALITY_NUM; i_modality++) {
    menuitem = gtk_menu_item_new_with_label(amitk_modality_get_name(i_modality));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
  }
  
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), raw_data_info->modality);
  g_signal_connect(G_OBJECT(option_menu), "changed", G_CALLBACK(change_modality_cb), raw_data_info);
#else
  menu = gtk_combo_box_new_text();
  for (i_modality=0; i_modality<AMITK_MODALITY_NUM; i_modality++) 
    gtk_combo_box_append_text(GTK_COMBO_BOX(menu),
			      amitk_modality_get_name(i_modality));
  gtk_combo_box_set_active(GTK_COMBO_BOX(menu), raw_data_info->modality);
  g_signal_connect(G_OBJECT(menu), "changed", G_CALLBACK(change_modality_cb), raw_data_info);
#endif
  gtk_table_attach(GTK_TABLE(packing_table), 
#if 1
		   GTK_WIDGET(option_menu), 1,2, 
#else
		   GTK_WIDGET(menu), 1,2, 
#endif
		   table_row,table_row+1,
		   X_PACKING_OPTIONS | GTK_FILL, 0, 
		   X_PADDING, Y_PADDING);
  table_row++;


  /* widgets to change the raw data file's  data format */
  label = gtk_label_new(_("data format:"));
  gtk_table_attach(GTK_TABLE(packing_table),
		   GTK_WIDGET(label), 0,1,
		   table_row, table_row+1,
		   0, 0,
		   X_PADDING, Y_PADDING);
#if 1
  option_menu = gtk_option_menu_new();
  menu = gtk_menu_new();

  for (i_raw_format=0; i_raw_format<AMITK_RAW_FORMAT_NUM; i_raw_format++) {
    menuitem = gtk_menu_item_new_with_label(amitk_raw_format_names[i_raw_format]);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_object_set_data(G_OBJECT(menuitem), "raw_format", GINT_TO_POINTER(i_raw_format)); 
    g_signal_connect(G_OBJECT(menuitem), "activate", 
		     G_CALLBACK(change_raw_format_cb), raw_data_info);
  }
  
  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), raw_data_info->raw_format);
#else
  menu = gtk_combo_box_new_text();
  for (i_raw_format=0; i_raw_format<AMITK_RAW_FORMAT_NUM; i_raw_format++) 
    gtk_combo_box_append_text(GTK_COMBO_BOX(menu),
			      amitk_raw_format_names[i_raw_format]);
  gtk_combo_box_set_active(GTK_COMBO_BOX(menu), raw_data_info->raw_format);
  g_signal_connect(G_OBJECT(menu), "changed", 
		   G_CALLBACK(change_raw_format_cb), raw_data_info);
#endif
  gtk_table_attach(GTK_TABLE(packing_table), 
#if 1
		   GTK_WIDGET(option_menu), 1,2, 
#else
		   GTK_WIDGET(menu), 1,2, 
#endif
		   table_row,table_row+1,
		   X_PACKING_OPTIONS | GTK_FILL, 0, 
		   X_PADDING, Y_PADDING);

  /* how many bytes we can read from the file */
  label = gtk_label_new(_("file size (bytes):"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 3,4,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  /* how many bytes we're currently reading from the file */
  temp_string = g_strdup_printf("%d", raw_data_info->total_file_size);
  label = gtk_label_new(temp_string);
  g_free(temp_string);
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 4,5,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  table_row++;


  /* what offset in the raw_data file we should start reading at */
  raw_data_info->read_offset_label = gtk_label_new("");
  update_offset_label(raw_data_info);
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(raw_data_info->read_offset_label), 0,1,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);

  entry = gtk_entry_new();
  temp_string = g_strdup_printf("%d", raw_data_info->offset);
  gtk_entry_set_text(GTK_ENTRY(entry), temp_string);
  g_free(temp_string);
  gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
  g_object_set_data(G_OBJECT(entry), "type", GINT_TO_POINTER(AMITK_DIM_NUM+AMITK_AXIS_NUM));
  g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(change_entry_cb),  raw_data_info);
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(entry),1,2,
		   table_row, table_row+1, X_PACKING_OPTIONS, 0, X_PADDING, Y_PADDING);


  /* how many bytes we're currently reading from the file */
  raw_data_info->num_bytes_label1 = gtk_label_new("");
  raw_data_info->num_bytes_label2 = gtk_label_new("");
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(raw_data_info->num_bytes_label1), 
		   3,4, table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(raw_data_info->num_bytes_label2), 
		   4,5, table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  update_num_bytes(raw_data_info); /* put something sensible into the label */
  table_row++;



  /* labels for the x, y, and z components */
  label = gtk_label_new(_("x"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 1,2,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  label = gtk_label_new(_("y"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 2,3,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  label = gtk_label_new(_("z"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 3,4,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  label = gtk_label_new(_("gates"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 4,5,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  label = gtk_label_new(_("frames"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 5,6,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);
  table_row++;


  /* widgets to change the dimensions of the data set */
  label = gtk_label_new(_("dimensions (# voxels)"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 0,1,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);


  for (i_dim=0; i_dim<AMITK_DIM_NUM; i_dim++) {
    entry = gtk_entry_new();
    temp_string = g_strdup_printf("%d", voxel_get_dim(raw_data_info->data_dim, i_dim));
    gtk_entry_set_text(GTK_ENTRY(entry), temp_string);
    g_free(temp_string);
    gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
    g_object_set_data(G_OBJECT(entry), "type", GINT_TO_POINTER(i_dim));
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(change_entry_cb), raw_data_info);
    gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(entry),i_dim+1,i_dim+2,
		     table_row, table_row+1, X_PACKING_OPTIONS, 0, X_PADDING, Y_PADDING);
  }
  table_row++;

  /* widgets to change the voxel size of the data set */
  label = gtk_label_new(_("voxel size (mm)"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 0,1,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);


  for (i_axis=0; i_axis<AMITK_AXIS_NUM; i_axis++) {
    entry = gtk_entry_new();
    temp_string = g_strdup_printf("%g", point_get_component(raw_data_info->voxel_size, i_axis));
    gtk_entry_set_text(GTK_ENTRY(entry), temp_string);
    g_free(temp_string);
    gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
    g_object_set_data(G_OBJECT(entry), "type", GINT_TO_POINTER(i_axis+AMITK_DIM_NUM));
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(change_entry_cb), 
		       raw_data_info);
    gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(entry),i_axis+1,i_axis+2,
		     table_row, table_row+1, X_PACKING_OPTIONS, 0, X_PADDING, Y_PADDING);
  }
  table_row++;


  /* scale factor to apply to the data */
  label = gtk_label_new(_("scale factor"));
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(label), 0,1,
		   table_row, table_row+1, 0, 0, X_PADDING, Y_PADDING);

  entry = gtk_entry_new();
  temp_string = g_strdup_printf("%5.3f", raw_data_info->scale_factor);
  gtk_entry_set_text(GTK_ENTRY(entry), temp_string);
  g_free(temp_string);
  gtk_editable_set_editable(GTK_EDITABLE(entry), TRUE);
  g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(change_scaling_cb), raw_data_info);
  gtk_table_attach(GTK_TABLE(packing_table), GTK_WIDGET(entry),1,2,
		   table_row, table_row+1, X_PACKING_OPTIONS, 0, X_PADDING, Y_PADDING);
  table_row++;

  gtk_widget_show_all(dialog);

  return dialog;
}



/* function to bring up the dialog widget to direct our importing of raw data */
AmitkDataSet * raw_data_import(const gchar * raw_data_filename, AmitkPreferences * preferences) {

  struct stat file_info;
  raw_data_info_t * raw_data_info;
  AmitkDataSet * ds;
  gint dialog_reply;
  GtkWidget * dialog;
  GtkWidget * progress_dialog = NULL;
  gboolean return_val;

  /* get space for our raw_data_info structure */
  if ((raw_data_info = g_try_new(raw_data_info_t,1)) == NULL) {
    g_warning(_("Couldn't allocate space for raw_data_info structure for raw data import"));
    return NULL;
  }

  /* initialize */
  raw_data_info->filename = g_strdup(raw_data_filename);
  read_last_values(&(raw_data_info->modality), &(raw_data_info->raw_format), 
		   &(raw_data_info->data_dim), &(raw_data_info->voxel_size),
		   &(raw_data_info->offset), &(raw_data_info->scale_factor));

  /* figure out the file size in bytes (file_info.st_size) */
  if (stat(raw_data_info->filename, &file_info) != 0) {
    g_warning(_("Couldn't get stat's on file %s for raw data import"), raw_data_info->filename);
    g_free(raw_data_info);
    return NULL;
  }
  raw_data_info->total_file_size = file_info.st_size;

  /* create the import_dialog */
  dialog = import_dialog(raw_data_info);

  /* block till the user closes the dialog */
  dialog_reply = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  /* and start loading in the file if we hit ok*/
  if (dialog_reply == GTK_RESPONSE_YES) {
    /* the progress dialog */
    progress_dialog = amitk_progress_dialog_new(NULL);
  
    ds = amitk_data_set_import_raw_file(raw_data_info->filename,
					raw_data_info->raw_format,
					raw_data_info->data_dim,
					raw_data_info->offset,
					preferences,
					raw_data_info->modality,
					raw_data_info->name,
					raw_data_info->voxel_size,
					raw_data_info->scale_factor,
					amitk_progress_dialog_update,
					progress_dialog);
    
  } else /* we hit the cancel button */
    ds = NULL;

  if (progress_dialog != NULL) {
    g_signal_emit_by_name(G_OBJECT(progress_dialog), "delete_event", NULL, &return_val);
    progress_dialog = NULL;
  }

  if (raw_data_info->filename != NULL) {
    g_free(raw_data_info->filename);
    raw_data_info->filename = NULL;
  }

  if (raw_data_info->name != NULL) {
    g_free(raw_data_info->name);
    raw_data_info->name = NULL;
  }

  if (raw_data_info != NULL) {
    g_free(raw_data_info); 
    raw_data_info = NULL;
  }

  return ds; /* NULL if we hit cancel */

}










