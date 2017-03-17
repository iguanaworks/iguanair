# Find the library / header in a known location or just fall back on
# (hopefully) the system install
If(NOT "${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
  If(EXISTS ${CMAKE_CURRENT_LIST_DIR}/../../../../../argp-standalone-1.3/)
    Set(ARGPDIR ${CMAKE_CURRENT_LIST_DIR}/../../../../../argp-standalone-1.3)
    include_directories(${ARGPDIR})
    Set(ARGPLIB argp)

    add_definitions(-DHAVE_MALLOC_H -D_CRT_SECURE_NO_WARNINGS)
    add_library(argp
      ${ARGPDIR}/argp-ba.c
      ${ARGPDIR}/argp-eexst.c
      ${ARGPDIR}/argp-fmtstream.c
      ${ARGPDIR}/argp-help.c
      ${ARGPDIR}/argp-parse.c
      ${ARGPDIR}/argp-pv.c
      ${ARGPDIR}/argp-pvh.c
      ${ARGPDIR}/mempcpy.c
      ${ARGPDIR}/strcasecmp.c
      ${ARGPDIR}/strchrnul.c
      ${ARGPDIR}/strndup.c
      ${ARGPDIR}/vsnprintf.c)

    If(NOT Argp_FIND_QUIETLY)
      Message(STATUS "Found argp in ${ARGPDIR}")
    EndIf()
  Else()
    If(NOT Argp_FIND_QUIETLY)
      Message(STATUS "Relying on system argp")
    EndIf()
  EndIf()
EndIf()

