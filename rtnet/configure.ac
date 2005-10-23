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

AM_INIT_AUTOMAKE([foreign no-exeext dist-bzip2 no-dist-gzip])
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


dnl ======================================================================
dnl             Load the RTnet configuration.
dnl ======================================================================

RTNET_KCONFIG_FILE=$srcdir/defconfig
kconfig_type=default

AC_ARG_WITH(kconfig-file,
    AS_HELP_STRING([--with-kconfig-file=<kconfig-file>], [KConfig parameter file]),
    [
        case "$withval" in
        "" | y | ye | yes | n | no)
            AC_MSG_ERROR([You must supply an argument to --with-kconfig-file.])
          ;;
        esac
        RTNET_KCONFIG_FILE="$withval"
        kconfig_type=supplied
    ])

AC_MSG_CHECKING([for RTnet Kconfig file])
if test \! -f $RTNET_KCONFIG_FILE; then
    AC_MSG_ERROR([Kconfig-file $RTNET_KCONFIG_FILE not found -- --with-kconfig-file sets Kconfig file])
fi
. $RTNET_KCONFIG_FILE
AC_MSG_RESULT([$RTNET_KCONFIG_FILE ($kconfig_type)])


dnl ======================================================================
dnl             RT-extension sources
dnl ======================================================================

AC_ARG_WITH(rtext,
    AS_HELP_STRING([--with-rtext=RT-EXTENSION], [installation path of RTAI or Xenomai]),
    [case "${withval}" in
        "" | y | ye | yes | n | no)
            AC_MSG_ERROR([You must supply an argument to --with-rtext.]) ;;
        *)
            CONFIG_RTEXT_DIR=${withval} ;;
    esac])


dnl ======================================================================
dnl             RT-FireWire sources
dnl ======================================================================
AC_ARG_WITH(rtfw,
    AS_HELP_STRING([--with-rtfw=RTFW], [path of RT-FireWire source code]),
    [case "${withval}" in
        "" | y | ye | yes | n | no)
            AC_MSG_ERROR([You must supply an argument to --with-rtfw.]) ;;
        *)
            CONFIG_RTFIREWIRE_DIR=${withval} ;;
    esac])


dnl ======================================================================
dnl             try to find RT-Extension
dnl ======================================================================

AC_MSG_CHECKING([for RT-extension installation])
RTAI_CONFIG="${CONFIG_RTEXT_DIR}/bin/rtai-config"
if test -x "${RTAI_CONFIG}"; then
    RTAI_DIR="`cd ${CONFIG_RTEXT_DIR} && pwd`"   # let the shell normalize the path
    XENO_DIR=
    AC_MSG_RESULT([${RTAI_DIR} (RTAI)])
else
    XENO_CONFIG="${CONFIG_RTEXT_DIR}/bin/xeno-config"
    if test -x "${XENO_CONFIG}"; then
        RTAI_DIR=
        XENO_DIR="`cd ${CONFIG_RTEXT_DIR} && pwd`"   # let the shell normalize the path
        AC_MSG_RESULT([${XENO_DIR} (Xenomai)])
    else
        AC_MSG_ERROR([*** No RT-extension found in $CONFIG_RTEXT_DIR])
    fi
fi

#
# from now we think we've found some RT-extension, so if we got an error
# it will be fatal, thus exit with AC_MSG_ERROR
#

if test "$RTAI_DIR" \!= ""; then
    #
    # check if RTAI is installed with DESTDIR
    # e.g.: 'make DESTDIR=/foo install'
    #
    AC_MSG_CHECKING([for RTAI DESTDIR])
    RTNET_RTEXT_PREFIX="`cd \`${RTAI_CONFIG} --prefix\` && pwd`"        # let the shell normalize the path
    if test "${RTNET_RTEXT_PREFIX}" \!= "${RTAI_DIR}"; then
        RTAI_DESTDIR="`cd \`echo ${RTAI_DIR} | sed -e s,${RTNET_RTEXT_PREFIX}$,,\` && pwd`"
        export DESTDIR="${RTAI_DESTDIR}"

        RTNET_RTEXT_PREFIX="`cd \`${RTAI_CONFIG} --prefix\` && pwd`"    # let the shell normalize the path
        if test "${RTNET_RTEXT_PREFIX}" = "${RTAI_DIR}"; then
            AC_MSG_RESULT([${DESTDIR}])
        else
            AC_MSG_ERROR([*** Cannot identify DESTDIR])
        fi
    else
        AC_MSG_RESULT([none])
    fi

    AC_MSG_CHECKING([for RTAI version])
    RTAI_VERSION="`${RTAI_CONFIG} --version`"

    case "$RTAI_VERSION" in
        magma)
            CONFIG_RTNET_RTEXT_VERSION=rtai-3x
            AC_DEFINE(CONFIG_RTAI_3x, 1, [We build for RTAI 3.x])
            ;;
        *)
            AC_MSG_ERROR([*** Unsupported RTAI version $RTAI_VERSION in $RTAI_DIR])
            ;;
    esac
    AC_MSG_RESULT([${RTAI_VERSION}])

    AC_MSG_CHECKING([for RTAI config file])
    RTAI_CONFIG_FILE="${RTAI_DIR}/share/rtai/config-rtai-${RTAI_VERSION}"
    if test \! -f ${RTAI_CONFIG_FILE}; then
        RTAI_CONFIG_FILE="`${RTAI_CONFIG} --config`"
        if test \! -f $RTAI_CONFIG_FILE; then
            AC_MSG_ERROR([*** RTAI configuration file not found])
        fi
    fi
    AC_MSG_RESULT([${RTAI_CONFIG_FILE}])

    # source (load) the rtai configuration
    . ${RTAI_CONFIG_FILE}

