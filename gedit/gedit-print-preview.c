/*
 * gedit-print-preview.c
 *
 * Copyright (C) 2008 Paolo Borelli
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

#define PRINTER_DPI (72.)
#define TOOLTIP_THRESHOLD 20

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

	GtkLayout *layout;

	/* real size of the page in inches */
	gdouble paper_width;
	gdouble paper_height;
	gdouble dpi;

	gdouble scale;

	/* size of the tile of a page (including padding
	 * and drop shadow) in pixels */
	gint tile_width;
	gint tile_height;

	/* multipage support */
	gint rows;
	gint cols;

	guint n_pages;
	guint cur_page;
	gint cursor_x;
	gint cursor_y;

	guint has_tooltip : 1;
};

G_DEFINE_TYPE (GeditPrintPreview, gedit_print_preview, GTK_TYPE_GRID)

static void
gedit_print_preview_grab_focus (GtkWidget *widget)
{
	GeditPrintPreview *preview;

	preview = GEDIT_PRINT_PREVIEW (widget);

	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
gedit_print_preview_class_init (GeditPrintPreviewClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

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
	/* force size of the drawing area to make the scrolled window work */
	gtk_layout_set_size (preview->layout,
	                     preview->tile_width * preview->cols,
	                     preview->tile_height * preview->rows);

	gtk_widget_queue_draw (GTK_WIDGET (preview->layout));
}

static void
set_rows_and_cols (GeditPrintPreview *preview,
		   gint	              rows,
		   gint	              cols)
{
	/* TODO: set the zoom appropriately */

	preview->rows = rows;
	preview->cols = cols;
	update_layout_size (preview);
}

/* get the paper size in points: these must be used only
 * after the widget has been mapped and the dpi is known */

static double
get_paper_width (GeditPrintPreview *preview)
{
	return preview->paper_width * preview->dpi;
}

static double
get_paper_height (GeditPrintPreview *preview)
{
	return preview->paper_height * preview->dpi;
}

#define PAGE_PAD 12
#define PAGE_SHADOW_OFFSET 5

/* The tile size is the size of the area where a page
 * will be drawn including the padding and idependent
 * of the orientation */

/* updates the tile size to the current zoom and page size */
static void
update_tile_size (GeditPrintPreview *preview)
{
	gint w, h;

	w = 2 * PAGE_PAD + floor (preview->scale * get_paper_width (preview) + 0.5);
	h = 2 * PAGE_PAD + floor (preview->scale * get_paper_height (preview) + 0.5);

	preview->tile_width = w;
	preview->tile_height = h;
}

/* Zoom should always be set with one of these two function
 * so that the tile size is properly updated */

static void
set_zoom_factor (GeditPrintPreview *preview,
                 double            zoom)
{
	preview->scale = zoom;

	update_tile_size (preview);
	update_layout_size (preview);
}

static void
set_zoom_fit_to_size (GeditPrintPreview *preview)
{
	GtkAdjustment *hadj, *vadj;
	double width, height;
	double p_width, p_height;
	double zoomx, zoomy;

	get_adjustments (preview, &hadj, &vadj);

	g_object_get (hadj, "page-size", &width, NULL);
	g_object_get (vadj, "page-size", &height, NULL);

	width /= preview->cols;
	height /= preview->rows;

	p_width = get_paper_width (preview);
	p_height = get_paper_height (preview);

	zoomx = MAX (1, width - 2 * PAGE_PAD) / p_width;
	zoomy = MAX (1, height - 2 * PAGE_PAD) / p_height;

	if (zoomx <= zoomy)
	{
		preview->tile_width = width;
		preview->tile_height = floor (0.5 + width * (p_height / p_width));
		preview->scale = zoomx;
	}
	else
	{
		preview->tile_width = floor (0.5 + height * (p_width / p_height));
		preview->tile_height = height;
		preview->scale = zoomy;
	}

	update_layout_size (preview);
}

#define ZOOM_IN_FACTOR (1.2)
#define ZOOM_OUT_FACTOR (1.0 / ZOOM_IN_FACTOR)

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
	gchar c[32];

	g_snprintf (c, 32, "%d", page + 1);
	gtk_entry_set_text (preview->page_entry, c);

	gtk_widget_set_sensitive (GTK_WIDGET (preview->prev_button),
	                          (page > 0) && (preview->n_pages > 1));
	gtk_widget_set_sensitive (GTK_WIDGET (preview->next_button),
	                          (page != (preview->n_pages - 1)) &&
	                          (preview->n_pages > 1));

	if (page != preview->cur_page)
	{
		preview->cur_page = page;
		if (preview->n_pages > 0)
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
		page = preview->cur_page - preview->rows * preview->cols;
	}

	goto_page (preview, MAX (page, 0));

	gdk_event_free (event);
}

