libmodelines_sources = files(
  'bedit-modeline-plugin.c',
  'modeline-parser.c',
)

libmodelines_deps = [
  libbedit_dep,
]

libmodelines_sha = shared_module(
  'modelines',
  sources: libmodelines_sources,
  include_directories: root_include_dir,
  dependencies: libmodelines_deps,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  ),
)

custom_target(
  'modelines.plugin',
  input: 'modelines.plugin.desktop.in',
  output: 'modelines.plugin',
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  )
)

install_data(
  'language-mappings',
  install_dir: join_paths(
    pkgdatadir,
    'plugins',
    'modelines',
  )
)