elif test "$XENO_DIR" \!= ""; then
    #
    # check if Xenomai is installed with DESTDIR
    # e.g.: 'make DESTDIR=/foo install'
    #
    AC_MSG_CHECKING([for Xenomai DESTDIR])
    RTNET_RTEXT_PREFIX="`cd \`${XENO_CONFIG} --prefix\` && pwd`"        # let the shell normalize the path
    if test "${RTNET_RTEXT_PREFIX}" \!= "${XENO_DIR}"; then
        XENO_DESTDIR="`cd \`echo ${XENO_DIR} | sed -e s,${RTNET_RTEXT_PREFIX}$,,\` && pwd`"
        export DESTDIR="${XENO_DESTDIR}"

        RTNET_RTEXT_PREFIX="`cd \`${XENO_CONFIG} --prefix\` && pwd`"    # let the shell normalize the path
        if test "${RTNET_RTEXT_PREFIX}" = "${XENO_DIR}"; then
            AC_MSG_RESULT([${DESTDIR}])
        else
            AC_MSG_ERROR([*** Cannot identify DESTDIR])
        fi
    else
        AC_MSG_RESULT([none])
    fi

    AC_MSG_CHECKING([for Xenomai version])
    XENO_VERSION="`${XENO_CONFIG} --version`"

    case "$XENO_VERSION" in
        1.9.9 | 2.*)
            CONFIG_RTNET_RTEXT_VERSION=xeno-2x
            AC_DEFINE(CONFIG_XENO_2x, 1, [We build for Xenomai 2.x])
            ;;
        *)
            AC_MSG_ERROR([*** Unsupported Xenomai version $XENO_VERSION in $XENO_DIR])
            ;;
    esac
    AC_MSG_RESULT([${XENO_VERSION}])

    AC_MSG_CHECKING([for Xenomai config file])
    XENO_CONFIG_FILE="`${XENO_CONFIG} --config`"
    if test \! -f $XENO_CONFIG_FILE; then
        AC_MSG_ERROR([*** Xenomai configuration file not found])
    fi
    AC_MSG_RESULT([${XENO_CONFIG_FILE}])

    # source (load) the Xenomai configuration
    . ${XENO_CONFIG_FILE}

else
    AC_MSG_ERROR([*** internal error])
fi

AM_CONDITIONAL(CONFIG_RTAI,[test "$RTAI_DIR" \!= ""])
AM_CONDITIONAL(CONFIG_XENO,[test "$XENO_DIR" \!= ""])

case "$CONFIG_RTNET_RTEXT_VERSION" in
    rtai-3x) # ---> RTAI 3.x <---

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

        # get linux kernel tree and version
        AC_MSG_CHECKING([for Linux source tree])
        RTEXT_LINUX_DIR="`${XENO_CONFIG} --linux-dir`"
        RTEXT_LINUX_VERSION="`${XENO_CONFIG} --linux-version`"

        ;;

    xeno-2x) # ---> Xenomai 2.x <---

        # check for RTDM skin, needed by RTnet
        AC_MSG_CHECKING([for RTDM skin])
        case "${CONFIG_XENO_SKIN_RTDM}" in
            y) AC_MSG_RESULT([${CONFIG_XENO_SKIN_RTDM}]) ;;
            *) AC_MSG_ERROR([*** Please enable RTDM skin]) ;;
        esac

        # check for pipes, needed by rtnet
#        AC_MSG_CHECKING([for RTAI pipes])
#        case "${CONFIG_RTAI_OPT_NATIVE_PIPE}" in
#            y) AC_MSG_RESULT([${CONFIG_RTAI_OPT_NATIVE_PIPE}]) ;;
#            *) AC_MSG_ERROR([*** Please enable pipes in RTAI]) ;;
#        esac

        # get linux kernel tree and version
        AC_MSG_CHECKING([for Linux source tree])
        RTEXT_LINUX_DIR="`${XENO_CONFIG} --linux-dir`"
        RTEXT_LINUX_VERSION="`${XENO_CONFIG} --linux-version`"

        ;;

    *)              # ---> oops <----
        AC_MSG_ERROR([*** internal error])
        ;;
esac

#source (load) the kernel configuration
. ${RTEXT_LINUX_DIR}/.config

AC_MSG_RESULT([$RTEXT_LINUX_DIR (kernel ${RTEXT_LINUX_VERSION}${CONFIG_SMP:+-SMP})])

dnl =========== end of RT-extension detection ============================


dnl ======================================================================
dnl =========== try to find rt-firewire ==================================
dnl ======================================================================
#
# the presence of ieee1394.h is a good indication for RT-FireWire
#
AC_MSG_CHECKING([for RT-FireWire source])
IEEE1394_H="${CONFIG_RTFIREWIRE_DIR}/include/ieee1394.h"
if test -f ${IEEE1394_H}; then
    RTNET_RTFW_PREFIX="`echo ${CONFIG_RTFIREWIRE_DIR} | sed -e s,[/]*$,,`"  # strip trailing slashes
    AC_MSG_RESULT([${RTNET_RTFW_PREFIX}])
else
    AC_MSG_RESULT([none])
fi


dnl ======================================================================
dnl             check linux version
dnl ======================================================================

