# SlintLu runs the LunaDash shell (shell.slint). LunaDash installs the SlintLu revision its
# shell was tested with to libexec/lunadash/slintlu, which the compositor prefers over any
# `slintlu` on PATH. Fetch the pinned source with scripts/fetch-slintlu.sh and configure with
# -DLUDASH_SLINTLU_SOURCE_DIR=<directory>; without it the session uses `slintlu` from PATH.
set(LUDASH_SLINTLU_SOURCE_DIR "" CACHE PATH
    "SlintLu checkout to build and install as the shell runtime (optional)")
if(LUDASH_SLINTLU_SOURCE_DIR)
    if(NOT EXISTS "${LUDASH_SLINTLU_SOURCE_DIR}/Cargo.toml")
        message(FATAL_ERROR "LUDASH_SLINTLU_SOURCE_DIR does not contain Cargo.toml: ${LUDASH_SLINTLU_SOURCE_DIR}")
    endif()
    find_program(LUDASH_CARGO cargo REQUIRED)
    set(LUDASH_SLINTLU_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/slintlu")
    set(LUDASH_SLINTLU_BINARY "${LUDASH_SLINTLU_TARGET_DIR}/release/slintlu")
    # Cargo tracks its own inputs; always ask it, it returns quickly when nothing changed.
    add_custom_target(ludash-slintlu ALL
        COMMAND "${LUDASH_CARGO}" build --release --locked --bin slintlu
            --manifest-path "${LUDASH_SLINTLU_SOURCE_DIR}/Cargo.toml"
            --target-dir "${LUDASH_SLINTLU_TARGET_DIR}"
        BYPRODUCTS "${LUDASH_SLINTLU_BINARY}"
        USES_TERMINAL
        COMMENT "Building SlintLu, the LunaDash shell runtime")
    install(PROGRAMS "${LUDASH_SLINTLU_BINARY}"
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/lunadash)
endif()
