macro (ARTS_TEST_RUN_CTLFILE TESTNAME CTLFILE)
  if (CMAKE_MAJOR_VERSION EQUAL 2 AND CMAKE_MINOR_VERSION EQUAL 6)
    set(ARTS ${CMAKE_CURRENT_BINARY_DIR}/../src/arts -r002)
    add_test(
      arts.ctlfile.${TESTNAME}
      COMMAND ${ARTS} ${CMAKE_CURRENT_SOURCE_DIR}/${CTLFILE}
      )
  else (CMAKE_MAJOR_VERSION EQUAL 2 AND CMAKE_MINOR_VERSION EQUAL 6)
    set(ARTS arts -r002)
    add_test(
      NAME arts.ctlfile.${TESTNAME}
      COMMAND ${ARTS} ${CMAKE_CURRENT_SOURCE_DIR}/${CTLFILE}
      )
  endif (CMAKE_MAJOR_VERSION EQUAL 2 AND CMAKE_MINOR_VERSION EQUAL 6)
endmacro (ARTS_TEST_RUN_CTLFILE)

macro (ARTS_TEST_CMDLINE TESTNAME OPTIONS)
  if (CMAKE_MAJOR_VERSION EQUAL 2 AND CMAKE_MINOR_VERSION EQUAL 6)
    set(ARTS ${CMAKE_CURRENT_BINARY_DIR}/../src/arts)
    add_test(
      arts.cmdline.${TESTNAME}
      COMMAND ${ARTS} ${OPTIONS} ${ARGN}
      )
  else (CMAKE_MAJOR_VERSION EQUAL 2 AND CMAKE_MINOR_VERSION EQUAL 6)
    set(ARTS arts)
    add_test(
      NAME arts.cmdline.${TESTNAME}
      COMMAND ${ARTS} ${OPTIONS} ${ARGN}
      )
  endif (CMAKE_MAJOR_VERSION EQUAL 2 AND CMAKE_MINOR_VERSION EQUAL 6)
endmacro (ARTS_TEST_CMDLINE TESTNAME OPTIONS)

macro (ARTS_TEST_CTLFILE_DEPENDS TESTNAME DEPENDNAME)
  set_tests_properties(
    arts.ctlfile.${TESTNAME}
    PROPERTIES DEPENDS arts.ctlfile.${DEPENDNAME}
    )
endmacro (ARTS_TEST_CTLFILE_DEPENDS)

