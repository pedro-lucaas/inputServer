find_package(Python3 COMPONENTS Interpreter QUIET)
find_program(INPUTSERVER_TEST_OPENSSL openssl)
if(Python3_Interpreter_FOUND AND INPUTSERVER_TEST_OPENSSL)
    add_test(NAME macos-client-handshake
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/client_handshake.py
                $<TARGET_FILE:input-leapc> ${INPUTSERVER_TEST_OPENSSL})
    set_tests_properties(macos-client-handshake PROPERTIES TIMEOUT 35)
endif()
