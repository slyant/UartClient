from building import *
Import('rtconfig')

src = []
path = []
cwd   = GetCurrentDir()

if GetDepend('PKG_USING_UART_CLIENT'):
    src += Glob('src/uart_client.c')
    path += [cwd + '/inc']

if GetDepend('PKG_USING_UART_CLIENT_SAMPLE'):
    src += Glob('examples/uart_client_sample.c')
    path += [cwd + '/examples']

# add src and include to group.
group = DefineGroup('uart_client', src, depend = ['PKG_USING_UART_CLIENT'], CPPPATH = path)

Return('group')
