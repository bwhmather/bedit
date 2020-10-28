libtrimtrailing_sources = files(
  'bedit-trim-trailing-plugin.c',
  'bedit-trim-trailing-view-activatable.c',
)

libtrimtrailing_deps = [
  libbedit_dep,
]

libtrimtrailing_sha = shared_module(
  'trimtrailing',
  sources: libtrimtrailing_sources,
  include_directories: root_include_dir,
  dependencies: libtrimtrailing_deps,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  ),
  name_suffix: module_suffix,
)

custom_target(
  'trimtrailing.plugin',
  input: 'trimtrailing.plugin.desktop.in',
  output: 'trimtrailing.plugin',
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  )
)
