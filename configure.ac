AC_INIT([helen], [1.0.0], [megrigorov@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_FILES([Makefile
		lib/Makefile
		listener/Makefile
		http/Makefile])
AC_CHECK_HEADER([openssl/ssl.h], [], [AC_MSG_FAILURE([could not find ssl header])])
AC_CHECK_LIB([ssl],
             [OPENSSL_init_ssl],
             [],
             [
              echo "Error: Could not find SSL."
              exit 1
             ])
AC_DEFINE([_DEBUG_], [], [For debugging])
AC_DEFINE([_TRACE_], [], [For enable tracing])
AC_DEFINE([OPENSSL_API_COMPAT], [0x10000000L], [SSL API version])
AC_CONFIG_HEADERS([config.h])
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_OUTPUT

