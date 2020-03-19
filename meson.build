project(
  'bedit', 'c',
  version: '3.35.2',
  meson_version: '>= 0.49'
)

if host_machine.system() == 'darwin'
  add_languages('objc')
endif

# Compiler flags and warnings
cc = meson.get_compiler('c')

global_c_args = []
if cc.get_id() == 'msvc'
  test_c_args = [
    '/FImsvc_recommended_pragmas.h',
  ]
else
  test_c_args = [
    '-Wcast-align',
    '-Werror=declaration-after-statement',
    '-Werror=address',
    '-Werror=array-bounds',
    '-Werror=empty-body',
    '-Werror=implicit',
    '-Werror=implicit-function-declaration',
    '-Werror=incompatible-pointer-types',
    '-Werror=init-self',
    '-Werror=int-conversion',
    '-Werror=int-to-pointer-cast',
    '-Werror=main',
    '-Werror=misleading-indentation',
    '-Werror=missing-braces',
    '-Werror=missing-include-dirs',
    '-Werror=nonnull',
    '-Werror=overflow',
    '-Werror=parenthesis',
    '-Werror=pointer-arith',
    '-Werror=pointer-to-int-cast',
    '-Werror=return-type',
    '-Werror=sequence-point',
    '-Werror=shadow',
    '-Werror=strict-prototypes',
    '-Werror=trigraphs',
    '-Werror=undef',
    '-Werror=write-strings',
    '-Wformat-nonliteral',
    '-Werror=format-security',
    '-Werror=format=2',
    '-Wignored-qualifiers',
    '-Wimplicit-function-declaration',
    '-Wlogical-op',
    '-Wmissing-format-attribute',
    '-Wmissing-include-dirs',
    '-Wmissing-noreturn',
    '-Wnested-externs',
    '-Wcast-function-type',
    '-Wno-missing-field-initializers',
    '-Wno-sign-compare',
    '-Wno-unused-parameter',
    '-Werror=old-style-definition',
    '-Wpointer-arith',
    '-Wstrict-prototypes',
    '-Wno-switch-default',
    '-Wno-switch-enum',
    '-Wundef',
    '-Werror=uninitialized',
    '-Wunused',
    '-Wmissing-declarations',
    '-Wdiscarded-qualifiers',
    '-Wall', '-Wextra',
    '-fno-strict-aliasing',
  ]
  if get_option('buildtype') != 'plain'
    test_c_args += '-fstack-protector-strong'
  endif
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach

add_project_arguments(global_c_args, language: 'c')

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')
python = import('python')

api_version = '3.36'

package_string = '@0@-@1@'.format(meson.project_name(), api_version)

# Paths
root_include_dir = include_directories('.')

srcdir = meson.current_source_dir()

bindir = join_paths(get_option('prefix'), get_option('bindir'))
libdir = join_paths(get_option('prefix'), get_option('libdir'))
libexecdir = join_paths(get_option('prefix'), get_option('libexecdir'))
datadir = join_paths(get_option('prefix'), get_option('datadir'))
pkgconfigdir = join_paths(libdir, 'pkgconfig')
pkglibdir = join_paths(libdir, meson.project_name())
pkgdatadir = join_paths(datadir, meson.project_name())

glibdir = join_paths(datadir, 'glib-2.0')
gtkdocdir = join_paths(datadir, 'gtk-doc')
vapidir = join_paths(datadir, 'vala', 'vapi')

# Subprojects
if not get_option('buildtype').contains('plain')
  run_command('git', '-C', meson.source_root(), 'submodule', 'update', '--init', '--recursive')
endif

# Dependencies
glib_req = '>= 2.52'
libpeas_req = '>= 1.14.1'

m_dep = cc.find_library('m', required: false)
glib_dep = dependency('glib-2.0', version: glib_req)
gobject_dep = dependency('gobject-2.0', version: glib_req)
gmodule_dep = dependency('gmodule-2.0', version: glib_req)
gio_dep = dependency('gio-2.0', version: glib_req)
gtk_dep = dependency('gtk+-3.0', version: '>= 3.22')
gtksourceview_dep = dependency('gtksourceview-4', version: '>= 4.0.2')
tepl_dep = dependency('tepl-4', version: '>= 4.2')
libpeas_dep = dependency('libpeas-1.0', version: libpeas_req)
libpeas_gtk_dep = dependency('libpeas-gtk-1.0', version: libpeas_req)

gspell_dep = dependency('gspell-1', version: '>= 1.0', required: true)

introspection_dep = dependency('gobject-introspection-1.0', required: false)
vapigen_dep = dependency('vapigen', version: '>= 0.25.1', required: false)

python3 = python.find_installation('python3')

build_checkupdate_plugin = host_machine.system() == 'windows'
if build_checkupdate_plugin
  libsoup_dep = dependency('libsoup-2.4', version: '>= 2.60', required: true)
endif

# Configurations
config_h = configuration_data()
config_h.set_quoted('PACKAGE_STRING', package_string)
config_h.set_quoted('GETTEXT_PACKAGE', meson.project_name())
config_h.set_quoted('LIBDIR', join_paths(get_option('prefix'), get_option('libdir')))
config_h.set_quoted('DATADIR', join_paths(get_option('prefix'), get_option('datadir')))
config_h.set_quoted('VERSION', meson.project_version())

version_array = meson.project_version().split('.')
config_h.set('BEDIT_MAJOR_VERSION', version_array[0])
config_h.set('BEDIT_MINOR_VERSION', version_array[1])
config_h.set('BEDIT_MICRO_VERSION', version_array[2])

configure_file(
  output: 'config.h',
  configuration: config_h
)

# Options
generate_gir = get_option('introspection')
if generate_gir and not introspection_dep.found()
  generate_gir = false
endif

generate_vapi = generate_gir and get_option('vapi')
if generate_vapi and not vapigen_dep.found()
  generate_vapi = false
endif

active_plugins = [
  'docinfo',
  'modelines',
  'filebrowser',
  'spell',
  'time',
]

if build_checkupdate_plugin
  active_plugins += 'checkupdate'
endif

subdir('data')
subdir('po')
subdir('bedit')
subdir('plugins')

if get_option('gtk_doc')
  subdir('docs/reference')
endif

if get_option('user_documentation')
  subdir('help')
endif

meson.add_install_script(
  'build-aux/meson/post_install.py',
  join_paths(get_option('prefix'), get_option('libdir')),
  join_paths(get_option('prefix'), get_option('datadir'))
)

# Summary message

summary = [
  'Configuration:',
  '',
  '        bedit version @0@ (API version @1@)'.format(meson.project_version(), api_version),
  '',
  '        Prefix:                @0@'.format(get_option('prefix')),
  '        API documentation:     @0@'.format(get_option('gtk_doc')),
  '        User documentation:    @0@'.format(get_option('user_documentation')),
  '        GObject Introspection: @0@'.format(generate_gir),
  '        Vala API:              @0@'.format(generate_vapi),
  '',
]
message('\n'.join(summary))
