# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

AC_INIT([RTnet],[0.9.0],[rtnet-users@lists.sourceforge.net])
AC_CONFIG_HEADERS([config/rtnet_config_pre.h])
AC_CONFIG_AUX_DIR([config/autoconf])
AC_CONFIG_MACRO_DIR([config/m4])
AC_CONFIG_SRCDIR([stack/rtskb.c])
AC_PREFIX_DEFAULT([/usr/local/rtnet])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_INSTALL

CFLAGS=${CFLAGS:--s}

# Checks for programs.
AC_ARG_WITH(CC,
    AS_HELP_STRING([--with-cc=compiler], [use specific C compiler]),
    [case "$withval" in
    "" | y | ye | yes | n | no)
        AC_MSG_ERROR([*** You must supply an argument to --with-cc.]) ;;
    esac
    CC="$withval"])
AC_PROG_CC

AC_DEFINE_UNQUOTED(CONFIG_RTNET_BUILD_STRING, "$build", [Build system alias])
RTNET_BUILD_STRING="$build"
AC_DEFINE_UNQUOTED(CONFIG_RTNET_HOST_STRING, "$host", [Host system alias])
RTNET_HOST_STRING="$host"
AC_DEFINE_UNQUOTED(CONFIG_RTNET_COMPILER, "`$CC -v 2>&1 | tail -n 1`" , [Compiler])

AM_INIT_AUTOMAKE([foreign no-exeext dist-bzip2])
AM_MAINTAINER_MODE
AC_PROG_LIBTOOL
AM_PROG_AS
AC_SUBST(LD)


dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai 3.0 +++ start ++++++++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

case "$host" in
    i*86*-*)
        RTNET_TARGET_ARCH=i386
        wanted_kernel_arch=CONFIG_X86
        ;;
    arm-*)
        RTNET_TARGET_ARCH=arm
        wanted_kernel_arch=CONFIG_ARM
        ;;
    powerpc-*|ppc-*)
        RTNET_TARGET_ARCH=ppc
        wanted_kernel_arch=CONFIG_PPC
        ;;
#    mips-*|mipsel-*)
#       RTNET_TARGET_ARCH=mips
#       wanted_kernel_arch=CONFIG_MIPS
#        ;;
#    cris-*)
#       RTNET_TARGET_ARCH=cris
#       dnl There is no non-ambiguous CONFIG_ETRAX|CRIS symbol
#       dnl so use the root device definition.
#       wanted_kernel_arch=CONFIG_ETRAX_ROOT_DEVICE
#        ;;
    *)
        AC_MSG_ERROR([*** Sorry - Unsupported architecture])
        ;;
esac

dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
dnl ++++++++++++ copied from rtai-3.0 +++ end ++++++++++++++++++++++++++++
dnl ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#
# define helper function
#
AC_DEFUN([AC_CHECK_LIBRARY],
    [ac_save_LIBS="$LIBS"
    LIBS="$$3 $ac_save_LIBS"
    AC_CHECK_LIB($1,$2)
    $3=`echo "$LIBS" | sed "s% $ac_save_LIBS$%%"`
    LIBS="$ac_save_LIBS"])


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
dnl             rtai sources
dnl ======================================================================

AC_ARG_WITH(rtai,
    AS_HELP_STRING([--with-rtai=RTAI], [installation path of RTAI 3.x or RTAI/fusion]),
    [case "${withval}" in
    "" | y | ye | yes | n | no)
        AC_MSG_ERROR([You must supply an argument to --with-rtai.]) ;;
    esac],
    with_rtai="/usr/src/rtai")


dnl ======================================================================
dnl =========== try to find rtai-3 =======================================
dnl ======================================================================

unset CONFIG_RTNET_RTAI_3

#
# the presence of rtai-config is a good indication for rtai >= 3
#
if test x"$CONFIG_RTNET_RTAI_3" \!= x"n"; then
    AC_MSG_CHECKING([for RTAI 3 installation])
    RTAI_CONFIG="${with_rtai}/bin/rtai-config"
    if test -x "${RTAI_CONFIG}"; then
        RTAI_DIR="`cd ${with_rtai} && pwd`"     # let the shell normalize the path
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
    AC_MSG_CHECKING([for RTAI DESTDIR])
    RTNET_RTAI_PREFIX="`cd \`${RTAI_CONFIG} --prefix\` && pwd`"                 # let the shell normalize the path
    if test "${RTNET_RTAI_PREFIX}" \!= "${RTAI_DIR}"; then
        RTNET_RTAI_DESTDIR="`cd \`echo ${RTAI_DIR} | sed -e s,${RTNET_RTAI_PREFIX}$,,\` && pwd`"
        export DESTDIR="${RTNET_RTAI_DESTDIR}"

        RTNET_RTAI_PREFIX="`cd \`${RTAI_CONFIG} --prefix\` && pwd`"             # let the shell normalize the path
        if test "${RTNET_RTAI_PREFIX}" = "${RTAI_DIR}"; then
            AC_MSG_RESULT([${DESTDIR}])
        else
            AC_MSG_ERROR([*** Cannot identify DESTDIR])
        fi
    else
        AC_MSG_RESULT([none])
    fi

    AC_MSG_CHECKING([for RTAI version])
    CONFIG_RTNET_RTAI_3_VERSION="`${RTAI_CONFIG} --version`"

    case "$CONFIG_RTNET_RTAI_3_VERSION" in
    3.0*)
        CONFIG_RTNET_RTAI_VERSION=30
        ;;
    3.1*)
        CONFIG_RTNET_RTAI_VERSION=31
        ;;
    3.2*)
        CONFIG_RTNET_RTAI_VERSION=32
        ;;
    0.8.99-fusion | 0.9*-fusion)
        CONFIG_RTNET_RTAI_VERSION=fusion-090
        ;;
    *)
        AC_MSG_ERROR([*** Unsupported RTAI version $CONFIG_RTNET_RTAI_3_VERSION in $RTAI_DIR])
        ;;
    esac
    AC_MSG_RESULT([${CONFIG_RTNET_RTAI_3_VERSION}])

    AC_MSG_CHECKING([for RTAI config file])
    CONFIG_RTNET_RTAI_CONFIG_FILE="${RTAI_DIR}/share/rtai/config-rtai-${CONFIG_RTNET_RTAI_3_VERSION}"
    if test \! -f ${CONFIG_RTNET_RTAI_CONFIG_FILE}; then
        CONFIG_RTNET_RTAI_CONFIG_FILE="`${RTAI_CONFIG} --config`"
        if test \! -f $CONFIG_RTNET_RTAI_CONFIG_FILE; then
            AC_MSG_ERROR([*** RTAI configuration file not found])
        fi
    fi
    AC_MSG_RESULT([${CONFIG_RTNET_RTAI_CONFIG_FILE}])

