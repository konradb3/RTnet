# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)

AC_INIT([RTnet],[0.6.1],[rtnet-users@lists.sourceforge.net])
AC_CONFIG_HEADERS([config/config.h])
AC_CONFIG_AUX_DIR([config/autoconf])
#AC_CONFIG_MACRO_DIR([config/m4])
AC_CONFIG_SRCDIR([rtskb.c])
AC_PREFIX_DEFAULT([/usr/local/rtnet])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_INSTALL

CFLAGS=${CFLAGS:--s}

# Checks for programs.
AC_ARG_WITH(CC,
    AC_HELP_STRING([--with-cc=compiler], [use specific C compiler]),
    [case "$withval" in
    "" | y | ye | yes | n | no)
	AC_MSG_ERROR([*** You must supply an argument to --with-cc.]) ;;
    esac
    CC="$withval"])
AC_PROG_CC

AC_DEFINE_UNQUOTED(CONFIG_RTNET_BUILD_STRING,"$build",[Build system alias])
RTNET_BUILD_STRING="$build"
AC_DEFINE_UNQUOTED(CONFIG_RTNET_HOST_STRING,"$host",[Host system alias])
RTNET_HOST_STRING="$host"
AC_DEFINE_UNQUOTED(CONFIG_RTNET_COMPILER,"`$CC -v 2>&1 | tail -n 1`",[Compiler])

AM_INIT_AUTOMAKE([foreign no-exeext dist-bzip2])
AM_MAINTAINER_MODE
AC_PROG_LIBTOOL
AM_PROG_AS
AC_SUBST(LD)


dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai 3.0 +++ start ++++++++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 
arch_supports_lxrt=
arch_supports_leds=
arch_supports_sim=
arch_supports_rtailab=

case "$host" in
    i*86*-*)
	RTNET_TARGET_ARCH=i386
	arch_supports_lxrt=y
	arch_supports_leds=y
	arch_supports_sim=y
	arch_supports_rtailab=y
	wanted_kernel_arch=CONFIG_X86
        ;;
#    arm-*)
#	RTNET_TARGET_ARCH=arm
#	wanted_kernel_arch=CONFIG_ARM
#        ;;
    powerpc-*|ppc-*)
	RTNET_TARGET_ARCH=ppc
	wanted_kernel_arch=CONFIG_PPC
        ;;
#    mips-*|mipsel-*)
#	RTNET_TARGET_ARCH=mips
#	wanted_kernel_arch=CONFIG_MIPS
#        ;;
#    cris-*)
#	RTNET_TARGET_ARCH=cris
#	dnl There is no non-ambiguous CONFIG_ETRAX|CRIS symbol
#	dnl so use the root device definition.
#	wanted_kernel_arch=CONFIG_ETRAX_ROOT_DEVICE
#        ;;
    *)
	AC_MSG_ERROR([*** sorry - unsupported architecture])
	;;
esac

dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai-3.0 +++ end ++++++++++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++


#
# define helper function
#
AC_DEFUN(AC_CHECK_LIBRARY, [dnl
			   ac_save_LIBS="$LIBS"
			   LIBS="$$3 $ac_save_LIBS"
			   AC_CHECK_LIB($1,$2)
			   $3=`echo "$LIBS" | sed "s% $ac_save_LIBS$%%"`
			   LIBS="$ac_save_LIBS"
			   ])


# Checks for libraries.
# FIXME: Replace `main' with a function in `-lncurses':
AC_CHECK_LIBRARY(ncurses, main, NCURSES_LIBS)
AM_CONDITIONAL(CONFIG_NCURSES,[test x"${NCURSES_LIBS}" != x""])
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

AC_ARG_WITH(rtai,
    AC_HELP_STRING([--with-rtai=RTAI], [path to configured RTAI 24.1.x sources, or RTAI 3 installation]),
    [case "${withval}" in
    "" | y | ye | yes | n | no)
	AC_MSG_ERROR([You must supply an argument to --with-rtai.]) ;;
    esac],
    with_rtai="/usr/src/rtai")


