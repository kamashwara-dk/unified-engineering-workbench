# Microsoft.Web.WebView2 SDK 1.0.4314.35 — static loader, x64 only
# Licence: third_party/webview2/LICENSE.txt (Microsoft SDK licence)
# Only available on Windows; guard at point of use with if(WIN32).
if(WIN32)
    add_library(webview2_loader STATIC IMPORTED GLOBAL)
    set_target_properties(webview2_loader PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/WebView2LoaderStatic.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()