# source (load) the rtai configuration
    . ${CONFIG_RTNET_RTAI_CONFIG_FILE}

    case "$CONFIG_RTNET_RTAI_VERSION" in
        30 | 31 | 32)   # ---> RTAI 3.x <---

            # check for semas, needed by rtnet
            AC_MSG_CHECKING([for RTAI semaphores])
            case "${CONFIG_RTAI_SEM}" in
                y | m) AC_MSG_RESULT([${CONFIG_RTAI_SEM}]) ;;
                *) AC_MSG_ERROR([*** Please enable semaphores in RTAI]) ;;
            esac

            # check for RT malloc, needed by rtnet
            AC_MSG_CHECKING([for RTAI malloc])
            case "${CONFIG_RTAI_MALLOC}" in
                y | m) AC_MSG_RESULT([${CONFIG_RTAI_MALLOC}]) ;;
                *) AC_MSG_ERROR([*** Please enable real-time malloc in RTAI]) ;;
            esac

            # check for fifos, needed by rtnet-examples
            AC_MSG_CHECKING([for RTAI fifos])
            case "${CONFIG_RTAI_FIFOS}" in
                y | m) AC_MSG_RESULT([${CONFIG_RTAI_FIFOS}]) ;;
                *) AC_MSG_ERROR([*** Please enable fifos in RTAI]) ;;
            esac

            ;;

        fusion-090)     # ---> FUSION <---

            # check for native RTAI skin, needed by rtnet
            AC_MSG_CHECKING([for RTAI RTDM skin])
            case "${CONFIG_RTAI_SKIN_RTDM}" in
                y) AC_MSG_RESULT([${CONFIG_RTAI_SKIN_RTDM}]) ;;
                *) AC_MSG_ERROR([*** Please enable RTDM skin]) ;;
            esac

            # check for semas, needed by rtnet
            AC_MSG_CHECKING([for RTAI Semaphores])
            case "${CONFIG_RTAI_OPT_NATIVE_SEM}" in
                y) AC_MSG_RESULT([${CONFIG_RTAI_OPT_NATIVE_SEM}]) ;;
                *) AC_MSG_ERROR([*** Please enable semaphores in RTAI]) ;;
            esac

            # malloc support is built-in for RTAI/fusion

            # check for pipes, needed by rtnet
            AC_MSG_CHECKING([for RTAI pipes])
            case "${CONFIG_RTAI_OPT_NATIVE_PIPE}" in
                y) AC_MSG_RESULT([${CONFIG_RTAI_OPT_NATIVE_PIPE}]) ;;
                *) AC_MSG_ERROR([*** Please enable pipes in RTAI]) ;;
            esac

            # check for mutexes, needed by rtnet
            AC_MSG_CHECKING([for RTAI mutexes])
            case "${CONFIG_RTAI_OPT_NATIVE_MUTEX}" in
                y) AC_MSG_RESULT([${CONFIG_RTAI_OPT_NATIVE_MUTEX}]) ;;
                *) AC_MSG_ERROR([*** Please enable mutexes in RTAI]) ;;
            esac

            ;;

        *)              # ---> oops <----
            AC_MSG_ERROR([*** internal error])
            ;;
    esac