dnl ======================================================================
dnl =========== try to find rtai-24.1.x ==================================
dnl ======================================================================
#
# try to find rtai-24.1.x
# set CONFIG_RTNET_RTAI_24 to 'n' to prevent it
#
unset CONFIG_RTNET_RTAI_24

# first check if there is a Makefile, this means probably rtai-24.1.x
if test x"$CONFIG_RTNET_RTAI_24" \!= x"n"; then
    AC_MSG_CHECKING([for RTAI 24.1.x source tree])
    if test \! -f $with_rtai/Makefile; then
	CONFIG_RTNET_RTAI_24=n
	AC_MSG_RESULT([no])
    fi
fi

# check for the right Makefile
if test "$CONFIG_RTNET_RTAI_24" != "n"; then
    if test "`sed -n -e \"s/^PROJECT = \(.*\)/\1/p\" ${with_rtai}/Makefile`" = "RTAI"; then
	RTAI_DIR=$with_rtai
    	AC_MSG_RESULT([$RTAI_DIR])
    else
 	CONFIG_RTNET_RTAI_24=n
	AC_MSG_RESULT([no])
    fi
fi

#
# if we've gone so far, we think that the user gives us a rtai-24.1.x source dir
# if the following checks, (not configured rtai, kernel, etc) fail, this will be fatal,
# so we simply stop checking
#

if test x"$CONFIG_RTNET_RTAI_24" \!= x"n"; then
    AC_MSG_CHECKING([for configured RTAI 24.1.x])
    if test -f ${RTAI_DIR}/.buildvars -a -f ${RTAI_DIR}/.config; then
	AC_MSG_RESULT([yes])
    else
	AC_MSG_ERROR([*** No configured RTAI found in $RTAI_DIR])
    fi

# source (load) the rtai configuration
    . ${RTAI_DIR}/.config

    CONFIG_RTNET_RTAI_VERSION=24

    RTAI_LINUX_DIR=`sed -n -e "s/^LINUXDIR=\(.*\)/\1/p" $RTAI_DIR/.buildvars`

#
# make some checks on the kernel
#
# __N_O_T_E__ the if expression ends after the checks copied from RTAI-unstable....
#

dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai 3.0 +++ start ++++++++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

AC_MSG_CHECKING([for Linux source tree])

if test \! -r ${RTAI_LINUX_DIR}/Makefile; then
   echo
   AC_MSG_ERROR([*** No Linux kernel tree in ${RTAI_LINUX_DIR}])
fi

eval linux_`grep '^EXTRAVERSION =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
eval linux_`grep '^PATCHLEVEL =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
eval linux_`grep '^SUBLEVEL =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
eval linux_`grep '^VERSION =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`

dnl Load Linux kernel configuration now, before config symbols are tested.
if test \! -r ${RTAI_LINUX_DIR}/.config; then
   AC_MSG_ERROR([*** $RTAI_LINUX_DIR is not configured])
fi

. $RTAI_LINUX_DIR/.config

AC_MSG_RESULT([$RTAI_LINUX_DIR (kernel $linux_VERSION.$linux_PATCHLEVEL.$linux_SUBLEVEL)])

dnl
dnl Identify the base technology used.
dnl

unset CONFIG_RTAI_RTHAL
unset CONFIG_RTAI_ADEOS

AC_MSG_CHECKING([for RTAI base technology])

if test -r $RTAI_LINUX_DIR/include/linux/adeos.h; then
#   AC_DEFINE(CONFIG_RTAI_ADEOS,1,[Use Adeos-based HAL])
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
#    AC_DEFINE(CONFIG_RTAI_RTHAL,1,[Use legacy RTHAL])
    if test "$CONFIG_X86" = y; then
       AC_MSG_RESULT([legacy RTHAL])
    else
       AC_MSG_RESULT([RTHAL])
    fi
    CONFIG_RTAI_RTHAL=y
    dnl Need Adeos to run Xenomai in native mode.
    unset CONFIG_RTAI_XENOMAI
