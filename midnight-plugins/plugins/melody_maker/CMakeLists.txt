# ── Shared include directory (algo.h) ─────────────────────────────────────────
set(_common_dir ${CMAKE_SOURCE_DIR}/common)

# ── CLAP ──────────────────────────────────────────────────────────────────────
add_library(melody_maker_clap MODULE plugin.cpp)

target_link_libraries(melody_maker_clap PRIVATE clap)

# A .clap file is just a shared library renamed.
set_target_properties(melody_maker_clap PROPERTIES
    PREFIX ""
    OUTPUT_NAME "MidnightMelodyMaker"
    SUFFIX ".clap"
    POSITION_INDEPENDENT_CODE ON
)

# On macOS a CLAP plugin is normally a bundle, but for now we ship the dylib.
if(APPLE)
    set_target_properties(melody_maker_clap PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
    )
endif()

# Convenience: copy the built plugin into the user CLAP folder so Zrythm sees it.
if(WIN32)
    set(_user_clap_dir "$ENV{LOCALAPPDATA}/Programs/Common/CLAP")
elseif(APPLE)
    set(_user_clap_dir "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP")
else()
    set(_user_clap_dir "$ENV{HOME}/.clap")
endif()

add_custom_target(install_user_clap
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_user_clap_dir}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:melody_maker_clap>" "${_user_clap_dir}/"
    DEPENDS melody_maker_clap
    COMMENT "Copying MidnightMelodyMaker.clap to ${_user_clap_dir}"
)

# ── VST3 ──────────────────────────────────────────────────────────────────────
set(_vst3_name "MidnightMelodyMaker")

# vstsinglecomponenteffect.cpp ships with the SDK but isn't included in the
# default `sdk` static library, so we add it directly to our target.
set(_vst3_extra_sources
    "${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/vstsinglecomponenteffect.cpp"
)

add_library(midnight_melody_vst3 MODULE plugin_vst3.cpp ${_vst3_extra_sources})

target_include_directories(midnight_melody_vst3 PRIVATE
    ${_common_dir}
    ${vst3sdk_SOURCE_DIR}
)

target_link_libraries(midnight_melody_vst3 PRIVATE sdk)

set_target_properties(midnight_melody_vst3 PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${_vst3_name}"
    SUFFIX ".vst3"
    POSITION_INDEPENDENT_CODE ON
)

# Ensure source / runtime strings are interpreted as UTF-8 on MSVC so that
# accented French labels (é, è, à, ç, …) display correctly in the GUI.
if(MSVC)
    target_compile_options(midnight_melody_vst3 PRIVATE /utf-8)
endif()

if(WIN32)
    # Assemble the VST3 bundle directory next to the build output.
    set(_vst3_bundle "${CMAKE_BINARY_DIR}/VST3/${_vst3_name}.vst3")
    add_custom_command(TARGET midnight_melody_vst3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_vst3_bundle}/Contents/x86_64-win"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:midnight_melody_vst3>"
            "${_vst3_bundle}/Contents/x86_64-win/${_vst3_name}.vst3"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_vst3_bundle}/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/../soundfonts/GeneralUser GS 1.472/GeneralUser GS v1.472.sf2"
            "${_vst3_bundle}/Contents/Resources/GeneralUser.sf2"
        COMMENT "Assembling VST3 bundle → ${_vst3_bundle}"
    )

    # Install to the system VST3 directory that FL Studio scans.
    set(_vst3_system_dir "$ENV{COMMONPROGRAMFILES}/VST3")
    add_custom_target(install_vst3
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_vst3_system_dir}/${_vst3_name}.vst3/Contents/x86_64-win"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:midnight_melody_vst3>"
            "${_vst3_system_dir}/${_vst3_name}.vst3/Contents/x86_64-win/${_vst3_name}.vst3"
        DEPENDS midnight_melody_vst3
        COMMENT "Installing ${_vst3_name}.vst3 to ${_vst3_system_dir}"
    )
endif()
