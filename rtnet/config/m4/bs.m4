# This file is part of Autoconf.                       -*- Autoconf -*-
# Checking for headers.
#
# Copyright (C) 2000, 2001, 2002, 2003 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#
# As a special exception, the Free Software Foundation gives unlimited
# permission to copy, distribute and modify the configure scripts that
# are the output of Autoconf.  You need not follow the terms of the GNU
# General Public License when using or distributing such scripts, even
# though portions of the text of Autoconf appear in them.  The GNU
# General Public License (GPL) does govern all other use of the material
# that constitutes the Autoconf program.
#
# Certain portions of the Autoconf source text are designed to be copied
# (in certain cases, depending on the input) into the output of
# Autoconf.  We call these the "data" portions.  The rest of the Autoconf
# source text consists of comments plus executable code that decides which
# of the data portions to output in any given case.  We call these
# comments and executable code the "non-data" portions.  Autoconf never
# copies any of the non-data portions into its output.
#
# This special exception to the GPL applies to versions of Autoconf
# released by the Free Software Foundation.  When you make and
# distribute a modified version of Autoconf, you may extend this special
# exception to the GPL to apply to your modified version as well, *unless*
# your modified version has the potential to copy into its output some
# of the text that was the non-data portion of the version that you started
# with.  (In other words, unless your change moves or copies text from
# the non-data portions to the data portions.)  If your modification has
# such potential, you must delete any notice of this special exception
# to the GPL from your modified version.
#
# Written by David MacKenzie, with help from
# Franc,ois Pinard, Karl Berry, Richard Pixley, Ian Lance Taylor,
# Roland McGrath, Noah Friedman, david d zuhn, and many others.


dnl ======================================================================
dnl ======================================================================
dnl ======================================================================
dnl ======================================================================

# BS_KCOMPILE_IFELSE(PROGRAM, [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# ---------------------------------------------------------------------
# Try to compile PROGRAM.
# This macro can be used during the selection of a compiler.
m4_define([BS_KCOMPILE_IFELSE],
[m4_ifvaln([$1], [AC_LANG_CONFTEST([$1])])dnl
rm -f Makefile
cat >Makefile <<_ACEOF
EXTRA_CFLAGS += ${CFLAGS}
EXTRA_CFLAGS += ${CPPFLAGS}
obj-m += conftest.o
_ACEOF
rm -f conftest.$bs_kmodext conftest.o
AS_IF([_AC_EVAL_STDERR($bs_kcompile > /dev/null)],
      [$2],
      [_AC_MSG_LOG_CONFTEST
m4_ifvaln([$3],[$3])dnl])dnl
rm -f conftest.err conftest.$bs_kmodext conftest.o .conftest.o.d .conftest.o.cmd .conftest.ko.cmd .conftest.mod.o.cmd .tmp_versions/conftest.mod Makefile m4_ifval([$1], [conftest.$ac_ext]); rmdir .tmp_versions[]dnl
])# BS_KCOMPILE_IFELSE

#AS_IF([_AC_EVAL_STDERR($bs_kcompile > /dev/null) &&
#	 AC_TRY_COMMAND([test -z "$ac_[]_AC_LANG_ABBREV[]_werror_flag"[]dnl
#			 || test ! -s conftest.err]) &&
#	 AC_TRY_COMMAND([test -s conftest.$bs_kmodext])],
#      [$2],
#      [_AC_MSG_LOG_CONFTEST



# BS_CHECK_KHEADER(HEADER-FILE,
#                  [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
#                  [INCLUDES])
# --------------------------------------------------------------
# Check the compiler accepts HEADER-FILE.  The INCLUDES are defaulted.
m4_define([BS_CHECK_KHEADER],
[AS_VAR_PUSHDEF([ac_Header], [ac_cv_header_$1])dnl
AC_CACHE_CHECK([for $1], ac_Header,
	       [BS_KCOMPILE_IFELSE([AC_LANG_SOURCE([AC_INCLUDES_DEFAULT([$4])
@%:@include <$1>])],
				  [AS_VAR_SET(ac_Header, yes)],
				  [AS_VAR_SET(ac_Header, no)])])
AS_IF([test AS_VAR_GET(ac_Header) = yes], [$2], [$3])[]dnl
AS_VAR_POPDEF([ac_Header])dnl
])# BS_CHECK_HEADER


# BS_CHECK_KHEADERS(HEADER-FILE...
#                   [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND],
#                   [INCLUDES])
# ----------------------------------------------------------
AC_DEFUN([BS_CHECK_KHEADERS],
[AH_CHECK_HEADERS([$1])dnl
for ac_header in $1
do
BS_CHECK_KHEADER($ac_header,
		 [AC_DEFINE_UNQUOTED(AS_TR_CPP(HAVE_$ac_header)) $2],
		 [$3],
		 [$4])dnl
done
])# BS_CHECK_KHEADERS

dnl ======================================================================
dnl ======================================================================
dnl ======================================================================
dnl ======================================================================