# - Find GreenLib
# This module finds if TLM2C is installed and determines where the
# include files and libraries are.
#

#=============================================================================
# Copyright 2014 GreenSocs
#
# KONRAD Frederic <fred.konrad@greensocs.com>
#
#=============================================================================

MESSAGE(STATUS "Searching for TLM2C")

# The HINTS option should only be used for values computed from the system.
SET(_TLM2C_HINTS
  ${TLM2C_PREFIX}/include
  ${TLM2C_PREFIX}/lib
  ${TLM2C_PREFIX}/lib64
  $ENV{TLM2C_PREFIX}/include
  $ENV{TLM2C_PREFIX}/lib
  $ENV{TLM2C_PREFIX}/lib64
  ${CMAKE_INSTALL_PREFIX}/include
  ${CMAKE_INSTALL_PREFIX}/lib
  ${CMAKE_INSTALL_PREFIX}/lib64
  )
# Hard-coded guesses should still go in PATHS. This ensures that the user
# environment can always override hard guesses.
SET(_TLM2C_PATHS
  /usr/include
  /usr/lib
  /usr/lib64
  /usr/local/lib
  /usr/local/lib64
  )

FIND_PATH(TLM2C_INCLUDE_DIRS
  NAMES tlm2c/tlm2c.h
  HINTS ${_TLM2C_HINTS}
  PATHS ${_TLM2C_PATHS}
)

FIND_PATH(TLM2C_LIBRARY_DIRS
  NAMES libtlm2c.a
  HINTS ${_TLM2C_HINTS}
  PATHS ${_TLM2C_PATHS}
)

if("${TLM2C_INCLUDE_DIRS}" MATCHES "TLM2C_INCLUDE_DIRS-NOTFOUND")
    SET(TLM2C_FOUND FALSE)
else("${TLM2C_INCLUDE_DIRS}" MATCHES "TLM2C_INCLUDE_DIRS-NOTFOUND")
    SET(TLM2C_FOUND TRUE)
    MESSAGE(STATUS "TLM2C include directory = ${TLM2C_INCLUDE_DIRS}")
    MESSAGE(STATUS "TLM2C library directory = ${TLM2C_LIBRARY_DIRS}")
    SET(TLM2C_LIBRARIES -llibtlm2c.a)
endif("${TLM2C_INCLUDE_DIRS}" MATCHES "TLM2C_INCLUDE_DIRS-NOTFOUND")