else
    echo
    AC_MSG_ERROR([*** RTAI patch not applied/enabled in $RTAI_LINUX_DIR])
fi

case "$linux_VERSION.$linux_PATCHLEVEL" in
  2.4)
    ;;
  *) 
    AC_MSG_ERROR([*** Unsupported kernel version $linux_VERSION.$linux_PATCHLEVEL])
esac

dnl
dnl Perform a few sanity checks.
dnl

arch_select=`eval echo \\$$wanted_kernel_arch`
if test x$arch_select = x; then
   echo
   echo "************************************************************"
   echo " The target RTAI architecture ($RTNET_TARGET_ARCH) does not match"
   echo " the kernel architecture selected in $RTAI_LINUX_DIR"
   echo " (i.e. $wanted_kernel_arch is undefined)"
   echo "************************************************************"
   exit 1
fi

if test x${CONFIG_MODULES} = x ; then
  AC_MSG_ERROR([*** Please set CONFIG_MODULES for $RTAI_LINUX_DIR and rebuild your kernel])
fi

if test x${CONFIG_MODVERSIONS} = xy ; then
  AC_MSG_ERROR([*** Please unset CONFIG_MODVERSIONS for $RTAI_LINUX_DIR and rebuild your kernel])
fi

if test x${CONFIG_RTAI_ADEOS} = xy -a x${CONFIG_ADEOS_CORE} = x; then
   AC_MSG_ERROR([*** Please activate the Adeos support (General menu) in $RTAI_LINUX_DIR and rebuild your kernel])
fi

if test x${CONFIG_RTAI_RTHAL} = xy -a x${CONFIG_RTHAL} = x; then
   AC_MSG_ERROR([*** Please activate the RTAI support in $RTAI_LINUX_DIR and rebuild your kernel])
fi


dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai-3.0 +++ end ++++++++++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
fi

dnl ===========	end of rtai 24.1.x detection =============================


dnl ======================================================================
dnl =========== try to find rtai-3 =======================================
dnl ======================================================================
#
# try to find rtai-3, but don't look for it, if...
# ...we've already found rtai-24.1.x
#
if test x"$CONFIG_RTNET_RTAI_24" \!= x"n"; then
    CONFIG_RTNET_RTAI_3=n
else
    unset CONFIG_RTNET_RTAI_3
fi

#
# the presence of rtai-config is a good indication for rtai-3
#
if test x"$CONFIG_RTNET_RTAI_3" \!= x"n"; then
    AC_MSG_CHECKING([for RTAI 3 installation])
    RTAI_CONFIG="${with_rtai}/bin/rtai-config"
    if test -x ${RTAI_CONFIG}; then
        RTAI_DIR="${with_rtai}"
	AC_MSG_RESULT([${RTAI_DIR}])
    else
	CONFIG_RTNET_RTAI_3=n
	AC_MSG_RESULT([no])
    fi
fi


#
# from now we think we've found RTAI-3, so if we got en error
# it will be fatal, thus exit with AC_MSG_ERROR
#