# get linux kernel tree and version
    AC_MSG_CHECKING([for Linux source tree])
    RTAI_LINUX_DIR="`${RTAI_CONFIG} --linux-dir`"
    case "$CONFIG_RTNET_RTAI_3_VERSION" in
        3.0 | 3.0r1 | 3.0r2)

            eval linux_`grep '^EXTRAVERSION =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
            eval linux_`grep '^PATCHLEVEL =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
            eval linux_`grep '^SUBLEVEL =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
            eval linux_`grep '^VERSION =' $RTAI_LINUX_DIR/Makefile | sed -e 's, ,,g'`
            RTAI_LINUX_VERSION="$linux_VERSION.$linux_PATCHLEVEL.$linux_SUBLEVEL.$linux_EXTRAVERSION"
            ;;
        *)
            RTAI_LINUX_VERSION="`${RTAI_CONFIG} --linux-version`"
            ;;
    esac

#source (load) the kernel configuration
    . ${RTAI_LINUX_DIR}/.config

    AC_MSG_RESULT([$RTAI_LINUX_DIR (kernel ${RTAI_LINUX_VERSION}${CONFIG_SMP:+-SMP})])
fi

dnl =========== end of rtai 3 detection ==================================


#
# now let's see wich version of rtai we've found
# exit with error if there isn't any version
#
case "$CONFIG_RTNET_RTAI_VERSION" in
    30)
        AC_DEFINE(CONFIG_RTAI_30, 1, [We build for rtai 3.0rx])
        ;;
    31)
        AC_DEFINE(CONFIG_RTAI_31, 1, [We build for rtai 3.1rx])
        ;;
    32)
        AC_DEFINE(CONFIG_RTAI_32, 1, [We build for rtai 3.2rx])
        ;;
    fusion-090)
        AC_DEFINE(CONFIG_FUSION_090, 1, [We build for fusion 0.9.x])
        ;;
    *)
        AC_MSG_ERROR([*** No RTAI found in $with_rtai])
        ;;
esac

AM_CONDITIONAL(CONFIG_FUSION,[test "$CONFIG_RTNET_RTAI_VERSION" = fusion-090])


dnl ======================================================================
dnl             check linux version
dnl ======================================================================

case "${RTAI_LINUX_VERSION}" in
    2.4.*)
        RTAI_LINUX_VERSION_SUBLEVEL="`echo ${RTAI_LINUX_VERSION} | sed -e 's/[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\).*/\1/'`"
        if test ${RTAI_LINUX_VERSION_SUBLEVEL} -le 18; then
            AC_MSG_ERROR([*** Unsupported kernel version $RTAI_LINUX_VERSION - please upgrade at least to 2.4.19])
        fi
        ;;
    2.6.*)
        ;;
    *)
        AC_MSG_ERROR([*** Unsupported kernel version $RTAI_LINUX_VERSION])
        ;;
esac



dnl ======================================================================
dnl             import settings from RTAI
dnl ======================================================================

# kbuild (linux 2.6) or not
case "${RTAI_LINUX_VERSION}" in
    2.4.*)
        unset CONFIG_KBUILD
        ;;
    2.6.*)
        CONFIG_KBUILD=y
        ;;
    *)
        AC_MSG_ERROR([*** internal error])
        ;;
esac
AM_CONDITIONAL(CONFIG_KBUILD,[test "$CONFIG_KBUILD" = y])

# common stuff
case "${CONFIG_RTNET_RTAI_VERSION}" in
    30 | 31 | 32)
        RTAI_USER_CFLAGS="`${RTAI_CONFIG} --lxrt-cflags`"
        CONFIG_RTNET_RTAI_ARCH="`${RTAI_CONFIG} --arch`"
        ;;
    fusion-090)
        RTAI_USER_CFLAGS="`${RTAI_CONFIG} --fusion-cflags`"
        CONFIG_RTNET_RTAI_ARCH="`${RTAI_CONFIG} --arch`"
        RTAI_LIBRARIES="`${RTAI_CONFIG} --library-dir`"
        ;;
    *)
        AC_MSG_ERROR([*** internal error])
        ;;
esac


if test "$CONFIG_KBUILD" = "y"; then
    case "${CONFIG_RTNET_RTAI_VERSION}" in
        30)
            AC_MSG_ERROR([*** internal error])
            ;;
        31 | 32)
            RTAI_KMOD_CFLAGS="`${RTAI_CONFIG} --lxrt-cflags`"
            ;;
        fusion-090)
            RTAI_KMOD_CFLAGS="`${RTAI_CONFIG} --fusion-cflags`"
            ;;
        *)
            AC_MSG_ERROR([*** internal error])
            ;;
    esac
else
    case "${CONFIG_RTNET_RTAI_VERSION}" in
        30 | 31 | 32)
            RTAI_KMOD_CFLAGS="`${RTAI_CONFIG} --module-cflags`"
            ;;
        fusion-090)
            AC_MSG_ERROR([*** internal error])
            ;;
        *)
            AC_MSG_ERROR([*** internal error])
            ;;
    esac
fi


dnl ======================================================================
dnl             architecture tests....
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
        AC_DEFINE(CONFIG_ARCH_I386, 1,[we are building for x86])
        ;;
    ppc)
        AC_DEFINE(CONFIG_ARCH_PPC, 1, [we are building for powerpc])
        ;;
    arm)
        AC_DEFINE(CONFIG_ARCH_ARM, 1, [we are building for arm])
        ;;
#    cris)
#       AC_DEFINE(CONFIG_ARCH_CRIS, 1, [we are trying to build for cris])
#       ;;
    *)
        AC_MSG_ERROR([*** unsupported architecture])
        ;;
esac

AC_SUBST(RTNET_TARGET_ARCH)


dnl ======================================================================
dnl             crosscompiling
dnl ======================================================================

AC_MSG_CHECKING([for CROSS_COMPILE])
if test "${CROSS_COMPILE}" = ""; then
    CROSS_COMPILE="`echo ${CC} | sed s/gcc//`"

    if test "${CROSS_COMPILE}gcc" \!= "${CC}"; then
        CROSS_COMPILE=""
        AC_MSG_RESULT([Cannot identify CROSS_COMPILE - assuming none - set manually if needed])
    else
        if test "${CROSS_COMPILE}" = ""; then
            AC_MSG_RESULT([none])
        else
            AC_MSG_RESULT([${CROSS_COMPILE}])
        fi
    fi
else
    AC_MSG_RESULT([${CROSS_COMPILE}])
fi
AC_SUBST(CROSS_COMPILE)


dnl ======================================================================
dnl             decide to build for 2.4 or 2.6 kernel
dnl ======================================================================

if test x$CONFIG_KBUILD = x; then
    RTNET_MODULE_EXT=.o
    RTNET_KBUILD_ENV=
    RTNET_KBUILD_CMD=
    RTNET_KBUILD_CLEAN=
else
    RTNET_MODULE_EXT=.ko
    RTNET_KBUILD_ENV='src2obj = $(patsubst %.$(1),%.o,$(filter %.$(1),$(call notdir,$(2))))'
    RTNET_KBUILD_CMD="                                                      \
        @for src in \$(filter-out FORCE, \$^); do                           \
            if test \\! -r \`basename \$\$src\`; then                       \
                \$(LN_S) \$\$src;                                           \
            fi;                                                             \
        done;                                                               \
        \$(MAKE) -C $RTAI_LINUX_DIR                                         \
                CROSS_COMPILE=\$(CROSS_COMPILE)                             \
                ARCH=\$(RTNET_TARGET_ARCH)                                  \
                M=\"\`pwd\`\"                                               \
                V=\$(V)                                                     \
                rtai_dir=\"${RTNET_RTAI_PREFIX}\"                           \
                top_srcdir=\"\`cd \$(top_srcdir) && pwd\`\"                 \
                top_builddir=\"\`cd \$(top_builddir) && pwd\`\"             \
                srcdir=\"\`cd \$(srcdir) && pwd\`\"                         \
                build_objs='\$(call src2obj,c,\$^)'                         \
                build_target='\$(basename \$@)'                             \
                modules                                                     \
                "
    RTNET_KBUILD_CLEAN="                                                            \
        @rm -f *.ko *.mod.c .*.cmd *.o;                                     \
        for src in \$^; do                                                  \
            if test -h \`basename \$\$src\`; then                           \
                rm -f \`basename \$\$src\`;                                 \
            fi;                                                             \
        done;                                                               \
        if test -d .tmp_versions; then                                      \
            rm -rf .tmp_versions;                                           \
        fi;                                                                 \
        "
    dnl === Hack to suppress undefined symbol warnings during MODPOST stage ===
    if test -e $RTAI_LINUX_DIR/Module.symvers; then
        mv $RTAI_LINUX_DIR/Module.symvers $RTAI_LINUX_DIR/Module.symvers.let_modpost_be_quiet
    fi
    if test -e $RTAI_LINUX_DIR/vmlinux; then
        mv $RTAI_LINUX_DIR/vmlinux $RTAI_LINUX_DIR/vmlinux.let_modpost_be_quiet
    fi
fi

AC_MSG_CHECKING([for kernel module extension])
AC_MSG_RESULT([$RTNET_MODULE_EXT])