case "${RTEXT_LINUX_VERSION}" in
    2.4.*)
        RTEXT_LINUX_VERSION_SUBLEVEL=
            "`echo ${RTEXT_LINUX_VERSION} | sed -e 's/[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\).*/\1/'`"
        if test ${RTEXT_LINUX_VERSION_SUBLEVEL} -le 18; then
            AC_MSG_ERROR([*** Unsupported kernel version $RTEXT_LINUX_VERSION - please upgrade at least to 2.4.19])
        fi
        ;;
    2.6.*)
        ;;
    *)
        AC_MSG_ERROR([*** Unsupported kernel version $RTEXT_LINUX_VERSION])
        ;;
esac



dnl ======================================================================
dnl             import settings from RT-extensions
dnl ======================================================================

# kbuild (linux 2.6) or not
case "${RTEXT_LINUX_VERSION}" in
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
case "${CONFIG_RTNET_RTEXT_VERSION}" in
    rtai-3x)
        RTEXT_USER_CFLAGS="`${RTAI_CONFIG} --lxrt-cflags`"
        CONFIG_RTNET_RTEXT_ARCH="`${RTAI_CONFIG} --arch`"
        ;;
    xeno-2x)
        RTEXT_USER_CFLAGS="`${XENO_CONFIG} --xeno-cflags`"
        CONFIG_RTNET_RTEXT_ARCH="`${XENO_CONFIG} --arch`"
        RTEXT_LIBRARIES="`${XENO_CONFIG} --library-dir`"
        RTNET_RTEXT_SYMBOLS="`${XENO_CONFIG} --symbol-dir`"
        ;;
    *)
        AC_MSG_ERROR([*** internal error])
        ;;
esac


if test "$CONFIG_KBUILD" = "y"; then
    case "${CONFIG_RTNET_RTEXT_VERSION}" in
        rtai-3x)
            RTEXT_KMOD_CFLAGS="`${RTAI_CONFIG} --module-cflags`"
            ;;
        xeno-2x)
            RTEXT_KMOD_CFLAGS="`${XENO_CONFIG} --module-cflags`"
            ;;
        *)
            AC_MSG_ERROR([*** internal error])
            ;;
    esac
else
    case "${CONFIG_RTNET_RTEXT_VERSION}" in
        rtai-3x)
            RTEXT_KMOD_CFLAGS="`${RTAI_CONFIG} --module-cflags`"
            ;;
        xeno-2x)
            AC_MSG_ERROR([*** internal error])
            ;;
        *)
            AC_MSG_ERROR([*** internal error])
            ;;
    esac
fi


dnl ======================================================================
dnl            import RT-FireWire includes
dnl ======================================================================

RTFW_KMOD_CFLAGS="-I${RTNET_RTFW_PREFIX}/include"


dnl ======================================================================
dnl             architecture tests....
dnl ======================================================================

AC_MSG_CHECKING([for RT-extension target arch])
AC_MSG_RESULT([${CONFIG_RTNET_RTEXT_ARCH}])

AC_MSG_CHECKING([for RTnet target arch])
AC_MSG_RESULT([${RTNET_TARGET_ARCH}])

if test x"${RTNET_TARGET_ARCH}" != x"${CONFIG_RTNET_RTEXT_ARCH}"; then
    AC_MSG_ERROR([*** RT-extension and RTnet target architectures are not the same])
fi

case "${CONFIG_RTNET_RTEXT_ARCH}" in
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
    RTNET_KBUILD_DISTCLEAN=
else
    RTNET_MODULE_EXT=.ko
    RTNET_KBUILD_ENV='src2obj = $(patsubst %.$(1),%.o,$(filter %.$(1),$(call notdir,$(2))))'
    RTNET_KBUILD_CMD="                                                      \
        @for src in \$(filter-out FORCE, \$^); do                           \
            if test \\! -r \`basename \$\$src\`; then                       \
                \$(LN_S) \$\$src;                                           \
            fi;                                                             \
        done;                                                               \
        \$(MAKE) -C $RTEXT_LINUX_DIR                                        \
                CROSS_COMPILE=\$(CROSS_COMPILE)                             \
                ARCH=\$(RTNET_TARGET_ARCH)                                  \
                M=\"\`pwd\`\"                                               \
                V=\$(V)                                                     \
                rtfw_dir=\"${RTNET_RTFW_PREFIX}\"                           \
                rtext_dir=\"${RTNET_RTEXT_PREFIX}\"                         \
                top_srcdir=\"\`cd \$(top_srcdir) && pwd\`\"                 \
                top_builddir=\"\`cd \$(top_builddir) && pwd\`\"             \
                srcdir=\"\`cd \$(srcdir) && pwd\`\"                         \
                build_objs='\$(call src2obj,c,\$^)'                         \
                build_target='\$(basename \$@)'                             \
                MODVERDIR=\"\`cd \$(top_builddir) && pwd\`/symbols\"        \
                modules                                                     \
                "
    RTNET_KBUILD_CLEAN="                                                    \
        @rm -f *.ko *.mod.c .*.cmd *.o;                                     \
        for src in \$^; do                                                  \
            if test -h \`basename \$\$src\`; then                           \
                rm -f \`basename \$\$src\`;                                 \
            fi;                                                             \
        done;                                                               \
        "
    RTNET_KBUILD_DISTCLEAN="                                                \
        @for mod in \${OBJS}; do                                            \
            rm -f \`cd \$(top_builddir) && pwd\`/symbols/\`echo \$\$mod |   \
                sed -e's/${RTNET_MODULE_EXT}//'\`.mod;                      \
        done;                                                               \
        "
    dnl === Hack to suppress undefined symbol warnings during MODPOST stage ===
    case "${CONFIG_RTNET_RTEXT_VERSION}" in
        rtai-3x)
            if test -e $RTEXT_LINUX_DIR/Module.symvers; then
                mv $RTEXT_LINUX_DIR/Module.symvers $RTEXT_LINUX_DIR/Module.symvers.let_modpost_be_quiet
            fi
            if test -e $RTEXT_LINUX_DIR/vmlinux; then
                mv $RTEXT_LINUX_DIR/vmlinux $RTEXT_LINUX_DIR/vmlinux.let_modpost_be_quiet
            fi
            ;;
    esac
