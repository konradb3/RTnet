# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([RTnet],[0.5.6],[jan.kiszka@web.de])
AC_CONFIG_HEADER(config/config.h)
AC_CONFIG_AUX_DIR(config/autoconf)
AC_CONFIG_SRCDIR([rtskb.c])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_INSTALL

CFLAGS=${CFLAGS:--s}

# Checks for programs.
AC_ARG_WITH(CC,
    [  --with-cc=compiler      use specific C compiler],
    [
        case "$withval" in
        "" | y | ye | yes | n | no)
            AC_MSG_ERROR([You must supply an argument to --with-cc.])
          ;;
        esac
        CC="$withval"
    ])
AC_PROG_CC

AC_DEFINE_UNQUOTED(CONFIG_RTAI_BUILD_STRING,"$build",[Build system alias])
RTAI_BUILD_STRING="$build"
AC_DEFINE_UNQUOTED(CONFIG_RTAI_HOST_STRING,"$host",[Host system alias])
RTAI_HOST_STRING="$host"


AM_INIT_AUTOMAKE([foreign no-exeext dist-bzip2])
AM_MAINTAINER_MODE
AC_PROG_LIBTOOL
AM_PROG_AS

dnl Find arch-specific requirements and supports. Features which
dnl are not supported by all archs must be listed here, so that a
dnl an Autoconf flag can later be defined/undefined in rtai_config.h.

arch_supports_lxrt=
arch_supports_leds=

case "$host" in
 i*86*-*)
        RTAI_TARGET_ARCH=i386
        arch_supports_lxrt=y
        arch_supports_leds=y
        wanted_kernel_arch=CONFIG_X86
        ;;
 arm-*)
        RTAI_TARGET_ARCH=arm
        wanted_kernel_arch=CONFIG_ARM
        ;;
 powerpc-*|ppc-*)
        RTAI_TARGET_ARCH=ppc
        wanted_kernel_arch=CONFIG_PPC
        ;;
 mips-*)
        RTAI_TARGET_ARCH=mips
        wanted_kernel_arch=CONFIG_MIPS
        ;;
 cris-*)
        RTAI_TARGET_ARCH=cris
        dnl There is no non-ambiguous CONFIG_ETRAX|CRIS symbol
        dnl so use the root device definition.
        wanted_kernel_arch=CONFIG_ETRAX_ROOT_DEVICE
        ;;
 *) echo ""
   echo "*******************************************"
   echo "Still unsupported host: $host -- Sorry."
   echo "*******************************************"
   echo ""
   exit 1;;
esac

dnl
dnl Installation directory for modules
dnl

AC_MSG_CHECKING([for module installation directory])

AC_ARG_WITH(module-dir,
    [  --with-module-dir=<module-dir>      Installation directory for modules],
    [
        case "$withval" in
        "" | y | ye | yes | n | no)
            AC_MSG_ERROR([You must supply an argument to --with-module-dir.])
          ;;
        esac
        RTAI_MODULE_DIR="$withval"
    ])

dnl Are we building into the source tree?
if test "$srcdir" = `pwd`; then
   CONFIG_RTAI_OLD_FASHIONED_BUILD=y
else
   CONFIG_RTAI_OLD_FASHIONED_BUILD=n
fi

dnl Set a default module dir if unspecified
if test "x" = "x$RTAI_MODULE_DIR"
then
   RTAI_MODULE_DIR=$prefix/modules
fi

AC_MSG_RESULT([$RTAI_MODULE_DIR])
CONFIG_RTAI_MODULE_DIR=$RTAI_MODULE_DIR

#
# define helper function
#

AC_DEFUN(AC_CHECK_LIBRARY, [dnl
KSAVE_LIBS="$LIBS"
LIBS="$$3 $KSAVE_LIBS"
AC_CHECK_LIB($1,$2)
$3=`echo "$LIBS" | sed "s% $KSAVE_LIBS$%%"`
LIBS="$KSAVE_LIBS"
])



# Checks for libraries.
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
   echo
   AC_MSG_ERROR(*** No Linux kernel tree in ${RTAI_LINUX_DIR})
fi

eval linux_`grep '^EXTRAVERSION =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
eval linux_`grep '^PATCHLEVEL =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
eval linux_`grep '^SUBLEVEL =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
eval linux_`grep '^VERSION =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`

AC_MSG_CHECKING([for Linux version])
AC_MSG_RESULT([$RTAI_LINUX_DIR (kernel $linux_VERSION.$linux_PATCHLEVEL.$linux_SUBLEVEL)])

dnl
dnl Load Linux kernel configuration.
dnl

if test \! -r ${RTAI_LINUX_DIR}/.config; then
   AC_MSG_ERROR(*** $RTAI_LINUX_DIR is not configured)
fi

AC_MSG_CHECKING([$RTAI_LINUX_DIR/.config])
. $RTAI_LINUX_DIR/.config
AC_MSG_RESULT([done])

dnl
dnl Identify the base technology used.
dnl

unset CONFIG_RTAI_RTHAL
unset CONFIG_RTAI_ADEOS

AC_MSG_CHECKING([for RTAI base technology])

if test -r $RTAI_LINUX_DIR/include/linux/adeos.h; then
   AC_DEFINE(CONFIG_RTAI_ADEOS,1,[Use Adeos-based HAL])
   AC_MSG_RESULT([Adeos])
   CONFIG_RTAI_ADEOS=y
