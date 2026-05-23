include_guard(GLOBAL)

# logman_enable_coverage(<target>)
#
# Adds Clang source-based coverage flags to <target> when
# LOGMAN_ENABLE_COVERAGE is ON. No-op on MSVC.
function(logman_enable_coverage target)
    if(NOT LOGMAN_ENABLE_COVERAGE)
        return()
    endif()
    if(MSVC)
        message(STATUS "logman: coverage requested but skipped on MSVC")
        return()
    endif()

    set(_cov_flags -fprofile-instr-generate -fcoverage-mapping)
    target_compile_options(${target} PRIVATE ${_cov_flags} -O0 -g)
    target_link_options   (${target} PRIVATE ${_cov_flags})
endfunction()