AC_SUBST(RTNET_MODULE_EXT)
AC_SUBST(RTNET_KBUILD_ENV)
AC_SUBST(RTNET_KBUILD_CMD)
AC_SUBST(RTNET_KBUILD_CLEAN)

bs_kmodext=$RTNET_MODULE_EXT
bs_kcompile="make -C $RTAI_LINUX_DIR ARCH=$RTNET_TARGET_ARCH CROSS_COMPILE=$CROSS_COMPILE V=1 M=`pwd` SUBDIRS=`pwd` modules"


dnl ======================================================================
dnl             RTAI header checks
dnl ======================================================================

ac_save_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${ac_save_CPPFLAGS} ${RTAI_KMOD_CFLAGS}"

# Working around some Autoconf ugliness here: names of variables
# stored into the cache are canonicalized, turning /'s into _'s. So if
# you need to match X_Y.h and X/Y.h as different filenames, you are
# toast. Whichever comes first will be cached as "X_Y_h" and the
# second one will never get checked in the fs, but simply
# uncached. This applies to rtai_sem.h and rtai/sem.h for instance.
# To prevent this, we must only check a single set of non-conflicting
# headers, depending on the RTAI version which has been
# identified. (rpm)
if test "${CONFIG_KBUILD}" = "y"; then  # building against linux-2.6
    case "${CONFIG_RTNET_RTAI_VERSION}" in
        30)
            AC_MSG_ERROR([*** internal error])
            ;;
        31 | 32)
            BS_CHECK_KHEADERS([rtai_sem.h rtai_malloc.h rtai_fifos.h],
                [],
                [AC_MSG_ERROR([*** header not found or working, please check RTAI installation])],
                [
#include <linux/kernel.h>
                ])
            ;;
        fusion-090)
            BS_CHECK_KHEADERS([rtai/sem.h rtai/mutex.h rtai/pipe.h rtdm/rtdm.h rtdm/rtdm_driver.h],
                [],
                [AC_MSG_ERROR([*** header not found or working, please check RTAI installation])],
                [
#include <linux/kernel.h>
                ])
            ;;
        *)
            AC_MSG_ERROR([*** internal error])
            ;;
    esac
else                                    # building against linux-2.4
    case "${CONFIG_RTNET_RTAI_VERSION}" in
        30 | 31 | 32)   # mandatory RTAI-3 headers
            AC_CHECK_HEADERS([rtai_sem.h rtai_malloc.h],
            [],
            [AC_MSG_ERROR([*** header not found or working, please check RTAI installation])],
            [
#include <linux/kernel.h>
            ])
            ;;
        fusion-090)
            AC_MSG_ERROR([*** internal error])
            ;;
        *)
            AC_MSG_ERROR([*** internal error])
            ;;
    esac
fi

CPPFLAGS="${ac_save_CFLAGS}"


dnl ======================================================================
dnl             Installation directory for modules
dnl ======================================================================

AC_MSG_CHECKING([for module installation directory])

AC_ARG_WITH(module-dir,
    AS_HELP_STRING([--with-module-dir=<module-dir>], [Installation directory for modules]),
    [case "${withval}" in
    "" | y | ye | yes | n | no)
        AC_MSG_ERROR([You must supply an argument to --with-module-dir.]) ;;
    esac],
    with_module_dir='${exec_prefix}/modules')
RTNET_MODULE_DIR="$with_module_dir"
AC_MSG_RESULT([$RTNET_MODULE_DIR])
AC_SUBST(RTNET_MODULE_DIR)


dnl ======================================================================
dnl             check for LXRT
dnl ======================================================================
AC_MSG_CHECKING([for RTAI 3.x LXRT enabled])
case "${CONFIG_RTNET_RTAI_VERSION}" in
    30 | 31 | 32)
        case "${CONFIG_RTAI_SCHED_LXRT}" in
            y | m) CONFIG_RTNET_LXRT=yes ;;
            *) CONFIG_RTNET_LXRT=no ;;
        esac
        ;;
    fusion-090) CONFIG_RTNET_LXRT=no
        ;;
    *)
        AC_MSG_ERROR([*** internal error])
        ;;
esac
AC_MSG_RESULT([${CONFIG_RTNET_LXRT}])

AC_MSG_CHECKING([for CONFIG_REGPARM activated in Linux Kernel])
if test "${CONFIG_RTNET_LXRT}" = "yes" -a "${CONFIG_REGPARM}" = "y"; then
    AC_MSG_WARN([

*** LXRT interface for RTnet disabled ***
Please unset CONFIG_RTNET_LXRT for
${RTAI_LINUX_DIR}
and rebuild your kernel in order to build LXRT interface for RTNET

])
    CONFIG_RTNET_LXRT=no
else
    AC_MSG_RESULT([${CONFIG_REGPARM:-no}])
fi

AM_CONDITIONAL(CONFIG_RTNET_LXRT,[test "$CONFIG_RTNET_LXRT" = "yes"])
if test "$CONFIG_RTNET_LXRT" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_LXRT, 1, [RTAI LXRT])
fi


dnl ======================================================================
dnl             network interface card driver
dnl ======================================================================

AC_MSG_CHECKING([whether to build all PCI network driver])
AC_ARG_ENABLE(allpci,
    AS_HELP_STRING([--enable-allpci], [build all PCI NIC drivers]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_ALLPCI=yes ;;
        *) CONFIG_RTNET_ALLPCI=no ;;
    esac],
    [CONFIG_RTNET_ALLPCI=no])
AC_MSG_RESULT([${CONFIG_RTNET_ALLPCI}])


AC_MSG_CHECKING([whether to build 3Com 59x driver])
AC_ARG_ENABLE(3c59x,
    AS_HELP_STRING([--enable-3c59x], [build 3Com 59x driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_3C59X=yes ;;
        *) CONFIG_RTNET_3C59X=no ;;
    esac],
    [CONFIG_RTNET_3C59X=no])
# 3Com should be enabled manually, not by allpci
AC_MSG_RESULT([${CONFIG_RTNET_3C59X}])
AM_CONDITIONAL(CONFIG_RTNET_3C59X,[test "$CONFIG_RTNET_3C59X" = "yes"])


AC_MSG_CHECKING([whether to build Realtek 8139 driver])
AC_ARG_ENABLE(8139,
    AS_HELP_STRING([--enable-8139], [build Realtek 8139 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_8139=yes ;;
        *) CONFIG_RTNET_8139=no ;;
    esac],
    [CONFIG_RTNET_8139=no])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
    CONFIG_RTNET_8139=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_8139}])
