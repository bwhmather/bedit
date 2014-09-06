/*
 * gedit-file-chooser-dialog-osx.c
 * This file is part of gedit
 *
 * Copyright (C) 2014 - Jesse van den Kieboom
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#import <Cocoa/Cocoa.h>
#include <gdk/gdkquartz.h>
#include <glib/gi18n.h>

#include "gedit-file-chooser-dialog-osx.h"
#include "gedit-encoding-items.h"
#include "gedit-encodings-dialog.h"
#include "gedit-utils.h"

struct _GeditFileChooserDialogOSXPrivate
{
	/* Note this can be either an NSSavePanel or NSOpenPanel,
	 * and NSOpenPanel inherits from NSSavePanel. */
	NSSavePanel *panel;
	GtkWindow *parent;
	NSPopUpButton *newline_button;
	NSPopUpButton *encoding_button;

	gboolean is_open;
	gboolean is_modal;
	gboolean is_running;

	GtkResponseType cancel_response;
	GtkResponseType accept_response;

	gulong destroy_id;

	GeditFileChooserFlags flags;
};

static void gedit_file_chooser_dialog_osx_chooser_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (GeditFileChooserDialogOSX,
                        gedit_file_chooser_dialog_osx,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GEDIT_TYPE_FILE_CHOOSER_DIALOG,
                                               gedit_file_chooser_dialog_osx_chooser_init)
                        G_ADD_PRIVATE (GeditFileChooserDialogOSX))

@interface NewlineItem : NSMenuItem

@property (readonly) GtkSourceNewlineType newline_type;

-(id)initWithType:(GtkSourceNewlineType)type;

@end

@implementation NewlineItem
-(id)initWithType:(GtkSourceNewlineType)type
{
	NSString *title;

	title = [NSString stringWithUTF8String:gedit_utils_newline_type_to_string (type)];

	self = [super initWithTitle:title action:nil keyEquivalent:@""];

	if (self)
	{
		_newline_type = type;
	}

	return self;
}
@end

@interface EncodingItem : NSMenuItem

@property (readonly) GeditEncodingItem *encoding;

-(id)initWithEncoding:(GeditEncodingItem *)encoding;

-(void)dealloc;
-(const GtkSourceEncoding *)source_encoding;

@end

@implementation EncodingItem
-(id)initWithEncoding:(GeditEncodingItem *)encoding
{
	NSString *title;

	title = [NSString stringWithUTF8String:gedit_encoding_item_get_name (encoding)];

	self = [super initWithTitle:title action:nil keyEquivalent:@""];

	if (self)
	{
		_encoding = encoding;
	}

	return self;
}

-(void)dealloc
{
	gedit_encoding_item_free (_encoding);
	_encoding = NULL;

	[super dealloc];
}

-(const GtkSourceEncoding *)source_encoding
{
	if (_encoding != NULL)
	{
		return gedit_encoding_item_get_encoding (_encoding);
	}

	return NULL;
}

@end

static void
chooser_set_encoding (GeditFileChooserDialog  *dialog,
                      const GtkSourceEncoding *encoding)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;
	gint i;

	if (priv->encoding_button == NULL)
	{
		return;
	}

	NSMenu *menu = [priv->encoding_button menu];
	NSArray *items = [menu itemArray];

	for (i = 0; i < [items count]; i++)
	{
		NSMenuItem *item = [items objectAtIndex:i];

		if ([item isKindOfClass:[EncodingItem class]])
		{
			EncodingItem *eitem = (EncodingItem *)item;

			if ([eitem source_encoding] == encoding)
			{
				[priv->encoding_button selectItemAtIndex:i];
				break;
			}
		}
	}
}

static const GtkSourceEncoding *
chooser_get_encoding (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;
	NSMenuItem *item;

	if (priv->encoding_button == NULL)
	{
		return gtk_source_encoding_get_utf8 ();
	}

	item = [priv->encoding_button selectedItem];

	if (item != nil && [item isKindOfClass:[EncodingItem class]])
	{
		return [(EncodingItem *)item source_encoding];
	}

	return NULL;
}

