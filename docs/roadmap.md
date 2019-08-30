gedit roadmap
=============

Make the gedit source code more re-usable
-----------------------------------------

**Status**: in progress.

https://wiki.gnome.org/Apps/Gedit/ReusableCode

Next steps:
- Start to use the [Tepl](https://wiki.gnome.org/Projects/Tepl) library.

Sébastien Wilmet

Changing character encoding and line ending type of opened files
----------------------------------------------------------------

To fully support GtkFileChooserNative and better sandboxing.

Note that the integrated file browser plugin needs access at least to the whole
home directory. But the work on this task (with the code in Tepl) would allow
better sandboxing for other text editors that don't have an integrated file
browser.

Handle problem with large files or files containing very long lines
-------------------------------------------------------------------

**Status**: in progress in Tepl.

As a quick stopgap measure, prevent those files from being loaded in the first
place, show an infobar with an error message.

Longer-term solution: fix the performance problem in GTK for very long lines.

For very big file size (e.g. a 1GB log file or SQL dump), it's more complicated
because the whole file is loaded in memory. It needs another data structure
implementation for the GtkTextView API.

Sébastien Wilmet

Do not allow incompatible plugins to be loaded
----------------------------------------------

There are currently no checks to see if a plugin is compatible with the gedit
version. Currently enabling a plugin can make gedit to crash.

Solution: include the gedit plugin API version in the directory names where
plugins need to be installed. Better solution: see
[this libpeas feature request](https://bugzilla.gnome.org/show_bug.cgi?id=642694#c15).

Sébastien Wilmet

Be able to quit the application with all documents saved, and restored on next start
------------------------------------------------------------------------------------

Like in Sublime Text. Even for unsaved and untitled files, be able to quit
gedit, restart it later and come back to the state before with all tabs
restored.

Better C language support
-------------------------

Needed for more dog-fooding of gedit.

- Code completion with Clang.
- Align function parameters on the parenthesis (function definition /
  function call).
- Generate and insert GTK-Doc comment header for a function.
- Split/join lines of a C comment with `*` at beginning of each line, ditto when
  pressing Enter (insert `*` at the beginning of the new line).

Sébastien Wilmet

Improve printing UI workflow
----------------------------

Implement it like in Firefox, show first a preview of the file to print.

Sébastien Wilmet

Replace search and replace dialog window by an horizontal bar above or below the text
-------------------------------------------------------------------------------------

To not hide the text.

Windows and Mac OS X support
----------------------------

To release new versions. And adapt/port the code if needed.

### Windows

**Status**: stalled

Ignacio Casal Quinteiro

### Mac OS X

**Status**: stalled

Jesse van den Kieboom
