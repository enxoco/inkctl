cmake_minimum_required(VERSION 3.16)

project(k8s_dashboard VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find the required Qt6 components
find_package(Qt6 REQUIRED COMPONENTS Quick)

# 2. Setup standard Qt project settings
qt_standard_project_setup(REQUIRES 6.5)

# 3. Define the executable and include the C++ source files
# Add KubectlRunner.cpp if you split the header/source, 
# otherwise just main.cpp if you keep it header-only.
qt_add_executable(k8s_dashboard 
    main.cpp
    KubectlRunner.h
)

# 4. Register the QML module
# URI is the "import" name used inside your QML files
qt_add_qml_module(k8s_dashboard
    URI k8s.dashboard
    VERSION 1.0
    QML_FILES Main.qml
)

# 5. Link the libraries
target_link_libraries(k8s_dashboard PRIVATE Qt6::Quick)

# 6. Deployment: This ensures the binary is easy to find after build
install(TARGETS k8s_dashboard
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