static void
chooser_set_newline_type (GeditFileChooserDialog *dialog,
                          GtkSourceNewlineType    newline_type)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;
	gint i;

	if (priv->newline_button == NULL)
	{
		return;
	}

	NSMenu *menu = [priv->newline_button menu];
	NSArray *items = [menu itemArray];

	for (i = 0; i < [items count]; i++)
	{
		NewlineItem *item = (NewlineItem *)[items objectAtIndex:i];

		if (item.newline_type == newline_type)
		{
			[priv->newline_button selectItemAtIndex:i];
			break;
		}
	}
}

static GtkSourceNewlineType
chooser_get_newline_type (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	if (priv->newline_button == NULL)
	{
		return GTK_SOURCE_NEWLINE_TYPE_DEFAULT;
	}

	NewlineItem *item = (NewlineItem *)[priv->newline_button selectedItem];
	return item.newline_type;
}

static void
chooser_set_current_folder (GeditFileChooserDialog *dialog,
                            GFile                  *folder)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	if (folder != NULL)
	{
		gchar *uri;

		uri = g_file_get_uri (folder);
		[priv->panel setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:uri]]];
		g_free (uri);
	}
}

static void
chooser_set_current_name (GeditFileChooserDialog *dialog,
                          const gchar            *name)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	[priv->panel setNameFieldStringValue:[NSString stringWithUTF8String:name]];
}

static void
chooser_set_file (GeditFileChooserDialog *dialog,
                  GFile                  *file)
{
	GFile *folder;
	gchar *name;

	if (file == NULL)
	{
		return;
	}

	folder = g_file_get_parent (file);
	name = g_file_get_basename (file);

	chooser_set_current_folder (dialog, folder);
	chooser_set_current_name (dialog, name);

	g_object_unref (folder);
	g_free (name);
}

static GSList *chooser_get_files (GeditFileChooserDialog *dialog);

static GFile *
ns_url_to_g_file (NSURL *url)
{
	if (url == nil)
	{
		return NULL;
	}

	return g_file_new_for_uri ([[url absoluteString] UTF8String]);
}

static GFile *
chooser_get_file (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	if (priv->is_open)
	{
		GSList *ret;
		GFile *file = NULL;

		ret = chooser_get_files (dialog);

		if (ret != NULL)
		{
			file = ret->data;
			ret = g_slist_delete_link (ret, ret);
		}

		g_slist_free_full (ret, (GDestroyNotify)g_object_unref);
		return file;
	}
	else
	{
		return ns_url_to_g_file ([priv->panel URL]);
	}
}


static GSList *
chooser_get_files (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	GSList *ret = NULL;

	if (priv->is_open)
	{
		NSArray *urls;
		gint i;

		urls = [(NSOpenPanel *)priv->panel URLs];

		for (i = 0; i < [urls count]; i++)
		{
			NSURL *url;

			url = (NSURL *)[urls objectAtIndex:i];
			ret = g_slist_prepend (ret, ns_url_to_g_file (url));
		}
	}
	else
	{
		GFile *file;

		file = chooser_get_file (dialog);

		if (file != NULL)
		{
			ret = g_slist_prepend (ret, file);
		}
	}

	return g_slist_reverse (ret);
}

static void
chooser_set_do_overwrite_confirmation (GeditFileChooserDialog *dialog,
                                       gboolean                overwrite_confirmation)
{
	// TODO: this is only implementable on OS X through hacks
	// I guess it's better just not having it
}

