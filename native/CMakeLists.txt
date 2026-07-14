# SPDX-License-Identifier: GPL-3.0-or-later

add_library(yohsai_cosserat SHARED
    src/c_api.cpp
    src/solver.cpp
)

target_include_directories(yohsai_cosserat
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(yohsai_cosserat PUBLIC cxx_std_20)
target_compile_definitions(yohsai_cosserat PRIVATE YSC_BUILD_DLL)

if(MSVC)
    target_compile_options(yohsai_cosserat PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(yohsai_cosserat PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(yohsai_cosserat PROPERTIES
    OUTPUT_NAME "yohsai_cosserat"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

install(TARGETS yohsai_cosserat
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)

if(BUILD_TESTING)
    add_executable(yohsai_cosserat_tests tests/test_solver.cpp)
    target_link_libraries(yohsai_cosserat_tests PRIVATE yohsai_cosserat)
    target_compile_features(yohsai_cosserat_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(yohsai_cosserat_tests PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(yohsai_cosserat_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    set_target_properties(yohsai_cosserat_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME yohsai_cosserat_tests COMMAND yohsai_cosserat_tests)
endif()
