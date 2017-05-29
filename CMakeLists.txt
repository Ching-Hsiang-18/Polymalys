set(CMAKE_INSTALL_RPATH "${ORIGIN}/../..;${ORIGIN}")
set(CMAKE_MACOSX_RPATH true)
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCES
    poly_PolyAnalysis.cpp
)

include_directories(".")

add_library(poly SHARED ${SOURCES})
set_property(TARGET poly PROPERTY PREFIX "")
set(SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}")
target_link_libraries(poly clp)

install(TARGETS poly DESTINATION "${PLUGDIR}/otawa")
install(FILES poly.eld DESTINATION "${PLUGDIR}/otawa")
