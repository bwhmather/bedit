install_subdir(
  'git',
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  )
)

custom_target(
  'git.plugin',
  input: 'git.plugin.desktop.in',
  output: 'git.plugin',
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: join_paths(
    pkglibdir,
    'plugins',
  )
)