static void
next_button_clicked (GtkWidget         *button,
		     GeditPrintPreview *preview)
{
	GdkEvent *event;
	gint page;

	event = gtk_get_current_event ();

	if (event->button.state & GDK_SHIFT_MASK)
	{
		page = preview->n_pages - 1;
	}
	else
	{
		page = preview->cur_page + preview->rows * preview->cols;
	}

	goto_page (preview, MIN (page, preview->n_pages - 1));

	gdk_event_free (event);
}

static void
page_entry_activated (GtkEntry          *entry,
		      GeditPrintPreview *preview)
{
	const gchar *text;
	gint page;

	text = gtk_entry_get_text (entry);

	page = CLAMP (atoi (text), 1, preview->n_pages) - 1;
	goto_page (preview, page);

	gtk_widget_grab_focus (GTK_WIDGET (preview->layout));
}

static void
page_entry_insert_text (GtkEditable *editable,
			const gchar *text,
			gint         length,
			gint        *position)
{
	gunichar c;
	const gchar *p;
	const gchar *end;

	p = text;
	end = text + length;

	while (p != end)
	{
		const gchar *next;
		next = g_utf8_next_char (p);

		c = g_utf8_get_char (p);

		if (!g_unichar_isdigit (c))
		{
			g_signal_stop_emission_by_name (editable, "insert-text");
			break;
		}

		p = next;
	}
}

static gboolean
page_entry_focus_out (GtkWidget         *widget,
		      GdkEventFocus     *event,
		      GeditPrintPreview *preview)
{
	const gchar *text;
	gint page;

	text = gtk_entry_get_text (GTK_ENTRY (widget));
	page = atoi (text) - 1;

	/* Reset the page number only if really needed */
	if (page != preview->cur_page)
	{
		gchar *str;

		str = g_strdup_printf ("%d", preview->cur_page + 1);
		gtk_entry_set_text (GTK_ENTRY (widget), str);
		g_free (str);
	}

	return FALSE;
}

static void
on_1x1_clicked (GtkMenuItem       *i,
		GeditPrintPreview *preview)
{
	set_rows_and_cols (preview, 1, 1);
}

static void
on_1x2_clicked (GtkMenuItem       *i,
		GeditPrintPreview *preview)
{
	set_rows_and_cols (preview, 1, 2);
}

static void
on_2x1_clicked (GtkMenuItem       *i,
		GeditPrintPreview *preview)
{
	set_rows_and_cols (preview, 2, 1);
}

static void
on_2x2_clicked (GtkMenuItem       *i,
		GeditPrintPreview *preview)
{
	set_rows_and_cols (preview, 2, 2);
}

static void
multi_pages_button_clicked (GtkWidget         *button,
			    GeditPrintPreview *preview)
{
	GtkWidget *m, *i;

	m = gtk_menu_new ();
	gtk_widget_show (m);
	g_signal_connect (m,
			 "selection_done",
			  G_CALLBACK (gtk_widget_destroy),
			  m);

	i = gtk_menu_item_new_with_label ("1x1");
	gtk_widget_show (i);
	gtk_menu_attach (GTK_MENU (m), i, 0, 1, 0, 1);
	g_signal_connect (i, "activate", G_CALLBACK (on_1x1_clicked), preview);

	i = gtk_menu_item_new_with_label ("2x1");
	gtk_widget_show (i);
	gtk_menu_attach (GTK_MENU (m), i, 0, 1, 1, 2);
	g_signal_connect (i, "activate", G_CALLBACK (on_2x1_clicked), preview);

	i = gtk_menu_item_new_with_label ("1x2");
	gtk_widget_show (i);
	gtk_menu_attach (GTK_MENU (m), i, 1, 2, 0, 1);
	g_signal_connect (i, "activate", G_CALLBACK (on_1x2_clicked), preview);

	i = gtk_menu_item_new_with_label ("2x2");
	gtk_widget_show (i);
	gtk_menu_attach (GTK_MENU (m), i, 1, 2, 1, 2);
	g_signal_connect (i, "activate", G_CALLBACK (on_2x2_clicked), preview);

	gtk_menu_popup (GTK_MENU (m),
			NULL, NULL, NULL, preview, 0,
			GDK_CURRENT_TIME);
}

