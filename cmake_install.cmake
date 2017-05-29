# Install script for directory: /home/clement/code/otawa/dist/linux-x86_64/otawa-core2-build/otawa/src/poly

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/home/clement/code/otawa/dist/linux-x86_64/otawa-core2")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  if(EXISTS "$ENV{DESTDIR}/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.so"
         RPATH "$ORIGIN/../..:$ORIGIN")
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.so")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
file(INSTALL DESTINATION "/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa" TYPE SHARED_LIBRARY FILES "/home/clement/code/otawa/dist/linux-x86_64/otawa-core2-build/otawa/src/poly/poly.so")
  if(EXISTS "$ENV{DESTDIR}/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.so"
         OLD_RPATH "/home/clement/code/otawa/dist/linux-x86_64/otawa-core2-build/otawa/src/clp:/home/clement/code/otawa/dist/linux-x86_64/otawa-core2-build/otawa/src/prog:/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib:/usr/local/lib:"
         NEW_RPATH "$ORIGIN/../..:$ORIGIN")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.so")
    endif()
  endif()
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa/poly.eld")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
file(INSTALL DESTINATION "/home/clement/code/otawa/dist/linux-x86_64/otawa-core2/lib/otawa/otawa" TYPE FILE FILES "/home/clement/code/otawa/dist/linux-x86_64/otawa-core2-build/otawa/src/poly/poly.eld")
endif()

