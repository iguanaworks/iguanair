message(STATUS "Looking for python ${PYVER}")

# unset a minimal set of variables to allow redetection
unset(PYTHON_INCLUDE_DIR CACHE)
unset(PYTHON_LIBRARY CACHE)
unset(PYTHON_LIBRARY)

find_package(PythonLibs ${PYVER} QUIET)
If(PYTHONLIBS_FOUND)
  message(STATUS "  Found ${PYTHONLIBS_VERSION_STRING}")
  set(FOUND_SOME_PYTHON TRUE PARENT_SCOPE)

  # ensure we have PYTHON_INCLUDE_DIR even on older CMake
  If("${PYTHON_INCLUDE_DIR}" STREQUAL "")
    Set(PYTHON_INCLUDE_DIR "${PYTHON_INCLUDE_PATH}")
  EndIf()

  # compute the version info we need to find site-packages
  # for cmake < 2.8.8 (without PYTHONLIBS_VERSION_STRING) default to 2.7
  Set(PYMAJMINVER "2.7")
  If(NOT "${PYTHONLIBS_VERSION_STRING}" STREQUAL "")
    string(SUBSTRING ${PYTHONLIBS_VERSION_STRING} 0 3 PYMAJMINVER)
  EndIf()

  # force the .so files into separate directories so we can keep both
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  # pull in the swig macros
  include(${SWIG_USE_FILE})
  include_directories(${PYTHON_INCLUDE_DIR} ${CMAKE_SOURCE_DIR})
  If(COMMAND swig_add_library)
    swig_add_library(iguanaIR${PYVER} LANGUAGE python
                     TYPE MODULE SOURCES ../iguanaIR.i)
  Else()
    swig_add_module(iguanaIR${PYVER} python ../iguanaIR.i)
  EndIf()
  swig_link_libraries(iguanaIR${PYVER} iguanaIR ${BASELIBS} ${PYTHON_LIBRARIES})

  # the target name is the same just in different output directories
  set_target_properties(_iguanaIR${PYVER} PROPERTIES OUTPUT_NAME _iguanaIR)

  # install the python support
  install(TARGETS _iguanaIR${PYVER}
          DESTINATION ${LIBDIR}/python${PYMAJMINVER}/site-packages)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/iguanaIR.py
          DESTINATION ${LIBDIR}/python${PYMAJMINVER}/site-packages)
Else()
  message(STATUS "  Not found")
EndIf()