if test x"$CONFIG_RTNET_RTAI_3" \!= x"n"; then
#
# check if rtai-3 is installed with DESTDIR
# e.g.: 'make DESTDIR=/foo install'
#
    AC_MSG_CHECKING([for RTAI 3 DESTDIR])
    RTNET_RTAI_PREFIX="`${RTAI_CONFIG} --prefix`"
    if test "${RTNET_RTAI_PREFIX}" \!= "${RTAI_DIR}"; then
	RTNET_RTAI_DESTDIR="`echo ${RTAI_DIR} | sed -e s,${RTNET_RTAI_PREFIX}$,,`"
	export DESTDIR="${RTNET_RTAI_DESTDIR}"

	RTNET_RTAI_PREFIX="`${RTAI_CONFIG} --prefix`"
	if test "${RTNET_RTAI_PREFIX}" = "${RTAI_DIR}"; then
	    AC_MSG_RESULT([${DESTDIR}])
	else
	    AC_MSG_ERROR([*** Cannot identify DESTDIR])
	fi
    else
	AC_MSG_RESULT([/])
    fi

    AC_MSG_CHECKING([for RTAI 3 version])
    CONFIG_RTNET_RTAI_3_VERSION="`${RTAI_CONFIG} --version`"

    case "$CONFIG_RTNET_RTAI_3_VERSION" in
    3.0*)
	AC_MSG_RESULT([${CONFIG_RTNET_RTAI_3_VERSION}])
	CONFIG_RTNET_RTAI_VERSION=30
	;;
    *)
	AC_MSG_ERROR([*** Unsupported RTAI version $CONFIG_RTNET_RTAI_3_VERSION in $RTAI_DIR])
	;;
    esac

    AC_MSG_CHECKING([for RTAI 3 config file])
    CONFIG_RTNET_RTAI_CONFIG_FILE="${RTAI_DIR}/share/rtai/config-rtai-${CONFIG_RTNET_RTAI_3_VERSION}"
    if test \! -f ${CONFIG_RTNET_RTAI_CONFIG_FILE}; then
	CONFIG_RTNET_RTAI_CONFIG_FILE="`${RTAI_CONFIG} --config`"
	if test \! -f $CONFIG_RTNET_RTAI_CONFIG_FILE; then
	    AC_MSG_ERROR([*** RTAI 3 configuration file not found])
	fi
    fi
    AC_MSG_RESULT([${CONFIG_RTNET_RTAI_CONFIG_FILE}])

# source (load) the rtai configuration
    . ${CONFIG_RTNET_RTAI_CONFIG_FILE}

# check for semas, needed by rtnet
    AC_MSG_CHECKING([for RTAI Semaphores])
    case "${CONFIG_RTAI_SEM}" in
	y | m) AC_MSG_RESULT([${CONFIG_RTAI_SEM}]) ;;
	*) AC_MSG_ERROR([*** Please enable Semaphores in RTAI]) ;;
    esac

# check for RT malloc, needed by rtnet
    AC_MSG_CHECKING([for RTAI malloc])
    case "${CONFIG_RTAI_MALLOC}" in
	y | m) AC_MSG_RESULT([${CONFIG_RTAI_MALLOC}]) ;;
	*) AC_MSG_ERROR([*** Please enable real-time malloc in RTAI]) ;;
    esac

# check for mailboxes, eventually needed by rtnet
#    AC_MSG_CHECKING([for RTAI Mailboxes])
#    case "${CONFIG_RTAI_MBX}" in
#	y | m) AC_MSG_RESULT([${CONFIG_RTAI_MBX}]) ;;
#	*) AC_MSG_ERROR([*** Please enable Mailboxes in RTAI]) ;;
#    esac
fi

dnl ===========	end of rtai 3 detection ==================================


#
# now let's see wich version of rtai we've found
# exit with error if there isn't any version
#
case "$CONFIG_RTNET_RTAI_VERSION" in
    24)
	AC_DEFINE(CONFIG_RTAI_24,1,[We build for rtai 24.1.x])
	;;
    30)
	AC_DEFINE(CONFIG_RTAI_30,1,[We build for rtai 3.0.x])
	;;
    *)
	AC_MSG_ERROR([*** No RTAI found in $with_rtai])
	;;
esac


dnl ======================================================================
dnl		import settings from RTAI
dnl ======================================================================

case "${CONFIG_RTNET_RTAI_VERSION}" in
    24 )
	RTAI_KMOD_CFLAGS="`sed -n -e "s/^CFLAGS=\(.*\)/\1/p" $RTAI_DIR/.buildvars` -I$RTAI_DIR/include -DEXPORT_SYMTAB -Wall"
	LXRT_CFLAGS="-I$RTAI_DIR/include -I$RTAI_DIR/lxrt/include"
	KERNEL_CFLAGS="-I$RTAI_LINUX_DIR/include"
	CONFIG_RTNET_RTAI_ARCH="`sed -n -e "s/^ARCH=\(.*\)/\1/p" $RTAI_DIR/.buildvars`"
	;;
    30 )
	RTAI_KMOD_CFLAGS="`${RTAI_CONFIG} --module-cflags`"
	LXRT_CFLAGS="`${RTAI_CONFIG} --lxrt-cflags`"
	KERNEL_CFLAGS="-I`${RTAI_CONFIG} --linux-dir`/include"
	CONFIG_RTNET_RTAI_ARCH="`${RTAI_CONFIG} --arch`"
	;;
    * )
	AC_MSG_ERROR([*** internal error])
