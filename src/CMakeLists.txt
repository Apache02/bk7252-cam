# libs
add_subdirectory(boards)
add_subdirectory(platform)
add_subdirectory(utils)
add_subdirectory(shell)

foreach(parent applications tests)
    file(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/${parent}
            CONFIGURE_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/${parent}/*)

    message(STATUS "${parent}:")
    foreach(child ${children})
        if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${parent}/${child} AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${parent}/${child}/CMakeLists.txt)
            message(STATUS "  - ${child}")
            add_subdirectory(${parent}/${child})
        endif()
    endforeach()
endforeach()
