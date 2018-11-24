/*
 * gedit-print-preview.c
 *
 * Copyright (C) 2008 Paolo Borelli
 * Copyright (C) 2015 Sébastien Wilmet
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "gedit-print-preview.h"

#include <math.h>
#include <stdlib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <cairo-pdf.h>

#define PRINTER_DPI (72.0)
#define TOOLTIP_THRESHOLD 20
#define PAGE_PAD 12
#define PAGE_SHADOW_OFFSET 5
#define ZOOM_IN_FACTOR (1.2)
#define ZOOM_OUT_FACTOR (1.0 / ZOOM_IN_FACTOR)

struct _GeditPrintPreview
{
	GtkGrid parent_instance;

	GtkPrintOperation *operation;
	GtkPrintContext *context;
	GtkPrintOperationPreview *gtk_preview;

	GtkButton *prev_button;
	GtkButton *next_button;
	GtkEntry *page_entry;
	GtkLabel *last_page_label;
	GtkButton *multi_pages_button;
	GtkButton *zoom_one_button;
	GtkButton *zoom_fit_button;
	GtkButton *zoom_in_button;
	GtkButton *zoom_out_button;
	GtkButton *close_button;

	/* The GtkLayout is where the pages are drawn. The layout should have
	 * the focus, because key-press-events and scroll-events are handled on
	 * the layout. It is AFAIK not easily possible to handle those events on
	 * the GeditPrintPreview itself because when a toolbar item has the
	 * focus, some key presses (like the arrows) moves the focus to a
	 * sibling toolbar item instead.
	 */
	GtkLayout *layout;

	gdouble scale;

	/* multipage support */
	gint n_columns;

	/* FIXME: handle correctly page selection (e.g. print only
	 * page 1-3, 7 and 12.
	 */
	guint cur_page; /* starts at 0 */

	gint cursor_x;
	gint cursor_y;

	guint has_tooltip : 1;
};

G_DEFINE_TYPE (GeditPrintPreview, gedit_print_preview, GTK_TYPE_GRID)

static void
gedit_print_preview_dispose (GObject *object)
{
	GeditPrintPreview *preview = GEDIT_PRINT_PREVIEW (object);

	if (preview->gtk_preview != NULL)
	{
		GtkPrintOperationPreview *gtk_preview;

		/* Set preview->gtk_preview to NULL because when calling
		 * end_preview() this dispose() function can be run a second
		 * time.
		 */
		gtk_preview = preview->gtk_preview;
		preview->gtk_preview = NULL;

		gtk_print_operation_preview_end_preview (gtk_preview);

		g_object_unref (gtk_preview);
	}

	g_clear_object (&preview->operation);
	g_clear_object (&preview->context);

	G_OBJECT_CLASS (gedit_print_preview_parent_class)->dispose (object);
}

static void
gedit_print_preview_grab_focus (GtkWidget *widget)
{
	GeditPrintPreview *preview = GEDIT_PRINT_PREVIEW (widget);

	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
gedit_print_preview_class_init (GeditPrintPreviewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = gedit_print_preview_dispose;

	widget_class->grab_focus = gedit_print_preview_grab_focus;

	/* Bind class to template */
	gtk_widget_class_set_template_from_resource (widget_class,
	                                             "/org/gnome/gedit/ui/gedit-print-preview.ui");
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, prev_button);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, next_button);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, page_entry);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, last_page_label);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, multi_pages_button);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, zoom_one_button);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, zoom_fit_button);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, zoom_in_button);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, zoom_out_button);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, close_button);
	gtk_widget_class_bind_template_child (widget_class, GeditPrintPreview, layout);
}

static gint
get_n_pages (GeditPrintPreview *preview)
{
	gint n_pages;

	g_object_get (preview->operation, "n-pages", &n_pages, NULL);

	return n_pages;
}

static gdouble
get_screen_dpi (GeditPrintPreview *preview)
{
	GdkScreen *screen;
	gdouble dpi;
	static gboolean warning_shown = FALSE;

	screen = gtk_widget_get_screen (GTK_WIDGET (preview));

	if (screen == NULL)
	{
		return PRINTER_DPI;
	}

	dpi = gdk_screen_get_resolution (screen);
	if (dpi < 30.0 || 600.0 < dpi)
	{
		if (!warning_shown)
		{
			g_warning ("Invalid the x-resolution for the screen, assuming 96dpi");
			warning_shown = TRUE;
		}

		dpi = 96.0;
	}

	return dpi;
}