static void
zoom_one_button_clicked (GtkWidget         *button,
			 GeditPrintPreview *preview)
{
	set_zoom_factor (preview, 1);
}

static void
zoom_fit_button_clicked (GtkWidget         *button,
			 GeditPrintPreview *preview)
{
	set_zoom_fit_to_size (preview);
}

static void
zoom_in_button_clicked (GtkWidget         *button,
			GeditPrintPreview *preview)
{
	zoom_in (preview);
}

static void
zoom_out_button_clicked (GtkWidget         *button,
			 GeditPrintPreview *preview)
{
	zoom_out (preview);
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
	return preview->cur_page - preview->cur_page % (preview->cols * preview->rows);
}

/* returns the page number (starting from 0) or -1 if no page */
static gint
get_page_at_coords (GeditPrintPreview *preview,
                    gint               x,
                    gint               y)
{
	GtkAdjustment *hadj, *vadj;
	gint r, c, pg;

	if (preview->tile_height <= 0 || preview->tile_width <= 0)
		return -1;

	get_adjustments (preview, &hadj, &vadj);

	x += gtk_adjustment_get_value (hadj);
	y += gtk_adjustment_get_value (vadj);

	r = 1 + y / (preview->tile_height);
	c = 1 + x / (preview->tile_width);

	if (c > preview->cols)
		return -1;

	pg = get_first_page_displayed (preview) - 1;
	pg += (r - 1) * preview->cols + c;

	if (pg >= preview->n_pages)
		return -1;

	/* FIXME: we could try to be picky and check
	 * if we actually are inside the page */
	return pg;
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
	gint pg;
	gchar *tip;

	if (preview->has_tooltip == TRUE)
	{
		pg = get_page_at_coords (preview, x, y);
		if (pg < 0)
			return FALSE;

		tip = g_strdup_printf (_("Page %d of %d"), pg + 1, preview->n_pages);
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
	double x, y;
	guint h, w;
	double hlower, hupper, vlower, vupper;
	double hpage, vpage;
	double hstep, vstep;
	gboolean domove = FALSE;
	gboolean ret = TRUE;

	get_adjustments (preview, &hadj, &vadj);

	x = gtk_adjustment_get_value (hadj);
	y = gtk_adjustment_get_value (vadj);

	g_object_get (hadj,
		      "lower", &hlower,
		      "upper", &hupper,
		      "page-size", &hpage,
		      NULL);
	g_object_get (vadj,
		      "lower", &vlower,
		      "upper", &vupper,
		      "page-size", &vpage,
		      NULL);

	gtk_layout_get_size (preview->layout, &w, &h);

	hstep = 10;
	vstep = 10;

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
				x = hupper - hpage;
			else
				x = MIN (hupper - hpage, x + hstep);
			domove = TRUE;
			break;
		case GDK_KEY_KP_Left:
		case GDK_KEY_Left:
			if (event->state & GDK_SHIFT_MASK)
				x = hlower;
			else
				x = MAX (hlower, x - hstep);
			domove = TRUE;
			break;
		case GDK_KEY_KP_Up:
		case GDK_KEY_Up:
			if (event->state & GDK_SHIFT_MASK)
				goto page_up;
			y = MAX (vlower, y - vstep);
			domove = TRUE;
			break;
		case GDK_KEY_KP_Down:
		case GDK_KEY_Down:
			if (event->state & GDK_SHIFT_MASK)
				goto page_down;
			y = MIN (vupper - vpage, y + vstep);
			domove = TRUE;
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
					y = (vupper - vpage);
				}
			}
			else
			{
				y = vlower;
			}
			domove = TRUE;
			break;
		case GDK_KEY_KP_Page_Down:
		case GDK_KEY_Page_Down:
		case ' ':
		page_down:
			if (y >= (vupper - vpage))
			{
				if (preview->cur_page < preview->n_pages - 1)
				{
					goto_page (preview, preview->cur_page + 1);
					y = vlower;
				}
			}
			else
			{
				y = (vupper - vpage);
			}
			domove = TRUE;
			break;
		case GDK_KEY_KP_Home:
		case GDK_KEY_Home:
			goto_page (preview, 0);
			y = 0;
			domove = TRUE;
			break;
		case GDK_KEY_KP_End:
		case GDK_KEY_End:
			goto_page (preview, preview->n_pages - 1);
			y = 0;
			domove = TRUE;
			break;
		case GDK_KEY_Escape:
			gtk_widget_destroy (GTK_WIDGET (preview));
			break;
		case 'c':
			if (event->state & GDK_MOD1_MASK)
			{
				gtk_widget_destroy (GTK_WIDGET (preview));
			}
			break;
		case 'p':
			if (event->state & GDK_MOD1_MASK)
			{
				gtk_widget_grab_focus (GTK_WIDGET (preview->page_entry));
			}
			break;
		default:
			/* by default do not stop the default handler */
			ret = FALSE;
	}

	if (domove)
	{
		gtk_adjustment_set_value (hadj, x);
		gtk_adjustment_set_value (vadj, y);
	}

	return ret;
}

