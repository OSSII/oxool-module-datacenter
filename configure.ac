AC_INIT([oxool-module-datacenter], [1.0.0], [tommy.wu@ossii.com.tw])
LT_INIT
AM_INIT_AUTOMAKE([foreign])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CC
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
OXOOLMODULE_VERSION_MAJOR=`echo $VERSION | awk -F. '{print $1}'`
OXOOLMODULE_VERSION_MINOR=`echo $VERSION | awk -F. '{print $2}'`
OXOOLMODULE_VERSION_MICRO=`echo $VERSION | awk -F. '{print $3}'`
OXOOLMODULE_VERSION="$OXOOLMODULE_VERSION_MAJOR.$OXOOLMODULE_VERSION_MINOR.$OXOOLMODULE_VERSION_MICRO"
LOOLWSD_VERSION=OXOOLMODULE_VERSION
OXOOLMODULE_VERSION_DIST=1

AC_ARG_ENABLE([debug],
              AS_HELP_STRING([--enable-debug],
                             [Enable debugging, link with debugging version of Poco libraries]))
BRANCH=`( cat dist_git_branch 2> /dev/null || git rev-parse --abbrev-ref HEAD 2> /dev/null  )`
AC_SUBST([BRANCH])
AC_DEFINE_UNQUOTED([BRANCH], [["$BRANCH"]],[git branch name])

AC_SUBST([OXOOLMODULE_VERSION])
AC_SUBST([OXOOLMODULE_VERSION_DIST])
AC_DEFINE_UNQUOTED([LOOLWSD_VERSION],[["$lOOLWSD_VERSION"]],[LibreOffice Online WebSocket server version])
AC_CONFIG_FILES([
                 Makefile
                 src/Makefile
                 datacenter.spec
                 datacenter.xml
                 ])
# Handle options
AS_IF([test "$enable_debug" = yes -a -n "$with_poco_libs"],
      [POCO_DEBUG_SUFFIX=d],
      [POCO_DEBUG_SUFFIX=])

LIBS="-lpam -lcap -lpng -ldl -lpcre -lsqlite3"
LIBS="$LIBS -lPocoNet${POCO_DEBUG_SUFFIX} -lPocoNetSSL${POCO_DEBUG_SUFFIX} -lPocoUtil${POCO_DEBUG_SUFFIX} -lPocoJSON${POCO_DEBUG_SUFFIX} -lPocoXML${POCO_DEBUG_SUFFIX} -lPocoFoundation${POCO_DEBUG_SUFFIX} -lPocoCrypto${POCO_DEBUG_SUFFIX} -lPocoData${POCO_DEBUG_SUFFIX} -lPocoDataSQLite${POCO_DEBUG_SUFFIX} -lPocoZip${POCO_DEBUG_SUFFIX}"

# Logging
LOG_FILE="/var/log/datacenter.log"
TEMPLATE_PATH="/var/lib/oxool/datacenter/templates/"
DB_FILE="/var/lib/oxool/datacenter/datacenter.sqlite"
if test "$enable_debug" = "yes"; then
    AC_DEFINE([ENABLE_DEBUG],1,[Whether to compile in some extra debugging support code and disable some security pieces])
    PWD = `pwd`
    AC_DEFINE_UNQUOTED([DEV_DIR],"$PWD",[Enable Dev environment.])
    ENABLE_DEBUG=true
    LOG_FILE="/tmp/datacenter.log"
    TEMPLATE_PATH="$PWD/runTimeData/templates/"
    DB_FILE="$PWD/runTimeData/datacenter.sqlite"
else
    AC_DEFINE([ENABLE_DEBUG],0,[Whether to compile in some extra debugging support code and disable some security pieces])
fi

AC_SUBST(LOG_FILE)
AC_SUBST(TEMPLATE_PATH)
AC_SUBST(DB_FILE)
AC_SUBST(ENABLE_DEBUG)
AM_CONDITIONAL([ENABLE_DEBUG], [test "$ENABLE_DEBUG" = "true"])
AC_OUTPUT
