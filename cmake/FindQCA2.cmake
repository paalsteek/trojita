# - Try to find the Qt Cryptographic Architecture
# Once done this will define
#  QCA2_FOUND - System has QCA2
#  QCA2_INCLUDE_DIRS - The QCA2 include directories
#  QCA2_LIBRARIES - The libraries needed to use QCA2
#  QCA2_DEFINITIONS - Compiler switches required for using QCA2
# Additional definitions (not yet implemented)
#  QCA2_GNUPG_FOUND - System has GnuPG support of QCA2
#  QCA2_GNUPG_LIBRARIES - The libraries needed for GnuPG support of QCA2

find_package(PkgConfig)
pkg_check_modules(PC_QCA2 QUIET qca2)
set(QCA2_DEFINITIONS ${PC_QCA2_CFLAGS_OTHER})

find_path(QCA2_INCLUDE_DIR qca.h
          HINTS ${PC_QCA2_INCLUDEDIR} ${PC_QCA2_INCLUDE_DIRS})

find_library(QCA2_LIBRARY NAMES qca libqca
             HINTS ${PC_QCA2_LIBDIR} ${PC_QCA2_LIBRARY_DIRS} )

#find_library(QCA2_GNUPG_LIBRARY NAMES qca-gnupg libqca-gnupg
#             HINTS ${PC_QCA2_LIBDIR} ${PC_QCA2_LIBRARY_DIRS} )

set(QCA2_LIBRARIES ${QCA2_LIBRARY} )
#set(QCA2_GNUPG_LIBRARIES ${QCA2_GNUPG_LIBRARIES} )
set(QCA2_INCLUDE_DIRS ${QCA2_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set QCA2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(QCA2  DEFAULT_MSG
                                  QCA2_LIBRARY QCA2_INCLUDE_DIR)
#find_package_handle_standard_args(QCA2_GNUPG  DEFAULT_MSG
#                                  QCA2_GNUPG_LIBRARY QCA2_INCLUDE_DIR)

mark_as_advanced(QCA2_INCLUDE_DIR QCA2_LIBRARY )
