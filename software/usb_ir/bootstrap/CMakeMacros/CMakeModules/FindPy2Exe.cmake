# Find py2exe
# ~~~~~~~~
#
# Find the installed version of py2exe. FindPy2Exe should be called after Python
# has been found.
#
# This file defines the following variables:
#
# PY2EXE_VERSION - The version of py2exe found as a human readable string.
#


IF(PY2EXE_VERSION)
  # Already in cache, be silent
  SET(PY2EXE_FOUND TRUE)
ELSE(PY2EXE_VERSION)

  FIND_FILE(_find_PY2EXE_py FindPy2Exe.py PATHS ${CMAKE_MODULE_PATH} ${PROJ_SOURCE_DIR}/config)

  EXECUTE_PROCESS(COMMAND ${PYTHON_EXECUTABLE} ${_find_PY2EXE_py} OUTPUT_VARIABLE PY2EXE_config)
  IF(PY2EXE_config)
    STRING(REGEX REPLACE "^py2exe_version_str:([^\n]+).*$" "\\1" PY2EXE_VERSION ${PY2EXE_config})
    SET(PY2EXE_FOUND TRUE)
  ENDIF(PY2EXE_config)

  IF(PY2EXE_FOUND)
    IF(NOT PY2EXE_FIND_QUIETLY)
      MESSAGE(STATUS "Found py2exe version: ${PY2EXE_VERSION}")
    ENDIF(NOT PY2EXE_FIND_QUIETLY)
  ELSE(PY2EXE_FOUND)
    IF(PY2EXE_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find py2exe")
    ENDIF(PY2EXE_FIND_REQUIRED)
  ENDIF(PY2EXE_FOUND)

ENDIF(PY2EXE_VERSION)
