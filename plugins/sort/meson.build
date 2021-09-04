libsort_sources = files(
  'bedit-sort-plugin.c',
)

libsort_deps = [
  libbedit_dep,
]

subdir('resources')

libsort_sha = shared_module(
  'sort',
  sources: libsort_sources,
  include_directories: root_include_dir,
  dependencies: libsort_deps,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  ),
)

custom_target(
  'sort.plugin',
  input: 'sort.plugin.desktop.in',
  output: 'sort.plugin',
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  )
)