esac

AC_SUBST(RTAI_KMOD_CFLAGS)
AC_SUBST(LXRT_CFLAGS)
AC_SUBST(KERNEL_CFLAGS)


dnl ======================================================================
dnl		architecture tests....
dnl ======================================================================

AC_MSG_CHECKING([for RTAI target arch])
AC_MSG_RESULT([${CONFIG_RTNET_RTAI_ARCH}])

AC_MSG_CHECKING([for RTnet target arch])
AC_MSG_RESULT([${RTNET_TARGET_ARCH}])

if test x"${RTNET_TARGET_ARCH}" != x"${CONFIG_RTNET_RTAI_ARCH}"; then
	AC_MSG_ERROR([*** RTAI and RTnet target architectures are not the same])
fi

case "${CONFIG_RTNET_RTAI_ARCH}" in
    i386)
	AC_DEFINE(CONFIG_ARCH_I386,1,[we are building for x86])
	;;
    ppc)
	AC_DEFINE(CONFIG_ARCH_PPC,1,[we are building for powerpc])
	;;
#    arm)
#	AC_DEFINE(CONFIG_ARCH_ARM,1,[we are trying to build for arm])
#	;;
#    cris)
#	AC_DEFINE(CONFIG_ARCH_CRIS,1,[we are trying to build for cris])
#	;;
    *)
	AC_MSG_ERROR([*** unsupported architecture])
	;;
esac


dnl ======================================================================
dnl		Installation directory for modules
dnl ======================================================================

AC_MSG_CHECKING([for module installation directory])

AC_ARG_WITH(module-dir,
    AC_HELP_STRING([--with-module-dir=<module-dir>], [Installation directory for modules]),
    [case "${withval}" in
    "" | y | ye | yes | n | no)
	AC_MSG_ERROR([You must supply an argument to --with-module-dir.]) ;;
    esac],
    with_module_dir='${exec_prefix}/modules')
RTNET_MODULE_DIR="$with_module_dir"
AC_MSG_RESULT([$RTNET_MODULE_DIR])
AC_SUBST(RTNET_MODULE_DIR)


dnl ======================================================================
dnl		check for NEWLXRT
dnl ======================================================================
AC_MSG_CHECKING([for RTAI NEWLXRT enabled])
case "${CONFIG_RTNET_RTAI_VERSION}" in
    24)
	case "$CONFIG_RTAI_NEWLXRT" in
	    y | m) CONFIG_NEWLXRT=yes ;;
	    *) unset CONFIG_NEWLXRT ;;
	esac
	;;
    30)
	case "${CONFIG_RTAI_SCHED_LXRT}" in
	    y | m) CONFIG_NEWLXRT=yes ;;
	    *) unset CONFIG_NEWLXRT ;;
	esac
	;;
    *)
	AC_MSG_ERROR([*** internal error])
	;;
esac
AC_MSG_RESULT([${CONFIG_NEWLXRT:-no}])
AM_CONDITIONAL(CONFIG_NEWLXRT,[test "$CONFIG_NEWLXRT" = "yes"])


dnl ======================================================================
dnl		RTAI header checks
dnl ======================================================================

ac_save_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${CPPFLAGS} ${LXRT_CFLAGS}"

# mandatory RTAI-24 headers
AC_CHECK_HEADERS(rtai_lxrt_user.h rtai_declare.h,
	[],
	[case "${CONFIG_RTNET_RTAI_VERSION}" in
	24 ) AC_MSG_ERROR([*** header not found or working, please check RTAI installation]) ;;
	30 ) ;;
	* ) AC_MSG_ERROR([*** internal error])
	esac])