fi

AC_MSG_CHECKING([for kernel module extension])
AC_MSG_RESULT([$RTNET_MODULE_EXT])

AC_SUBST(RTNET_MODULE_EXT)
AC_SUBST(RTNET_KBUILD_ENV)
AC_SUBST(RTNET_KBUILD_CMD)
AC_SUBST(RTNET_KBUILD_CLEAN)
AC_SUBST(RTNET_KBUILD_DISTCLEAN)

bs_kmodext=$RTNET_MODULE_EXT
bs_kcompile="make -C $RTEXT_LINUX_DIR ARCH=$RTNET_TARGET_ARCH CROSS_COMPILE=$CROSS_COMPILE V=1 M=`pwd` SUBDIRS=`pwd` modules"


dnl ======================================================================
dnl             header checks
dnl ======================================================================

ac_save_CPPFLAGS="${CPPFLAGS}"
CPPFLAGS="${ac_save_CPPFLAGS} ${RTEXT_KMOD_CFLAGS}"

# Working around some Autoconf ugliness here: names of variables
# stored into the cache are canonicalized, turning /'s into _'s. So if
# you need to match X_Y.h and X/Y.h as different filenames, you are
# toast. Whichever comes first will be cached as "X_Y_h" and the
# second one will never get checked in the fs, but simply
# uncached. This applies to rtai_sem.h and rtai/sem.h for instance.
# To prevent this, we must only check a single set of non-conflicting
# headers, depending on the RTAI version which has been
# identified. (rpm)

BS_CHECK_KHEADERS([rtdm/rtdm.h rtdm/rtdm_driver.h],
    [],
    [AC_MSG_ERROR([*** header not found or working, please check RTAI installation])],
    [
#include <linux/kernel.h>
    ])

if test "${CONFIG_KBUILD}" = "y"; then  # building against linux-2.6
    case "${CONFIG_RTNET_RTEXT_VERSION}" in
        rtai-3x)
            BS_CHECK_KHEADERS([rtai_sem.h rtai_malloc.h rtai_fifos.h],
                [],
                [AC_MSG_ERROR([*** header not found or working, please check RTAI installation])],
                [
#include <linux/kernel.h>
                ])
            ;;
        xeno-2x)
            # nothing further to do
            ;;
        *)
            AC_MSG_ERROR([*** internal error])
            ;;
    esac
else                                    # building against linux-2.4
    case "${CONFIG_RTNET_RTEXT_VERSION}" in
        rtai-3x)    # mandatory RTAI-3 headers
            AC_CHECK_HEADERS([rtai_sem.h rtai_malloc.h],
            [],
            [AC_MSG_ERROR([*** header not found or working, please check RTAI installation])],
            [
#include <linux/kernel.h>
            ])
            ;;
        xeno-2x)
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
case "${CONFIG_RTNET_RTEXT_VERSION}" in
    rtai-3x)
        AC_MSG_CHECKING([for RTAI 3.x LXRT enabled])
        case "${CONFIG_RTAI_SCHED_LXRT}" in
            y | m) CONFIG_RTNET_LXRT=y ;;
            *) CONFIG_RTNET_LXRT=n ;;
        esac
        AC_MSG_RESULT([${CONFIG_RTNET_LXRT}])

        AC_MSG_CHECKING([for CONFIG_REGPARM activated in Linux Kernel])
        if test "${CONFIG_RTNET_LXRT}" = "y" -a "${CONFIG_REGPARM}" = "y"; then
            AC_MSG_WARN([

        *** LXRT interface for RTnet disabled ***
        Please unset CONFIG_RTNET_LXRT for
        ${RTEXT_LINUX_DIR}
        and rebuild your kernel in order to build LXRT interface for RTNET

        ])
            CONFIG_RTNET_LXRT=n
        else
            AC_MSG_RESULT([${CONFIG_REGPARM:-n}])
        fi
        ;;
    xeno-2x) CONFIG_RTNET_LXRT=n
        ;;
    *)
        AC_MSG_ERROR([*** internal error])
        ;;
esac

AM_CONDITIONAL(CONFIG_RTNET_LXRT,[test "$CONFIG_RTNET_LXRT" = "y"])
if test "$CONFIG_RTNET_LXRT" = "y"; then
    AC_DEFINE(CONFIG_RTNET_LXRT, 1, [RTAI LXRT])
fi


dnl ======================================================================
dnl             network interface card driver
dnl ======================================================================

AC_MSG_CHECKING([whether to build Realtek 8139 driver])
AC_ARG_ENABLE(8139,
    AS_HELP_STRING([--enable-8139], [build Realtek 8139 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_8139=y ;;
        *) CONFIG_RTNET_DRV_8139=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_8139:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_8139,[test "$CONFIG_RTNET_DRV_8139" = "y"])


AC_MSG_CHECKING([whether to build Intel EtherExpressPro/100 driver])
AC_ARG_ENABLE(eepro100,
    AS_HELP_STRING([--enable-eepro100], [build Intel EtherExpressPro/100 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_EEPRO100=y ;;
        *) CONFIG_RTNET_DRV_EEPRO100=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_EEPRO100:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_EEPRO100,[test "$CONFIG_RTNET_DRV_EEPRO100" = "y"])

AC_MSG_CHECKING([EEPRO/100 Command Timeout])
AC_ARG_ENABLE(eepro100-cmdtimeout,
    AS_HELP_STRING([--enable-eepro100-cmdtimeout], [Set EEPRO/100 command timeout]),
    [case "$enableval" in
        [[0-9]]*) CONFIGRTNET_DRV_EEPRO100_CMDTIMEOUT=$enableval ;;
        *) AC_MSG_ERROR([Bad argument to option: --enable-eepro100-cmdtimeout=<microseconds>]) ;;
    esac])
