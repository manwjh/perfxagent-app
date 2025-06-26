@echo off
REM =============================================================================
REM Windows 包验证脚本 - PerfxAgent
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

REM 全局变量
set "BUILD_DIR=build"
set "ZIP_FILE="
set "VERSION="

REM 检查结果统计
set "TOTAL_CHECKS=0"
set "PASSED_CHECKS=0"
set "FAILED_CHECKS=0"
set "WARNINGS=0"

REM 增加检查计数
:increment_check
set /a TOTAL_CHECKS+=1
goto :eof

REM 记录成功
:record_success
set /a PASSED_CHECKS+=1
call :log_success "%~1"
goto :eof

REM 记录失败
:record_failure
set /a FAILED_CHECKS+=1
call :log_error "%~1"
goto :eof

REM 记录警告
:record_warning
set /a WARNINGS+=1
call :log_warning "%~1"
goto :eof

REM 获取文件大小
:get_file_size
set "file_path=%~1"
for %%A in ("%file_path%") do set "size=%%~zA"
if %size% gtr 1073741824 (
    set /a "size_gb=%size% / 1073741824"
    echo %size_gb%GiB
) else if %size% gtr 1048576 (
    set /a "size_mb=%size% / 1048576"
    echo %size_mb%MiB
) else if %size% gtr 1024 (
    set /a "size_kb=%size% / 1024"
    echo %size_kb%KiB
) else (
    echo %size%B
)
goto :eof

REM 检测版本和包文件
:detect_version_and_packages
call :log_info "Detecting version and package files..."

REM 查找ZIP文件
for /r "%BUILD_DIR%" %%f in (*.zip) do (
    set "ZIP_FILE=%%f"
    call :log_info "Found ZIP: %%~nxf"
    
    REM 从文件名提取版本
    set "filename=%%~nf"
    for /f "tokens=2 delims=-" %%a in ("!filename!") do (
        set "version_part=%%a"
        for /f "tokens=1 delims=-" %%b in ("!version_part!") do (
            set "VERSION=%%b"
            call :log_success "Detected version: !VERSION!"
        )
    )
    goto :found_zip
)

:found_zip
call :increment_check
if defined VERSION (
    call :record_success "Version detection completed: !VERSION!"
) else (
    call :record_failure "Failed to detect version"
)
goto :eof

REM 检查安装包文件
:check_package_files
call :log_info "Checking package files..."

call :increment_check
if exist "!ZIP_FILE!" (
    for %%A in ("!ZIP_FILE!") do set "zip_size=%%~zA"
    call :log_info "ZIP size: !zip_size! bytes"
    if !zip_size! gtr 50000000 (
        call :record_success "ZIP file exists and size is reasonable"
    ) else (
        call :record_warning "ZIP file is smaller than expected"
    )
) else (
    call :record_failure "ZIP file not found"
)
goto :eof

REM 检查可执行文件
:check_executable
call :log_info "Checking executable file..."

call :increment_check
if exist "!BUILD_DIR!\bin\PerfxAgent-ASR.exe" (
    for %%A in ("!BUILD_DIR!\bin\PerfxAgent-ASR.exe") do set "exe_size=%%~zA"
    call :log_info "Executable size: !exe_size! bytes"
    if !exe_size! gtr 100000 (
        call :record_success "Executable exists and size is reasonable"
    ) else (
        call :record_warning "Executable is smaller than expected"
    )
) else (
    call :record_failure "Executable not found"
)
goto :eof

REM 检查ZIP包内容
:check_zip_contents
call :log_info "Checking ZIP package contents..."

REM 创建临时目录
set "temp_dir=%TEMP%\perfxagent_zip_extract"
if exist "!temp_dir!" rmdir /s /q "!temp_dir!"
mkdir "!temp_dir!"

REM 解压ZIP文件
call :log_info "Extracting ZIP package for validation..."
powershell -command "Expand-Archive -Path '!ZIP_FILE!' -DestinationPath '!temp_dir!' -Force"

if %errorlevel% equ 0 (
    call :log_success "ZIP package extracted successfully"
    
    REM 查找可执行文件
    if exist "!temp_dir!\PerfxAgent-ASR.exe" (
        call :record_success "Found executable in ZIP package"
        
        REM 检查文件类型
        file "!temp_dir!\PerfxAgent-ASR.exe" | findstr /i "PE32" >nul
        if %errorlevel% equ 0 (
            call :record_success "Executable is valid Windows PE32 file"
        ) else (
            call :record_warning "Executable file type check failed"
        )
    ) else (
        call :record_failure "Executable not found in ZIP package"
    )
    
    REM 检查依赖文件
    call :check_windows_dependencies
    
    REM 检查文档文件
    if exist "!temp_dir!\README.md" (
        call :record_success "README.md found in package"
    ) else (
        call :record_warning "README.md not found in package"
    )
    
    if exist "!temp_dir!\LICENSE" (
        call :record_success "LICENSE found in package"
    ) else (
        call :record_warning "LICENSE not found in package"
    )
    
    REM 显示包内容
    call :log_info "Package contents:"
    dir "!temp_dir!" /b /s
    
) else (
    call :record_failure "Failed to extract ZIP package"
)