# mandatory RTAI-30 headers
AC_CHECK_HEADERS(rtai_sem.h rtai_malloc.h,
	[],
	[case "${CONFIG_RTNET_RTAI_VERSION}" in
	24 ) ;;
	30 ) AC_MSG_ERROR([*** header not found or working, please check RTAI installation]) ;;
	* ) AC_MSG_ERROR([*** internal error])
	esac])

CPPFLAGS="${ac_save_CFLAGS}"


dnl ======================================================================
dnl		network interface card driver
dnl ======================================================================

AC_MSG_CHECKING([whether to build all PCI network driver])
AC_ARG_ENABLE(allpci,
	AC_HELP_STRING([--enable-allpci], [build all PCI NIC drivers]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_ALLPCI=yes ;;
	*) unset CONFIG_RTNET_ALLPCI ;;
	esac])
AC_MSG_RESULT([${CONFIG_RTNET_ALLPCI:-no}])


AC_MSG_CHECKING([whether to build 3Com 59x driver])
AC_ARG_ENABLE(3c59x,
	AC_HELP_STRING([--enable-3c59x], [build 3Com 59x driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_3C59X=yes ;;
	*) unset CONFIG_RTNET_3C59X ;;
	esac])
# 3Com should be enabled manually, not by allpci
AC_MSG_RESULT([${CONFIG_RTNET_3C59X:-no}])
AM_CONDITIONAL(CONFIG_RTNET_3C59X,[test "$CONFIG_RTNET_3C59X" = "yes"])


AC_MSG_CHECKING([whether to build Realtek 8139 driver])
AC_ARG_ENABLE(8139,
	AC_HELP_STRING([--enable-8139], [build Realtek 8139 driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_8139=yes ;;
	*) unset CONFIG_RTNET_8139 ;;
	esac])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
	CONFIG_RTNET_8139=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_8139:-no}])
AM_CONDITIONAL(CONFIG_RTNET_8139,[test "$CONFIG_RTNET_8139" = "yes"])


AC_MSG_CHECKING([whether to build Intel EtherExpressPro/100 driver])
AC_ARG_ENABLE(eepro100,
	AC_HELP_STRING([--enable-eepro100], [build Intel EtherExpressPro/100 driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_EEPRO100=yes ;;
	*) unset CONFIG_RTNET_EEPRO100 ;;
	esac])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
	CONFIG_RTNET_EEPRO100=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_EEPRO100:-no}])
AM_CONDITIONAL(CONFIG_RTNET_EEPRO100,[test "$CONFIG_RTNET_EEPRO100" = "yes"])


AC_MSG_CHECKING([whether to build AMD PCnet32 driver])
AC_ARG_ENABLE(pcnet32,
	AC_HELP_STRING([--enable-pcnet32], [build AMD PCnet32 driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_PCNET32=yes ;;
	*) unset CONFIG_RTNET_PCNET32 ;;
	esac])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
	CONFIG_RTNET_PCNET32=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_PCNET32:-no}])
AM_CONDITIONAL(CONFIG_RTNET_PCNET32,[test "$CONFIG_RTNET_PCNET32" = "yes"])


AC_MSG_CHECKING([whether to build VIA Rhine driver])
AC_ARG_ENABLE(via-rhine,
	AC_HELP_STRING([--enable-via-rhine], [build VIA Rhine driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_VIA_RHINE=yes ;;
	*) unset CONFIG_RTNET_VIA_RHINE ;;
	esac])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
	CONFIG_RTNET_VIA_RHINE=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_VIA_RHINE:-no}])
AM_CONDITIONAL(CONFIG_RTNET_VIA_RHINE,[test "$CONFIG_RTNET_VIA_RHINE" = "yes"])


AC_MSG_CHECKING([whether to build NatSemi driver])
AC_ARG_ENABLE(natsemi,
	AC_HELP_STRING([--enable-natsemi], [build NatSemi driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_NATSEMI=yes ;;
	*) unset CONFIG_RTNET_NATSEMI ;;
	esac])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
	CONFIG_RTNET_NATSEMI=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_NATSEMI:-no}])