/* Get the paper size in points: these must be used only
 * after the widget has been mapped and the dpi is known.
 */
static gdouble
get_paper_width (GeditPrintPreview *preview)
{
	GtkPageSetup *page_setup;
	gdouble paper_width;

	page_setup = gtk_print_context_get_page_setup (preview->context);
	paper_width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_INCH);

	return paper_width * get_screen_dpi (preview);
}

static gdouble
get_paper_height (GeditPrintPreview *preview)
{
	GtkPageSetup *page_setup;
	gdouble paper_height;

	page_setup = gtk_print_context_get_page_setup (preview->context);
	paper_height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_INCH);

	return paper_height * get_screen_dpi (preview);
}

/* The tile size is the size in pixels of the area where a page will be
 * drawn, including the padding. The size is independent of the
 * orientation.
 */
static void
get_tile_size (GeditPrintPreview *preview,
	       gint              *tile_width,
	       gint              *tile_height)
{
	if (tile_width != NULL)
	{
		*tile_width = 2 * PAGE_PAD + round (preview->scale * get_paper_width (preview));
	}

	if (tile_height != NULL)
	{
		*tile_height = 2 * PAGE_PAD + round (preview->scale * get_paper_height (preview));
	}
}

static void
get_adjustments (GeditPrintPreview  *preview,
		 GtkAdjustment     **hadj,
		 GtkAdjustment     **vadj)
{
	*hadj = gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (preview->layout));
	*vadj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (preview->layout));
}

static void
update_layout_size (GeditPrintPreview *preview)
{
	gint tile_width;
	gint tile_height;

	get_tile_size (preview, &tile_width, &tile_height);

	/* force size of the drawing area to make the scrolled window work */
	gtk_layout_set_size (preview->layout,
	                     tile_width * preview->n_columns,
	                     tile_height);

	gtk_widget_queue_draw (GTK_WIDGET (preview->layout));
}

/* Zoom should always be set with one of these two function
 * so that the tile size is properly updated.
 */

static void
set_zoom_factor (GeditPrintPreview *preview,
		 gdouble            zoom)
{
	preview->scale = zoom;
	update_layout_size (preview);
}

static void
set_zoom_fit_to_size (GeditPrintPreview *preview)
{
	GtkAdjustment *hadj, *vadj;
	gdouble width, height;
	gdouble paper_width, paper_height;
	gdouble zoomx, zoomy;

	get_adjustments (preview, &hadj, &vadj);

	width = gtk_adjustment_get_page_size (hadj);
	height = gtk_adjustment_get_page_size (vadj);

	width /= preview->n_columns;

	paper_width = get_paper_width (preview);
	paper_height = get_paper_height (preview);

	zoomx = MAX (1, width - 2 * PAGE_PAD) / paper_width;
	zoomy = MAX (1, height - 2 * PAGE_PAD) / paper_height;

	set_zoom_factor (preview, zoomx <= zoomy ? zoomx : zoomy);
}

static void
zoom_in (GeditPrintPreview *preview)
{
	set_zoom_factor (preview, preview->scale * ZOOM_IN_FACTOR);
}

static void
zoom_out (GeditPrintPreview *preview)
{
	set_zoom_factor (preview, preview->scale * ZOOM_OUT_FACTOR);
}

static void
goto_page (GeditPrintPreview *preview,
           gint               page)
{
	gchar *page_str;
	gint n_pages;

	page_str = g_strdup_printf ("%d", page + 1);
	gtk_entry_set_text (preview->page_entry, page_str);
	g_free (page_str);

	n_pages = get_n_pages (preview);

	gtk_widget_set_sensitive (GTK_WIDGET (preview->prev_button),
	                          page > 0 &&
				  n_pages > 1);

	gtk_widget_set_sensitive (GTK_WIDGET (preview->next_button),
	                          page < (n_pages - 1) &&
	                          n_pages > 1);

	if (page != preview->cur_page)
	{
		preview->cur_page = page;
		if (n_pages > 0)
		{
			gtk_widget_queue_draw (GTK_WIDGET (preview->layout));
		}
	}
}