AM_CONDITIONAL(CONFIG_RTNET_8139,[test "$CONFIG_RTNET_8139" = "yes"])


AC_MSG_CHECKING([whether to build Intel EtherExpressPro/100 driver])
AC_ARG_ENABLE(eepro100,
    AS_HELP_STRING([--enable-eepro100], [build Intel EtherExpressPro/100 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_EEPRO100=yes ;;
        *) CONFIG_RTNET_EEPRO100=no ;;
    esac],
    [CONFIG_RTNET_EEPRO100=no])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
    CONFIG_RTNET_EEPRO100=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_EEPRO100}])
AM_CONDITIONAL(CONFIG_RTNET_EEPRO100,[test "$CONFIG_RTNET_EEPRO100" = "yes"])


AC_MSG_CHECKING([whether to build AMD PCnet32 driver])
AC_ARG_ENABLE(pcnet32,
    AS_HELP_STRING([--enable-pcnet32], [build AMD PCnet32 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_PCNET32=yes ;;
        *) CONFIG_RTNET_PCNET32=no ;;
    esac],
    [CONFIG_RTNET_PCNET32=no])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
    CONFIG_RTNET_PCNET32=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_PCNET32}])
AM_CONDITIONAL(CONFIG_RTNET_PCNET32,[test "$CONFIG_RTNET_PCNET32" = "yes"])


AC_MSG_CHECKING([whether to build VIA Rhine driver])
AC_ARG_ENABLE(via-rhine,
    AS_HELP_STRING([--enable-via-rhine], [build VIA Rhine driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_VIA_RHINE=yes ;;
        *) CONFIG_RTNET_VIA_RHINE=no ;;
    esac],
    [CONFIG_RTNET_VIA_RHINE=no])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
    CONFIG_RTNET_VIA_RHINE=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_VIA_RHINE}])
AM_CONDITIONAL(CONFIG_RTNET_VIA_RHINE,[test "$CONFIG_RTNET_VIA_RHINE" = "yes"])


AC_MSG_CHECKING([whether to build NatSemi driver])
AC_ARG_ENABLE(natsemi,
    AS_HELP_STRING([--enable-natsemi], [build NatSemi driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_NATSEMI=yes ;;
        *) CONFIG_RTNET_NATSEMI=no ;;
    esac],
    [CONFIG_RTNET_NATSEMI=no])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
    CONFIG_RTNET_NATSEMI=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_NATSEMI}])
AM_CONDITIONAL(CONFIG_RTNET_NATSEMI,[test "$CONFIG_RTNET_NATSEMI" = "yes"])


AC_MSG_CHECKING([whether to build tulip driver])
AC_ARG_ENABLE(tulip,
    AS_HELP_STRING([--enable-tulip], [build tulip driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_TULIP=yes ;;
        *) CONFIG_RTNET_TULIP=no ;;
    esac],
    [CONFIG_RTNET_TULIP=no])
if test "$CONFIG_RTNET_ALLPCI" = "yes"; then
    CONFIG_RTNET_TULIP=yes
fi
AC_MSG_RESULT([${CONFIG_RTNET_TULIP}])
AM_CONDITIONAL(CONFIG_RTNET_TULIP,[test "$CONFIG_RTNET_TULIP" = "yes"])


AC_MSG_CHECKING([whether to build fcc enet driver])
AC_ARG_ENABLE(fcc-enet,
    AS_HELP_STRING([--enable-fcc-enet], [build fcc enet driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_FCC_ENET=yes ;;
        *) CONFIG_RTNET_FCC_ENET=no ;;
    esac],
    [CONFIG_RTNET_FCC_ENET=no])
AC_MSG_RESULT([${CONFIG_RTNET_FCC_ENET}])
AM_CONDITIONAL(CONFIG_RTNET_FCC_ENET,[test "$CONFIG_RTNET_FCC_ENET" = "yes"])


AC_MSG_CHECKING([whether to build scc enet driver])
AC_ARG_ENABLE(scc-enet,
    AS_HELP_STRING([--enable-scc-enet], [build scc enet driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_SCC_ENET=yes ;;
        *) CONFIG_RTNET_SCC_ENET=no ;;
    esac],
    [CONFIG_RTNET_SCC_ENET=no])
AC_MSG_RESULT([${CONFIG_RTNET_SCC_ENET}])
AM_CONDITIONAL(CONFIG_RTNET_SCC_ENET,[test "$CONFIG_RTNET_SCC_ENET" = "yes"])


AC_MSG_CHECKING([whether to build fec enet driver])
AC_ARG_ENABLE(fec-enet,
    AS_HELP_STRING([--enable-fec-enet], [build fec enet driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_FEC_ENET=yes ;;
        *) CONFIG_RTNET_FEC_ENET=no ;;
    esac],
    [CONFIG_RTNET_FEC_ENET=no])
AC_MSG_RESULT([${CONFIG_RTNET_FEC_ENET}])
AM_CONDITIONAL(CONFIG_RTNET_FEC_ENET,[test "$CONFIG_RTNET_FEC_ENET" = "yes"])


AC_MSG_CHECKING([whether to build MPC52xx fec driver])
AC_ARG_ENABLE(mpc52xx-fec,
    AS_HELP_STRING([--enable-mpc52xx-fec], [build MPC52xx fec driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_MPC52XX_FEC=yes ;;
        *) CONFIG_RTNET_MPC52XX_FEC=no ;;
    esac],
    [CONFIG_RTNET_MPC52XX_FEC=no])
AC_MSG_RESULT([${CONFIG_RTNET_MPC52XX_FEC}])
AM_CONDITIONAL(CONFIG_RTNET_MPC52XX_FEC,[test "$CONFIG_RTNET_MPC52XX_FEC" = "yes"])


AC_MSG_CHECKING([whether to build SMSC LAN91C111 driver])
AC_ARG_ENABLE(smc91111,
    AS_HELP_STRING([--enable-smc91111], [build SMSC LAN91C111 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_SMC91111=yes ;;
        *) CONFIG_RTNET_SMC91111=no ;;
    esac],
    [CONFIG_RTNET_SMC91111=no])
AC_MSG_RESULT([${CONFIG_RTNET_SMC91111}])
AM_CONDITIONAL(CONFIG_RTNET_SMC91111,[test "$CONFIG_RTNET_SMC91111" = "yes"])


