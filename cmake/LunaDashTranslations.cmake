# Feature catalogs supplement the original language pack. Keep these explicit so
# both source archives and installed binaries include the same translations.
qt_add_resources(ludash-localization desktop_translations
    PREFIX /LuDash
    BASE ${CMAKE_CURRENT_LIST_DIR}/..
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/../data/translations/zh_TW/desktop.json
        ${CMAKE_CURRENT_LIST_DIR}/../data/translations/zh_TW/files.json
        ${CMAKE_CURRENT_LIST_DIR}/../data/translations/zh_TW/settings.json
        ${CMAKE_CURRENT_LIST_DIR}/../data/translations/zh_TW/plugins.json
        ${CMAKE_CURRENT_LIST_DIR}/../data/translations/zh_TW/network_devices.json
        ${CMAKE_CURRENT_LIST_DIR}/../data/translations/zh_TW/file_associations.json
        ${CMAKE_CURRENT_LIST_DIR}/../data/translations/zh_TW/file_actions.json
)
if(TARGET ludash-file-operations)
    target_link_libraries(ludash-file-operations PUBLIC ludash-localization)
endif()
