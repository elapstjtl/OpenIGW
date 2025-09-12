project('southbound-api', 'cpp',
  version: '1.0.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
  ],
)

inc = include_directories('Inc')

headers = [
  'Inc/Types.hpp',
  'Inc/IAdapter.hpp',
  'Inc/Factory.hpp',
]

install_headers(headers, subdir: 'southbound')

pkg = import('pkgconfig')
pkg.generate(
  name: 'southbound-api',
  description: 'Southbound adapter API C++ interfaces',
  version: meson.project_version(),
  filebase: 'southbound-api',
  subdirs: 'southbound',
) 