idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
)

set(RESOLUTION "320_240")
set(ASSETS_NAME "asset_test")
set(BOOT_ANIM_NAME "anim_360_360")

if (CONFIG_EXAMPLE_EXPRESSIONS_ENABLE_EMOTE)
    set(ASSETS_FILE "${CMAKE_BINARY_DIR}/${ASSETS_NAME}.bin")
    build_speaker_assets_bin("anim_icon" ${RESOLUTION} ${ASSETS_FILE} ${CONFIG_MMAP_FILE_NAME_LENGTH})
    message(STATUS "Generated emote assets: ${ASSETS_FILE} -> anim_icon partition")
    esptool_py_flash_to_partition(flash "anim_icon" "${ASSETS_FILE}")
else()
    message(STATUS "Emote is not enabled, skipping emote assets generation")
endif()


spiffs_create_partition_image(storage ../spiffs FLASH_IN_PROJECT)