if test x$CONFIG_RTNET_DRV_EEPRO100_CMDTIMEOUT = x ; then
    $CONFIG_RTNET_DRV_EEPRO100_CMDTIMEOUT = 20
fi
AC_MSG_RESULT($CONFIG_RTNET_DRV_EEPRO100_CMDTIMEOUT us)
AC_DEFINE_UNQUOTED(CONFIG_RTNET_DRV_EEPRO100_CMDTIMEOUT,
    $CONFIG_RTNET_DRV_EEPRO100_CMDTIMEOUT, [EEPRO/100 command timeout])

AC_MSG_CHECKING([whether to enble EEPRO/100 debugging])
AC_ARG_ENABLE(eepro100-dbg,
    AS_HELP_STRING([--enable-eepro100-dbg], [enable EEPRO/100 debugging]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_EEPRO100_DBG=y ;;
        *) CONFIG_RTNET_DRV_EEPRO100_DBG=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_EEPRO100_DBG:-n}])
if test "$CONFIG_RTNET_DRV_EEPRO100_DBG" = "y"; then
    AC_DEFINE(CONFIG_RTNET_DRV_EEPRO100_DBG, 1, [EEPRO/100 debugging])
fi


AC_MSG_CHECKING([whether to build AMD PCnet32 driver])
AC_ARG_ENABLE(pcnet32,
    AS_HELP_STRING([--enable-pcnet32], [build AMD PCnet32 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_PCNET32=y ;;
        *) CONFIG_RTNET_DRV_PCNET32=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_PCNET32:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_PCNET32,[test "$CONFIG_RTNET_DRV_PCNET32" = "y"])


AC_MSG_CHECKING([whether to build VIA Rhine driver])
AC_ARG_ENABLE(via-rhine,
    AS_HELP_STRING([--enable-via-rhine], [build VIA Rhine driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_VIA_RHINE=y ;;
        *) CONFIG_RTNET_DRV_VIA_RHINE=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_VIA_RHINE:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_VIA_RHINE,[test "$CONFIG_RTNET_DRV_VIA_RHINE" = "y"])


AC_MSG_CHECKING([whether to build NatSemi driver])
AC_ARG_ENABLE(natsemi,
    AS_HELP_STRING([--enable-natsemi], [build NatSemi driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_NATSEMI=y ;;
        *) CONFIG_RTNET_DRV_NATSEMI=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_NATSEMI:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_NATSEMI,[test "$CONFIG_RTNET_DRV_NATSEMI" = "y"])


AC_MSG_CHECKING([whether to build tulip driver])
AC_ARG_ENABLE(tulip,
    AS_HELP_STRING([--enable-tulip], [build tulip driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_TULIP=y ;;
        *) CONFIG_RTNET_DRV_TULIP=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_TULIP:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_TULIP,[test "$CONFIG_RTNET_DRV_TULIP" = "y"])


AC_MSG_CHECKING([whether to build fcc enet driver])
AC_ARG_ENABLE(fcc-enet,
    AS_HELP_STRING([--enable-fcc-enet], [build fcc enet driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_FCC_ENET=y ;;
        *) CONFIG_RTNET_DRV_FCC_ENET=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_FCC_ENET:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_FCC_ENET,[test "$CONFIG_RTNET_DRV_FCC_ENET" = "y"])


AC_MSG_CHECKING([whether to build scc enet driver])
AC_ARG_ENABLE(scc-enet,
    AS_HELP_STRING([--enable-scc-enet], [build scc enet driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_SCC_ENET=y ;;
        *) CONFIG_RTNET_DRV_SCC_ENET=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_SCC_ENET:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_SCC_ENET,[test "$CONFIG_RTNET_DRV_SCC_ENET" = "y"])


AC_MSG_CHECKING([whether to build fec enet driver])
AC_ARG_ENABLE(fec-enet,
    AS_HELP_STRING([--enable-fec-enet], [build fec enet driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_FEC_ENET=y ;;
        *) CONFIG_RTNET_DRV_FEC_ENET=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_FEC_ENET:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_FEC_ENET,[test "$CONFIG_RTNET_DRV_FEC_ENET" = "y"])


AC_MSG_CHECKING([whether to build MPC52xx fec driver])
AC_ARG_ENABLE(mpc52xx-fec,
    AS_HELP_STRING([--enable-mpc52xx-fec], [build MPC52xx fec driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_MPC52XX_FEC=y ;;
        *) CONFIG_RTNET_DRV_MPC52XX_FEC=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_MPC52XX_FEC:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_MPC52XX_FEC,[test "$CONFIG_RTNET_DRV_MPC52XX_FEC" = "y"])


AC_MSG_CHECKING([whether to build SMSC LAN91C111 driver])
AC_ARG_ENABLE(smc91111,
    AS_HELP_STRING([--enable-smc91111], [build SMSC LAN91C111 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_SMC91111=y ;;
        *) CONFIG_RTNET_DRV_SMC91111=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_SMC91111:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_SMC91111,[test "$CONFIG_RTNET_DRV_SMC91111" = "y"])


AC_MSG_CHECKING([whether to build Eth1394 driver])
AC_ARG_ENABLE(eth1394,
    AS_HELP_STRING([--enable-eth1394], [build Eth1394 driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_ETH1394=y ;;
        *) CONFIG_RTNET_DRV_ETH1394=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_ETH1394:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_ETH1394,[test "$CONFIG_RTNET_DRV_ETH1394" = "y"])


AC_MSG_CHECKING([whether to build loopback driver])
AC_ARG_ENABLE(loopback,
    AS_HELP_STRING([--enable-loopback], [build loopback driver @<:@default=yes@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_LOOPBACK=y ;;
        *) CONFIG_RTNET_DRV_LOOPBACK=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_LOOPBACK:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_LOOPBACK,[test "$CONFIG_RTNET_DRV_LOOPBACK" = "y"])


AC_MSG_CHECKING([whether to build 3Com 59x driver])
AC_ARG_ENABLE(3c59x,
    AS_HELP_STRING([--enable-3c59x], [build 3Com 59x driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_3C59X=y ;;
        *) CONFIG_RTNET_DRV_3C59X=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_3C59X:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_3C59X,[test "$CONFIG_RTNET_DRV_3C59X" = "y"])


AC_MSG_CHECKING([whether to build Realtek 8169 (Gigabit) driver])
AC_ARG_ENABLE(r8169,
    AS_HELP_STRING([--enable-r8169], [build Realtek 8169 (Gigabit) driver]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_DRV_R8169=y ;;
        *) CONFIG_RTNET_DRV_R8169=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_DRV_R8169:-n}])
AM_CONDITIONAL(CONFIG_RTNET_DRV_R8169,[test "$CONFIG_RTNET_DRV_R8169" = "y"])


dnl ======================================================================
dnl             Real-Time IPv4
dnl ======================================================================

AC_MSG_CHECKING([whether to build real-time IPv4 support])
AC_ARG_ENABLE(rtipv4,
    AS_HELP_STRING([--enable-rtipv4], [enable real-time IPv4 support @<:@default=yes@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTIPV4=y ;;
        *) CONFIG_RTNET_RTIPV4=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_RTIPV4:-n}])
AM_CONDITIONAL(CONFIG_RTNET_RTIPV4,[test "$CONFIG_RTNET_RTIPV4" = "y"])
if test "$CONFIG_RTNET_RTIPV4" = "y"; then
    AC_DEFINE(CONFIG_RTNET_RTIPV4,1,[Real-Time IPv4 support])
fi

AC_MSG_CHECKING([whether to enable IP network routing])
AC_ARG_ENABLE(net-routing,
    AS_HELP_STRING([--enable-net-routing], [enable IP network routing @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTIPV4_NETROUTING=y ;;
        *) CONFIG_RTNET_RTIPV4_NETROUTING=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_RTIPV4_NETROUTING:-n}])
if test "$CONFIG_RTNET_RTIPV4_NETROUTING" = "y"; then
    AC_DEFINE(CONFIG_RTNET_RTIPV4_NETROUTING, 1, [Network routing])
fi

AC_MSG_CHECKING([whether to enable IP router support])
AC_ARG_ENABLE(router,
    AS_HELP_STRING([--enable-router], [enable IP router @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTIPV4_ROUTER=y ;;
        *) CONFIG_RTNET_RTIPV4_ROUTER=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_RTIPV4_ROUTER:-n}])
if test "CONFIG_RTNET_RTIPV4_ROUTER" = "y"; then
    AC_DEFINE(CONFIG_RTNET_RTIPV4_ROUTER, 1, [Router])
fi


dnl ======================================================================
dnl             Real-Time AF_PACKET
dnl ======================================================================

AC_MSG_CHECKING([whether to build real-time packet socket support])
AC_ARG_ENABLE(rtpacket,
    AS_HELP_STRING([--enable-rtpacket], [build Real-Time Packet socket support @<:@default=yes@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTPACKET=y ;;
        *) CONFIG_RTNET_RTPACKET=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_RTPACKET:-n}])
AM_CONDITIONAL(CONFIG_RTNET_RTPACKET,[test "$CONFIG_RTNET_RTPACKET" = "y"])


dnl ======================================================================
dnl             RTcfg
dnl ======================================================================

AC_MSG_CHECKING([whether to build RTcfg])
AC_ARG_ENABLE(rtcfg,
    AS_HELP_STRING([--enable-rtcfg], [build RTcfg @<:@default=yes@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTCFG=y ;;
        *) CONFIG_RTNET_RTCFG=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_RTCFG:-n}])
AM_CONDITIONAL(CONFIG_RTNET_RTCFG,[test "$CONFIG_RTNET_RTCFG" = "y"])

AC_MSG_CHECKING([whether to enable RTcfg debug support])
AC_ARG_ENABLE(rtcfg-dbg,
    AS_HELP_STRING([--enable-rtcfg-dbg], [enable RTcfg debugging @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTCFG_DEBUG=y ;;
        *) CONFIG_RTNET_RTCFG_DEBUG=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_RTCFG_DEBUG:-n}])
if test "$CONFIG_RTNET_RTCFG_DEBUG" = "y"; then
    AC_DEFINE(CONFIG_RTNET_RTCFG_DEBUG,1,[RTcfg debug])
fi


dnl ======================================================================
dnl             RTmac
dnl ======================================================================

AC_MSG_CHECKING([whether to build RTmac])
AC_ARG_ENABLE(nomac,
    AS_HELP_STRING([--enable-rtmac], [build Real-Time Media Access Control Layer @<:@default=yes@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_RTMAC=y ;;
        *) CONFIG_RTNET_RTMAC=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_RTMAC:-n}])
AM_CONDITIONAL(CONFIG_RTNET_RTMAC,[test "$CONFIG_RTNET_RTMAC" = "y"])


dnl ======================================================================
dnl             TDMA discipline for RTmac
dnl ======================================================================

AC_MSG_CHECKING([whether to build TDMA])
AC_ARG_ENABLE(tdma,
    AS_HELP_STRING([--enable-tdma], [build TDMA discipline for RTmac @<:@default=yes@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_TDMA=y ;;
        *) CONFIG_RTNET_TDMA=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_TDMA:-n}])
AM_CONDITIONAL(CONFIG_RTNET_TDMA,[test "$CONFIG_RTNET_TDMA" = "y"])

AC_MSG_CHECKING([whether to enable TDMA master support])
AC_ARG_ENABLE(tdma-master,
    AS_HELP_STRING([--enable-tdma-master], [enable TDMA master support @<:@default=yes@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_TDMA_MASTER=y ;;
        *) CONFIG_RTNET_TDMA_MASTER=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_TDMA_MASTER:-n}])
if test "$CONFIG_RTNET_TDMA_MASTER" = "y"; then
    AC_DEFINE(CONFIG_RTNET_TDMA_MASTER,1,[TDMA master support])
fi


dnl ======================================================================
dnl             NoMAC discipline for RTmac
dnl ======================================================================

AC_MSG_CHECKING([whether to build NoMAC])
AC_ARG_ENABLE(nomac,
    AS_HELP_STRING([--enable-nomac], [build NoMAC discipline for RTmac @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_NOMAC=y ;;
        *) CONFIG_RTNET_NOMAC=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_NOMAC:-n}])
