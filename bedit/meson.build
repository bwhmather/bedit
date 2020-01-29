libbedit_public_h = files(
  'bedit-app-activatable.h',
  'bedit-app.h',
  'bedit-commands.h',
  'bedit-debug.h',
  'bedit-document.h',
  'bedit-encodings-combo-box.h',
  'bedit-menu-extension.h',
  'bedit-message-bus.h',
  'bedit-message.h',
  'bedit-progress-info-bar.h',
  'bedit-statusbar.h',
  'bedit-tab.h',
  'bedit-utils.h',
  'bedit-view-activatable.h',
  'bedit-view.h',
  'bedit-window-activatable.h',
  'bedit-window.h',
)

libbedit_private_h = files(
  'bedit-app-private.h',
  'bedit-close-confirmation-dialog.h',
  'bedit-commands-private.h',
  'bedit-dirs.h',
  'bedit-document-private.h',
  'bedit-encoding-items.h',
  'bedit-encodings-dialog.h',
  'bedit-file-chooser-dialog-gtk.h',
  'bedit-file-chooser-dialog.h',
  'bedit-highlight-mode-dialog.h',
  'bedit-highlight-mode-selector.h',
  'bedit-history-entry.h',
  'bedit-io-error-info-bar.h',
  'bedit-menu-stack-switcher.h',
  'bedit-notebook.h',
  'bedit-notebook-popup-menu.h',
  'bedit-pango.h',
  'bedit-plugins-engine.h',
  'bedit-preferences-dialog.h',
  'bedit-print-job.h',
  'bedit-print-preview.h',
  'bedit-recent.h',
  'bedit-replace-dialog.h',
  'bedit-settings.h',
  'bedit-status-menu-button.h',
  'bedit-tab-label.h',
  'bedit-tab-private.h',
  'bedit-view-frame.h',
  'bedit-window-private.h',
)

libbedit_sources = files(
  'bedit-app-activatable.c',
  'bedit-app.c',
  'bedit-close-confirmation-dialog.c',
  'bedit-commands-documents.c',
  'bedit-commands-edit.c',
  'bedit-commands-file.c',
  'bedit-commands-file-print.c',
  'bedit-commands-help.c',
  'bedit-commands-search.c',
  'bedit-commands-view.c',
  'bedit-debug.c',
  'bedit-dirs.c',
  'bedit-document.c',
  'bedit-encoding-items.c',
  'bedit-encodings-combo-box.c',
  'bedit-encodings-dialog.c',
  'bedit-file-chooser-dialog.c',
  'bedit-file-chooser-dialog-gtk.c',
  'bedit-highlight-mode-dialog.c',
  'bedit-highlight-mode-selector.c',
  'bedit-history-entry.c',
  'bedit-io-error-info-bar.c',
  'bedit-menu-extension.c',
  'bedit-menu-stack-switcher.c',
  'bedit-message-bus.c',
  'bedit-message.c',
  'bedit-notebook.c',
  'bedit-notebook-popup-menu.c',
  'bedit-pango.c',
  'bedit-plugins-engine.c',
  'bedit-preferences-dialog.c',
  'bedit-print-job.c',
  'bedit-print-preview.c',
  'bedit-progress-info-bar.c',
  'bedit-recent.c',
  'bedit-replace-dialog.c',
  'bedit-settings.c',
  'bedit-statusbar.c',
  'bedit-status-menu-button.c',
  'bedit-tab.c',
  'bedit-tab-label.c',
  'bedit-utils.c',
  'bedit-view-activatable.c',
  'bedit-view.c',
  'bedit-view-frame.c',
  'bedit-window-activatable.c',
  'bedit-window.c',
)

libbedit_c_args = []
libbedit_link_args = []

libbedit_deps = [
  glib_dep,
  gobject_dep,
  gio_dep,
  gtk_dep,
  gtksourceview_dep,
  tepl_dep,
  introspection_dep,
  libpeas_dep,
  libpeas_gtk_dep,
  libgd_dep,
]

if host_machine.system() == 'darwin'
  libbedit_sources += files(
    'bedit-app-osx.m',
    'bedit-file-chooser-dialog-osx.m',
  )
  libbedit_c_args += [
    '-DOS_OSX=1',
  ]
  libbedit_link_args += [
    '-Wl,-framework', '-Wl,Foundation',
    '-Wl,-framework', '-Wl,AppKit',
  ]
  gtk_mac_integration_dep = dependency('gtk-mac-integration-gtk3')
  libbedit_deps += [
    gtk_mac_integration_dep,
  ]
