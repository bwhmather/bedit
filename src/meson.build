bedit_sources = [
  'main.vala',
  'window.vala',
]

bedit_deps = [
  dependency('gio-2.0', version: '>= 2.50'),
  dependency('gtk+-3.0', version: '>= 3.22'),
  dependency('gtksourceview-4', version: '>= 4.4'),
]

gnome = import('gnome')

bedit_sources += gnome.compile_resources('bedit-resources',
  'bedit.gresource.xml',
  c_name: 'bedit'
)

executable('bedit', bedit_sources,
  vala_args: '--target-glib=2.50',  dependencies: bedit_deps,
  install: true,
)