AM_CONDITIONAL(CONFIG_RTNET_NOMAC,[test "$CONFIG_RTNET_NOMAC" = "y"])


dnl ======================================================================
dnl             Add-Ons
dnl ======================================================================

AC_MSG_CHECKING([whether to enable RTcap support])
AC_ARG_ENABLE(rtcap,
    AS_HELP_STRING([--enable-rtcap], [enable RTcap support and build capturing module @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_ADDON_RTCAP=y ;;
        *) CONFIG_RTNET_ADDON_RTCAP=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_ADDON_RTCAP:-n}])
AM_CONDITIONAL(CONFIG_RTNET_ADDON_RTCAP,[test "$CONFIG_RTNET_ADDON_RTCAP" = "y"])
if test "$CONFIG_RTNET_ADDON_RTCAP" = "y"; then
    AC_DEFINE(CONFIG_RTNET_ADDON_RTCAP,1,[RTcap support])
fi

AC_MSG_CHECKING([whether to build rtnetproxy])
AC_ARG_ENABLE(proxy,
    AS_HELP_STRING([--enable-proxy], [build IP protocol proxy driver (legacy) @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_ADDON_PROXY=y ;;
        *) CONFIG_RTNET_ADDON_PROXY=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_ADDON_PROXY:-n}])
