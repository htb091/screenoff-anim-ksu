# Build ScreenOffAnim Zygisk module
# Prerequisites: Android NDK, Dobby in external/Dobby
# Usage: .\build.ps1

$NDK = "C:\Users\12466\AppData\Local\Android\Sdk\ndk\28.2.13676358"
$TOOLCHAIN = "$NDK\toolchains\llvm\prebuilt\windows-x86_64"
$BUILD = "build"
$OUTPUT = "module\zygisk\arm64-v8a.so"

Remove-Item -Recurse -Force $BUILD -ErrorAction SilentlyContinue

Write-Host "Configuring..."
& cmake -B $BUILD -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE="$NDK\build\cmake\android.toolchain.cmake" `
    -DANDROID_ABI=arm64-v8a `
    -DANDROID_PLATFORM=android-26 `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_MAKE_PROGRAM="$NDK\prebuilt\windows-x86_64\bin\ninja.exe" 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure failed. Trying without Ninja..."
    & cmake -B $BUILD `
        -DCMAKE_TOOLCHAIN_FILE="$NDK\build\cmake\android.toolchain.cmake" `
        -DANDROID_ABI=arm64-v8a `
        -DANDROID_PLATFORM=android-26 `
        -DCMAKE_BUILD_TYPE=Release 2>&1
}

Write-Host "Building..."
& cmake --build $BUILD --target screenoff-anim -j8 2>&1

$soFiles = Get-ChildItem -Recurse "$BUILD\libscreenoff-anim.so" | Select-Object -First 1
if ($soFiles) {
    New-Item -ItemType Directory -Force -Path "module\zygisk" | Out-Null
    Copy-Item $soFiles.FullName $OUTPUT -Force
    Write-Host "SUCCESS: $OUTPUT" -ForegroundColor Green
} else {
    $soFiles = Get-ChildItem -Recurse "$BUILD\*.so" | Select-Object -First 5 FullName
    Write-Host "Looking for .so files..."
    $soFiles | ForEach-Object { Write-Host $_.FullName }
    Write-Host "BUILD FAILED - no .so found" -ForegroundColor Red
}