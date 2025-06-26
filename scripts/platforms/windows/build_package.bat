@echo off
REM =============================================================================
REM Windows 构建脚本 - PerfxAgent
REM =============================================================================

setlocal enabledelayedexpansion

REM 设置颜色代码
set "RED=[91m"
set "GREEN=[92m"
set "YELLOW=[93m"
set "BLUE=[94m"
set "NC=[0m"

REM 日志函数
:log_info
echo %BLUE%[INFO]%NC% %~1
goto :eof

:log_success
echo %GREEN%[SUCCESS]%NC% %~1
goto :eof

:log_warning
echo %YELLOW%[WARNING]%NC% %~1
goto :eof

:log_error
echo %RED%[ERROR]%NC% %~1
goto :eof

REM 检查依赖
:check_dependencies
call :log_info "Checking build dependencies..."

REM 检查CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    call :log_error "CMake is not installed or not in PATH"
    exit /b 1
)

REM 检查编译器
where cl >nul 2>&1
if %errorlevel% equ 0 (
    set "COMPILER=MSVC"
    call :log_info "Found MSVC compiler"
) else (
    where g++ >nul 2>&1
    if %errorlevel% equ 0 (
        set "COMPILER=MinGW"
        call :log_info "Found MinGW compiler"
    ) else (
        call :log_error "No C++ compiler found (MSVC or MinGW required)"
        exit /b 1
    )
)

REM 检查Qt6
set "QT_FOUND=false"
for %%p in (
    "C:\Qt\6.9.0\msvc2019_64"
    "C:\Qt\6.8.0\msvc2019_64"
    "C:\Qt\6.7.0\msvc2019_64"
    "C:\Qt\6.6.0\msvc2019_64"
    "C:\Qt\6.5.0\msvc2019_64"
    "C:\Qt\6.4.0\msvc2019_64"
    "C:\Qt\6.3.0\msvc2019_64"
    "C:\Qt\6.2.0\msvc2019_64"
    "C:\Qt\6.1.0\msvc2019_64"
    "C:\Qt\6.0.0\msvc2019_64"
) do (
    if exist "%%p\lib\cmake\Qt6" (
        set "QT_PATH=%%p"
        set "QT_FOUND=true"
        call :log_info "Found Qt6 at: %%p"
        goto :qt_found
    )
)

:qt_found
if "%QT_FOUND%"=="false" (
    call :log_error "Qt6 not found. Please install Qt6 from https://qt.io"
    exit /b 1
)

call :log_success "All dependencies are available"
goto :eof

REM 清理构建目录
:clean_build
call :log_info "Cleaning build directory..."
if exist build rmdir /s /q build
mkdir build
call :log_success "Build directory cleaned"
goto :eof

REM 构建项目
:build_project
call :log_info "Building project..."

cd build

REM 检查构建类型
set "BUILD_TYPE=Release"
if "%1"=="debug" (
    set "BUILD_TYPE=Debug"
    call :log_info "Building in Debug mode (development)"
) else (
    call :log_info "Building in Release mode (production)"
)

REM 配置项目
call :log_info "Configuring project with CMake (%BUILD_TYPE%)..."
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_PREFIX_PATH="%QT_PATH%"
if %errorlevel% neq 0 (
    call :log_error "CMake configuration failed"
    exit /b 1
)

REM 编译项目
call :log_info "Compiling project..."
cmake --build . --config %BUILD_TYPE% --parallel
if %errorlevel% neq 0 (
    call :log_error "Build failed"
    exit /b 1
)

cd ..
call :log_success "Project built successfully in %BUILD_TYPE% mode"
goto :eof

REM 测试应用程序
:test_application
call :log_info "Testing application..."

if not exist "build\bin\PerfxAgent-ASR.exe" (
    call :log_error "Application not found in build\bin\PerfxAgent-ASR.exe"
    exit /b 1
)

call :log_success "Application test completed"
goto :eof

REM 创建安装包
:create_package
call :log_info "Creating installation package..."
cd build

REM 使用CPack创建安装包
cpack -G ZIP
if %errorlevel% neq 0 (
    call :log_error "Package creation failed"
    exit /b 1
)

cd ..
call :log_success "Installation package created"
goto :eof

REM 验证安装包
:verify_package
call :log_info "Verifying installation package..."

set "PACKAGES_FOUND=0"
for %%f in (build\*.zip) do (
    call :log_success "Found package: %%~nxf"
    dir "%%f" | find "File(s)"
    set /a PACKAGES_FOUND+=1
)

if %PACKAGES_FOUND% equ 0 (
    call :log_error "No installation packages found"
    exit /b 1
)

call :log_success "Package verification completed"
goto :eof

REM 主函数
:main
call :log_info "Starting PerfxAgent Windows package build process..."

REM 解析命令行参数
set "BUILD_MODE=release"
if "%1"=="--debug" (
    set "BUILD_MODE=debug"
) else if "%1"=="--help" (
    echo Usage: %0 [--debug^|--help]
    echo   --debug    Build in debug mode (development^)
    echo   --help     Show this help message
    echo.
    echo Default: Build in release mode (production^)
    exit /b 0
)

call :log_info "Build mode: %BUILD_MODE%"

REM 检查依赖
call :check_dependencies
if %errorlevel% neq 0 exit /b 1

REM 清理构建目录
call :clean_build

REM 构建项目
call :build_project %BUILD_MODE%
if %errorlevel% neq 0 exit /b 1

REM 测试应用程序
call :test_application
if %errorlevel% neq 0 exit /b 1

REM 创建安装包
call :create_package
if %errorlevel% neq 0 exit /b 1

REM 验证安装包
call :verify_package
if %errorlevel% neq 0 exit /b 1

call :log_success "PerfxAgent Windows package build completed successfully!"
call :log_info "Installation packages are available in the build\ directory"
call :log_info "Build mode used: %BUILD_MODE%"

exit /b 0 