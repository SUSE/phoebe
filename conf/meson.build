conf_data = configuration_data()
conf_data.set('plugin_path',  get_option('prefix') / get_option('libdir') / meson.project_name())
conf_data.set('rates_file', get_option('prefix') / get_option('datadir') / meson.project_name() / 'rates.csv')
settings_json = configure_file(input : 'settings.json.in',
               output : 'settings.json',
               configuration : conf_data)
install_data(settings_json, install_dir : get_option('sysconfdir') / meson.project_name())