static void
fill_encodings (GeditFileChooserDialogOSX *dialog)
{
	NSPopUpButton *button;
	GSList *encodings;
	NSMenu *menu;
	gint i = 0;
	gint first = 0;
	const GtkSourceEncoding *encoding;

	encoding = gedit_file_chooser_dialog_get_encoding (GEDIT_FILE_CHOOSER_DIALOG (dialog));

	button = dialog->priv->encoding_button;
	menu = [button menu];

	while (i < [menu numberOfItems])
	{
		NSMenuItem *item = [menu itemAtIndex:i];

		if ([item isKindOfClass:[EncodingItem class]])
		{
			EncodingItem *eitem = (EncodingItem *)item;

			if ([eitem source_encoding] != NULL)
			{
				if (first == 0)
				{
					first = i;
				}

				[menu removeItemAtIndex:i];
			}
			else
			{
				i++;
			}
		}
		else
		{
			i++;

			if ((dialog->priv->flags & GEDIT_FILE_CHOOSER_OPEN) != 0 && first == 0)
			{
				first = i;
			}
		}
	}

	encodings = gedit_encoding_items_get ();

	while (encodings)
	{
		GeditEncodingItem *item = encodings->data;

		[menu insertItem:[[EncodingItem alloc] initWithEncoding:item] atIndex:first];

		if (encoding == gedit_encoding_item_get_encoding (item))
		{
			[button selectItemAtIndex:first];
		}

		first++;
		encodings = g_slist_delete_link (encodings, encodings);
	}

	if (encoding == NULL)
	{
		[button selectItemAtIndex:0];
	}
}

