# Copyright (C) 2018 Fondazione Istituto Italiano di Tecnologia (IIT)
# All Rights Reserved.
# Authors: Mohamed Babiker Mohamed Elobaid <mohamed.elobaid@iit.it>
#          Giulio Romualdi <giulio.romualdi@iit.it>
#
# set target name

set(EXE_TARGET_NAME VirtualizerModule)


option(ENABLE_RPATH "Enable RPATH for this library" ON)
mark_as_advanced(ENABLE_RPATH)
include(AddInstallRPATHSupport)
add_install_rpath_support(BIN_DIRS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
  LIB_DIRS "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
  INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}"
  DEPENDS ENABLE_RPATH
  USE_LINK_PATH)

# set cpp files
set(${EXE_TARGET_NAME}_SRC
  src/main.cpp
  src/VirtualizerModule.cpp
  )

# set hpp files
set(${EXE_TARGET_NAME}_HDR
  include/VirtualizerModule.hpp
  )

# Thrift
set(${EXE_TARGET_NAME}_THRIFT_HDR
  thrift/virtualizerCommand.thrift
  )
yarp_add_idl(${EXE_TARGET_NAME}_THRIFT_SRC ${${EXE_TARGET_NAME}_THRIFT_HDR})


# add an executable to the project using the specified source files.
add_executable(${EXE_TARGET_NAME} ${${EXE_TARGET_NAME}_SRC} ${${EXE_TARGET_NAME}_HDR}
  ${${EXE_TARGET_NAME}_THRIFT_SRC})

# add include directories to the build.
target_include_directories(${EXE_TARGET_NAME} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include )

target_link_libraries(${EXE_TARGET_NAME}
  ${YARP_LIBRARIES}
  CybSDK
  UtilityLibrary
  Eigen3::Eigen
  )

install(TARGETS ${EXE_TARGET_NAME} DESTINATION bin)
