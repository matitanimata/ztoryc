# SPDX-FileCopyrightText: (C) 2021 Daniel Nicoletti <dantti12@gmail.com>
# SPDX-License-Identifier: MIT

# - Config information for QXlsx
# This file defines:
#
#  QXlsxQt6_INCLUDE_DIR - the QXlsx include directory
#  QXlsxQt6_LIBRARY_DIR - the QXlsx library directory
#  QXlsxQt6_LIBRARIES   - Link these to use QXlsx


####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was qxlsx-config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

set_and_check(QXlsxQt6_INCLUDE_DIR "${PACKAGE_PREFIX_DIR}/include")
set_and_check(QXlsxQt6_LIBRARY_DIR "${PACKAGE_PREFIX_DIR}/lib")

include("${CMAKE_CURRENT_LIST_DIR}/QXlsxQt6Targets.cmake")

set(QXlsxQt6_LIBRARIES "QXlsxQt6")

check_required_components(QXlsxQt6)
