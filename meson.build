project('libepz2023plus', 'c',
        version : '0.5',
        license : 'GPL-2.0-or-later',
        meson_version : '>= 0.59.0',
        default_options : ['buildtype=release', 'c_std=gnu17'],
)

libusb_dep = dependency('libusb-1.0')

libezp2023plus_dep = declare_dependency(
    sources : ['src/ezp_prog.c', 'src/ezp_chips_data_file.c'],
    dependencies : [libusb_dep],
    include_directories : include_directories('include/'),
)
