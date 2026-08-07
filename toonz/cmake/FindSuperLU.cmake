
if(WITH_SYSTEM_SUPERLU)
    # Homebrew (Intel: /usr/local, Apple Silicon: /opt/homebrew)
    if(DEFINED ENV{HOMEBREW_PREFIX})
      set(_brew_prefix "$ENV{HOMEBREW_PREFIX}")
    elseif(EXISTS /opt/homebrew/opt/superlu)
      set(_brew_prefix "/opt/homebrew")
    else()
      set(_brew_prefix "/usr/local")
    endif()
    set(_header_hints
        "${_brew_prefix}/opt/superlu/include"
    )
    set(_header_suffixes
        superlu
        SuperLU
        .
    )
    set(_lib_hints
        "${_brew_prefix}/opt/superlu/lib"
    )
    set(_lib_suffixes)
else()
    # preferred homebrew's directories.
    set(_header_hints
        ${THIRDPARTY_LIBS_HINTS}
    )
    set(_header_suffixes
        superlu43/4.3_1/include/superlu
        superlu/SuperLU_4.1/include
    )
    set(_lib_hints
        ${THIRDPARTY_LIBS_HINTS}
    )
    set(_lib_suffixes
        superlu
    )
endif()

find_path(
    SUPERLU_INCLUDE_DIR
    NAMES
        slu_Cnames.h
    HINTS
        ${_header_hints}
    PATH_SUFFIXES
        ${_header_suffixes}
)

# Homebrew SuperLU 7+ installs headers to include/superlu/*.h. Only on Apple,
# toonz/sources/CMakeLists.txt appends "/superlu" to SUPERLU_INCLUDE_DIR; find_path
# must then yield the parent (e.g. .../include). Linux/BSD use SUPERLU_INCLUDE_DIR as-is.
if(WITH_SYSTEM_SUPERLU AND SUPERLU_INCLUDE_DIR AND APPLE)
    if(EXISTS "${SUPERLU_INCLUDE_DIR}/slu_Cnames.h")
        get_filename_component(_superlu_include_leaf "${SUPERLU_INCLUDE_DIR}" NAME)
        if(_superlu_include_leaf STREQUAL "superlu")
            get_filename_component(SUPERLU_INCLUDE_DIR "${SUPERLU_INCLUDE_DIR}" DIRECTORY)
        endif()
    endif()
endif()

find_library(
    SUPERLU_LIBRARY
    NAMES
        superlu
        libsuperlu.dylib
        libsuperlu.so
        libsuperlu.a
        libsuperlu_4.1.a
        libsuperlu.dylib
    HINTS
        ${_lib_hints}
    PATH_SUFFIXES
        ${_lib_suffixes}
)

message("***** SuperLU Header path:" ${SUPERLU_INCLUDE_DIR})
message("***** SuperLU Library path:" ${SUPERLU_LIBRARY})

set(SUPERLU_NAMES ${SUPERLU_NAMES} SuperLU)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SuperLU
    DEFAULT_MSG SUPERLU_LIBRARY SUPERLU_INCLUDE_DIR)

if(SUPERLU_FOUND)
    set(SUPERLU_LIBRARIES ${SUPERLU_LIBRARY})
endif()

mark_as_advanced(
    SUPERLU_LIBRARY
    SUPERLU_INCLUDE_DIR
)

unset(_superlu_include_leaf)
unset(_header_hints)
unset(_header_suffixes)
unset(_lib_hints)
unset(_lib_suffixes)
