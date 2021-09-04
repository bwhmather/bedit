msgfmt_plugin_cmd = [
  find_program('msgfmt'),
  '--desktop',
  '--keyword=Name',
  '--keyword=Description',
  '--template=@INPUT@',
  '-d', join_paths(srcdir, 'po'),
  '--output=@OUTPUT@'
]

subdir('docinfo')
subdir('git')
subdir('modelines')
subdir('quickhighlight')
subdir('sort')
subdir('spell')
subdir('time')
subdir('trimtrailing')
