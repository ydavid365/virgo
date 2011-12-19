{
  'targets': [
    {
      'target_name': 'uvtls',
      'type': 'static_library',
      'dependencies': [
        '../uv/uv.gyp:uv',
        '../openssl.gyp:openssl',
      ],
      'sources': [
        'src/uvtls.c',
      ],
      'include_dirs': [
          # TODO: figure out how to get includes working right from
          # dependencies like this :(
          '../uv/include',
          '../openssl-configs/realized',
          '../openssl/include',
          '../openssl-configs',
          '../openssl-configs/<(OS)-<(target_arch)',
          '../openssl-configs/<(OS)',
          '../openssl-configs/<(target_arch)',
          'include',
          'src',
      ],
      'direct_dependent_settings': {
          'include_dirs': [
            'include',
          ],
        },
    },
  ],
}

