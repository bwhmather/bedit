glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html', 'glib')

gobject_prefix = gobject_dep.get_pkgconfig_variable('prefix')
gobject_docpath = join_paths(gobject_prefix, 'share', 'gtk-doc', 'html', 'gobject')

gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')
gtk_docpath = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html', 'gtk')

bedit_doc_deps = [
  glib_dep,
  gobject_dep,
  gio_dep,
  gtk_dep,
  gtksourceview_dep,
  libpeas_dep,
  libpeas_gtk_dep,
]

bedit_doc_dep = declare_dependency(
  link_with: libbedit_sha,
  include_directories: root_include_dir,
  dependencies: bedit_doc_deps,
  sources: libbedit_dep_sources,
)

libbedit_reference_ignored_h = [
  'bedit-app-osx.h',
  'bedit-app-win32.h',
  'bedit-app-x11.h',
  'bedit-close-confirmation-dialog.h',
  'bedit-dirs.h',
  'bedit-document-private.h',
  'bedit-documents-panel.h',
  'bedit-encoding-items.h',
  'bedit-encodings-dialog.h',
  'bedit-enum-types.h',
  'bedit-file-chooser-dialog-gtk.h',
  'bedit-file-chooser-dialog-osx.h',
  'bedit-file-chooser-dialog.h',
  'bedit-highlight-mode-dialog.h',
  'bedit-highlight-mode-selector.h',
  'bedit-history-entry.h',
  'bedit-io-error-info-bar.h',
  'bedit-menu-stack-switcher.h',
  'bedit-multi-notebook.h',
  'bedit-notebook-popup-menu.h',
  'bedit-notebook-stack-switcher.h',
  'bedit-notebook.h',
  'bedit-open-menu-button.h',
  'bedit-pango.h',
  'bedit-plugins-engine.h',
  'bedit-preferences-dialog.h',
  'bedit-print-job.h',
  'bedit-print-preview.h',
  'bedit-recent.h',
  'bedit-replace-dialog.h',
  'bedit-settings.h',
  'bedit-small-button.h',
  'bedit-status-menu-button.h',
  'bedit-tab-label.h',
  'bedit-view-frame.h',
  'bedit-window-private.h',
]

libbedit_reference_scan_args = [
  '--rebuild-types',
  '--deprecated-guards=BEDIT_DISABLE_DEPRECATED',
  '--ignore-headers=' + ' '.join(libbedit_reference_ignored_h),
]

libbedit_reference_mkdb_args = [
  '--xml-mode',
  '--output-format=xml',
  '--name-space=bedit',
]

libbedit_reference_fixxref_args = [
  '--html-dir=@0@'.format(join_paths(gtkdocdir, 'html', package_string)),
  '--extra-dir=@0@'.format(glib_docpath),
  '--extra-dir=@0@'.format(gobject_docpath),
  '--extra-dir=@0@'.format(gtk_docpath),
]

gnome.gtkdoc(
  meson.project_name(),
  main_sgml: 'bedit-docs.xml',
  src_dir: join_paths(srcdir, 'bedit'),
  dependencies: bedit_doc_dep,
  scan_args: libbedit_reference_scan_args,
  mkdb_args: libbedit_reference_mkdb_args,
  fixxref_args: libbedit_reference_fixxref_args,
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
