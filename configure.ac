AC_INIT([RandomFields], 3.3.6)

USE_FLAGS=true

if "${R_HOME}/bin/R" CMD config CXX11 > /dev/null ; then
  RANDOMFIELDS_CXX=`"${R_HOME}/bin/R" CMD config CXX11`
elif "${R_HOME}/bin/R" CMD config CXX1X > /dev/null ; then
  RANDOMFIELDS_CXX=`"${R_HOME}/bin/R" CMD config CXX1X`
else
  RANDOMFIELDS_CXX=`"${R_HOME}/bin/R" CMD config CXX`
fi

if test "x$TARGET" = "x" ; then
  case `basename $RANDOMFIELDS_CXX` in
  g++ | clang++ )
    if uname -m | grep  -iq sparc ; then
      echo Since this is a sparc processor we set TARGET=default
      TARGET=default
    else
      TARGET=native
    fi
    ;;
  *)
    echo unknown compiler: $RANDOMFIELDS_CXX therefore we set TARGET=default
    TARGET=default
    ;;
  esac
fi

if test "x$RANDOMFIELDS_TARGET" != "x" ; then
  TARGET=$RANDOMFIELDS_TARGET
fi

if test $USE_FLAGS = false ; then
  TARGET=default
fi

echo Using target: $TARGET 
echo Using further args: $@


if test $TARGET = native ; then
  CXX_FLAGS="-O2 -march=native $CXX_FLAGS"
elif test $TARGET = default ; then
  CXX_FLAGS="$CXX_FLAGS"
elif test $TARGET = empty ; then
  echo No compiler flags used
fi


echo Config in `pwd`
echo Config: using CXX_FLAGS=$CXX_FLAGS

RANDOMFIELDS_CXXFLAGS=$CXX_FLAGS

AC_SUBST(RANDOMFIELDS_CXXFLAGS)
AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