AM_CONDITIONAL(CONFIG_RTNET_NATSEMI,[test "$CONFIG_RTNET_NATSEMI" = "yes"])


AC_MSG_CHECKING([whether to build tulip driver])
AC_ARG_ENABLE(tulip,
	AC_HELP_STRING([--enable-tulip], [build tulip driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_TULIP=yes ;;
	*) unset CONFIG_RTNET_TULIP ;;
	esac])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
	CONFIG_RTNET_TULIP=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_TULIP:-no}])
AM_CONDITIONAL(CONFIG_RTNET_TULIP,[test "$CONFIG_RTNET_TULIP" = "yes"])


AC_MSG_CHECKING([whether to build fcc enet driver])
AC_ARG_ENABLE(fcc-enet,
	AC_HELP_STRING([--enable-fcc-enet], [build fcc enet driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_FCC_ENET=yes ;;
	*) unset CONFIG_RTNET_FCC_ENET ;;
	esac])
AC_MSG_RESULT([${CONFIG_RTNET_FCC_ENET:-no}])
AM_CONDITIONAL(CONFIG_RTNET_FCC_ENET,[test "$CONFIG_RTNET_FCC_ENET" = "yes"])


AC_MSG_CHECKING([whether to build scc enet driver])
AC_ARG_ENABLE(scc-enet,
	AC_HELP_STRING([--enable-scc-enet], [build scc enet driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_SCC_ENET=yes ;;
	*) unset CONFIG_RTNET_SCC_ENET ;;
	esac])
AC_MSG_RESULT([${CONFIG_RTNET_SCC_ENET:-no}])
AM_CONDITIONAL(CONFIG_RTNET_SCC_ENET,[test "$CONFIG_RTNET_SCC_ENET" = "yes"])


AC_MSG_CHECKING([whether to build fec enet driver])
AC_ARG_ENABLE(fec-enet,
	AC_HELP_STRING([--enable-fec-enet], [build fec enet driver]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_FEC_ENET=yes ;;
	*) unset CONFIG_RTNET_FEC_ENET ;;
	esac])
AC_MSG_RESULT([${CONFIG_RTNET_FEC_ENET:-no}])
AM_CONDITIONAL(CONFIG_RTNET_FEC_ENET,[test "$CONFIG_RTNET_FEC_ENET" = "yes"])


AC_MSG_CHECKING([whether to build loopback driver])
AC_ARG_ENABLE(loopback,
	AC_HELP_STRING([--enable-loopback], [build loopback driver (default=yes)]),
	[case "$enableval" in
	n | no ) CONFIG_RTNET_LOOPBACK=no ;;
	*) CONFIG_RTNET_LOOPBACK=yes ;;
	esac],
	CONFIG_RTNET_LOOPBACK=yes)
AC_MSG_RESULT([${CONFIG_RTNET_LOOPBACK:-no}])
AM_CONDITIONAL(CONFIG_RTNET_LOOPBACK,[test "$CONFIG_RTNET_LOOPBACK" = "yes"])


dnl ======================================================================
dnl		RTcfg
dnl ======================================================================

AC_MSG_CHECKING([whether to build RTcfg])
AC_ARG_ENABLE(rtcfg,
	AC_HELP_STRING([--enable-rtcfg], [build RTcfg (default=yes)]),
	[case "$enableval" in
	n | no ) CONFIG_RTNET_RTCFG=no ;;
	*) CONFIG_RTNET_RTCFG=yes ;;
	esac],
	CONFIG_RTNET_RTCFG=yes)
AC_MSG_RESULT([${CONFIG_RTNET_RTCFG:-no}])
AM_CONDITIONAL(CONFIG_RTNET_RTCFG,[test "$CONFIG_RTNET_RTCFG" = "yes"])

