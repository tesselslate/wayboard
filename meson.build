project('wayboard', 'c',
  version : '0.0.0',
  license : 'GPL3',
)

add_project_arguments(
  [
    '-Wno-unused-parameter',
  ],
  language: 'c',
)

wayland_protocols = dependency('wayland-protocols')
wayland_scanner = dependency('wayland-scanner', native: true)

wl_scanner = find_program(wayland_scanner.get_variable('wayland_scanner'), native: true)
wl_proto_dir = wayland_protocols.get_variable('pkgdatadir')
wl_proto_xml = [
  wl_proto_dir + '/stable/xdg-shell/xdg-shell.xml',
]

wl_proto_header = []
wl_proto_src = []
foreach proto : wl_proto_xml
  wl_proto_header += custom_target(
    proto.underscorify() + '-header',
    input: proto,
    output: '@BASENAME@.h',
    command: [wl_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  wl_proto_src += custom_target(
    proto.underscorify() + '-src',
    input: proto,
    output: '@BASENAME@.c',
    command: [wl_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
endforeach

cc = meson.get_compiler('c')
executable('wayboard',
  wl_proto_src, wl_proto_header,
  'wayboard.c',
  dependencies: [
    dependency('fcft'),

    dependency('libinput'),
    dependency('libudev'),

    cc.find_library('rt'),
    dependency('libconfig'),
    dependency('pixman-1'),
    dependency('wayland-client'),
  ],
  install: true,
)