AC_MSG_CHECKING([whether to build loopback driver])
AC_ARG_ENABLE(loopback,
    AS_HELP_STRING([--enable-loopback], [build loopback driver @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_LOOPBACK=no ;;
        *) CONFIG_RTNET_LOOPBACK=yes ;;
    esac],
    [CONFIG_RTNET_LOOPBACK=yes])
AC_MSG_RESULT([${CONFIG_RTNET_LOOPBACK}])
AM_CONDITIONAL(CONFIG_RTNET_LOOPBACK,[test "$CONFIG_RTNET_LOOPBACK" = "yes"])


dnl ======================================================================
dnl             Real-Time IPv4
dnl ======================================================================

AC_MSG_CHECKING([whether to build real-time IPv4 support])
AC_ARG_ENABLE(rtipv4,
    AS_HELP_STRING([--enable-rtipv4], [enable real-time IPv4 support @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_RTIPV4=no ;;
        *) CONFIG_RTNET_RTIPV4=yes ;;
    esac],
    [CONFIG_RTNET_RTIPV4=yes])
AC_MSG_RESULT([${CONFIG_RTNET_RTIPV4}])
AM_CONDITIONAL(CONFIG_RTNET_RTIPV4,[test "$CONFIG_RTNET_RTIPV4" = "yes"])
if test "$CONFIG_RTNET_RTIPV4" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_RTIPV4,1,[Real-Time IPv4 support])
fi

AC_MSG_CHECKING([whether to enable IP network routing])
AC_ARG_ENABLE(net-routing,
    AS_HELP_STRING([--enable-net-routing], [enable IP network routing @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_NETWORK_ROUTING=yes ;;
        *) CONFIG_RTNET_NETWORK_ROUTING=no ;;
    esac],
    [CONFIG_RTNET_NETWORK_ROUTING=no])
AC_MSG_RESULT([${CONFIG_RTNET_NETWORK_ROUTING}])
if test "$CONFIG_RTNET_NETWORK_ROUTING" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_NETWORK_ROUTING, 1, [Network routing])
fi

AC_MSG_CHECKING([whether to enable IP router support])
AC_ARG_ENABLE(router,
    AS_HELP_STRING([--enable-router], [enable IP router @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_ROUTER=yes ;;
        *) CONFIG_RTNET_ROUTER=no ;;
    esac],
    [CONFIG_RTNET_ROUTER=no])
AC_MSG_RESULT([${CONFIG_RTNET_ROUTER}])
if test "$CONFIG_RTNET_ROUTER" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_ROUTER, 1, [Router])
fi


dnl ======================================================================
dnl             Real-Time AF_PACKET
dnl ======================================================================

AC_MSG_CHECKING([whether to build real-time packet socket support])
AC_ARG_ENABLE(rtpacket,
    AS_HELP_STRING([--enable-rtpacket], [build real-time packet socket support @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_RTPACKET=no ;;
        *) CONFIG_RTNET_RTPACKET=yes ;;
    esac],
    [CONFIG_RTNET_RTPACKET=yes])
AC_MSG_RESULT([${CONFIG_RTNET_RTPACKET}])
AM_CONDITIONAL(CONFIG_RTNET_RTPACKET,[test "$CONFIG_RTNET_RTPACKET" = "yes"])


dnl ======================================================================
dnl             RTcfg
dnl ======================================================================

AC_MSG_CHECKING([whether to build RTcfg])
AC_ARG_ENABLE(rtcfg,
    AS_HELP_STRING([--enable-rtcfg], [build RTcfg @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_RTCFG=no ;;
        *) CONFIG_RTNET_RTCFG=yes ;;
    esac],
    [CONFIG_RTNET_RTCFG=yes])
AC_MSG_RESULT([${CONFIG_RTNET_RTCFG}])
AM_CONDITIONAL(CONFIG_RTNET_RTCFG,[test "$CONFIG_RTNET_RTCFG" = "yes"])

AC_MSG_CHECKING([whether to enable RTcfg debug support])
AC_ARG_ENABLE(rtcfg-dbg,
    AS_HELP_STRING([--enable-rtcfg-dbg], [enable RTcfg debugging @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTCFG_DEBUG=yes ;;
        *) CONFIG_RTNET_RTCFG_DEBUG=no ;;
    esac],
    [CONFIG_RTNET_RTCFG_DEBUG=no])
AC_MSG_RESULT([${CONFIG_RTNET_RTCFG_DEBUG}])
if test "$CONFIG_RTNET_RTCFG_DEBUG" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_RTCFG_DEBUG,1,[RTcfg debug])
fi


dnl ======================================================================
dnl             RTcap
dnl ======================================================================

AC_MSG_CHECKING([whether to enable RTcap support])
AC_ARG_ENABLE(rtcap,
    AS_HELP_STRING([--enable-rtcap], [enable RTcap support and build capturing module @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTCAP=yes ;;
        *) CONFIG_RTNET_RTCAP=no ;;
    esac],
    [CONFIG_RTNET_RTCAP=no])
AC_MSG_RESULT([${CONFIG_RTNET_RTCAP}])
AM_CONDITIONAL(CONFIG_RTNET_RTCAP,[test "$CONFIG_RTNET_RTCAP" = "yes"])
if test "$CONFIG_RTNET_RTCAP" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_RTCAP,1,[RTcap support])
fi


dnl ======================================================================
dnl             rtnetproxy
dnl ======================================================================

AC_MSG_CHECKING([whether to build rtnetproxy])
AC_ARG_ENABLE(proxy,
    AS_HELP_STRING([--enable-proxy], [build IP protocol proxy driver (legacy) @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_PROXY=yes ;;
        *) CONFIG_RTNET_PROXY=no ;;
    esac],
    [CONFIG_RTNET_PROXY=no])
AC_MSG_RESULT([${CONFIG_RTNET_PROXY}])
AM_CONDITIONAL(CONFIG_RTNET_PROXY,[test "$CONFIG_RTNET_PROXY" = "yes"])
if test "$CONFIG_RTNET_PROXY" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_PROXY,1,[rtnetproxy support])
fi


dnl ======================================================================
dnl             TDMA discipline for RTmac
dnl ======================================================================

