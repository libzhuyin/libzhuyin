set(
    LIBPINYIN_HEADERS
    zhuyin.h
)

set(
    LIBPINYIN_SOURCES
    zhuyin.cpp
)

add_library(
    libzhuyin
    SHARED
    ${LIBPINYIN_SOURCES}
)

target_link_libraries(
    libzhuyin
    storage
    lookup
)

set_target_properties(
    libzhuyin
    PROPERTIES
        OUTPUT_NAME
            pinyin
        VERSION
            0.0.0
        SOVERSION
            0
)

install(
    TARGETS
        libzhuyin
    LIBRARY DESTINATION
        ${DIR_LIBRARY}
)

install(
    FILES
        ${LIBPINYIN_HEADERS}
    DESTINATION
        ${DIR_INCLUDE_LIBPINYIN}
)

add_subdirectory(include)
add_subdirectory(storage)
add_subdirectory(lookup)
