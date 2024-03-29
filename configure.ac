#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
AC_PREREQ(2.61)
AC_INIT(evm, scm)
AM_INIT_AUTOMAKE([foreign no-dist no-installinfo no-installman -Wall])
AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_MACRO_DIR([m4])

#
# Checks for programs.
#
AC_LANG_C
AC_PROG_CC

#
# define luarocks variables
#
AC_SUBST([LIBFLAG], ["$LIBFLAG"])
AC_SUBST([OBJ_EXTENSION], ["$OBJ_EXTENSION"])
AC_SUBST([LIB_EXTENSION], ["$LIB_EXTENSION"])
AC_SUBST([LIBDIR], ["$LIBDIR"])

#
# checking required headers
#
AC_CHECK_HEADERS(
    stdlib.h unistd.h string.h errno.h math.h time.h signal.h stdint.h,,
    AC_MSG_FAILURE([required header not found])
)

#
# checking required types
#
AC_CHECK_TYPES([ int8_t, int16_t, int32_t, int64_t,
                 uint8_t, uint16_t, uint32_t, uint64_t,
                 intptr_t, uintptr_t, ptrdiff_t,
                 size_t, ssize_t, off_t,
                 mode_t, pid_t, socklen_t, sigset_t, time_t ])

#
# checking required functions
#
AC_CHECK_FUNCS(
    [ malloc calloc realloc memcpy free sigemptyset sigaddset sigismember \
      printf close read ],,
    AC_MSG_FAILURE([required function not found])
)

#
# checking kevent
#
AC_CHECK_HEADERS(
    sys/event.h,
    [ AC_SUBST([EVENT_SRC], ["kqueue"]) ]
)
#
# checking epoll
#
AC_CHECK_HEADERS(
    sys/epoll.h,
    [ AC_SUBST([EVENT_SRC], ["epoll"]) ]
)

#
# confirm event library
#
AS_IF( [test x"$EVENT_SRC" = x"" ],
    AC_MSG_FAILURE([kqueue/epoll not found])
)
AS_IF( [test x"$EVENT_SRC" = x"kqueue" ],
    [
        #
        # checking required functions
        #
        AC_CHECK_FUNCS(
            [ kevent ],,
            AC_MSG_FAILURE([required function not found])
        )
    ]
)
AS_IF( [test x"$EVENT_SRC" = x"epoll" ],
    [
        #
        # checking required headers
        #
        AC_CHECK_HEADERS( \
            sys/timerfd.h sys/signalfd.h,,
            AC_MSG_FAILURE([required header not found])
        )
        #
        # checking required functions
        #
        AC_CHECK_FUNCS(
            [ epoll_create epoll_ctl epoll_pwait signalfd timerfd_create \
              timerfd_settime ],,
            AC_MSG_FAILURE([required function not found])
        )
        #
        # checking optional functions
        #
        AC_CHECK_FUNCS( [epoll_create1] )
        # checking clock_gettime
        AC_CHECK_LIB(
            rt, clock_gettime,,
            AC_MSG_FAILURE([librt not found])
        )
    ]
)

AC_CONFIG_FILES([ \
    Makefile \
    src/Makefile \
])
AC_OUTPUT
