add_library(platform_tlv INTERFACE)

target_sources(
        platform_tlv INTERFACE
        tlv.c
        manual_cal.c
)

target_include_directories(
        platform_tlv INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)
