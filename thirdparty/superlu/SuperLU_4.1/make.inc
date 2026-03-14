############################################################################
#
#  Program:         SuperLU
#
#  Module:          make.inc
#
#  Purpose:         Top-level Definitions
#
#  Creation date:   October 2, 1995
#
#  Modified:	    February 4, 1997  Version 1.0
#		    November 15, 1997 Version 1.1
#		    September 1, 1999 Version 2.0
#
############################################################################
#
#  The machine (platform) identifier to append to the library names
#
PLAT = _mac_arm64

#
#  The name of the libraries to be created/linked to
#
SuperLUroot	= /Volumes/ZioSam/tahoma2d-workspace/tahoma2d/thirdparty/superlu/SuperLU_4.1
SUPERLULIB   	= $(SuperLUroot)/lib/libsuperlu_4.1.a
TMGLIB       	= libtmglib.a

# Use the bundled CBLAS (no external BLAS dependency).
BLASLIB 	= $(SuperLUroot)/lib/libblas$(PLAT).a

LIBS		= $(SUPERLULIB) $(BLASLIB)

#
#  The archiver and the flag(s) to use when building archive (library)
#  If your system has no ranlib, set RANLIB = echo.
#
ARCH         = ar
ARCHFLAGS    = cr
RANLIB       = ranlib

CC           = clang
CFLAGS       = -DPRNTlevel=0 -O2 -fPIC -arch arm64 -std=gnu89 -Wno-implicit-function-declaration -Wno-error=implicit-function-declaration
NOOPTS       = 
FORTRAN	     =
FFLAGS       =
LOADER       = $(CC)
LOADOPTS     =

#
#  C preprocessor defs for compilation for the Fortran interface
#  (-DNoChange, -DAdd_, -DAdd__, or -DUpCase)
#
CDEFS        = -DAdd_
#
# The directory in which Matlab is installed
#
MATLAB	     = /usr/sww/matlab
