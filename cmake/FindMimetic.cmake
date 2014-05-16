# - Try to find the mimetic library
# Once done this will define
#  MIMETIC_FOUND - System has MIMETIC
#  MIMETIC_INCLUDE_DIRS - The MIMETIC include directories
#  MIMETIC_LIBRARIES - The libraries needed to use MIMETIC
#  MIMETIC_DEFINITIONS - Compiler switches required for using MIMETIC

find_package(PkgConfig)
pkg_check_modules(PC_MIMETIC QUIET mimetic)
set(MIMETIC_DEFINITIONS ${PC_MIMETIC_CFLAGS_OTHER})

find_path(MIMETIC_INCLUDE_DIR mimetic.h
          HINTS ${PC_MIMETIC_INCLUDEDIR} ${PC_MIMETIC_INCLUDE_DIRS}
          PATH_SUFFIXES mimetic)

find_library(MIMETIC_LIBRARY NAMES mimetic libmimetic
             HINTS ${PC_MIMETIC_LIBDIR} ${PC_MIMETIC_LIBRARY_DIRS} )

set(MIMETIC_LIBRARIES ${MIMETIC_LIBRARY} )
set(MIMETIC_INCLUDE_DIRS ${MIMETIC_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set MIMETIC_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Mimetic  DEFAULT_MSG
                                  MIMETIC_LIBRARY MIMETIC_INCLUDE_DIR)

set(Mimetic_FOUND ${MIMETIC_FOUND})

mark_as_advanced(MIMETIC_INCLUDE_DIR MIMETIC_LIBRARY )
