include_guard(GLOBAL)

# logman_enable_sanitizers(<target>)
#
# Adds AddressSanitizer + UndefinedBehaviorSanitizer flags to <target> when
# LOGMAN_ENABLE_SANITIZERS is ON and the toolchain is GCC or Clang.
function(logman_enable_sanitizers target)
    if(NOT LOGMAN_ENABLE_SANITIZERS)
        return()
    endif()

    if(MSVC)
        message(STATUS "logman: sanitizers requested but skipped on MSVC")
        return()
    endif()

    set(_san_flags
        -fsanitize=address
        -fsanitize=undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )

    target_compile_options(${target} PRIVATE ${_san_flags})
    target_link_options   (${target} PRIVATE ${_san_flags})
endfunction()
