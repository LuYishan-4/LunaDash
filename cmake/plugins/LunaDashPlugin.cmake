include_guard(GLOBAL)
include(GNUInstallDirs)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(LUNADASH_PLUGIN_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(lunadash_add_plugin target)
    cmake_parse_arguments(PLUGIN "" "METADATA" "SOURCES;FILES" ${ARGN})
    if(NOT PLUGIN_METADATA OR PLUGIN_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "lunadash_add_plugin requires METADATA and explicit SOURCES/FILES")
    endif()
    get_filename_component(metadata "${PLUGIN_METADATA}" ABSOLUTE)
    get_filename_component(source_dir "${metadata}" DIRECTORY)
    file(READ "${metadata}" manifest)
    string(JSON id ERROR_VARIABLE json_error GET "${manifest}" id)
    if(json_error OR NOT id MATCHES "^[a-zA-Z0-9][a-zA-Z0-9._-]+$")
        message(FATAL_ERROR "Invalid LunaDash plugin ID in ${metadata}")
    endif()
    set(output "${CMAKE_BINARY_DIR}/plugins/${id}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${metadata}" "${LUNADASH_PLUGIN_CMAKE_DIR}/ValidatePlugin.py"
        "${LUNADASH_PLUGIN_CMAKE_DIR}/SettingsSchema.py" "${LUNADASH_PLUGIN_TARGETS}")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${LUNADASH_PLUGIN_CMAKE_DIR}/ValidatePlugin.py"
        --metadata "${metadata}" --targets "${LUNADASH_PLUGIN_TARGETS}" --output "${output}"
        RESULT_VARIABLE validation COMMAND_ERROR_IS_FATAL ANY)
    string(JSON kind GET "${manifest}" type)
    if(kind STREQUAL "effect")
        if(NOT PLUGIN_SOURCES)
            message(FATAL_ERROR "Native effects require explicit SOURCES")
        endif()
        string(JSON entry GET "${manifest}" entry)
        string(REGEX REPLACE "\\.so$" "" library "${entry}")
        add_library(${target} MODULE ${PLUGIN_SOURCES} "${output}/Registration.c" "${LUNADASH_PLUGIN_API_SOURCE}")
        target_include_directories(${target} PRIVATE "${LUNADASH_PLUGIN_INCLUDE_DIR}")
        set_target_properties(${target} PROPERTIES
            C_STANDARD 11 C_STANDARD_REQUIRED ON CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
            PREFIX "" OUTPUT_NAME "${library}" SUFFIX ".so"
            LIBRARY_OUTPUT_DIRECTORY "${output}" C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
        install(TARGETS ${target} LIBRARY DESTINATION "${CMAKE_INSTALL_DATADIR}/lunadash/plugins/${id}")
    elseif(kind STREQUAL "opengl")
        # Qt Quick 6 consumes baked GLSL packages; its OpenGL scene graph uses
        # the GLSL variants within them. Software rendering keeps the built-in.
        find_package(Qt6 6.4 REQUIRED COMPONENTS ShaderTools)
        set(shaders)
        foreach(stage IN ITEMS vertex fragment)
            string(JSON shader GET "${manifest}" shaders ${stage})
            configure_file("${source_dir}/${shader}" "${output}/${shader}" COPYONLY)
            add_custom_command(OUTPUT "${output}/${shader}.qsb"
                COMMAND Qt6::qsb --glsl "100 es,120,150" -o "${output}/${shader}.qsb" "${source_dir}/${shader}"
                DEPENDS "${source_dir}/${shader}" VERBATIM)
            list(APPEND shaders "${output}/${shader}.qsb")
            install(FILES "${output}/${shader}" "${output}/${shader}.qsb"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/lunadash/plugins/${id}")
        endforeach()
        add_custom_target(${target} ALL DEPENDS ${shaders})
    else()
        string(JSON entry GET "${manifest}" entry)
        list(APPEND PLUGIN_FILES "${source_dir}/${entry}")
        add_custom_target(${target} ALL)
    endif()

    # Package-local image icons are declared in metadata just like QML/JS
    # assets. The SDK copies them automatically so plugin authors do not need
    # a second install rule merely to upload an icon with the plugin.
    string(JSON icon ERROR_VARIABLE icon_error GET "${manifest}" icon)
    if(NOT icon_error)
        string(TOLOWER "${icon}" icon_lower)
        if(icon_lower MATCHES "\\.(png|jpg|jpeg|webp|svg)$")
            list(APPEND PLUGIN_FILES "${source_dir}/${icon}")
        endif()
    endif()
    list(REMOVE_DUPLICATES PLUGIN_FILES)

    foreach(file IN LISTS PLUGIN_FILES)
        get_filename_component(name "${file}" NAME)
        if(name MATCHES "^(metadata\\.json|\\.lunadash-sdk\\.json|Registration\\.c)$")
            message(FATAL_ERROR "Reserved plugin file: ${name}")
        endif()
        configure_file("${file}" "${output}/${name}" COPYONLY)
        install(FILES "${output}/${name}" DESTINATION "${CMAKE_INSTALL_DATADIR}/lunadash/plugins/${id}")
    endforeach()
    install(FILES "${output}/metadata.json" "${output}/.lunadash-sdk.json"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/lunadash/plugins/${id}")
endfunction()