static void
gedit_print_preview_init (GeditPrintPreview *preview)
{
	preview->operation = NULL;
	preview->context = NULL;
	preview->gtk_preview = NULL;

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

	g_object_set (preview->layout, "has-tooltip", TRUE, NULL);
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

	preview->cur_page = 0;
	preview->paper_width = 0;
	preview->paper_height = 0;
	preview->dpi = PRINTER_DPI;
	preview->scale = 1.0;
	preview->rows = 1;
	preview->cols = 1;
	preview->cursor_x = 0;
	preview->cursor_y = 0;
	preview->has_tooltip = TRUE;
}

static void
draw_page_content (cairo_t            *cr,
		   gint	               page_number,
		   GeditPrintPreview  *preview)
{
	/* scale to the desired size */
	cairo_scale (cr, preview->scale, preview->scale);

	gtk_print_context_set_cairo_context (preview->context,
	                                     cr,
	                                     preview->dpi,
	                                     preview->dpi);

	gtk_print_operation_preview_render_page (preview->gtk_preview,
	                                         page_number);
}

/* For the frame, we scale and rotate manually, since
 * the line width should not depend on the zoom and
 * the drop shadow should be on the bottom right no matter
 * the orientation */
static void
draw_page_frame (cairo_t            *cr,
		 GeditPrintPreview  *preview)
{
	double w, h;

	w = get_paper_width (preview);
	h = get_paper_height (preview);

	w *= preview->scale;
	h *= preview->scale;

	/* drop shadow */
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr,
			 PAGE_SHADOW_OFFSET, PAGE_SHADOW_OFFSET,
			 w, h);
	cairo_fill (cr);

	/* page frame */
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_rectangle (cr,
			 0, 0,
			 w, h);
	cairo_fill_preserve (cr);
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

static void
draw_page (cairo_t           *cr,
	   double             x,
	   double             y,
	   gint	              page_number,
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
	gint pg;
	gint i, j;

	bin_window = gtk_layout_get_bin_window (preview->layout);

	if (gtk_cairo_should_draw_window (cr, bin_window))
	{
		cairo_save (cr);

		gtk_cairo_transform_to_window (cr, widget, bin_window);

		/* get the first page to display */
		pg = get_first_page_displayed (preview);

		for (i = 0; i < preview->cols; ++i)
		{
			for (j = 0; j < preview->rows; ++j)
			{
				if (!gtk_print_operation_preview_is_selected (preview->gtk_preview, pg))
				{
					continue;
				}

				if (pg == preview->n_pages)
				{
					break;
				}

				draw_page (cr,
					   j * preview->tile_width,
					   i * preview->tile_height,
					   pg,
					   preview);

				++pg;
			}
		}

		cairo_restore (cr);
	}

	return TRUE;
}

static double
get_screen_dpi (GeditPrintPreview *preview)
{
	GdkScreen *screen;
	double dpi;

	screen = gtk_widget_get_screen (GTK_WIDGET (preview));

	dpi = gdk_screen_get_resolution (screen);
	if (dpi < 30. || 600. < dpi)
	{
		g_warning ("Invalid the x-resolution for the screen, assuming 96dpi");
		dpi = 96.;
	}

	return dpi;
}

static void
set_n_pages (GeditPrintPreview *preview,
	     gint               n_pages)
{
	gchar *str;

	preview->n_pages = n_pages;

	/* FIXME: count the visible pages */

	str =  g_strdup_printf ("%d", n_pages);
	gtk_label_set_markup (preview->last_page_label, str);
	g_free (str);
}

