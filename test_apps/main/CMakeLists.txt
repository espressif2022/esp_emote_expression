idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    WHOLE_ARCHIVE
)

set(RESOLUTION "320_240")
set(ASSETS_NAME "asset_test")
set(BOOT_ANIM_NAME "anim_360_360")

set(ASSETS_FILE "${CMAKE_BINARY_DIR}/${ASSETS_NAME}.bin")
build_speaker_assets_bin("anim_icon" ${RESOLUTION} ${ASSETS_FILE} ${CONFIG_MMAP_FILE_NAME_LENGTH})
message(STATUS "Generated emote assets: ${ASSETS_FILE} -> anim_icon partition")
esptool_py_flash_to_partition(flash "anim_icon" "${ASSETS_FILE}")

# Generate boot assets
set(BOOT_SRC "${BOOT_ANIM_NAME}.eaf")
set(BOOT_FILE "${CMAKE_BINARY_DIR}/${BOOT_ANIM_NAME}.bin")
build_boot_assets_bin("anim_boot" ${BOOT_SRC} ${BOOT_FILE} ${CONFIG_MMAP_FILE_NAME_LENGTH})
message(STATUS "Generated boot assets: ${BOOT_FILE} -> anim_boot partition")
esptool_py_flash_to_partition(flash "anim_boot" "${BOOT_FILE}")
