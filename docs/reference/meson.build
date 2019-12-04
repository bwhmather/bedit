glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html', 'glib')

gobject_prefix = gobject_dep.get_pkgconfig_variable('prefix')
gobject_docpath = join_paths(gobject_prefix, 'share', 'gtk-doc', 'html', 'gobject')

gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')
gtk_docpath = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html', 'gtk')

tepl_prefix = tepl_dep.get_pkgconfig_variable('prefix')
tepl_docpath = join_paths(tepl_prefix, 'share', 'gtk-doc', 'html', 'tepl-4.0')

gedit_doc_deps = [
  glib_dep,
  gobject_dep,
  gio_dep,
  gtk_dep,
  gtksourceview_dep,
  tepl_dep,
  libpeas_dep,
  libpeas_gtk_dep,
]

gedit_doc_dep = declare_dependency(
  link_with: libgedit_sha,
  include_directories: rootdir,
  dependencies: gedit_doc_deps,
  sources: libgedit_dep_sources,
)

libgedit_reference_ignored_h = [
  'gedit-app-osx.h',
  'gedit-app-win32.h',
  'gedit-app-x11.h',
  'gedit-close-confirmation-dialog.h',
  'gedit-dirs.h',
  'gedit-document-private.h',
  'gedit-documents-panel.h',
  'gedit-encoding-items.h',
  'gedit-encodings-dialog.h',
  'gedit-enum-types.h',
  'gedit-file-chooser-dialog-gtk.h',
  'gedit-file-chooser-dialog-osx.h',
  'gedit-file-chooser-dialog.h',
  'gedit-highlight-mode-dialog.h',
  'gedit-highlight-mode-selector.h',
  'gedit-history-entry.h',
  'gedit-io-error-info-bar.h',
  'gedit-menu-stack-switcher.h',
  'gedit-multi-notebook.h',
  'gedit-notebook-popup-menu.h',
  'gedit-notebook-stack-switcher.h',
  'gedit-notebook.h',
  'gedit-open-menu-button.h',
  'gedit-pango.h',
  'gedit-plugins-engine.h',
  'gedit-preferences-dialog.h',
  'gedit-print-job.h',
  'gedit-print-preview.h',
  'gedit-recent.h',
  'gedit-replace-dialog.h',
  'gedit-settings.h',
  'gedit-small-button.h',
  'gedit-status-menu-button.h',
  'gedit-tab-label.h',
  'gedit-view-frame.h',
  'gedit-window-private.h',
]

libgedit_reference_scan_args = [
  '--rebuild-types',
  '--deprecated-guards=GEDIT_DISABLE_DEPRECATED',
  '--ignore-headers=' + ' '.join(libgedit_reference_ignored_h),
]

libgedit_reference_mkdb_args = [
  '--xml-mode',
  '--output-format=xml',
  '--name-space=gedit',
]

libgedit_reference_fixxref_args = [
  '--html-dir=@0@'.format(join_paths(gtkdocdir, 'html', package_string)),
  '--extra-dir=@0@'.format(glib_docpath),
  '--extra-dir=@0@'.format(gobject_docpath),
  '--extra-dir=@0@'.format(gtk_docpath),
  '--extra-dir=@0@'.format(tepl_docpath),
]

gnome.gtkdoc(
  meson.project_name(),
  main_sgml: 'gedit-docs.xml',
  src_dir: join_paths(srcdir, 'gedit'),
  dependencies: gedit_doc_dep,
  scan_args: libgedit_reference_scan_args,
  mkdb_args: libgedit_reference_mkdb_args,
  fixxref_args: libgedit_reference_fixxref_args,
  content_files : [
    'api-breaks.xml'
  ],
  install: true,
  install_dir: join_paths(
    gtkdocdir,
    'html',
    meson.project_name(),
  )
)
