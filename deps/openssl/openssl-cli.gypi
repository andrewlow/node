{
  'target_name': 'openssl-cli',
  'type': 'executable',
  'dependencies': ['openssl'],
  'defines': [
    'MONOLITH'
  ],
  'includes': ['openssl.gypi'],
  'sources': ['<@(openssl_cli_sources)'],
  'conditions': [
    ['OS=="solaris"', {
      'libraries': ['<@(openssl_cli_libraries_solaris)']
    }, 'OS=="zos"', {
      'dependencies': [ '../../deps/zoslib/zoslib.gyp:zoslib' ],
    }, 'OS=="win"', {
      'link_settings': {
        'libraries': ['<@(openssl_cli_libraries_win)'],
      },
    }, 'OS in "linux android"', {
      'link_settings': {
        'libraries': [
          '-ldl',
        ],
      },
    }],
  ],
}
