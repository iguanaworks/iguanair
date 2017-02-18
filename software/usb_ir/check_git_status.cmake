#
# CMakeLists support script
#
# What: Check if directory is git clean, warn if not.
#


execute_process(COMMAND git status -s
                 OUTPUT_VARIABLE STDOUT)
	     string(STRIP "${STDOUT}" STDOUT)
	     if (NOT "x_${STDOUT}" EQUAL "x_")
		 Message(WARNING "Warning: unclean git directory. git stash/commit/clean?")
    string(STRIP "${STDOUT}" STDOUT)
    Message(${STDOUT})
endif()
