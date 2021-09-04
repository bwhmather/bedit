gresource_xml = configuration_data()

gresource_xml.set('OS_DEPENDENT_RESOURCE_FILES', '')

libbedit_res_xml = configure_file(
  input: 'bedit.gresource.xml.in',
  output: 'bedit.gresource.xml',
  configuration: gresource_xml
)

libbedit_res = gnome.compile_resources(
  'bedit-resources',
  libbedit_res_xml,
)
