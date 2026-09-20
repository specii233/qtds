cmake_minimum_required(VERSION 3.21) # 设置 CMake 的最低版本要求
project(qtds LANGUAGES CXX)  # 设置项目名称和使用的语言

set(CMAKE_CXX_STANDARD 17) # 设置 C++ 标准为 C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON) # 设置 C++ 标准为必需

# 让 CMake 自动调用 Qt 工具：moc 处理 Q_OBJECT（否则报 undefined reference to vtable）、
# uic 处理 .ui、rcc 处理 .qrc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# ---------- 依赖：凡是源码里 #include 的第三方库，都在这里显式声明 ----------
# 规则：一个库 = 一次 find_package + 一次 target_link_libraries，不允许"靠别的库顺带带上"
# （vcpkg 的每个包都会导出整个 include 前缀根，所以漏写也能编过——那是隐式依赖，必须避免）
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Core) # Qt6 图形界面 + OpenGL 控件
find_package(cpr CONFIG REQUIRED)                           # HTTP 客户端（libcurl 封装）
find_package(nlohmann_json CONFIG REQUIRED)                 # JSON 解析（头文件库，无链接产物）

# ---------- 目标 ----------
# 头文件也列进来：AUTOMOC 需要"看得见"含 Q_OBJECT 的头文件（gameLoop.h），IDE 也靠它建项目视图
# qtds.qrc：AUTORCC 会把着色器等资源编译进可执行文件（无需运行时拷贝 shader 文件）
add_executable(qtds
    main.cpp
    my3d.h
    my3d.cpp
    myCamera.h
    myCharacter.h
    myCharacter.cpp
    myLogManager.h
    myLogManager.cpp
    myMesh.h
    myMesh.cpp
    myModel.h
    myShader.h
    myShader.cpp
    gameLoop.h
    gameLoop.cpp
    qtds.qrc
)

target_link_libraries(qtds PRIVATE
    Qt6::Widgets
    Qt6::OpenGLWidgets
    Qt6::Core
    cpr::cpr
    nlohmann_json::nlohmann_json
)

# 构建后自动把 vcpkg 运行时 DLL 复制到 exe 旁边（libcpr/libcurl/zlib），免手动配 PATH
if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR)
    file(GLOB _VCPKG_RUNTIME_DLLS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/*.dll")
    if(_VCPKG_RUNTIME_DLLS)
        add_custom_command(TARGET qtds POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_VCPKG_RUNTIME_DLLS} $<TARGET_FILE_DIR:qtds>
            COMMENT "Copying vcpkg runtime DLLs next to qtds.exe")
    endif()
endif()

# 构建后自动部署 Qt 运行时（Qt6*.dll、platforms/qwindows.dll、MinGW 运行时），
# 这样 exe 双击/调试都不需要手动把 Qt bin 加进 PATH
if(WIN32)
    get_filename_component(_QT_BIN_DIR "${Qt6_DIR}/../../../bin" ABSOLUTE)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_QT_BIN_DIR}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET qtds POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations "$<TARGET_FILE:qtds>"
            COMMENT "Deploying Qt runtime next to qtds.exe")
    endif()
endif()
