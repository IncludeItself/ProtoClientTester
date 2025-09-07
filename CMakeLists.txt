cmake_minimum_required(VERSION 3.10)
project(ProtoClientTester)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Qt6_DIR "/home/wu/Qt/6.9.0/gcc_64/lib/cmake/Qt6")
# 查找Qt组件（对应.pro中的QT += ...）
find_package(Qt6 COMPONENTS Core Gui Widgets Network Concurrent REQUIRED)

# 查找Protobuf
find_package(Protobuf REQUIRED)

# 处理UI文件（对应.pro中的FORMS）
set(FORMS mainwindow.ui)
qt6_wrap_ui(UIS_HEADERS ${FORMS})  # 注意：Qt6使用qt6_wrap_ui，Qt5使用qt5_wrap_ui

# 处理包含Q_OBJECT的头文件（关键修复：生成moc代码）
set(HEADERS
        mainwindow.h
        networkmanager.h
        protoc/data_proto.pb.h
        protoc/error_code/common.pb.h
        protoc/error_code/network.pb.h
        protoclient.h
        sessionmanager.h
)
qt6_wrap_cpp(MOC_SOURCES ${HEADERS})  # 添加这行：通过moc处理头文件

# 源文件列表（对应.pro中的SOURCES和HEADERS）
set(SOURCES
        main.cpp
        mainwindow.cpp
        networkmanager.cpp
        protoc/data_proto.pb.cc
        protoc/error_code/common.pb.cc
        protoc/error_code/network.pb.cc
        protoclient.cpp
        sessionmanager.cpp
)

# 创建可执行目标（添加MOC_SOURCES和UIS_HEADERS）
add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${HEADERS}
        ${UIS_HEADERS}
        ${MOC_SOURCES}  # 添加生成的moc代码
)

# 添加protobuf头文件目录和构建目录（解决ui_mainwindow.h找不到的问题）
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/protoc
        ${CMAKE_CURRENT_BINARY_DIR}  # 添加构建目录，存放uic生成的ui_*.h文件
)

# 链接Qt和Protobuf库（对应.pro中的LIBS和PKGCONFIG）
target_link_libraries(${PROJECT_NAME}
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Concurrent
        protobuf::libprotobuf
)

# 可选：启用Qt翻译支持（对应.pro中的TRANSLATIONS相关配置）
# set(TRANSLATIONS ProtoClientTester_en_US.ts)
# qt5_add_translation(QM_FILES ${TRANSLATIONS})
# target_sources(${PROJECT_NAME} PRIVATE ${QM_FILES})

# 可选：禁用Qt6之前的过时API（对应.pro中的DEFINES += QT_DISABLE_DEPRECATED_BEFORE）
# target_compile_definitions(${PROJECT_NAME} PRIVATE QT_DISABLE_DEPRECATED_BEFORE=0x060000)
