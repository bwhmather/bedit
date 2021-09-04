libtime_sources = files(
  'bedit-time-plugin.c',
)

libtime_deps = [
  libbedit_dep,
]

gnome.mkenums(
  'com.bwhmather.bedit.plugins.time.enums.xml',
  sources: libtime_sources,
  comments: '<!-- @comment@ -->',
  fhead: '<schemalist>',
  vhead: '  <@type@ id="com.bwhmather.bedit.plugins.time.@EnumName@">',
  vprod: '    <value nick="@valuenick@" value="@valuenum@"/>',
  vtail: '  </@type@>',
  ftail: '</schemalist>',
  install_header: true,
  install_dir: join_paths(
    glibdir,
    'schemas',
  )
)

subdir('resources')

libtime_sha = shared_module(
  'time',
  sources: libtime_sources,
  include_directories: root_include_dir,
  dependencies: libtime_deps,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  ),
)

time_gschema_file = files('com.bwhmather.bedit.plugins.time.gschema.xml')
install_data(
  time_gschema_file,
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'glib-2.0/schemas')
)

if xmllint.found()
  test(
    'validate-time-gschema',
    xmllint,
    args: [
      '--noout',
      '--dtdvalid', gschema_dtd,
      time_gschema_file,
    ]
  )
endif

custom_target(
  'time.plugin',
  input: 'time.plugin.desktop.in',
  output: 'time.plugin',
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  )
)
