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
    find_program(LUDASH_RUSTC rustc REQUIRED)
    execute_process(COMMAND "${LUDASH_RUSTC}" --version
        OUTPUT_VARIABLE LUDASH_RUST_VERSION_TEXT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE LUDASH_RUST_VERSION_RESULT)
    if(NOT LUDASH_RUST_VERSION_RESULT EQUAL 0 OR
       NOT LUDASH_RUST_VERSION_TEXT MATCHES "rustc ([0-9]+\\.[0-9]+)")
        message(FATAL_ERROR "Could not determine the Rust compiler version required by SlintLu")
    endif()
    if(CMAKE_MATCH_1 VERSION_LESS "1.92")
        message(FATAL_ERROR "SlintLu (Slint) requires Rust 1.92 or newer; found ${LUDASH_RUST_VERSION_TEXT}")
    endif()
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