AC_MSG_CHECKING([whether to build TDMA])
AC_ARG_ENABLE(tdma,
    AS_HELP_STRING([--enable-tdma], [build TDMA discipline for RTmac @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_TDMA=no ;;
        *) CONFIG_RTNET_TDMA=yes ;;
    esac],
    [CONFIG_RTNET_TDMA=yes])
AC_MSG_RESULT([${CONFIG_RTNET_TDMA}])
AM_CONDITIONAL(CONFIG_RTNET_TDMA,[test "$CONFIG_RTNET_TDMA" = "yes"])

AC_MSG_CHECKING([whether to enable TDMA master support])
AC_ARG_ENABLE(tdma-master,
    AS_HELP_STRING([--enable-tdma-master], [enable TDMA master support @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_TDMA_MASTER=no ;;
        *) CONFIG_RTNET_TDMA_MASTER=yes ;;
    esac],
    [CONFIG_RTNET_TDMA_MASTER=yes])
AC_MSG_RESULT([${CONFIG_RTNET_TDMA_MASTER}])
if test "$CONFIG_RTNET_TDMA_MASTER" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_TDMA_MASTER,1,[TDMA master support])
fi


dnl ======================================================================
dnl             NoMAC discipline for RTmac
dnl ======================================================================

AC_MSG_CHECKING([whether to build NoMAC])
AC_ARG_ENABLE(nomac,
    AS_HELP_STRING([--enable-nomac], [build NoMAC discipline for RTmac @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_NOMAC=yes ;;
        *) CONFIG_RTNET_NOMAC=no ;;
    esac],
    [CONFIG_RTNET_NOMAC=no])
AC_MSG_RESULT([${CONFIG_RTNET_NOMAC}])
AM_CONDITIONAL(CONFIG_RTNET_NOMAC,[test "$CONFIG_RTNET_NOMAC" = "yes"])


dnl ======================================================================
dnl             Enclosed RTDM (RTAI/classic)
dnl ======================================================================

AC_MSG_CHECKING([whether to build enclosed RTDM])
AC_ARG_ENABLE(enclosed-rtdm,
    AS_HELP_STRING([--enable-enclosed-rtdm], [built enclosed RTDM module @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_ENCLOSED_RTAI_RTDM=no ;;
        *) CONFIG_RTNET_ENCLOSED_RTAI_RTDM=yes ;;
    esac],
    [CONFIG_RTNET_ENCLOSED_RTAI_RTDM=yes])
case "${CONFIG_RTNET_RTAI_VERSION}" in
    fusion-090)
        CONFIG_RTNET_ENCLOSED_RTAI_RTDM=no ;;
esac
AC_MSG_RESULT([${CONFIG_RTNET_ENCLOSED_RTAI_RTDM}])
AM_CONDITIONAL(CONFIG_RTNET_ENCLOSED_RTAI_RTDM,[test "$CONFIG_RTNET_ENCLOSED_RTAI_RTDM" = "yes"])
if test "$CONFIG_RTNET_ENCLOSED_RTAI_RTDM" = "yes"; then
    AC_CONFIG_LINKS(stack/include/rtdm.h:rtai_rtdm/rtdm.h)
    AC_CONFIG_LINKS(stack/include/rtdm_driver.h:rtai_rtdm/rtdm_driver.h)
else
    rm -f stack/include/rtdm.h stack/include/rtdm_driver.h
fi


dnl ======================================================================
dnl             RTDM select (should become a compilation parameter someday)
dnl ======================================================================

AC_MSG_CHECKING([whether to enable RTDM select])
AC_ARG_ENABLE(rtdm-select,
    AS_HELP_STRING([--enable-rtdm-select], [enable select function for RTDM (experimental) @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTDM_SELECT=yes ;;
        *) CONFIG_RTNET_RTDM_SELECT=no ;;
    esac],
    [CONFIG_RTNET_RTDM_SELECT=no])
AC_MSG_RESULT([${CONFIG_RTNET_RTDM_SELECT}])

dnl When RTDM select has matured, the example compilation should no longer
dnl depend on the internal select support by RTnet. Instead, running the
dnl example shall give an error.

AM_CONDITIONAL(CONFIG_RTNET_RTDM_SELECT, [test "$CONFIG_RTNET_RTDM_SELECT" = "yes"])

if test "$CONFIG_RTNET_RTDM_SELECT" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_RTDM_SELECT, 1, [RTDM select support])
fi


dnl ======================================================================
dnl             Examples
dnl ======================================================================

AC_MSG_CHECKING([whether to build examples])
AC_ARG_ENABLE(examples,
    AS_HELP_STRING([--enable-examples], [build examples @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_EXAMPLES=no ;;
        *) CONFIG_RTNET_EXAMPLES=yes ;;
    esac],
    [CONFIG_RTNET_EXAMPLES=yes])
AC_MSG_RESULT([${CONFIG_RTNET_EXAMPLES}])
AM_CONDITIONAL(CONFIG_RTNET_EXAMPLES,[test "$CONFIG_RTNET_EXAMPLES" = "yes"])


dnl ======================================================================
dnl             compilation parameters
dnl ======================================================================

AC_MSG_CHECKING([whether to enable internal bug checks])
AC_ARG_ENABLE(checks,
    AS_HELP_STRING([--enable-checks], [enable internal bug checks @<:@default=yes@:>@]),
    [case "$enableval" in
        n | no) CONFIG_RTNET_CHECKED=no ;;
        *) CONFIG_RTNET_CHECKED=yes ;;
    esac],
    [CONFIG_RTNET_CHECKED=yes])
AC_MSG_RESULT([${CONFIG_RTNET_CHECKED}])
if test "$CONFIG_RTNET_CHECKED" = "yes"; then
    AC_DEFINE(CONFIG_RTNET_CHECKED, 1, [Bug checks])
fi


dnl ======================================================================
dnl             hac^H^H^Hfix problem with multible defined symbols problem
dnl             shamelessly stolen from RTAI-3.1 tnx to Philippe
dnl ======================================================================

dnl CAUTION: We need to have the CONFIG_RTNET_XX symbols always defined when
dnl the configuration header is read, but we want the Autoconf-produced
dnl symbols to be defined only when compiling RTNET. This way, we won't
dnl pollute the namespace with the latter when our configuration header is
dnl indirectly included by a client application. To achieve this, we ask
dnl autoheader to produce the following header structure:
dnl #define CONFIG_XX
dnl #define CONFIG_XX ...
dnl #ifdef __IN_RTNET__
dnl <Autoconf-defined symbols>
dnl #endif /* __IN_RTNET__ */
dnl This is quite a hack since we have to rely on the fact that
dnl all Autoconf-generated symbols are lexicographically sorted
dnl after CONFIG_RTNET_XX ones, but, well...