AM_CONDITIONAL(CONFIG_RTNET_ADDON_PROXY,[test "$CONFIG_RTNET_ADDON_PROXY" = "y"])
if test "$CONFIG_RTNET_ADDON_PROXY" = "y"; then
    AC_DEFINE(CONFIG_RTNET_ADDON_PROXY,1,[rtnetproxy support])
fi


#dnl ======================================================================
#dnl             RTDM select (disabled until RTDM actually supports this)
#dnl ======================================================================
#
#AC_MSG_CHECKING([whether to enable RTDM select])
#AC_ARG_ENABLE(rtdm-select,
#    AS_HELP_STRING([--enable-rtdm-select], [enable select function for RTDM (experimental) @<:@default=no@:>@]),
#    [case "$enableval" in
#        y | yes) CONFIG_RTNET_RTDM_SELECT=y ;;
#        *) CONFIG_RTNET_RTDM_SELECT=n ;;
#    esac],
#    [CONFIG_RTNET_RTDM_SELECT=n])
#AC_MSG_RESULT([${CONFIG_RTNET_RTDM_SELECT:-n}])
#
#dnl When RTDM select has matured, the example compilation should no longer
#dnl depend on the internal select support by RTnet. Instead, running the
#dnl example shall give an error.
#
#AM_CONDITIONAL(CONFIG_RTNET_RTDM_SELECT, [test "$CONFIG_RTNET_RTDM_SELECT" = "y"])
#
#if test "$CONFIG_RTNET_RTDM_SELECT" = "y"; then
#    AC_DEFINE(CONFIG_RTNET_RTDM_SELECT, 1, [RTDM select support])
#fi


dnl ======================================================================
dnl             Examples
dnl ======================================================================

AC_MSG_CHECKING([whether to build examples])
AC_ARG_ENABLE(examples,
    AS_HELP_STRING([--enable-examples], [build examples @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_EXAMPLES=y ;;
        *) CONFIG_RTNET_EXAMPLES=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_EXAMPLES:-n}])
AM_CONDITIONAL(CONFIG_RTNET_EXAMPLES,[test "$CONFIG_RTNET_EXAMPLES" = "y"])


dnl ======================================================================
dnl             compilation parameters
dnl ======================================================================

