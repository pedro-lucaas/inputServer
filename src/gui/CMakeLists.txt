
find_package (Qt6 ${REQUIRED_QT_VERSION} COMPONENTS Core Widgets Network LinguistTools REQUIRED)

set (CMAKE_AUTOMOC ON)
set (CMAKE_AUTORCC ON)
set (CMAKE_AUTOUIC ON)
set (CMAKE_INCLUDE_CURRENT_DIR ON)

# files that are used both in tests and the app
set(GUI_COMMON_SOURCE_FILES
    src/Action.cpp
    src/Hotkey.cpp
    src/KeySequence.cpp
)

set(GUI_COMMON_HEADER_FILES
    src/Action.h
    src/Hotkey.h
    src/KeySequence.h
)

set(GUI_SOURCE_FILES
    src/AboutDialog.cpp
    src/ActionDialog.cpp
    src/AddClientDialog.cpp
    src/AppConfig.cpp
    src/AppLocale.cpp
    src/BaseConfig.cpp
    src/FingerprintAcceptDialog.cpp
    src/HotkeyDialog.cpp
    src/IpcClient.cpp
    src/Ipc.cpp
    src/IpcReader.cpp
    src/KeySequenceWidget.cpp
    src/LogWindow.cpp
    src/main.cpp
    src/MainWindow.cpp
    src/NewScreenWidget.cpp
    src/QInputLeapApplication.cpp
    src/QUtility.cpp
    src/Screen.cpp
    src/ScreenSettingsDialog.cpp
    src/ScreenSetupModel.cpp
    src/ScreenSetupView.cpp
    src/ServerConfig.cpp
    src/ServerBrowserDialog.cpp
    src/ServerConfigDialog.cpp
    src/SettingsDialog.cpp
    src/SslCertificate.cpp
    src/Theme.cpp
    src/TrashScreenWidget.cpp
    src/UdpDiscovery.cpp
)

set(GUI_HEADER_FILES
    src/AboutDialog.h
    src/ActionDialog.h
    src/AddClientDialog.h
    src/AppConfig.h
    src/AppConnectionState.h
    src/AppLocale.h
    src/BaseConfig.h
    src/ElevateMode.h
    src/HotkeyDialog.h
    src/IpcClient.h
    src/Ipc.h
    src/IpcReader.h
    src/KeySequenceWidget.h
    src/LogWindow.h
    src/MainWindow.h
    src/NewScreenWidget.h
    src/ProcessorArch.h
    src/QInputLeapApplication.h
    src/QUtility.h
    src/Screen.h
    src/ScreenSettingsDialog.h
    src/ScreenSetupModel.h
    src/ScreenSetupView.h
    src/ServerBrowserDialog.h
    src/ServerConfigDialog.h
    src/ServerConfig.h
    src/SettingsDialog.h
    src/SslCertificate.h
    src/Theme.h
    src/TrashScreenWidget.h
    src/UdpDiscovery.h
)

set(GUI_UI_FILES
    src/AboutDialog.ui
    src/ActionDialog.ui
    src/AddClientDialog.ui
    src/FingerprintAcceptDialog.ui
    src/HotkeyDialog.ui
    src/LogWindow.ui
    src/MainWindow.ui
    src/ScreenSettingsDialog.ui
    src/ServerConfigDialog.ui
    src/SettingsDialog.ui
)

set(TS_FILES
    res/lang/gui_af-ZA.ts
    res/lang/gui_ar.ts
    res/lang/gui_bg-BG.ts
    res/lang/gui_ca-AD.ts
    res/lang/gui_cs-CZ.ts
    res/lang/gui_cy.ts
    res/lang/gui_da.ts
    res/lang/gui_de.ts
    res/lang/gui_es.ts
    res/lang/gui_et-EE.ts
    res/lang/gui_fi.ts
    res/lang/gui_fr.ts
    res/lang/gui_gl.ts
    res/lang/gui_grk.ts
    res/lang/gui_he.ts
    res/lang/gui_hi.ts
    res/lang/gui_hr-HR.ts
    res/lang/gui_hu-HU.ts
    res/lang/gui_id.ts
    res/lang/gui_is-IS.ts
    res/lang/gui_it.ts
    res/lang/gui_ja-JP.ts
    res/lang/gui_ko.ts
    res/lang/gui_lt.ts
    res/lang/gui_lv.ts
    res/lang/gui_mr.ts
    res/lang/gui_nl-NL.ts
    res/lang/gui_no.ts
    res/lang/gui_pes-IR.ts
    res/lang/gui_pl-PL.ts
    res/lang/gui_pt-BR.ts
    res/lang/gui_pt-PT.ts
    res/lang/gui_ro.ts
    res/lang/gui_ru.ts
    res/lang/gui_si.ts
    res/lang/gui_sk-SK.ts
    res/lang/gui_sl-SI.ts
    res/lang/gui_sq-AL.ts
    res/lang/gui_sr.ts
    res/lang/gui_sv.ts
    res/lang/gui_th-TH.ts
    res/lang/gui_tr-TR.ts
    res/lang/gui_uk.ts
    res/lang/gui_ur.ts
    res/lang/gui_vi.ts
    res/lang/gui_zh-CN.ts
    res/lang/gui_zh-TW.ts
)

