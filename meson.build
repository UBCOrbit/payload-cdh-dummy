project('payload-cdh-dummy', 'c')

include = include_directories('lib')

sha_src = ['lib/sha256.c', 'sha256_utils.c']
sender_src = ['sender.c', sha_src]

executable('command-tests', sender_src, include_directories : include)