static void
dialog_response_cb (GtkDialog                 *dialog,
                    gint                       response_id,
                    GeditFileChooserDialogOSX *chooser_dialog)
{
	if (response_id == GTK_RESPONSE_OK)
	{
		fill_encodings (chooser_dialog);
	}

	if (response_id != GTK_RESPONSE_HELP)
	{
		[chooser_dialog->priv->panel setAlphaValue:1.0f];
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

@interface ConfigureEncodings : NSObject {
	GeditFileChooserDialogOSX *_dialog;
	const GtkSourceEncoding *_current;
}

-(id)initWithDialog:(GeditFileChooserDialogOSX *)dialog;

-(void)activateConfigure:(id)sender;
-(void)selectionChanged:(id)sender;

@end

@implementation ConfigureEncodings

-(id)initWithDialog:(GeditFileChooserDialogOSX *)dialog
{
	self = [super init];

	if (self)
	{
		_dialog = dialog;
	}

	return self;
}

-(void)activateConfigure:(id)sender
{
	GtkWidget *dialog;

	gedit_file_chooser_dialog_set_encoding (GEDIT_FILE_CHOOSER_DIALOG (_dialog),
	                                        _current);

	dialog = gedit_encodings_dialog_new ();

	if (_dialog->priv->parent != NULL)
	{
		GtkWindowGroup *wg;

		gtk_window_set_transient_for (GTK_WINDOW (dialog),
					      _dialog->priv->parent);

		if (gtk_window_has_group (_dialog->priv->parent))
		{
			wg = gtk_window_get_group (_dialog->priv->parent);
		}
		else
		{
			wg = gtk_window_group_new ();
			gtk_window_group_add_window (wg, _dialog->priv->parent);
		}

		gtk_window_group_add_window (wg, GTK_WINDOW (dialog));
	}

	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

	g_signal_connect_after (dialog,
				"response",
				G_CALLBACK (dialog_response_cb),
				_dialog);

	gtk_widget_show (dialog);

	/* We set the alpha value to 0 here because it was the only way that we could
	   find to make the panel not receive any events. Ideally the encodings dialog
	   should be modal on top of the NSPanel, but the modal loops of gtk and NS
	   do not mix. This is an ugly hack, but at least we can't interact with the
	   NSPanel while showing the encodings dialog... */
	[_dialog->priv->panel setAlphaValue:0.0f];
}

-(void)selectionChanged:(id)sender
{
	_current = gedit_file_chooser_dialog_get_encoding (GEDIT_FILE_CHOOSER_DIALOG (_dialog));
}

@end

static gint
create_encoding_combo (GeditFileChooserDialogOSX *dialog,
                       NSView                    *container)
{
	NSTextField *label;
	NSPopUpButton *button;
	NSMenu *menu;
	ConfigureEncodings *config;
	NSMenuItem *citem;

	label = [NSTextField new];

	[label setTranslatesAutoresizingMaskIntoConstraints:NO];
	[label setStringValue:[NSString stringWithUTF8String:_("Character Encoding:")]];
	[label setDrawsBackground:NO];
	[label setBordered:NO];
	[label setBezeled:NO];
	[label setSelectable:NO];
	[label setEditable:NO];

	[container addSubview:label];

	button = [NSPopUpButton new];
	[button setTranslatesAutoresizingMaskIntoConstraints:NO];

	menu = [button menu];

	if ((dialog->priv->flags & GEDIT_FILE_CHOOSER_OPEN) != 0)
	{
		NSString *title;

		title = [NSString stringWithUTF8String:_("Automatically Detected")];

		[menu addItem:[[EncodingItem alloc] initWithTitle:title action:nil keyEquivalent:@""]];
		[menu addItem:[NSMenuItem separatorItem]];
	}

	config = [[ConfigureEncodings alloc] initWithDialog:dialog];

	[menu addItem:[NSMenuItem separatorItem]];
	citem = [[EncodingItem alloc] initWithTitle:[NSString stringWithUTF8String:_("Add or Remove...")]
	                                     action:@selector(activateConfigure:)
	                              keyEquivalent:@""];

	[citem setTarget:config];
	[menu addItem:citem];

	[button setTarget:config];
	[button setAction:@selector(selectionChanged:)];

	[button synchronizeTitleAndSelectedItem];
	[config selectionChanged:nil];

	[container addSubview:button];

	[container addConstraint:[NSLayoutConstraint constraintWithItem:button
	                                                      attribute:NSLayoutAttributeTop
	                                                      relatedBy:NSLayoutRelationEqual
	                                                         toItem:container
	                                                      attribute:NSLayoutAttributeTop
	                                                     multiplier:1
	                                                       constant:3]];

	[container addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"|-[label]-[button]"
	                                                                  options:NSLayoutFormatAlignAllBaseline
	                                                                  metrics:nil
	                                                                    views:NSDictionaryOfVariableBindings(label, button)]];

	dialog->priv->encoding_button = button;

	fill_encodings (dialog);

	return (gint)([button intrinsicContentSize].width + [label intrinsicContentSize].width);
}

static gint
create_newline_combo (GeditFileChooserDialogOSX *dialog,
                      NSView                    *container)
{
	NSTextField *label;
	NSMenu *menu;
	NSPopUpButton *button;

	label = [NSTextField new];

	[label setTranslatesAutoresizingMaskIntoConstraints:NO];
	[label setStringValue:[NSString stringWithUTF8String:_("Line Ending:")]];
	[label setDrawsBackground:NO];
	[label setBordered:NO];
	[label setBezeled:NO];
	[label setSelectable:NO];
	[label setEditable:NO];

	[container addSubview:label];

	button = [NSPopUpButton new];

	[button setTranslatesAutoresizingMaskIntoConstraints:NO];

	menu = [button menu];

	[menu addItem:[[NewlineItem alloc] initWithType:GTK_SOURCE_NEWLINE_TYPE_LF]];
	[menu addItem:[[NewlineItem alloc] initWithType:GTK_SOURCE_NEWLINE_TYPE_CR]];
	[menu addItem:[[NewlineItem alloc] initWithType:GTK_SOURCE_NEWLINE_TYPE_CR_LF]];

	[button synchronizeTitleAndSelectedItem];

	[container addSubview:button];

	[container addConstraint:[NSLayoutConstraint constraintWithItem:button
	                                                      attribute:NSLayoutAttributeTop
	                                                      relatedBy:NSLayoutRelationEqual
	                                                         toItem:container
	                                                      attribute:NSLayoutAttributeTop
	                                                     multiplier:1
	                                                       constant:3]];

	[container addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"[label]-[button]-|"
	                                                                  options:NSLayoutFormatAlignAllBaseline
	                                                                  metrics:nil
	                                                                    views:NSDictionaryOfVariableBindings(label, button)]];

	dialog->priv->newline_button = button;

	return (gint)([button intrinsicContentSize].width + [label intrinsicContentSize].width);
}

