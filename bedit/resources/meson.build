gresource_xml = configuration_data()

if host_machine.system() == 'darwin'
  gresource_xml.set(
    'OS_DEPENDENT_RESOURCE_FILES',
    '<file preprocess="xml-stripblanks">gtk/menus-traditional.ui</file>'
  )
else
  gresource_xml.set('OS_DEPENDENT_RESOURCE_FILES', '')
endif

libbedit_res_xml = configure_file(
  input: 'bedit.gresource.xml.in',
  output: 'bedit.gresource.xml',
  configuration: gresource_xml
)

libbedit_res = gnome.compile_resources(
  'bedit-resources',
  libbedit_res_xml,
)
