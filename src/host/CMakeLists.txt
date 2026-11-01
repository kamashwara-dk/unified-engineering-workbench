# Launch: Win32 + WebView2 host. Windows only; WIN32 subsystem (no console window).
if(WIN32)
    add_executable(Launch WIN32
        main.cpp
        Launch.rc
    )
    target_link_libraries(Launch PRIVATE
        workbench_server_lib
        webview2_loader
        # Ole32 for CoInitializeEx; Shell32 for ShellExecuteW; Shlwapi for path helpers.
        Ole32 Shell32 Shlwapi
    )
    # /utf-8: source and execution charset UTF-8 (Windows needs this explicitly).
    target_compile_options(Launch PRIVATE /utf-8)
    wb_set_warnings(Launch)
endif()