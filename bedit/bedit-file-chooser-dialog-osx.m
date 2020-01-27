/*
 * bedit-file-chooser-dialog-osx.c
 * This file is part of bedit
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

#include "config.h"
#include "bedit-file-chooser-dialog-osx.h"

#import <Cocoa/Cocoa.h>
#include <gdk/gdkquartz.h>
#include <glib/gi18n.h>

#include "bedit-encoding-items.h"
#include "bedit-encodings-dialog.h"
#include "bedit-utils.h"

struct _BeditFileChooserDialogOSX
{
	GObject parent_instance;

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

	BeditFileChooserFlags flags;
};

static void bedit_file_chooser_dialog_osx_chooser_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_EXTENDED (BeditFileChooserDialogOSX,
                        bedit_file_chooser_dialog_osx,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (BEDIT_TYPE_FILE_CHOOSER_DIALOG,
                                               bedit_file_chooser_dialog_osx_chooser_init))

@interface NewlineItem : NSMenuItem

@property (readonly) GtkSourceNewlineType newline_type;

-(id)initWithType:(GtkSourceNewlineType)type;

@end

@implementation NewlineItem
-(id)initWithType:(GtkSourceNewlineType)type
{
	NSString *title;

	title = [NSString stringWithUTF8String:bedit_utils_newline_type_to_string (type)];

	self = [super initWithTitle:title action:nil keyEquivalent:@""];

	if (self)
	{
		_newline_type = type;
	}

	return self;
}
@end

@interface EncodingItem : NSMenuItem

@property (readonly) BeditEncodingItem *encoding;

-(id)initWithEncoding:(BeditEncodingItem *)encoding;

-(void)dealloc;
-(const GtkSourceEncoding *)source_encoding;

@end

@implementation EncodingItem
-(id)initWithEncoding:(BeditEncodingItem *)encoding
{
	NSString *title;

	title = [NSString stringWithUTF8String:bedit_encoding_item_get_name (encoding)];

	self = [super initWithTitle:title action:nil keyEquivalent:@""];

	if (self)
	{
		_encoding = encoding;
	}

	return self;
}

-(void)dealloc
{
	bedit_encoding_item_free (_encoding);
	_encoding = NULL;

	[super dealloc];
}

-(const GtkSourceEncoding *)source_encoding
{
	if (_encoding != NULL)
	{
		return bedit_encoding_item_get_encoding (_encoding);
	}

	return NULL;
}

@end

static void
chooser_set_encoding (BeditFileChooserDialog  *dialog,
                      const GtkSourceEncoding *encoding)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);
	gint i;

	if (dialog_osx->encoding_button == NULL)
	{
		return;
	}

	NSMenu *menu = [dialog_osx->encoding_button menu];
	NSArray *items = [menu itemArray];

	for (i = 0; i < [items count]; i++)
	{
		NSMenuItem *item = [items objectAtIndex:i];

		if ([item isKindOfClass:[EncodingItem class]])
		{
			EncodingItem *eitem = (EncodingItem *)item;

			if ([eitem source_encoding] == encoding)
			{
				[dialog_osx->encoding_button selectItemAtIndex:i];
				break;
			}
		}
	}
}

static const GtkSourceEncoding *
chooser_get_encoding (BeditFileChooserDialog *dialog)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);
	NSMenuItem *item;

	if (dialog_osx->encoding_button == NULL)
	{
		return gtk_source_encoding_get_utf8 ();
	}

	item = [dialog_osx->encoding_button selectedItem];

	if (item != nil && [item isKindOfClass:[EncodingItem class]])
	{
		return [(EncodingItem *)item source_encoding];
	}

	return NULL;
}

static void
chooser_set_newline_type (BeditFileChooserDialog *dialog,
                          GtkSourceNewlineType    newline_type)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);
	gint i;

	if (dialog_osx->newline_button == NULL)
	{
		return;
	}

	NSMenu *menu = [dialog_osx->newline_button menu];
	NSArray *items = [menu itemArray];

	for (i = 0; i < [items count]; i++)
	{
		NewlineItem *item = (NewlineItem *)[items objectAtIndex:i];

		if (item.newline_type == newline_type)
		{
			[dialog_osx->newline_button selectItemAtIndex:i];
			break;
		}
	}
}

static GtkSourceNewlineType
chooser_get_newline_type (BeditFileChooserDialog *dialog)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	if (dialog_osx->newline_button == NULL)
	{
		return GTK_SOURCE_NEWLINE_TYPE_DEFAULT;
	}

	NewlineItem *item = (NewlineItem *)[dialog_osx->newline_button selectedItem];
	return item.newline_type;
}

static void
chooser_set_current_folder (BeditFileChooserDialog *dialog,
                            GFile                  *folder)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	if (folder != NULL)
	{
		gchar *uri;

		uri = g_file_get_uri (folder);
		[dialog_osx->panel setDirectoryURL:[NSURL URLWithString:[NSString stringWithUTF8String:uri]]];
		g_free (uri);
	}
}

static void
chooser_set_current_name (BeditFileChooserDialog *dialog,
                          const gchar            *name)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	[dialog_osx->panel setNameFieldStringValue:[NSString stringWithUTF8String:name]];
}

static void
chooser_set_file (BeditFileChooserDialog *dialog,
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

static GSList *chooser_get_files (BeditFileChooserDialog *dialog);

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
chooser_get_file (BeditFileChooserDialog *dialog)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	if (dialog_osx->is_open)
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
		return ns_url_to_g_file ([dialog_osx->panel URL]);
	}
}


static GSList *
chooser_get_files (BeditFileChooserDialog *dialog)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	GSList *ret = NULL;

	if (dialog_osx->is_open)
	{
		NSArray *urls;
		gint i;

		urls = [(NSOpenPanel *)dialog_osx->panel URLs];

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
chooser_set_do_overwrite_confirmation (BeditFileChooserDialog *dialog,
                                       gboolean                overwrite_confirmation)
{
	// TODO: this is only implementable on OS X through hacks
	// I guess it's better just not having it
}

static void
fill_encodings (BeditFileChooserDialogOSX *dialog)
{
	NSPopUpButton *button;
	GSList *encodings;
	NSMenu *menu;
	gint i = 0;
	gint first = 0;
	const GtkSourceEncoding *encoding;

	encoding = bedit_file_chooser_dialog_get_encoding (BEDIT_FILE_CHOOSER_DIALOG (dialog));

	button = dialog->encoding_button;
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

			if ((dialog->flags & BEDIT_FILE_CHOOSER_OPEN) != 0 && first == 0)
			{
				first = i;
			}
		}
	}

	encodings = bedit_encoding_items_get ();

	while (encodings)
	{
		BeditEncodingItem *item = encodings->data;

		[menu insertItem:[[EncodingItem alloc] initWithEncoding:item] atIndex:first];

		if (encoding == bedit_encoding_item_get_encoding (item))
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
                    BeditFileChooserDialogOSX *chooser_dialog)
{
	if (response_id == GTK_RESPONSE_OK)
	{
		fill_encodings (chooser_dialog);
	}

	if (response_id != GTK_RESPONSE_HELP)
	{
		[chooser_dialog->panel setAlphaValue:1.0f];
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

@interface ConfigureEncodings : NSObject {
	BeditFileChooserDialogOSX *_dialog;
	const GtkSourceEncoding *_current;
}

-(id)initWithDialog:(BeditFileChooserDialogOSX *)dialog;

-(void)activateConfigure:(id)sender;
-(void)selectionChanged:(id)sender;

@end

@implementation ConfigureEncodings

-(id)initWithDialog:(BeditFileChooserDialogOSX *)dialog
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

	bedit_file_chooser_dialog_set_encoding (BEDIT_FILE_CHOOSER_DIALOG (_dialog),
	                                        _current);

	dialog = bedit_encodings_dialog_new ();

	if (_dialog->parent != NULL)
	{
		GtkWindowGroup *wg;

		gtk_window_set_transient_for (GTK_WINDOW (dialog), _dialog->parent);

		if (gtk_window_has_group (_dialog->parent))
		{
			wg = gtk_window_get_group (_dialog->parent);
		}
		else
		{
			wg = gtk_window_group_new ();
			gtk_window_group_add_window (wg, _dialog->parent);
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
	[_dialog->panel setAlphaValue:0.0f];
}

-(void)selectionChanged:(id)sender
{
	_current = bedit_file_chooser_dialog_get_encoding (BEDIT_FILE_CHOOSER_DIALOG (_dialog));
}

@end

static gint
create_encoding_combo (BeditFileChooserDialogOSX *dialog,
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

	if ((dialog->flags & BEDIT_FILE_CHOOSER_OPEN) != 0)
	{
		NSString *title;

		title = [NSString stringWithUTF8String:_("Automatically Detected")];

		[menu addItem:[[EncodingItem alloc] initWithTitle:title action:nil keyEquivalent:@""]];
		[menu addItem:[NSMenuItem separatorItem]];
	}

	config = [[ConfigureEncodings alloc] initWithDialog:dialog];

	[menu addItem:[NSMenuItem separatorItem]];
	citem = [[EncodingItem alloc] initWithTitle:[NSString stringWithUTF8String:_("Add or Remove…")]
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

	dialog->encoding_button = button;

	fill_encodings (dialog);

	return (gint)([button intrinsicContentSize].width + [label intrinsicContentSize].width);
}

static gint
create_newline_combo (BeditFileChooserDialogOSX *dialog,
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

	dialog->newline_button = button;

	return (gint)([button intrinsicContentSize].width + [label intrinsicContentSize].width);
}

static void
create_extra_widget (BeditFileChooserDialogOSX *dialog)
{
	gboolean needs_encoding;
	gboolean needs_line_ending;
	BeditFileChooserFlags flags;
	NSSize size;
	NSView *parent;
	NSView *container;
	gint minw = 0;

	flags = dialog->flags;

	needs_encoding = (flags & BEDIT_FILE_CHOOSER_ENABLE_ENCODING) != 0;
	needs_line_ending = (flags & BEDIT_FILE_CHOOSER_ENABLE_LINE_ENDING) != 0;

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

	[dialog->panel setAccessoryView:container];

	parent = [[container superview] superview];

	if ([parent isKindOfClass:[NSBox class]])
	{
		NSBox *box = (NSBox *)parent;
		[box setTransparent:YES];
	}

	size = [[container superview] frame].size;
	size.width = minw;

	[container setFrame:NSMakeRect(0, 0, size.width, size.height)];
	[dialog->panel setContentMinSize:size];
}

static void
chooser_show (BeditFileChooserDialog *dialog)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	if (dialog_osx->is_running)
	{
		// Just show it again
		[dialog_osx->panel makeKeyAndOrderFront:nil];
		return;
	}

	dialog_osx->is_running = TRUE;

	void (^handler)(NSInteger ret) = ^(NSInteger result) {
		GtkResponseType response;

		if (result == NSFileHandlingPanelOKButton)
		{
			response = dialog_osx->accept_response;
		}
		else
		{
			response = dialog_osx->cancel_response;
		}

		g_signal_emit_by_name (dialog, "response", response);
	};

	if (dialog_osx->parent != NULL && dialog_osx->is_modal)
	{
		GdkWindow *win;
		NSWindow *nswin;

		win = gtk_widget_get_window (GTK_WIDGET (dialog_osx->parent));
		nswin = gdk_quartz_window_get_nswindow (win);

		[dialog_osx->panel setLevel:NSModalPanelWindowLevel];

		[dialog_osx->panel beginSheetModalForWindow:nswin completionHandler:handler];
	}
	else
	{
		[dialog_osx->panel setLevel:NSModalPanelWindowLevel];
		[dialog_osx->panel beginWithCompletionHandler:handler];
	}
}

static void
chooser_hide (BeditFileChooserDialog *dialog)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	if (!dialog_osx->is_running || dialog_osx->panel == NULL)
	{
		return;
	}

	[dialog_osx->panel orderOut:nil];
}

static void
chooser_destroy (BeditFileChooserDialog *dialog)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	if (dialog_osx->parent != NULL)
	{
		g_object_remove_weak_pointer (G_OBJECT (dialog_osx->parent),
		                              (gpointer *)&dialog_osx->parent);

		if (dialog_osx->destroy_id != 0)
		{
			g_signal_handler_disconnect (dialog_osx->parent, dialog_osx->destroy_id);
			dialog_osx->destroy_id = 0;
		}
	}

	if (dialog_osx->panel != NULL)
	{
		[dialog_osx->panel close];
		dialog_osx->panel = NULL;
	}

	g_object_unref (dialog);
}

static void
chooser_set_modal (BeditFileChooserDialog *dialog,
                   gboolean is_modal)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (dialog);

	dialog_osx->is_modal = is_modal;
}

static void
bedit_file_chooser_dialog_osx_chooser_init (gpointer g_iface,
                                            gpointer iface_data)
{
	BeditFileChooserDialogInterface *iface = g_iface;

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
bedit_file_chooser_dialog_osx_dispose (GObject *object)
{
	BeditFileChooserDialogOSX *dialog_osx = BEDIT_FILE_CHOOSER_DIALOG_OSX (object);

	if (dialog_osx->panel != NULL)
	{
		[dialog_osx->panel close];
		dialog_osx->panel = NULL;
	}

	if (G_OBJECT_CLASS (bedit_file_chooser_dialog_osx_parent_class)->dispose != NULL)
	{
		G_OBJECT_CLASS (bedit_file_chooser_dialog_osx_parent_class)->dispose (object);
	}
}

static void
bedit_file_chooser_dialog_osx_class_init (BeditFileChooserDialogOSXClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = bedit_file_chooser_dialog_osx_dispose;
}

static void
bedit_file_chooser_dialog_osx_init (BeditFileChooserDialogOSX *dialog)
{
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
                     BeditFileChooserDialogOSX *dialog)
{
	chooser_destroy (BEDIT_FILE_CHOOSER_DIALOG (dialog));
}

BeditFileChooserDialog *
bedit_file_chooser_dialog_osx_create (const gchar             *title,
			              GtkWindow               *parent,
			              BeditFileChooserFlags    flags,
			              const GtkSourceEncoding *encoding,
			              const gchar             *cancel_label,
			              GtkResponseType          cancel_response,
			              const gchar             *accept_label,
			              GtkResponseType          accept_response)
{
	BeditFileChooserDialogOSX *ret;
	gchar *nomnem;

	ret = g_object_new (BEDIT_TYPE_FILE_CHOOSER_DIALOG_OSX, NULL);

	ret->cancel_response = cancel_response;
	ret->accept_response = accept_response;

	if ((flags & BEDIT_FILE_CHOOSER_SAVE) != 0)
	{
		NSSavePanel *panel = [[NSSavePanel savePanel] retain];

		if ([panel respondsToSelector:@selector(setShowsTagField:)])
		{
			[(id<CanSetShowsTagField>)panel setShowsTagField:NO];
		}

		ret->panel = panel;
		ret->is_open = FALSE;
	}
	else
	{
		NSOpenPanel *panel = [[NSOpenPanel openPanel] retain];

		[panel setAllowsMultipleSelection:YES];
		[panel setCanChooseDirectories:NO];

		ret->panel = panel;
		ret->is_open = TRUE;
	}

	[ret->panel setReleasedWhenClosed:YES];

	nomnem = strip_mnemonic (accept_label);
	[ret->panel setPrompt:[NSString stringWithUTF8String:nomnem]];
	g_free (nomnem);

	if (parent != NULL)
	{
		ret->parent = parent;
		g_object_add_weak_pointer (G_OBJECT (parent), (gpointer *)&ret->parent);

		ret->destroy_id = g_signal_connect (parent,
		                                    "destroy",
		                                    G_CALLBACK (on_parent_destroyed),
		                                    ret);
	}

	ret->flags = flags;
	create_extra_widget (ret);

	[ret->panel setTitle:[NSString stringWithUTF8String:title]];
	return BEDIT_FILE_CHOOSER_DIALOG (ret);
}