static void
create_extra_widget (GeditFileChooserDialogOSX *dialog)
{
	gboolean needs_encoding;
	gboolean needs_line_ending;
	GeditFileChooserFlags flags;
	NSSize size;
	NSView *parent;
	NSView *container;
	gint minw = 0;

	flags = dialog->priv->flags;

	needs_encoding = (flags & GEDIT_FILE_CHOOSER_ENABLE_ENCODING) != 0;
	needs_line_ending = (flags & GEDIT_FILE_CHOOSER_ENABLE_LINE_ENDING) != 0;

	if (!needs_encoding && !needs_line_ending)
	{
		return;
	}

	container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 400, 30)];

	if (needs_encoding)
	{
		minw += create_encoding_combo (dialog, container);
	}

	if (needs_line_ending)
	{
		minw += create_newline_combo (dialog, container);
	}

	minw += 90;

	[container setFrame:NSMakeRect(0, 0, minw, 30)];

	[dialog->priv->panel setAccessoryView:container];

	parent = [[container superview] superview];

	if ([parent isKindOfClass:[NSBox class]])
	{
		NSBox *box = (NSBox *)parent;
		[box setTransparent:YES];
	}

	size = [[container superview] frame].size;
	size.width = minw;

	[container setFrame:NSMakeRect(0, 0, size.width, size.height)];
	[dialog->priv->panel setContentMinSize:size];
}

static void
chooser_show (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	if (priv->is_running)
	{
		// Just show it again
		[priv->panel makeKeyAndOrderFront:nil];
		return;
	}

	priv->is_running = TRUE;

	void (^handler)(NSInteger ret) = ^(NSInteger result) {
		GtkResponseType response;

		if (result == NSFileHandlingPanelOKButton)
		{
			response = priv->accept_response;
		}
		else
		{
			response = priv->cancel_response;
		}

		g_signal_emit_by_name (dialog, "response", response);
	};

	if (priv->parent != NULL && priv->is_modal)
	{
		GdkWindow *win;
		NSWindow *nswin;

		win = gtk_widget_get_window (GTK_WIDGET (priv->parent));
		nswin = gdk_quartz_window_get_nswindow (win);

		[priv->panel setLevel:NSModalPanelWindowLevel];

		[priv->panel beginSheetModalForWindow:nswin completionHandler:handler];
	}
	else
	{
		[priv->panel setLevel:NSModalPanelWindowLevel];
		[priv->panel beginWithCompletionHandler:handler];
	}
}

static void
chooser_hide (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	if (!priv->is_running || priv->panel == NULL)
	{
		return;
	}

	[priv->panel orderOut:nil];
}

static void
chooser_destroy (GeditFileChooserDialog *dialog)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	if (priv->parent != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (priv->parent),
		                              (gpointer *)&priv->parent);

		if (priv->destroy_id != 0)
		{
			g_signal_handler_disconnect (priv->parent, priv->destroy_id);
			priv->destroy_id = 0;
		}
	}

	if (priv->panel != NULL)
	{
		[priv->panel close];
		priv->panel = NULL;
	}

	g_object_unref (dialog);
}

static void
chooser_set_modal (GeditFileChooserDialog *dialog,
                   gboolean is_modal)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (dialog)->priv;

	priv->is_modal = is_modal;
}

static void
gedit_file_chooser_dialog_osx_chooser_init (gpointer g_iface,
                                            gpointer iface_data)
{
	GeditFileChooserDialogInterface *iface = g_iface;

	iface->set_encoding = chooser_set_encoding;
	iface->get_encoding = chooser_get_encoding;

	iface->set_newline_type = chooser_set_newline_type;
	iface->get_newline_type = chooser_get_newline_type;

	iface->set_current_folder = chooser_set_current_folder;
	iface->set_current_name = chooser_set_current_name;
	iface->set_file = chooser_set_file;
	iface->get_file = chooser_get_file;
	iface->get_files = chooser_get_files;
	iface->set_do_overwrite_confirmation = chooser_set_do_overwrite_confirmation;
	iface->show = chooser_show;
	iface->hide = chooser_hide;
	iface->destroy = chooser_destroy;
	iface->set_modal = chooser_set_modal;
}

