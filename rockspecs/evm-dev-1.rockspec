rockspec_format = "3.0"
package = "evm"
version = "dev-1"
source = {
    url = "git+https://github.com/mah0x211/lua-evm.git",
}
description = {
    summary = "kqueue/epoll event module",
    homepage = "https://github.com/mah0x211/lua-evm",
    license = "MIT/X11",
    maintainer = "Masatoshi Fukunaga",
}
dependencies = {
    "lua >= 5.1",
}
build = {
    type = "command",
    build_command = [[
        autoreconf -ivf && CFLAGS="$(CFLAGS)" CPPFLAGS="-I$(LUA_INCDIR)" LIBFLAG="$(LIBFLAG)" OBJ_EXTENSION="$(OBJ_EXTENSION)" LIB_EXTENSION="$(LIB_EXTENSION)" LIBDIR="$(LIBDIR)" ./configure && make clean && make
    ]],
    install_command = "make install",
}