static void
preview_ready (GtkPrintOperationPreview *gtk_preview,
	       GtkPrintContext          *context,
	       GeditPrintPreview        *preview)
{
	gint n_pages;

	g_object_get (preview->operation, "n-pages", &n_pages, NULL);
	set_n_pages (preview, n_pages);
	goto_page (preview, 0);

	/* figure out the dpi */
	preview->dpi = get_screen_dpi (preview);

	set_zoom_factor (preview, 1.0);

	/* let the default gtklayout handler clear the background */
	g_signal_connect_after (preview->layout,
				"draw",
				G_CALLBACK (preview_draw),
				preview);

	gtk_widget_queue_draw (GTK_WIDGET (preview->layout));
}

static void
update_paper_size (GeditPrintPreview *preview,
		   GtkPageSetup      *page_setup)
{
	preview->paper_width = gtk_page_setup_get_paper_width (page_setup, GTK_UNIT_INCH);
	preview->paper_height = gtk_page_setup_get_paper_height (page_setup, GTK_UNIT_INCH);
}

static void
preview_got_page_size (GtkPrintOperationPreview *gtk_preview,
		       GtkPrintContext          *context,
		       GtkPageSetup             *page_setup,
		       GeditPrintPreview        *preview)
{
	update_paper_size (preview, page_setup);
}

/* HACK: we need a dummy surface to paginate... can we use something simpler? */

static cairo_status_t
dummy_write_func (G_GNUC_UNUSED gpointer      closure,
		  G_GNUC_UNUSED const guchar *data,
		  G_GNUC_UNUSED guint         length)
{
	return CAIRO_STATUS_SUCCESS;
}

#define PRINTER_DPI (72.)

static cairo_surface_t *
create_preview_surface_platform (GtkPaperSize *paper_size,
				 double       *dpi_x,
				 double       *dpi_y)
{
	double width, height;
	cairo_surface_t *sf;

	width = gtk_paper_size_get_width (paper_size, GTK_UNIT_POINTS);
	height = gtk_paper_size_get_height (paper_size, GTK_UNIT_POINTS);

	*dpi_x = *dpi_y = PRINTER_DPI;

	sf = cairo_pdf_surface_create_for_stream (dummy_write_func, NULL,
						  width, height);

	return sf;
}

static cairo_surface_t *
create_preview_surface (GeditPrintPreview *preview,
			double            *dpi_x,
			double            *dpi_y)
{
	GtkPageSetup *page_setup;
	GtkPaperSize *paper_size;

	page_setup = gtk_print_context_get_page_setup (preview->context);

	/* gtk_page_setup_get_paper_size swaps width and height for landscape */
	paper_size = gtk_page_setup_get_paper_size (page_setup);

	return create_preview_surface_platform (paper_size, dpi_x, dpi_y);
}

GtkWidget *
gedit_print_preview_new (GtkPrintOperation        *op,
			 GtkPrintOperationPreview *gtk_preview,
			 GtkPrintContext          *context)
{
	GeditPrintPreview *preview;
	GtkPageSetup *page_setup;
	cairo_surface_t *surface;
	cairo_t *cr;
	double dpi_x, dpi_y;

	g_return_val_if_fail (GTK_IS_PRINT_OPERATION (op), NULL);
	g_return_val_if_fail (GTK_IS_PRINT_OPERATION_PREVIEW (gtk_preview), NULL);

	preview = g_object_new (GEDIT_TYPE_PRINT_PREVIEW, NULL);

	preview->operation = g_object_ref (op);
	preview->gtk_preview = g_object_ref (gtk_preview);
	preview->context = g_object_ref (context);

	/* FIXME: is this legal?? */
	gtk_print_operation_set_unit (op, GTK_UNIT_POINTS);

	g_signal_connect (gtk_preview, "ready",
			  G_CALLBACK (preview_ready), preview);
	g_signal_connect (gtk_preview, "got-page-size",
			  G_CALLBACK (preview_got_page_size), preview);

	page_setup = gtk_print_context_get_page_setup (preview->context);
	update_paper_size (preview, page_setup);

	/* FIXME: we need a cr to paginate... but we can't get the drawing
	 * area surface because it's not there yet... for now I create
	 * a dummy pdf surface */

	surface = create_preview_surface (preview, &dpi_x, &dpi_y);
	cr = cairo_create (surface);
	gtk_print_context_set_cairo_context (context, cr, dpi_x, dpi_y);
	cairo_destroy (cr);
	cairo_surface_destroy (surface);

	return GTK_WIDGET (preview);
}

/* ex:set ts=8 noet: */