set_source_files_properties(${TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/res/lang")
qt_add_translation(QM_FILES ${TS_FILES} OPTIONS -silent)

# The following file includes X11 headers that later break Qt headers.
set_source_files_properties("src/DisplayIsValid.cpp" PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE)

if (WIN32)
    set (GUI_RC_FILES res/win/InputLeap.rc)
    set_property(SOURCE res/win/InputLeap.rc PROPERTY COMPILE_FLAGS /nologo)
endif()

add_executable (input-leap WIN32
    ${GUI_COMMON_SOURCE_FILES}
    ${GUI_COMMON_HEADER_FILES}
    ${GUI_SOURCE_FILES}
    ${GUI_HEADER_FILES}
    ${GUI_UI_FILES}
    ${GUI_RC_FILES}
    res/InputLeap.qrc
)

include_directories (./src)

target_link_libraries(input-leap
    net
    arch
    base
    io
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

target_link_libraries(input-leap OpenSSL::SSL OpenSSL::Crypto)
set_target_properties(input-leap PROPERTIES OUTPUT_NAME "InputServer")

target_compile_definitions (input-leap PRIVATE -DINPUTLEAP_VERSION_STAGE="${INPUTLEAP_VERSION_STAGE}")
target_compile_definitions (input-leap PRIVATE -DINPUTLEAP_REVISION="${INPUTLEAP_REVISION}")

if (BUILD_MSWINDOWS)
    set_target_properties (input-leap PROPERTIES LINK_FLAGS "/NODEFAULTLIB:LIBCMT")
endif()
if (BUILD_CARBON)
    find_library(APPSERVICES_LIB ApplicationServices)
    target_link_libraries(input-leap ${APPSERVICES_LIB})
endif()
if (BUILD_XWINDOWS)
    target_link_libraries (input-leap X11 Threads::Threads)
endif()

target_link_libraries (input-leap common)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    install(TARGETS input-leap DESTINATION ${INPUTLEAP_BUNDLE_BINARY_DIR})
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "BSD")
    install (TARGETS input-leap DESTINATION bin)
elseif(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
        set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
    endif()
    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)
    add_custom_command(
        TARGET input-leap POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_BINARY_DIR}/qtDeploy"
        COMMAND ${QT_DEPLOY_TOOL} --no-compiler-runtime --no-system-d3d-compiler
            --dir "${CMAKE_BINARY_DIR}/qtDeploy" $<TARGET_FILE:input-leap>
    )
    install(TARGETS input-leap DESTINATION .)
    install(
        DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/
        DESTINATION .
        FILES_MATCHING PATTERN "*.*"
    )
endif()

if(INPUTLEAP_BUILD_TESTS)
    add_executable(serverbrowser-tests
        test/ServerBrowserTests.cpp
        src/ServerBrowserDialog.cpp
        src/ServerBrowserDialog.h)
    target_link_libraries(serverbrowser-tests gtest Qt6::Widgets Qt6::Network ${libs})
    add_test(NAME serverbrowser-tests COMMAND serverbrowser-tests)
    set_tests_properties(serverbrowser-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_executable(client-enrollment-tests
        test/ClientEnrollmentTests.cpp
        src/ServerConfig.cpp src/Screen.cpp src/AddClientDialog.cpp src/BaseConfig.cpp
        src/AddClientDialog.h src/AddClientDialog.ui
        ${GUI_COMMON_SOURCE_FILES})
    target_include_directories(client-enrollment-tests PRIVATE src)
    target_link_libraries(client-enrollment-tests gtest Qt6::Widgets Qt6::Network ${libs})
    add_test(NAME client-enrollment-tests COMMAND client-enrollment-tests)
    set_tests_properties(client-enrollment-tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    set(GUI_TEST_SOURCE_FILES
        test/KeySequenceTests.cpp
        test/HotkeyTests.cpp
        test/UdpDiscoveryTests.cpp
        src/UdpDiscovery.cpp
        test/main.cpp
    )

    add_executable(guiunittests
        ${GUI_TEST_SOURCE_FILES}
        ${GUI_COMMON_SOURCE_FILES}
        ${GUI_COMMON_HEADER_FILES}
    )

    add_test(NAME guiunittests
             COMMAND guiunittests
             WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

    target_include_directories(guiunittests PUBLIC ../../ext)
    target_link_libraries(guiunittests gtest gmock Qt6::Core Qt6::Widgets Qt6::Network ${libs})
endif()