AC_MSG_CHECKING([whether to enable internal bug checks])
AC_ARG_ENABLE(checks,
    AS_HELP_STRING([--enable-checks], [enable internal bug checks @<:@default=no@:>@]),
    [case "$enableval" in
        y | yes) CONFIG_RTNET_CHECKED=y ;;
        *) CONFIG_RTNET_CHECKED=n ;;
    esac])
AC_MSG_RESULT([${CONFIG_RTNET_CHECKED:-n}])
if test "$CONFIG_RTNET_CHECKED" = "y"; then
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


RTEXT_KMOD_CFLAGS="-D__IN_RTNET__ $RTEXT_KMOD_CFLAGS"
RTEXT_USER_CFLAGS="-D__IN_RTNET__ $RTEXT_USER_CFLAGS"
AC_SUBST(RTEXT_KMOD_CFLAGS)
AC_SUBST(RTEXT_USER_CFLAGS)
AC_SUBST(RTEXT_LIBRARIES)
AC_SUBST(RTFW_KMOD_CFLAGS)


dnl ======================================================================
dnl             generate RTDM version define
dnl ======================================================================

MAJOR=`echo ${VERSION} | sed -e 's/\([[0-9]]*\)\.[[0-9]]*\.[[0-9]]*/\1/'`
MINOR=`echo ${VERSION} | sed -e 's/[[0-9]]*\.\([[0-9]]*\)\.[[0-9]]*/\1/'`
PATCH=`echo ${VERSION} | sed -e 's/[[0-9]]*\.[[0-9]]*\.\([[0-9]]*\)/\1/'`
AC_DEFINE_UNQUOTED(RTNET_RTDM_VER,
    [RTDM_DRIVER_VER($MAJOR, $MINOR, $PATCH)],
    [RTDM Driver Version])


dnl ======================================================================
dnl             create links to Makefiles used by linux-2.6.x
dnl ======================================================================

if test x$CONFIG_KBUILD = xy; then
    for DIR in \
        drivers/8139too \
        drivers/eepro100 \
        drivers/eth1394 \
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
        drivers/experimental/r8169 \
        \
        stack \
        stack/packet \
        stack/ipv4 \
        stack/rtcfg \
        stack/rtmac \
        stack/rtmac/nomac \
        stack/rtmac/tdma \
        \
        addons/rtcap \
        addons/rtnetproxy \
        \
        examples/xenomai/native/kernel \
        ; do
            AC_CONFIG_LINKS(${DIR}/Makefile:${DIR}/Makefile.kbuild)
    done
fi

dnl        addons/examples/round-trip-time/client \
dnl        addons/examples/round-trip-time/server \
dnl        addons/examples/rtmac/event/collector \
dnl        addons/examples/rtmac/event/handler \
dnl        addons/examples/rtmac/netshm \
dnl        addons/examples/rtmac/netshm/kernel-demo \
dnl        addons/examples/rtmac/round-trip-time/client \
dnl        addons/examples/rtmac/round-trip-time/multi-client \
dnl        addons/examples/rtmac/round-trip-time/parport-client \
dnl        addons/examples/rtmac/round-trip-time/server \
dnl        addons/examples/select \


dnl ======================================================================
dnl             create links to Xenomai symbols
dnl ======================================================================

case "${CONFIG_RTNET_RTEXT_VERSION}" in
    xeno-2x)
        if test x$CONFIG_KBUILD = xy; then
            mkdir -p symbols
            rm -rf symbols/*
            for FILE in `ls ${RTNET_RTEXT_SYMBOLS}/*`; do
                ${LN_S} ${FILE} symbols/$(basename ${FILE})
            done
        fi
        ;;
esac


dnl ======================================================================
dnl             generate output
dnl ======================================================================

AC_CONFIG_FILES([ \
    GNUmakefile \
    \
    Documentation/GNUmakefile \
    \
    drivers/GNUmakefile \
    drivers/8139too/GNUmakefile \
    drivers/eepro100/GNUmakefile \
    drivers/eth1394/GNUmakefile \
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
    drivers/experimental/r8169/GNUmakefile \
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
    \
    addons/GNUmakefile \
    addons/rtcap/GNUmakefile \
    addons/rtnetproxy/GNUmakefile \
    \
    examples/GNUmakefile \
    examples/generic/GNUmakefile \
    examples/rtai/GNUmakefile \
    examples/xenomai/GNUmakefile \
    examples/xenomai/native/GNUmakefile \
    examples/xenomai/native/kernel/GNUmakefile \
    \
    scripts/GNUmakefile \
    scripts/kconfig/GNUmakefile \
    scripts/kconfig/lxdialog/GNUmakefile \
    ])

dnl    examples/round-trip-time/GNUmakefile \
dnl    examples/round-trip-time/client/GNUmakefile \
dnl    examples/round-trip-time/linux-client/GNUmakefile \
dnl    examples/round-trip-time/server/GNUmakefile \
dnl    examples/rtmac/GNUmakefile \
dnl    examples/rtmac/event/GNUmakefile \
dnl    examples/rtmac/event/collector/GNUmakefile \
dnl    examples/rtmac/event/handler/GNUmakefile \
dnl    examples/rtmac/netshm/GNUmakefile \
dnl    examples/rtmac/netshm/kernel-demo/GNUmakefile \
dnl    examples/rtmac/round-trip-time/GNUmakefile \
dnl    examples/rtmac/round-trip-time/client/GNUmakefile \
dnl    examples/rtmac/round-trip-time/multi-client/GNUmakefile \
dnl    examples/rtmac/round-trip-time/parport-client/GNUmakefile \
dnl    examples/rtmac/round-trip-time/server/GNUmakefile \
dnl    examples/select/GNUmakefile \

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