dnl Use a key which will cause the verbatim string to be put after
dnl all CONFIG_RTNET_XX symbols, but still before any Autoconf-generated
dnl symbol, hence CONFIG_RTNET___.
AH_VERBATIM(CONFIG_RTNET___,[#ifdef __IN_RTNET__])

dnl Now we can close the conditional section, right after all
dnl Autoconf-generated symbols have been listed.
AH_BOTTOM([#endif /* __IN_RTNET__ */])


RTAI_KMOD_CFLAGS="-D__IN_RTNET__ $RTAI_KMOD_CFLAGS"
RTAI_USER_CFLAGS="-D__IN_RTNET__ $RTAI_USER_CFLAGS"
AC_SUBST(RTAI_KMOD_CFLAGS)
AC_SUBST(RTAI_USER_CFLAGS)
AC_SUBST(RTAI_LIBRARIES)


dnl ======================================================================
dnl             create links to Makefiles used by linux-2.6.x
dnl ======================================================================

if test x$CONFIG_KBUILD = xy; then
    for DIR in \
        addons/examples/frag-ip \
        addons/examples/raw-packets \
        addons/examples/round-trip-time/client \
        addons/examples/round-trip-time/server \
        addons/examples/rtmac/event/collector \
        addons/examples/rtmac/event/handler \
        addons/examples/rtmac/netshm \
        addons/examples/rtmac/netshm/kernel-demo \
        addons/examples/rtmac/round-trip-time/client \
        addons/examples/rtmac/round-trip-time/multi-client \
        addons/examples/rtmac/round-trip-time/parport-client \
        addons/examples/rtmac/round-trip-time/server \
        addons/examples/select \
        \
        addons/rtcap \
        addons/rtnetproxy \
        \
        drivers/8139too \
        drivers/eepro100 \
        drivers/loopback \
        drivers/mpc52xx_fec \
        drivers/mpc8260_fcc_enet \
        drivers/mpc8xx_enet \
        drivers/mpc8xx_fec \
        drivers/natsemi \
        drivers/pcnet32 \
        drivers/smc91111 \
        drivers/tulip \
        drivers/via-rhine \
        drivers/experimental/3c59x \
        \
        rtai_rtdm \
        \
        stack \
        stack/packet \
        stack/ipv4 \
        stack/rtcfg \
        stack/rtmac \
        stack/rtmac/nomac \
        stack/rtmac/tdma \
        ; do
            AC_CONFIG_LINKS(${DIR}/Makefile:${DIR}/Makefile.kbuild)
        done
fi


dnl ======================================================================
dnl             generate output
dnl ======================================================================

AC_CONFIG_FILES([ \
    GNUmakefile \
    \
    Documentation/GNUmakefile \
    \
    addons/GNUmakefile \
    addons/examples/GNUmakefile \
    addons/examples/frag-ip/GNUmakefile \
    addons/examples/fusion/GNUmakefile \
    addons/examples/lxrt/GNUmakefile \
    addons/examples/raw-packets/GNUmakefile \
    addons/examples/round-trip-time/GNUmakefile \
    addons/examples/round-trip-time/client/GNUmakefile \
    addons/examples/round-trip-time/linux-client/GNUmakefile \
    addons/examples/round-trip-time/server/GNUmakefile \
    addons/examples/rtmac/GNUmakefile \
    addons/examples/rtmac/event/GNUmakefile \
    addons/examples/rtmac/event/collector/GNUmakefile \
    addons/examples/rtmac/event/handler/GNUmakefile \
    addons/examples/rtmac/netshm/GNUmakefile \
    addons/examples/rtmac/netshm/kernel-demo/GNUmakefile \
    addons/examples/rtmac/round-trip-time/GNUmakefile \
    addons/examples/rtmac/round-trip-time/client/GNUmakefile \
    addons/examples/rtmac/round-trip-time/multi-client/GNUmakefile \
    addons/examples/rtmac/round-trip-time/parport-client/GNUmakefile \
    addons/examples/rtmac/round-trip-time/server/GNUmakefile \
    addons/examples/select/GNUmakefile \
    \
    addons/rtcap/GNUmakefile \
    addons/rtnetproxy/GNUmakefile \
    \
    drivers/GNUmakefile \
    drivers/8139too/GNUmakefile \
    drivers/eepro100/GNUmakefile \
    drivers/loopback/GNUmakefile \
    drivers/mpc52xx_fec/GNUmakefile \
    drivers/mpc8260_fcc_enet/GNUmakefile \
    drivers/mpc8xx_enet/GNUmakefile \
    drivers/mpc8xx_fec/GNUmakefile \
    drivers/natsemi/GNUmakefile \
    drivers/pcnet32/GNUmakefile \
    drivers/smc91111/GNUmakefile \
    drivers/tulip/GNUmakefile \
    drivers/via-rhine/GNUmakefile \
    drivers/experimental/GNUmakefile \
    drivers/experimental/3c59x/GNUmakefile \
    \
    rtai_rtdm/GNUmakefile \
    \
    stack/GNUmakefile \
    stack/include/GNUmakefile \
    stack/packet/GNUmakefile \
    stack/ipv4/GNUmakefile \
    stack/rtcfg/GNUmakefile \
    stack/rtmac/GNUmakefile \
    stack/rtmac/nomac/GNUmakefile \
    stack/rtmac/tdma/GNUmakefile \
    \
    tools/GNUmakefile \
    tools/rtnet \
    tools/rtnet.conf \
    ])

AC_OUTPUT

dnl ======================================================================
dnl             generate include/rtnet_config.h
dnl ======================================================================

AC_MSG_NOTICE([creating stack/include/rtnet_config.h])
sed -e "s/ PACKAGE/ RTNET_PACKAGE/" -e "s/ VERSION/ RTNET_VERSION/" \
    config/rtnet_config_pre.h > stack/include/rtnet_config.h.new
if cmp -s stack/include/rtnet_config.h.new stack/include/rtnet_config.h; then
    rm stack/include/rtnet_config.h.new
    AC_MSG_NOTICE([stack/include/rtnet_config.h is unchanged])
else
    mv stack/include/rtnet_config.h.new stack/include/rtnet_config.h
fi
