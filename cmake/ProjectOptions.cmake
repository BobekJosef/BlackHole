# Per-target settings for the project's own code: strict warnings and optional sanitizers.
# Third-party targets never get these; their headers are included as SYSTEM.

function(blackhole_configure_target target)
    if(MSVC)
        set(warnings /W4 /permissive- /w14265 /w14640 /w14826 /w14928)
        if(BLACKHOLE_WERROR)
            list(APPEND warnings /WX)
        endif()
    else()
        set(warnings
            -Wall -Wextra -Wpedantic
            -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
            -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wsuggest-override
            -Wcast-align -Wnull-dereference -Wformat=2 -Wimplicit-fallthrough -Wextra-semi)
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            list(APPEND warnings
                -Wuseless-cast -Wduplicated-cond -Wduplicated-branches -Wlogical-op)
        endif()
        if(BLACKHOLE_WERROR)
            list(APPEND warnings -Werror)
        endif()
    endif()
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${warnings}>)

    if(BLACKHOLE_SANITIZE AND NOT MSVC)
        set(sanitizers -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
        target_compile_options(${target} PRIVATE ${sanitizers})
        target_link_options(${target} PRIVATE ${sanitizers})
    endif()
endfunction()
