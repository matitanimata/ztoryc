# Minimal CMakeLists for the vendored QXlsx (MIT) — Ztoryc build.
# Replaces the upstream file (which auto-selects Qt6 and pulls in CPack/install
# rules referencing files we don't vendor).  We force Qt5 to match Tahoma2D and
# build a plain static library linked into the Ztoryc executable.
cmake_minimum_required(VERSION 3.16)
project(QXlsx LANGUAGES CXX)

find_package(Qt5 REQUIRED COMPONENTS Core Gui)

file(GLOB QXLSX_SRC ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
# Headers live in a separate dir from the sources; list them in the target so
# AUTOMOC scans them and generates moc for the Q_OBJECT classes (e.g. Document).
file(GLOB QXLSX_HDR ${CMAKE_CURRENT_SOURCE_DIR}/header/*.h)

add_library(QXlsx STATIC ${QXLSX_SRC} ${QXLSX_HDR})
add_library(QXlsx::QXlsx ALIAS QXlsx)

set_target_properties(QXlsx PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# QXlsx sources are written against QT_NO_KEYWORDS (Q_EMIT/Q_SLOTS forms).
target_compile_definitions(QXlsx PRIVATE QT_NO_KEYWORDS)

target_include_directories(QXlsx
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/header
    PRIVATE ${Qt5Gui_PRIVATE_INCLUDE_DIRS}   # for <private/qzipwriter_p.h>
)

target_link_libraries(QXlsx PUBLIC Qt5::Core Qt5::Gui)
