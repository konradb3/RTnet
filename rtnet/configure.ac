# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

#AC_PREREQ(2.57)
AC_INIT(RTnet)
AM_INIT_AUTOMAKE(RTnet, 0.2.10, jan.kiszka@web.de)
AC_PREFIX_DEFAULT(/usr/local/rtnet)

AC_CONFIG_SRCDIR([iovec.c])
AM_CONFIG_HEADER(config/config.h)

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
#AC_PROG_SED

AC_DEFUN(AC_CHECK_LIBRARY, [dnl
KSAVE_LIBS="$LIBS"
LIBS="$$3 $KSAVE_LIBS"
AC_CHECK_LIB($1,$2)
$3=`echo "$LIBS" | sed "s% $KSAVE_LIBS$%%"`
LIBS="$KSAVE_LIBS"
])

# Checks for libraries.
# FIXME: Replace `main' with a function in `-llxrt':
AC_CHECK_LIBRARY(lxrt, main, LXRT_LIBS)
AC_SUBST(LXRT_LIBS)

# FIXME: Replace `main' with a function in `-lncurses':
AC_CHECK_LIBRARY(ncurses, main, NCURSES_LIBS)
AC_SUBST(NCURSES_LIBS)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_C_VOLATILE

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([inet_ntoa memset socket strchr strerror])


dnl ======================================================================
dnl		rtai sources
dnl ======================================================================

AC_ARG_WITH(rtai,[  --with-rtai=RTAI    path to configured RTAI sources])
if test "$with_rtai" = ""; then
   with_rtai=/usr/src/rtai
fi

AC_MSG_CHECKING(for RTAI source tree)

if test \! -f $with_rtai/Makefile; then
   AC_MSG_ERROR(*** RTAI not found in $with_rtai)
fi

if test "`sed -n -e \"s/^PROJECT = \(.*\)/\1/p\" ${with_rtai}/Makefile`" = "RTAI" -a \
 -f ${with_rtai}/.buildvars; then
   RTAI_DIR=$with_rtai
else
   AC_MSG_ERROR(*** No configured RTAI found in $with_rtai)
fi

AC_MSG_RESULT($RTAI_DIR)
AC_SUBST(RTAI_DIR)

AC_MSG_CHECKING([for Linux source tree])
RTAI_LINUX_DIR=`sed -n -e "s/^LINUXDIR=\(.*\)/\1/p" $RTAI_DIR/.buildvars`
AC_MSG_RESULT($RTAI_LINUX_DIR)



dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai-unstabel +++ start +++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

if test \! -r ${RTAI_LINUX_DIR}/Makefile; then
   AC_MSG_ERROR(*** No Linux kernel tree in ${RTAI_LINUX_DIR})
fi

dnl
dnl Identify the base technology used.
dnl

AC_MSG_CHECKING([for RTAI base technology])

eval `grep '^EXTRAVERSION =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
eval `grep '^PATCHLEVEL =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
eval `grep '^VERSION =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`

case "$EXTRAVERSION" in
  *rthal*)
    AC_MSG_RESULT([rthal])
    CONFIG_RTAI_RTHAL=y
    ;;
  *adeos*)
    AC_DEFINE(CONFIG_RTAI_ADEOS,1,[Use Adeos-based HAL])
    AC_MSG_RESULT([Adeos])
    ;;
  *) 
    AC_MSG_ERROR(*** The RTAI kernel patch has not yet been applied to your kernel)
esac

case "$VERSION.$PATCHLEVEL" in
  2.4)
    AC_DEFINE(CONFIG_RTAI_LINUX24,1,[Kconfig])
    ;;
  *) 
    AC_MSG_ERROR(*** Unsupported kernel version $VERSION.$PATCHLEVEL)
esac

if test \! -r ${RTAI_LINUX_DIR}/.config; then
   AC_MSG_ERROR(*** $RTAI_LINUX_DIR is not configured)
fi

dnl
dnl Load Linux kernel configuration and perform a few sanity checks.
dnl

. $RTAI_LINUX_DIR/.config

if test x${CONFIG_MODVERSIONS} = xy ; then
   AC_MSG_ERROR(*** Please unset CONFIG_MODVERSIONS from $RTAI_LINUX_DIR)
fi

dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai-unstabel +++ end +++++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



dnl ======================================================================
dnl		import settings from RTAI
dnl ======================================================================

RTAI_KMOD_CFLAGS="`sed -n -e "s/^CFLAGS=\(.*\)/\1/p" $RTAI_DIR/.buildvars` -I$RTAI_DIR/include -DEXPORT_SYMTAB -Wall"
RTAI_CFLAGS=-I$RTAI_DIR/include
KERNEL_CFLAGS=-I$RTAI_LINUX_DIR/include
CROSS_COMPILE=`sed -n -e "s/^CROSS_COMPILE=\(.*\)/\1/p" $RTAI_DIR/.buildvars`