static void
prev_button_clicked (GtkWidget         *button,
		     GeditPrintPreview *preview)
{
	GdkEvent *event;
	gint page;

	event = gtk_get_current_event ();

	if (event->button.state & GDK_SHIFT_MASK)
	{
		page = 0;
	}
	else
	{
		page = preview->cur_page - preview->n_columns;
	}

	goto_page (preview, MAX (page, 0));

	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));

	gdk_event_free (event);
}

static void
next_button_clicked (GtkWidget         *button,
		     GeditPrintPreview *preview)
{
	GdkEvent *event;
	gint page;
	gint n_pages = get_n_pages (preview);

	event = gtk_get_current_event ();

	if (event->button.state & GDK_SHIFT_MASK)
	{
		page = n_pages - 1;
	}
	else
	{
		page = preview->cur_page + preview->n_columns;
	}

	goto_page (preview, MIN (page, n_pages - 1));

	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));

	gdk_event_free (event);
}

static void
page_entry_activated (GtkEntry          *entry,
		      GeditPrintPreview *preview)
{
	const gchar *text;
	gint page;
	gint n_pages = get_n_pages (preview);

	text = gtk_entry_get_text (entry);

	page = CLAMP (atoi (text), 1, n_pages) - 1;
	goto_page (preview, page);

	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
page_entry_insert_text (GtkEditable *editable,
			const gchar *text,
			gint         length,
			gint        *position)
{
	const gchar *end;
	const gchar *p;

	end = text + length;

	for (p = text; p < end; p = g_utf8_next_char (p))
	{
		if (!g_unichar_isdigit (g_utf8_get_char (p)))
		{
			g_signal_stop_emission_by_name (editable, "insert-text");
			break;
		}
	}
}

static gboolean
page_entry_focus_out (GtkEntry          *entry,
		      GdkEventFocus     *event,
		      GeditPrintPreview *preview)
{
	const gchar *text;
	gint page;

	text = gtk_entry_get_text (entry);
	page = atoi (text) - 1;

	/* Reset the page number only if really needed */
	if (page != preview->cur_page)
	{
		gchar *str;

		str = g_strdup_printf ("%d", preview->cur_page + 1);
		gtk_entry_set_text (entry, str);
		g_free (str);
	}

	return GDK_EVENT_PROPAGATE;
}

static void
on_1x1_clicked (GtkMenuItem       *item,
		GeditPrintPreview *preview)
{
	preview->n_columns = 1;
	update_layout_size (preview);
	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
on_1x2_clicked (GtkMenuItem       *item,
		GeditPrintPreview *preview)
{
	preview->n_columns = 2;
	set_zoom_fit_to_size (preview);
	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
multi_pages_button_clicked (GtkWidget         *button,
			    GeditPrintPreview *preview)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new ();
	gtk_widget_show (menu);
	g_signal_connect (menu,
			  "selection-done",
			  G_CALLBACK (gtk_widget_destroy),
			  NULL);

	item = gtk_menu_item_new_with_label ("1x1");
	gtk_widget_show (item);
	gtk_menu_attach (GTK_MENU (menu), item, 0, 1, 0, 1);
	g_signal_connect (item, "activate", G_CALLBACK (on_1x1_clicked), preview);

	item = gtk_menu_item_new_with_label ("1x2");
	gtk_widget_show (item);
	gtk_menu_attach (GTK_MENU (menu), item, 1, 2, 0, 1);
	g_signal_connect (item, "activate", G_CALLBACK (on_1x2_clicked), preview);

	gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void
zoom_one_button_clicked (GtkWidget         *button,
			 GeditPrintPreview *preview)
{
	set_zoom_factor (preview, 1);
	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
zoom_fit_button_clicked (GtkWidget         *button,
			 GeditPrintPreview *preview)
{
	set_zoom_fit_to_size (preview);
	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
zoom_in_button_clicked (GtkWidget         *button,
			GeditPrintPreview *preview)
{
	zoom_in (preview);
	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
zoom_out_button_clicked (GtkWidget         *button,
			 GeditPrintPreview *preview)
{
	zoom_out (preview);
	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
close_button_clicked (GtkWidget         *button,
		      GeditPrintPreview *preview)
{
	gtk_widget_destroy (GTK_WIDGET (preview));
}

static gboolean
scroll_event_activated (GtkWidget         *widget,
		        GdkEventScroll    *event,
		        GeditPrintPreview *preview)
{
	if (event->state & GDK_CONTROL_MASK)
	{
		if ((event->direction == GDK_SCROLL_UP) ||
		    (event->direction == GDK_SCROLL_SMOOTH &&
		     event->delta_y < 0))
		{
			zoom_in (preview);
		}
		else if ((event->direction == GDK_SCROLL_DOWN) ||
		         (event->direction == GDK_SCROLL_SMOOTH &&
		          event->delta_y > 0))
		{
			zoom_out (preview);
		}

		return GDK_EVENT_STOP;
	}

	return GDK_EVENT_PROPAGATE;
}

static gint
get_first_page_displayed (GeditPrintPreview *preview)
{
	return preview->cur_page - (preview->cur_page % preview->n_columns);
}

/* Returns the page number (starting from 0) or -1 if no page. */
static gint
get_page_at_coords (GeditPrintPreview *preview,
                    gint               x,
                    gint               y)
{
	gint tile_width, tile_height;
	GtkAdjustment *hadj, *vadj;
	gint col, page;

	get_tile_size (preview, &tile_width, &tile_height);

	if (tile_height <= 0 || tile_width <= 0)
	{
		return -1;
	}

	get_adjustments (preview, &hadj, &vadj);

	x += gtk_adjustment_get_value (hadj);
	y += gtk_adjustment_get_value (vadj);

	col = x / tile_width;

	if (col >= preview->n_columns || y > tile_height)
	{
		return -1;
	}

	page = get_first_page_displayed (preview) + col;

	if (page >= get_n_pages (preview))
	{
		return -1;
	}

	/* FIXME: we could try to be picky and check if we actually are inside
	 * the page (i.e. not in the padding or shadow).
	 */
	return page;
}

static gboolean
on_preview_layout_motion_notify (GtkWidget         *widget,
                                 GdkEvent          *event,
                                 GeditPrintPreview *preview)
{
	gint temp_x;
	gint temp_y;
	gint diff_x;
	gint diff_y;

	temp_x = ((GdkEventMotion*)event)->x;
	temp_y = ((GdkEventMotion*)event)->y;
	diff_x = abs (temp_x - preview->cursor_x);
	diff_y = abs (temp_y - preview->cursor_y);

	if ((diff_x >= TOOLTIP_THRESHOLD) || (diff_y >= TOOLTIP_THRESHOLD))
	{
		preview->has_tooltip = FALSE;
		preview->cursor_x = temp_x;
		preview->cursor_y = temp_y;
	}
	else
	{
		preview->has_tooltip = TRUE;
	}

	return GDK_EVENT_STOP;
}

static gboolean
preview_layout_query_tooltip (GtkWidget         *widget,
			      gint               x,
			      gint               y,
			      gboolean           keyboard_tip,
			      GtkTooltip        *tooltip,
			      GeditPrintPreview *preview)
{
	if (preview->has_tooltip)
	{
		gint page;
		gchar *tip;

		page = get_page_at_coords (preview, x, y);
		if (page < 0)
		{
			return FALSE;
		}

		tip = g_strdup_printf (_("Page %d of %d"),
				       page + 1,
				       get_n_pages (preview));

		gtk_tooltip_set_text (tooltip, tip);
		g_free (tip);

		return TRUE;
	}
	else
	{
		preview->has_tooltip = TRUE;
		return FALSE;
	}
}

static gint
preview_layout_key_press (GtkWidget         *widget,
			  GdkEventKey       *event,
			  GeditPrintPreview *preview)
{
	GtkAdjustment *hadj, *vadj;
	gdouble x, y;
	gdouble hlower, vlower;
	gdouble hupper, vupper;
	gdouble visible_width, visible_height;
	gdouble hstep, vstep;
	gint n_pages;
	gboolean do_move = FALSE;

	get_adjustments (preview, &hadj, &vadj);

	x = gtk_adjustment_get_value (hadj);
	y = gtk_adjustment_get_value (vadj);

	hlower = gtk_adjustment_get_lower (hadj);
	vlower = gtk_adjustment_get_lower (vadj);

	hupper = gtk_adjustment_get_upper (hadj);
	vupper = gtk_adjustment_get_upper (vadj);

	visible_width = gtk_adjustment_get_page_size (hadj);
	visible_height = gtk_adjustment_get_page_size (vadj);

	hstep = 10;
	vstep = 10;

	n_pages = get_n_pages (preview);

	switch (event->keyval)
	{
		case '1':
			set_zoom_fit_to_size (preview);
			break;

		case '+':
		case '=':
		case GDK_KEY_KP_Add:
			zoom_in (preview);
			break;

		case '-':
		case '_':
		case GDK_KEY_KP_Subtract:
			zoom_out (preview);
			break;

		case GDK_KEY_KP_Right:
		case GDK_KEY_Right:
			if (event->state & GDK_SHIFT_MASK)
				x = hupper - visible_width;
			else
				x = MIN (hupper - visible_width, x + hstep);
			do_move = TRUE;
			break;

		case GDK_KEY_KP_Left:
		case GDK_KEY_Left:
			if (event->state & GDK_SHIFT_MASK)
				x = hlower;
			else
				x = MAX (hlower, x - hstep);
			do_move = TRUE;
			break;

		case GDK_KEY_KP_Up:
		case GDK_KEY_Up:
			if (event->state & GDK_SHIFT_MASK)
				goto page_up;

			y = MAX (vlower, y - vstep);
			do_move = TRUE;
			break;

		case GDK_KEY_KP_Down:
		case GDK_KEY_Down:
			if (event->state & GDK_SHIFT_MASK)
				goto page_down;

			y = MIN (vupper - visible_height, y + vstep);
			do_move = TRUE;
			break;

		case GDK_KEY_KP_Page_Up:
		case GDK_KEY_Page_Up:
		case GDK_KEY_Delete:
		case GDK_KEY_KP_Delete:
		case GDK_KEY_BackSpace:
		page_up:
			if (y <= vlower)
			{
				if (preview->cur_page > 0)
				{
					goto_page (preview, preview->cur_page - 1);
					y = (vupper - visible_height);
				}
			}
			else
			{
				y = vlower;
			}
			do_move = TRUE;
			break;

		case GDK_KEY_KP_Page_Down:
		case GDK_KEY_Page_Down:
		case ' ':
		page_down:
			if (y >= (vupper - visible_height))
			{
				if (preview->cur_page < n_pages - 1)
				{
					goto_page (preview, preview->cur_page + 1);
					y = vlower;
				}
			}
			else
			{
				y = (vupper - visible_height);
			}
			do_move = TRUE;
			break;

		case GDK_KEY_KP_Home:
		case GDK_KEY_Home:
			goto_page (preview, 0);
			y = vlower;
			do_move = TRUE;
			break;

		case GDK_KEY_KP_End:
		case GDK_KEY_End:
			goto_page (preview, n_pages - 1);
			y = vlower;
			do_move = TRUE;
			break;

		case GDK_KEY_Escape:
			gtk_widget_destroy (GTK_WIDGET (preview));
			break;

		case 'p':
			if (event->state & GDK_MOD1_MASK)
			{
				gtk_widget_grab_focus (GTK_WIDGET (preview->page_entry));
			}
			break;

		default:
			return GDK_EVENT_PROPAGATE;
	}

	if (do_move)
	{
		gtk_adjustment_set_value (hadj, x);
		gtk_adjustment_set_value (vadj, y);
	}

	return GDK_EVENT_STOP;
}

static void
gedit_print_preview_init (GeditPrintPreview *preview)
{
	preview->cur_page = 0;
	preview->scale = 1.0;
	preview->n_columns = 1;
	preview->cursor_x = 0;
	preview->cursor_y = 0;
	preview->has_tooltip = TRUE;

	gtk_widget_init_template (GTK_WIDGET (preview));

	g_signal_connect (preview->prev_button,
			  "clicked",
			  G_CALLBACK (prev_button_clicked),
			  preview);

	g_signal_connect (preview->next_button,
			  "clicked",
			  G_CALLBACK (next_button_clicked),
			  preview);

	g_signal_connect (preview->page_entry,
			  "activate",
			  G_CALLBACK (page_entry_activated),
			  preview);

	g_signal_connect (preview->page_entry,
			  "insert-text",
			  G_CALLBACK (page_entry_insert_text),
			  NULL);

	g_signal_connect (preview->page_entry,
			  "focus-out-event",
			  G_CALLBACK (page_entry_focus_out),
			  preview);

	g_signal_connect (preview->multi_pages_button,
			  "clicked",
			  G_CALLBACK (multi_pages_button_clicked),
			  preview);

	g_signal_connect (preview->zoom_one_button,
			  "clicked",
			  G_CALLBACK (zoom_one_button_clicked),
			  preview);

	g_signal_connect (preview->zoom_fit_button,
			  "clicked",
			  G_CALLBACK (zoom_fit_button_clicked),
			  preview);

	g_signal_connect (preview->zoom_in_button,
			  "clicked",
			  G_CALLBACK (zoom_in_button_clicked),
			  preview);

	g_signal_connect (preview->zoom_out_button,
			  "clicked",
			  G_CALLBACK (zoom_out_button_clicked),
			  preview);

	g_signal_connect (preview->close_button,
			  "clicked",
			  G_CALLBACK (close_button_clicked),
			  preview);

	g_signal_connect (preview->layout,
			  "query-tooltip",
			  G_CALLBACK (preview_layout_query_tooltip),
			  preview);

	g_signal_connect (preview->layout,
			  "key-press-event",
			  G_CALLBACK (preview_layout_key_press),
			  preview);

	g_signal_connect (preview->layout,
			  "scroll-event",
			  G_CALLBACK (scroll_event_activated),
			  preview);

	/* hide the tooltip once we move the cursor, since gtk does not do it for us */
	g_signal_connect (preview->layout,
			  "motion-notify-event",
			  G_CALLBACK (on_preview_layout_motion_notify),
			  preview);

	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
draw_page_content (cairo_t           *cr,
		   gint               page_number,
		   GeditPrintPreview *preview)
{
	gdouble dpi;

	/* scale to the desired size */
	cairo_scale (cr, preview->scale, preview->scale);

	dpi = get_screen_dpi (preview);
	gtk_print_context_set_cairo_context (preview->context, cr, dpi, dpi);

	gtk_print_operation_preview_render_page (preview->gtk_preview,
	                                         page_number);
}

/* For the frame, we scale and rotate manually, since
 * the line width should not depend on the zoom and
 * the drop shadow should be on the bottom right no matter
 * the orientation.
 */
static void
draw_page_frame (cairo_t           *cr,
		 GeditPrintPreview *preview)
{
	gdouble width;
	gdouble height;

	width = get_paper_width (preview) * preview->scale;
	height = get_paper_height (preview) * preview->scale;

	/* drop shadow */
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr,
			 PAGE_SHADOW_OFFSET, PAGE_SHADOW_OFFSET,
			 width, height);
	cairo_fill (cr);

	/* page frame */
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle (cr,
			 0, 0,
			 width, height);
	cairo_fill_preserve (cr);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

static void
draw_page (cairo_t           *cr,
	   gdouble            x,
	   gdouble            y,
	   gint               page_number,
	   GeditPrintPreview *preview)
{
	cairo_save (cr);

	/* move to the page top left corner */
	cairo_translate (cr, x + PAGE_PAD, y + PAGE_PAD);

	draw_page_frame (cr, preview);
	draw_page_content (cr, page_number, preview);

	cairo_restore (cr);
}

static gboolean
preview_draw (GtkWidget         *widget,
	      cairo_t           *cr,
	      GeditPrintPreview *preview)
{
	GdkWindow *bin_window;
	gint tile_width;
	gint page_num;
	gint n_pages;
	gint col;

	bin_window = gtk_layout_get_bin_window (preview->layout);

	if (!gtk_cairo_should_draw_window (cr, bin_window))
	{
		return GDK_EVENT_STOP;
	}

	cairo_save (cr);

	gtk_cairo_transform_to_window (cr, widget, bin_window);

	get_tile_size (preview, &tile_width, NULL);
	n_pages = get_n_pages (preview);

	col = 0;
	page_num = get_first_page_displayed (preview);

	while (col < preview->n_columns && page_num < n_pages)
	{
		if (!gtk_print_operation_preview_is_selected (preview->gtk_preview, page_num))
		{
			page_num++;
			continue;
		}

		draw_page (cr,
			   col * tile_width,
			   0,
			   page_num,
			   preview);

		col++;
		page_num++;
	}

	cairo_restore (cr);

	return GDK_EVENT_STOP;
}

static void
init_last_page_label (GeditPrintPreview *preview)
{
	gchar *str;

	str = g_strdup_printf ("%d", get_n_pages (preview));
	gtk_label_set_text (preview->last_page_label, str);
	g_free (str);
}

static void
preview_ready (GtkPrintOperationPreview *gtk_preview,
	       GtkPrintContext          *context,
	       GeditPrintPreview        *preview)
{
	init_last_page_label (preview);
	goto_page (preview, 0);

	set_zoom_factor (preview, 1.0);

	/* let the default gtklayout handler clear the background */
	g_signal_connect_after (preview->layout,
				"draw",
				G_CALLBACK (preview_draw),
				preview);

	gtk_widget_queue_draw (GTK_WIDGET (preview->layout));
}

/* HACK: we need a dummy surface to paginate... can we use something simpler? */

static cairo_status_t
dummy_write_func (G_GNUC_UNUSED gpointer      closure,
		  G_GNUC_UNUSED const guchar *data,
		  G_GNUC_UNUSED guint         length)
{
	return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *
create_preview_surface_platform (GtkPaperSize *paper_size,
				 gdouble      *dpi_x,
				 gdouble      *dpi_y)
{
	gdouble width, height;

	width = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
	height = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);

	*dpi_x = *dpi_y = PRINTER_DPI;

	return cairo_pdf_surface_create_for_stream (dummy_write_func, NULL,
						    width, height);
}

static cairo_surface_t *
create_preview_surface (GeditPrintPreview *preview,
			gdouble           *dpi_x,
			gdouble           *dpi_y)
{
	GtkPageSetup *page_setup;
	GtkPaperSize *paper_size;

	page_setup = gtk_print_context_get_page_setup (preview->context);

	/* Note: gtk_page_setup_get_paper_size() swaps width and height for
	 * landscape.
	 */
	paper_size = gtk_page_setup_get_paper_size (page_setup);

	return create_preview_surface_platform (paper_size, dpi_x, dpi_y);
}

GtkWidget *
gedit_print_preview_new (GtkPrintOperation        *operation,
			 GtkPrintOperationPreview *gtk_preview,
			 GtkPrintContext          *context)
{
	GeditPrintPreview *preview;
	cairo_surface_t *surface;
	cairo_t *cr;
	gdouble dpi_x, dpi_y;

	g_return_val_if_fail (GTK_IS_PRINT_OPERATION (operation), NULL);
	g_return_val_if_fail (GTK_IS_PRINT_OPERATION_PREVIEW (gtk_preview), NULL);

	preview = g_object_new (GEDIT_TYPE_PRINT_PREVIEW, NULL);

	preview->operation = g_object_ref (operation);
	preview->gtk_preview = g_object_ref (gtk_preview);
	preview->context = g_object_ref (context);

	/* FIXME: is this legal?? */
	gtk_print_operation_set_unit (operation, GTK_UNIT_POINTS);

	g_signal_connect_object (gtk_preview,
				 "ready",
				 G_CALLBACK (preview_ready),
				 preview,
				 0);

	/* FIXME: we need a cr to paginate... but we can't get the drawing
	 * area surface because it's not there yet... for now I create
	 * a dummy pdf surface.
	 * gtk_print_context_set_cairo_context() should be called in the
	 * got-page-size handler.
	 */
	surface = create_preview_surface (preview, &dpi_x, &dpi_y);
	cr = cairo_create (surface);
	gtk_print_context_set_cairo_context (context, cr, dpi_x, dpi_y);
	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	return GTK_WIDGET (preview);
}

/* ex:set ts=8 noet: */
