/* amitk_preferences.h
 *
 * Part of amide - Amide's a Medical Image Data Examiner
 * Copyright (C) 2003-2004 Andy Loening
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

#ifndef __AMITK_PREFERENCES_H__
#define __AMITK_PREFERENCES_H__

/* header files that are always needed with this file */
#include <gtk/gtk.h>
#include "amide.h"
#include "amitk_color_table.h"

G_BEGIN_DECLS

#define	AMITK_TYPE_PREFERENCES		  (amitk_preferences_get_type ())
#define AMITK_PREFERENCES(object)	  (G_TYPE_CHECK_INSTANCE_CAST ((object), AMITK_TYPE_PREFERENCES, AmitkPreferences))
#define AMITK_PREFERENCES_CLASS(klass)	  (G_TYPE_CHECK_CLASS_CAST ((klass), AMITK_TYPE_PREFERENCES, AmitkPreferencesClass))
#define AMITK_IS_PREFERENCES(object)	  (G_TYPE_CHECK_INSTANCE_TYPE ((object), AMITK_TYPE_PREFERENCES))
#define AMITK_IS_PREFERENCES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), AMITK_TYPE_PREFERENCES))
#define	AMITK_PREFERENCES_GET_CLASS(object)  (G_TYPE_CHECK_GET_CLASS ((object), AMITK_TYPE_PREFERENCES, AmitkPreferencesClass))

#define AMITK_PREFERENCES_WARNINGS_TO_CONSOLE(object)     (AMITK_PREFERENCES(object)->warnings_to_console)

#define AMITK_PREFERENCES_PROMPT_FOR_SAVE_ON_EXIT(object) (AMITK_PREFERENCES(object)->prompt_for_save_on_exit)
#define AMITK_PREFERENCES_SAVE_XIF_AS_DIRECTORY(object)   (AMITK_PREFERENCES(object)->save_xif_as_directory)

#define AMITK_PREFERENCES_CANVAS_ROI_WIDTH(pref)         (AMITK_PREFERENCES(pref)->canvas_roi_width)
#define AMITK_PREFERENCES_CANVAS_LINE_STYLE(pref)        (AMITK_PREFERENCES(pref)->canvas_line_style)
#define AMITK_PREFERENCES_CANVAS_LAYOUT(pref)            (AMITK_PREFERENCES(pref)->canvas_layout)
#define AMITK_PREFERENCES_CANVAS_MAINTAIN_SIZE(pref)     (AMITK_PREFERENCES(pref)->canvas_maintain_size)
#define AMITK_PREFERENCES_CANVAS_TARGET_EMPTY_AREA(pref) (AMITK_PREFERENCES(pref)->canvas_target_empty_area)
#define AMITK_PREFERENCES_DEFAULT_COLOR_TABLE(pref, modality) (AMITK_PREFERENCES(pref)->default_color_table[modality])
#define AMITK_PREFERENCES_DEFAULT_WINDOW(pref, window, limit)   (AMITK_PREFERENCES(pref)->default_window[window][limit])
#define AMITK_PREFERENCES_DIALOG(pref)                   (AMITK_PREFERENCES(pref)->dialog)

#define AMITK_PREFERENCES_DEFAULT_CANVAS_ROI_WIDTH 2
#define AMITK_PREFERENCES_DEFAULT_CANVAS_LINE_STYLE GDK_LINE_SOLID
#define AMITK_PREFERENCES_DEFAULT_CANVAS_LAYOUT AMITK_LAYOUT_LINEAR
#define AMITK_PREFERENCES_DEFAULT_CANVAS_MAINTAIN_SIZE TRUE
#define AMITK_PREFERENCES_DEFAULT_CANVAS_TARGET_EMPTY_AREA 5
#define AMITK_PREFERENCES_DEFAULT_WARNINGS_TO_CONSOLE FALSE
#define AMITK_PREFERENCES_DEFAULT_PROMPT_FOR_SAVE_ON_EXIT TRUE
#define AMITK_PREFERENCES_DEFAULT_SAVE_XIF_AS_DIRECTORY FALSE

#define AMITK_PREFERENCES_MIN_ROI_WIDTH 1
#define AMITK_PREFERENCES_MAX_ROI_WIDTH 5
#define AMITK_PREFERENCES_MIN_TARGET_EMPTY_AREA 0
#define AMITK_PREFERENCES_MAX_TARGET_EMPTY_AREA 25



typedef struct _AmitkPreferencesClass AmitkPreferencesClass;
typedef struct _AmitkPreferences      AmitkPreferences;

struct _AmitkPreferences {

  GObject parent;

  /* debug preferences */
  gboolean warnings_to_console;

  /* file saving preferences */
  gboolean prompt_for_save_on_exit;
  gboolean save_xif_as_directory;

  /* canvas preferences -> study preferences */
  gint canvas_roi_width;
  GdkLineStyle canvas_line_style;
  AmitkLayout canvas_layout;
  gboolean canvas_maintain_size;
  gint canvas_target_empty_area; /* in pixels */

  /* data set preferences */
  AmitkColorTable default_color_table[AMITK_MODALITY_NUM];
  amide_data_t default_window[AMITK_WINDOW_NUM][AMITK_LIMIT_NUM];

  /* misc pointers */
  GtkWidget * dialog;

};

struct _AmitkPreferencesClass
{
  GObjectClass parent_class;

  void (* data_set_preferences_changed) (AmitkPreferences * preferences);
  void (* study_preferences_changed)    (AmitkPreferences * preferences);
  void (* misc_preferences_changed)     (AmitkPreferences * preferences);

};


/* ------------ external functions ---------- */

GType	            amitk_preferences_get_type	                 (void);
AmitkPreferences*   amitk_preferences_new                        (void);
void                amitk_preferences_set_canvas_roi_width       (AmitkPreferences * preferences, 
							          gint roi_width);
void                amitk_preferences_set_canvas_line_style      (AmitkPreferences * preferences, 
							          GdkLineStyle line_style);
void                amitk_preferences_set_canvas_layout          (AmitkPreferences * preferences, 
							          AmitkLayout layout);
void                amitk_preferences_set_canvas_maintain_size   (AmitkPreferences * preferences, 
							          gboolean maintain_size);
void                amitk_preferences_set_canvas_target_empty_area(AmitkPreferences * preferences, 
								   gint target_empty_area);
void                amitk_preferences_set_warnings_to_console    (AmitkPreferences * preferences, 
								  gboolean new_value);
void                amitk_preferences_set_prompt_for_save_on_exit(AmitkPreferences * preferences,
								  gboolean new_value);
void                amitk_preferences_set_xif_as_directory       (AmitkPreferences * preferences,
							          gboolean new_value);
void                amitk_preferences_set_color_table            (AmitkPreferences * preferences,
								  AmitkModality modality,
								  AmitkColorTable color_table);
void                amitk_preferences_set_default_window         (AmitkPreferences * preferences,
								  const AmitkWindow window,
								  const AmitkLimit limit,
								  const amide_data_t value);
void                amitk_preferences_set_dialog                 (AmitkPreferences * preferences,
								  GtkWidget * dialog);


#endif /* __AMITK_PREFERENCES_H__ */