AC_MSG_CHECKING([whether to enable RTcfg debug support])
AC_ARG_ENABLE(rtcfg-dbg,
	AC_HELP_STRING([--enable-rtcfg-dbg], [enable RTcfg debugging (default=yes)]),
	[case "$enableval" in
	n | no ) CONFIG_RTNET_RTCFG_DEBUG=no ;;
	*) CONFIG_RTNET_RTCFG_DEBUG=yes ;;
	esac],
	CONFIG_RTNET_RTCFG_DEBUG=yes)
AC_MSG_RESULT([${CONFIG_RTNET_RTCFG_DEBUG:-yes}])
if test "$CONFIG_RTNET_RTCFG_DEBUG" = "yes"; then
	AC_DEFINE(CONFIG_RTNET_RTCFG_DEBUG,1,[RTcfg debug])
fi


dnl ======================================================================
dnl		RTcap
dnl ======================================================================

AC_MSG_CHECKING([whether to enable RTcap support])
AC_ARG_ENABLE(rtcap,
	AC_HELP_STRING([--enable-rtcap], [enable RTcap support and build capturing module]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_RTCAP=yes ;;
	*) unset CONFIG_RTNET_RTCAP ;;
	esac])
AC_MSG_RESULT([${CONFIG_RTNET_RTCAP:-no}])
AM_CONDITIONAL(CONFIG_RTNET_RTCAP,[test "$CONFIG_RTNET_RTCAP" = "yes"])
if test "$CONFIG_RTNET_RTCAP" = "yes"; then
	AC_DEFINE(CONFIG_RTNET_RTCAP,1,[RTcap support])
fi


dnl ======================================================================
dnl		rtnetproxy
dnl ======================================================================

AC_MSG_CHECKING([whether to build rtnetproxy])
AC_ARG_ENABLE(proxy,
	AC_HELP_STRING([--enable-proxy], [build IP protocol proxy driver (legacy)]),
	[case "$enableval" in
	y | yes ) CONFIG_RTNET_PROXY=yes ;;
	*) unset CONFIG_RTNET_PROXY ;;
	esac])
AC_MSG_RESULT([${CONFIG_RTNET_PROXY:-no}])
AM_CONDITIONAL(CONFIG_RTNET_PROXY,[test "$CONFIG_RTNET_PROXY" = "yes"])
if test "$CONFIG_RTNET_PROXY" = "yes"; then
	AC_DEFINE(CONFIG_RTNET_PROXY,1,[rtnetproxy support])
fi


dnl ======================================================================
dnl		compilation parameters
dnl ======================================================================

AC_MSG_CHECKING([whether to enable internal bug checks])
AC_ARG_ENABLE(checks,
	AC_HELP_STRING([--enable-checks], [enable internal bug checks (default=yes)]),
	[case "$enableval" in
	n | no ) CONFIG_RTNET_CHECKED=no ;;
	*) CONFIG_RTNET_CHECKED=yes ;;
	esac],
	CONFIG_RTNET_CHECKED=yes)
AC_MSG_RESULT([${CONFIG_RTNET_CHECKED:-yes}])
if test "$CONFIG_RTNET_CHECKED" = "yes"; then
	AC_DEFINE(CONFIG_RTNET_CHECKED,1,[Bug checks])
fi


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
rtmac/tdma/Makefile \
rtmac/examples/Makefile \
rtmac/examples/event/Makefile \
rtmac/examples/mrtt/Makefile \
rtmac/examples/rtt/Makefile \
rtcfg/Makefile \
include/Makefile \
rtcap/Makefile \
])

AC_OUTPUT


dnl ======================================================================
dnl		generate include/rtnet_config.h
dnl ======================================================================

AC_MSG_NOTICE([creating include/rtnet_config.h])
sed -e "s/ PACKAGE/ RTNET_PACKAGE/" -e "s/ VERSION/ RTNET_VERSION/" \
    config/config.h >include/rtnet_config.h.new
if cmp -s include/rtnet_config.h.new include/rtnet_config.h; then
	rm include/rtnet_config.h.new
	AC_MSG_NOTICE([include/rtnet_config.h is unchanged])
else
	mv include/rtnet_config.h.new include/rtnet_config.h

fi