static void
gedit_file_chooser_dialog_osx_dispose (GObject *object)
{
	GeditFileChooserDialogOSXPrivate *priv = GEDIT_FILE_CHOOSER_DIALOG_OSX (object)->priv;

	if (priv->panel != NULL)
	{
		[priv->panel close];
		priv->panel = NULL;
	}

	if (G_OBJECT_CLASS (gedit_file_chooser_dialog_osx_parent_class)->dispose != NULL)
	{
		G_OBJECT_CLASS (gedit_file_chooser_dialog_osx_parent_class)->dispose (object);
	}
}

static void
gedit_file_chooser_dialog_osx_class_init (GeditFileChooserDialogOSXClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = gedit_file_chooser_dialog_osx_dispose;
}

static void
gedit_file_chooser_dialog_osx_init (GeditFileChooserDialogOSX *dialog)
{
	dialog->priv = gedit_file_chooser_dialog_osx_get_instance_private (dialog);
}

@protocol CanSetShowsTagField
- (void)setShowsTagField:(BOOL)val;
@end

static gchar *
strip_mnemonic (const gchar *s)
{
	gchar *escaped;
	gchar *ret = NULL;

	escaped = g_markup_escape_text (s, -1);
	pango_parse_markup (escaped, -1, '_', NULL, &ret, NULL, NULL);

	if (ret != NULL)
	{
		return ret;
	}
	else
	{
		return g_strdup (s);
	}
}

static void
on_parent_destroyed (GtkWindow                 *parent,
                     GeditFileChooserDialogOSX *dialog)
{
	chooser_destroy (GEDIT_FILE_CHOOSER_DIALOG (dialog));
}

GeditFileChooserDialog *
gedit_file_chooser_dialog_osx_create (const gchar             *title,
			              GtkWindow               *parent,
			              GeditFileChooserFlags    flags,
			              const GtkSourceEncoding *encoding,
			              const gchar             *cancel_label,
			              GtkResponseType          cancel_response,
			              const gchar             *accept_label,
			              GtkResponseType          accept_response)
{
	GeditFileChooserDialogOSX *ret;
	gchar *nomnem;

	ret = g_object_new (GEDIT_TYPE_FILE_CHOOSER_DIALOG_OSX, NULL);

	ret->priv->cancel_response = cancel_response;
	ret->priv->accept_response = accept_response;

	if ((flags & GEDIT_FILE_CHOOSER_SAVE) != 0)
	{
		NSSavePanel *panel = [[NSSavePanel savePanel] retain];

		if ([panel respondsToSelector:@selector(setShowsTagField:)])
		{
			[(id<CanSetShowsTagField>)panel setShowsTagField:NO];
		}

		ret->priv->panel = panel;
		ret->priv->is_open = FALSE;
	}
	else
	{
		NSOpenPanel *panel = [[NSOpenPanel openPanel] retain];

		[panel setAllowsMultipleSelection:YES];
		[panel setCanChooseDirectories:NO];

		ret->priv->panel = panel;
		ret->priv->is_open = TRUE;
	}

	[ret->priv->panel setReleasedWhenClosed:YES];

	nomnem = strip_mnemonic (accept_label);
	[ret->priv->panel setPrompt:[NSString stringWithUTF8String:nomnem]];
	g_free (nomnem);

	if (parent != NULL)
	{
		ret->priv->parent = parent;
		g_object_add_weak_pointer (G_OBJECT (parent), (gpointer *)&ret->priv->parent);

		ret->priv->destroy_id = g_signal_connect (parent,
		                                          "destroy",
		                                          G_CALLBACK (on_parent_destroyed),
		                                          ret);
	}

	ret->priv->flags = flags;
	create_extra_widget (ret);

	[ret->priv->panel setTitle:[NSString stringWithUTF8String:title]];
	return GEDIT_FILE_CHOOSER_DIALOG (ret);
}

/* ex:set ts=8 noet: */