REM 清理临时目录
if exist "!temp_dir!" rmdir /s /q "!temp_dir!"
goto :eof

REM 检查Windows依赖
:check_windows_dependencies
call :log_info "Checking Windows dependencies..."

REM 检查Qt DLLs
set "qt_dlls=0"
for /r "!temp_dir!" %%f in (Qt*.dll) do set /a qt_dlls+=1
if !qt_dlls! gtr 0 (
    call :record_success "Found !qt_dlls! Qt DLLs"
) else (
    call :record_warning "No Qt DLLs found"
)

REM 检查音频库
set "audio_libs_found=0"
for %%dll in (portaudio.dll opus.dll ogg.dll sndfile.dll) do (
    if exist "!temp_dir!\%%dll" (
        set /a audio_libs_found+=1
        call :record_success "Found audio library: %%dll"
    ) else (
        call :record_warning "Audio library not found: %%dll"
    )
)

REM 检查OpenSSL
if exist "!temp_dir!\libssl*.dll" (
    call :record_success "Found OpenSSL SSL library"
) else (
    call :record_warning "OpenSSL SSL library not found"
)

if exist "!temp_dir!\libcrypto*.dll" (
    call :record_success "Found OpenSSL Crypto library"
) else (
    call :record_warning "OpenSSL Crypto library not found"
)

REM 检查插件目录
if exist "!temp_dir!\plugins" (
    set "plugin_count=0"
    for /r "!temp_dir!\plugins" %%f in (*.dll) do set /a plugin_count+=1
    call :record_success "Found !plugin_count! plugins"
) else (
    call :record_warning "No plugins directory found"
)
goto :eof

REM 检查版本信息
:check_version_info
call :log_info "Checking version information..."

REM 检查可执行文件的版本信息
if exist "!BUILD_DIR!\bin\PerfxAgent-ASR.exe" (
    call :increment_check
    REM 使用PowerShell获取文件版本信息
    for /f "tokens=*" %%i in ('powershell -command "Get-ItemProperty '!BUILD_DIR!\bin\PerfxAgent-ASR.exe' | Select-Object -ExpandProperty VersionInfo | Select-Object -ExpandProperty FileVersion"') do (
        set "file_version=%%i"
    )
    if defined file_version (
        call :record_success "Executable version info found: !file_version!"
    ) else (
        call :record_warning "No version info found in executable"
    )
) else (
    call :record_failure "Cannot check version - executable not found"
)
goto :eof

REM 生成检查报告
:generate_report
call :log_info "Generating Windows package validation report..."

echo.
echo ==========================================
echo      WINDOWS PACKAGE VALIDATION REPORT
echo ==========================================
echo Platform: Windows
echo Version: !VERSION!
echo Build Directory: !BUILD_DIR!
echo Timestamp: %date% %time%
echo.
echo Check Results:
echo   Total Checks: !TOTAL_CHECKS!
echo   Passed: !PASSED_CHECKS!
echo   Failed: !FAILED_CHECKS!
echo   Warnings: !WARNINGS!
echo.

if !FAILED_CHECKS! equ 0 (
    if !WARNINGS! equ 0 (
        call :log_success "🎉 ALL CHECKS PASSED! Windows package is ready for distribution."
        exit /b 0
    ) else (
        call :log_warning "⚠️  ALL CHECKS PASSED with warnings. Windows package is ready for distribution."
        exit /b 0
    )
) else (
    call :log_error "❌ VALIDATION FAILED! Please fix the issues before distribution."
    exit /b 1
)
goto :eof

REM 主函数
:main
call :log_info "Starting Windows package validation..."

REM 检查构建目录
if not exist "!BUILD_DIR!" (
    call :log_error "Build directory not found: !BUILD_DIR!"
    exit /b 1
)

REM 执行所有检查
call :detect_version_and_packages
call :check_package_files
call :check_executable
call :check_zip_contents
call :check_version_info

REM 生成报告
call :generate_report

call :log_success "Windows package validation completed!"
exit /b 0 