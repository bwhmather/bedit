libfilebrowser_public_h = files(
  'bedit-file-browser-window-activatable.h',
  'bedit-file-browser-bookmarks-store.h',
  'bedit-file-browser-error.h',
  'bedit-file-browser-store.h',
  'bedit-file-browser-view.h',
  'bedit-file-browser-widget.h',
  'bedit-file-browser-utils.h',
  'bedit-file-browser-plugin.h',
  'bedit-file-browser-messages.h',
)

libfilebrowser_sources = files(
  'bedit-file-browser-app-activatable.c',
  'bedit-file-browser-window-activatable.c',
  'bedit-file-browser-bookmarks-store.c',
  'bedit-file-browser-messages.c',
  'bedit-file-browser-plugin.c',
  'bedit-file-browser-store.c',
  'bedit-file-browser-utils.c',
  'bedit-file-browser-view.c',
  'bedit-file-browser-location.c',
  'bedit-file-browser-widget.c',
  'bedit-file-browser-search-view.c',
)

libfilebrowser_deps = [
  libbedit_dep,
]

subdir('messages')

libfilebrowser_register_enums = gnome.mkenums(
  'bedit-file-browser-enum-register',
  sources: libfilebrowser_public_h,
  c_template: 'bedit-file-browser-enum-register.c.template',
)

libfilebrowser_type_enums = gnome.mkenums(
  'bedit-file-browser-enum-types',
  depends : [libfilebrowser_register_enums],
  sources: libfilebrowser_public_h,
  h_template: 'bedit-file-browser-enum-types.h.template',
  c_template: 'bedit-file-browser-enum-types-stage1.c.template',
)

# cat won't work on Windows so this
# will need to be reimplemented as a script
cat = find_program('cat')

# Combine the 2 C mkenums templates together before compiling
libfilebrowser_enums_c = custom_target('libfilebrowser_enums_c',
  input: [libfilebrowser_type_enums.get(0),
          libfilebrowser_register_enums],
  output: 'bedit-file-browser-enum-types.c',
  command: [cat, '@INPUT0@', '@INPUT1@'],
  # redirects the command output since we can't use >> here
  capture: true,
)

libfilebrowser_sources += [
  libfilebrowser_enums_c,
  libfilebrowser_type_enums.get(1),
]

subdir('resources')

libfilebrowser_sha = shared_module(
  'filebrowser',
  sources: libfilebrowser_sources,
  include_directories: root_include_dir,
  dependencies: libfilebrowser_deps,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  ),
  name_suffix: module_suffix,
)

# FIXME: https://github.com/mesonbuild/meson/issues/1687
custom_target(
  'com.bwhmather.bedit.plugins.filebrowser.enums.xml',
  input : libfilebrowser_sources + libfilebrowser_public_h,
  output: 'com.bwhmather.bedit.plugins.filebrowser.enums.xml',
  capture: true,
  command: [
    'glib-mkenums',
    '--comments', '<!-- @comment@ -->',
    '--fhead', '<schemalist>',
    '--vhead', '  <@type@ id="com.bwhmather.bedit.plugins.filebrowser.@EnumName@">',
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

filebrowser_gschema_file = files('com.bwhmather.bedit.plugins.filebrowser.gschema.xml')
install_data(
  filebrowser_gschema_file,
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'glib-2.0/schemas')
)

if xmllint.found()
  test(
    'validate-filebrowser-gschema',
    xmllint,
    args: [
      '--noout',
      '--dtdvalid', gschema_dtd,
      filebrowser_gschema_file,
    ]
  )
endif

custom_target(
  'filebrowser.plugin',
  input: 'filebrowser.plugin.desktop.in',
  output: 'filebrowser.plugin',
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  )
)
