include_guard(GLOBAL)

# logman_enable_tsan(<target>)
#
# Adds ThreadSanitizer flags to <target> when LOGMAN_ENABLE_TSAN is ON.
# Mutually exclusive with LOGMAN_ENABLE_SANITIZERS (ASan + TSan cannot coexist).
function(logman_enable_tsan target)
    if(NOT LOGMAN_ENABLE_TSAN)
        return()
    endif()

    if(MSVC)
        message(STATUS "logman: ThreadSanitizer requested but skipped on MSVC")
        return()
    endif()

    if(LOGMAN_ENABLE_SANITIZERS)
        message(FATAL_ERROR
            "logman: LOGMAN_ENABLE_TSAN and LOGMAN_ENABLE_SANITIZERS are mutually exclusive. "
            "Use separate build directories.")
    endif()

    set(_tsan_flags
        -fsanitize=thread
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )

    target_compile_options(${target} PRIVATE ${_tsan_flags})
    target_link_options   (${target} PRIVATE ${_tsan_flags})
endfunction()