AC_SUBST(RTAI_KMOD_CFLAGS)
AC_SUBST(RTAI_CFLAGS)
AC_SUBST(KERNEL_CFLAGS)
AC_SUBST(CROSS_COMPILE)



dnl ======================================================================
dnl		network interface card driver
dnl ======================================================================

AC_MSG_CHECKING(whether to build 3Com 59x driver)
AC_ARG_ENABLE(3c59x,
	[  --enable-3c59x         build 3Com 59x driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_3C59X=yes;;
	*) unset CONFIG_RTNET_3C59X ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_3C59X:-no})
AM_CONDITIONAL(CONFIG_RTNET_3C59X,[test "$CONFIG_RTNET_3C59X" = "yes"])


AC_MSG_CHECKING(whether to build Realtek 8139 driver)
AC_ARG_ENABLE(8139,
	[  --enable-8139          build Realtek 8139 driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_8139=yes;;
	*) unset CONFIG_RTNET_8139 ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_8139:-no})
AM_CONDITIONAL(CONFIG_RTNET_8139,[test "$CONFIG_RTNET_8139" = "yes"])


AC_MSG_CHECKING(whether to build Intel EtherExpressPro/100 driver)
AC_ARG_ENABLE(eepro100,
	[  --enable-eepro100      build Intel EtherExpressPro/100 driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_EEPRO100=yes;;
	*) unset CONFIG_RTNET_EEPRO100 ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_EEPRO100:-no})
AM_CONDITIONAL(CONFIG_RTNET_EEPRO100,[test "$CONFIG_RTNET_EEPRO100" = "yes"])


AC_MSG_CHECKING(whether to build loopback driver)
AC_ARG_ENABLE(loopback,
	[  --enable-loopback      build loopback driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_LOOPBACK=yes;;
	*) unset CONFIG_RTNET_LOOPBACK ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_LOOPBACK:-no})
AM_CONDITIONAL(CONFIG_RTNET_LOOPBACK,[test "$CONFIG_RTNET_LOOPBACK" = "yes"])


AC_MSG_CHECKING(whether to build tulip driver)
AC_ARG_ENABLE(tulip,
	[  --enable-tulip         build tulip driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_TULIP=yes;;
	*) unset CONFIG_RTNET_TULIP ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_TULIP:-no})
AM_CONDITIONAL(CONFIG_RTNET_TULIP,[test "$CONFIG_RTNET_TULIP" = "yes"])


AC_MSG_CHECKING(whether to build fcc enet driver)
AC_ARG_ENABLE(fcc-enet,
	[  --enable-fcc-enet      build fcc enet driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_FCC_ENET=yes;;
	*) unset CONFIG_RTNET_FCC_ENET ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_FCC_ENET:-no})
AM_CONDITIONAL(CONFIG_RTNET_FCC_ENET,[test "$CONFIG_RTNET_FCC_ENET" = "yes"])


AC_MSG_CHECKING(whether to build scc enet driver)
AC_ARG_ENABLE(scc-enet,
	[  --enable-scc-enet      build scc enet driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_SCC_ENET=yes;;
	*) unset CONFIG_RTNET_SCC_ENET ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_SCC_ENET:-no})
AM_CONDITIONAL(CONFIG_RTNET_SCC_ENET,[test "$CONFIG_RTNET_SCC_ENET" = "yes"])


AC_MSG_CHECKING(whether to build fec enet driver)
AC_ARG_ENABLE(fec-enet,
	[  --enable-fec-enet      build fec enet driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_FEC_ENET=yes ;;
	*) unset CONFIG_RTNET_FEC_ENET ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_FEC_ENET:-no})
AM_CONDITIONAL(CONFIG_RTNET_FEC_ENET,[test "$CONFIG_RTNET_FEC_ENET" = "yes"])


AC_OUTPUT([
Makefile \
examples/Makefile \
examples/round_trip_time/Makefile \
examples/round_trip_time/client/Makefile \
examples/round_trip_time/server/Makefile \
examples/rtnet_lxrt/Makefile \
examples/rtskb_alloc/Makefile \
examples/rtt/Makefile \
driver/Makefile \
driver/tulip/Makefile \
ethernet/Makefile \
ipv4/Makefile \
lib/Makefile \
tools/Makefile \
rtmac/Makefile
rtmac/examples/Makefile
rtmac/examples/event/Makefile
rtmac/examples/mrtt/Makefile
rtmac/examples/rtt/Makefile
])

#examples/rtnet_lxrt/Makefile \