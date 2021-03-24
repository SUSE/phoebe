systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_pkgconfig_variable('systemdsystemunitdir')


configure_file(
    copy: true,
    input: 'phoebe.service',
    install: true,
    install_dir: systemd_system_unit_dir,
    output: 'phoebe.service',
)
