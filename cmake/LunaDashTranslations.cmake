# Bundle every feature catalog and future language pack. CONFIGURE_DEPENDS
# reruns CMake when a catalog is added, so a passing source check cannot leave
# newly translated pages absent from installed Qt resources.
set(ludash_translation_root "${CMAKE_CURRENT_LIST_DIR}/../data/translations")
file(GLOB_RECURSE ludash_feature_catalogs CONFIGURE_DEPENDS
    "${ludash_translation_root}/*.json")
# These two original catalogs are in the existing `translations` resource.
list(REMOVE_ITEM ludash_feature_catalogs
    "${ludash_translation_root}/en_US.json"
    "${ludash_translation_root}/zh_TW.json")
qt_add_resources(ludash-localization desktop_translations
    PREFIX /LuDash
    BASE ${CMAKE_CURRENT_LIST_DIR}/..
    FILES ${ludash_feature_catalogs}
)
if(TARGET ludash-file-operations)
    target_link_libraries(ludash-file-operations PUBLIC ludash-localization)
endif()