elif host_machine.system() == 'windows'
  libbedit_sources += files(
    'bedit-app-win32.c',
  )
else
  libbedit_sources += files(
    'bedit-app-x11.c',
  )

  libbedit_deps += [
    x11_dep,
  ]
endif

libbedit_enums = gnome.mkenums(
  'bedit-enum-types',
  sources: libbedit_public_h + ['bedit-notebook.h'],
  h_template: 'bedit-enum-types.h.template',
  c_template: 'bedit-enum-types.c.template',
)

subdir('resources')

libbedit_sources += [
  libbedit_enums.get(0),
  libbedit_res.get(0),
]

libbedit_dep_sources = []

install_headers(
  libbedit_public_h,
  install_dir: join_paths(
    get_option('prefix'),
    get_option('includedir'),
    package_string,
    'bedit'
  )
)

libbedit_sha = shared_library(
  package_string,
  libbedit_sources + [libbedit_enums.get(1), libbedit_res.get(1)],
  include_directories: root_include_dir,
  dependencies: libbedit_deps,
  c_args: libbedit_c_args,
  link_args: libbedit_link_args,
  install: true,
  install_dir: pkglibdir,
)

if generate_gir == true
  libbedit_public_h += [
    libbedit_enums.get(1),
  ]

  libbedit_gir_includes = [
    'Gtk-3.0',
    'GtkSource-4',
  ]

  libbedit_gir = gnome.generate_gir(
    libbedit_sha,
    sources: libbedit_sources + libbedit_public_h,
    nsversion: '3.0',
    namespace: 'Bedit',
    symbol_prefix: 'bedit',
    identifier_prefix: 'Bedit',
    export_packages: [package_string],
    includes: libbedit_gir_includes,
    link_with: libbedit_sha,
    install: true,
    install_dir_gir: join_paths(pkgdatadir, 'gir-1.0'),
    install_dir_typelib: join_paths(pkglibdir, 'girepository-1.0'),
  )

  python3.install_sources(
    'Bedit.py',
    subdir: join_paths(
      'gi',
      'overrides',
    )
  )

  libbedit_dep_sources += [
    libbedit_gir,
  ]

  if generate_vapi == true
    libbedit_vapi_packages = [
      'gio-2.0',
      'atk',
      'gdk-3.0',
      'gtk+-3.0',
      'gtksourceview-4',
    ]

    libbedit_vapi = gnome.generate_vapi(
      meson.project_name(),
      sources: libbedit_gir.get(0),
      packages: libbedit_vapi_packages,
      install: true,
      install_dir: vapidir,
    )
  endif
endif

libbedit_dep = declare_dependency(
  link_with: libbedit_sha,
  include_directories: root_include_dir,
  dependencies: libbedit_deps,
  sources: libbedit_dep_sources,
)

libbedit_pc = pkg.generate(
  name: meson.project_name(),
  description: 'GNOME text editor',
  requires: ['gtksourceview-4', 'libpeas-1.0', 'libpeas-gtk-1.0'],
  version: meson.project_version(),
  subdirs: package_string,
  libraries: libbedit_sha,
  install_dir: pkgconfigdir,
)

# FIXME: https://github.com/mesonbuild/meson/issues/1687
custom_target(
  'com.bwhmather.bedit.enums.xml',
  input : libbedit_sources + libbedit_public_h + ['bedit-notebook.h'],
  output: 'com.bwhmather.bedit.enums.xml',
  capture: true,
  command: [
    'glib-mkenums',
    '--comments', '<!-- @comment@ -->',
    '--fhead', '<schemalist>',
    '--vhead', '  <@type@ id="com.bwhmather.bedit.@EnumName@">',
    '--vprod', '    <value nick="@valuenick@" value="@valuenum@"/>',
    '--vtail', '  </@type@>',
    '--ftail', '</schemalist>',
    '@INPUT@'
  ],
  install: true,
  install_dir: join_paths(
    glibdir,
    'schemas',
  )
)

bedit_sources = [
  'bedit.c',
]

bedit_c_args = []

if host_machine.system() == 'darwin'
  bedit_c_args += [
    '-DOS_OSX=1',
  ]
endif

bedit_deps = [
  libbedit_dep,
]

bedit_exe = executable(
  meson.project_name(),
  bedit_sources,
  dependencies: bedit_deps,
  c_args: bedit_c_args,
  install: true,
  install_dir: bindir,
  install_rpath: pkglibdir,
  gui_app: true,
)