elif test x${CONFIG_RTHAL} = xy; then
    dnl This test has a flaw. Only x86 forcibly defines CONFIG_RTHAL
    dnl as a result of having applied the RTAI patch. Other platforms
    dnl like PPC for instance leave it as an option, so the kernel is
    dnl patched, but the RTAI support has been configured out.
    dnl This is the reason why the error message remains ambiguous.
    dnl We should definitely have _one_ common way of identifying the
    dnl RTHAL signature inside a kernel tree, that does not rely on
    dnl the Makefile's EXTRAVERSION variable, because it happened that
    dnl some people change its value manually to suit their needs...
    AC_DEFINE(CONFIG_RTAI_RTHAL,1,[Use classic RTHAL])
    AC_MSG_RESULT([rthal])
    CONFIG_RTAI_RTHAL=y
    dnl Need Adeos to run Xenomai in native mode.
    unset CONFIG_RTAI_XENOMAI
else
    echo
    AC_MSG_ERROR([*** RTAI patch not applied/enabled in $RTAI_LINUX_DIR])
fi

case "$linux_VERSION.$linux_PATCHLEVEL" in
  2.4)
    AC_DEFINE(CONFIG_RTAI_LINUX24,1,[Kconfig])
    ;;
  *)
    AC_MSG_ERROR(*** Unsupported kernel version $linux_VERSION.$linux_PATCHLEVEL)
esac

dnl
dnl Perform a few sanity checks.
dnl

arch_select=`eval echo \\$$wanted_kernel_arch`
if test \! x$arch_select = xy; then
   echo
   echo "************************************************************"
   echo " The target RTAI architecture ($RTAI_TARGET_ARCH) does not match"
   echo " the kernel architecture selected in $RTAI_LINUX_DIR"
   echo " (i.e. $wanted_kernel_arch is undefined)"
   echo "************************************************************"
   exit 1
fi

if test x${CONFIG_MODVERSIONS} = xy ; then
   AC_MSG_ERROR(*** Please unset CONFIG_MODVERSIONS from $RTAI_LINUX_DIR and rebuild your kernel)
fi

if test x${CONFIG_RTAI_ADEOS} = xy -a x${CONFIG_ADEOS_CORE} = x; then
   AC_MSG_ERROR(*** Please activate the Adeos support (General menu) in $RTAI_LINUX_DIR and rebuild your kernel)
fi

if test x${CONFIG_RTAI_RTHAL} = xy -a x${CONFIG_RTHAL} = x; then
   AC_MSG_ERROR(*** Please activate the RTAI support in $RTAI_LINUX_DIR and rebuild your kernel)
fi

dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai-unstabel +++ end +++++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

dnl ======================================================================
dnl		check for NEWLXRT
dnl ======================================================================

AC_MSG_CHECKING([for RTAI NEWLXRT enabled])

CONFIG_RTAI_NEWLXRT="`sed -n -e "s/^CONFIG_RTAI_NEWLXRT=\(.*\)/\1/p" $RTAI_DIR/.config`"

case $CONFIG_RTAI_NEWLXRT in
     y | m)
	CONFIG_NEWLXRT=yes;
	;;
     *)
	unset CONFIG_NEWLXRT;
	;;
esac
AC_MSG_RESULT(${CONFIG_NEWLXRT:-no})
AM_CONDITIONAL(CONFIG_NEWLXRT,[test "$CONFIG_NEWLXRT" = "yes"])

dnl ======================================================================
dnl		import settings from RTAI
dnl ======================================================================

RTAI_KMOD_CFLAGS="`sed -n -e "s/^CFLAGS=\(.*\)/\1/p" $RTAI_DIR/.buildvars` -I$RTAI_DIR/include -DEXPORT_SYMTAB -Wall"
LXRT_CFLAGS="-I$RTAI_DIR/include -I$RTAI_DIR/lxrt/include"
KERNEL_CFLAGS=-I$RTAI_LINUX_DIR/include

AC_SUBST(RTAI_KMOD_CFLAGS)
AC_SUBST(LXRT_CFLAGS)
AC_SUBST(KERNEL_CFLAGS)
AC_SUBST(LD)

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


AC_MSG_CHECKING(whether to build AMD PCnet32 driver)
AC_ARG_ENABLE(pcnet32,
	[  --enable-pcnet32       build AMD PCnet32 driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_PCNET32=yes;;
	*) unset CONFIG_RTNET_PCNET32 ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_PCNET32:-no})
AM_CONDITIONAL(CONFIG_RTNET_PCNET32,[test "$CONFIG_RTNET_PCNET32" = "yes"])


AC_MSG_CHECKING(whether to build VIA Rhine driver)
AC_ARG_ENABLE(via-rhine,
	[  --enable-via-rhine     build VIA Rhine driver],
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_VIA_RHINE=yes;;
	*) unset CONFIG_RTNET_VIA_RHINE ;;
	esac])
AC_MSG_RESULT(${CONFIG_RTNET_VIA_RHINE:-no})
AM_CONDITIONAL(CONFIG_RTNET_VIA_RHINE,[test "$CONFIG_RTNET_VIA_RHINE" = "yes"])


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



AC_CONFIG_FILES([ \
Makefile \
examples/Makefile \
examples/round_trip_time/Makefile \
examples/round_trip_time/client/Makefile \
examples/round_trip_time/server/Makefile \
examples/rtnet_lxrt/Makefile \
examples/rtskb_alloc/Makefile \
examples/rtt/Makefile \
examples/frag_ip/Makefile \
examples/raw_packets/Makefile \
driver/Makefile \
driver/tulip/Makefile \
ethernet/Makefile \
ipv4/Makefile \
packet/Makefile \
lib/Makefile \
tools/Makefile \
rtmac/Makefile \
rtmac/examples/Makefile \
rtmac/examples/event/Makefile \
rtmac/examples/mrtt/Makefile \
rtmac/examples/rtt/Makefile \
rtcfg/Makefile \
])

AC_OUTPUT
