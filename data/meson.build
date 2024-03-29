subdir('icons')

install_man('bedit.1')

appdata = 'com.bwhmather.bedit.appdata.xml'
appdata_file = i18n.merge_file(
  input: appdata + '.in',
  output: appdata,
  po_dir: '../po/',
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'metainfo')
)

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test(
    'validate-appdata',
    appstream_util,
    args: [
      'validate-relax',
      '--nonet',
      appdata_file.full_path()
    ]
  )
endif

desktop_file = 'com.bwhmather.bedit.desktop'
desktop_output_file = i18n.merge_file(
  type: 'desktop',
  input: desktop_file + '.in',
  output: desktop_file,
  po_dir: '../po/',
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'applications')
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
  test(
    'validate-desktop',
    desktop_file_validate,
    args: [
      desktop_output_file.full_path(),
    ]
  )
endif

quoted_plugins = []
foreach plugin_name : active_plugins
  quoted_plugins += '\'@0@\''.format(plugin_name)
endforeach

gschema_in = configuration_data()
gschema_in.set('ACTIVE_PLUGINS', ', '.join(quoted_plugins))

gschema_file = configure_file(
  input: 'com.bwhmather.bedit.gschema.xml.in',
  output: 'com.bwhmather.bedit.gschema.xml',
  configuration: gschema_in,
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'glib-2.0/schemas')
)

gschema_dtd = join_paths(
  gio_dep.get_pkgconfig_variable('schemasdir'),
  'gschema.dtd',
)

xmllint = find_program('xmllint', required: false)
if xmllint.found()
  test(
    'validate-gschema',
    xmllint,
    args: [
      '--noout',
      '--dtdvalid', gschema_dtd,
      gschema_file,
    ]
  )
endif

service_in = configuration_data()
service_in.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))

configure_file(
  input: 'com.bwhmather.bedit.service.in',
  output: 'com.bwhmather.bedit.service',
  configuration: service_in,
  install_dir: join_paths(get_option('prefix'), get_option('datadir'), 'dbus-1/services')
)
