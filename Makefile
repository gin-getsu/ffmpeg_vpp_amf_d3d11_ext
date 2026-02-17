# === Path definitions ===
ROOT := /d/Data/ffmpeg_vpp_amf_d3d11_ext
INSTALL_TO := /d/utils/ffmpeg-self
BUILD_DIR := $(ROOT)/build
DOWNLOADS := $(ROOT)/downloads
PKG_CONFIG_PATH := $(BUILD_DIR)/lib/pkgconfig:$(BUILD_DIR)/share/pkgconfig

# === MSYS2 package list ===
MSYS2_PKGS := mingw-w64-x86_64-toolchain \
	mingw-w64-x86_64-pkgconf mingw-w64-x86_64-yasm \
	mingw-w64-x86_64-nasm mingw-w64-x86_64-cmake \
	mingw-w64-x86_64-meson mingw-w64-x86_64-ninja \
	git make diffutils autoconf automake libtool pkgconf \
	gettext-devel subversion patch texinfo \
	mingw-w64-x86_64-libsoxr \
	mingw-w64-x86_64-opus \
	mingw-w64-x86_64-libtheora \
	mingw-w64-x86_64-aom \
	mingw-w64-x86_64-libass \
	mingw-w64-x86_64-libbluray \
	mingw-w64-x86_64-dav1d \
	mingw-w64-x86_64-libjxl \
	mingw-w64-x86_64-kvazaar \
	mingw-w64-x86_64-libvpl \
	mingw-w64-x86_64-lame \
	mingw-w64-x86_64-openjpeg2 \
	mingw-w64-x86_64-libopenmpt \
	mingw-w64-x86_64-rav1e \
	mingw-w64-x86_64-snappy \
	mingw-w64-x86_64-srt \
	mingw-w64-x86_64-svt-av1 \
	mingw-w64-x86_64-twolame \
	mingw-w64-x86_64-vid.stab \
	mingw-w64-x86_64-vmaf \
	mingw-w64-x86_64-libvpx \
	mingw-w64-x86_64-x264 \
	mingw-w64-x86_64-xavs \
	mingw-w64-x86_64-shaderc \
	mingw-w64-x86_64-glslang

# === Source library list ===
SRC_LIBS := \
    libuavs3d \
    libxavs2 \
    libxml2 \
    libbluray \
    libjxl \
    opencl_header \
    opencl_loader \
    vulkan_header \
    vulkan_loader \
    shaderc_static \
    amf_sdk

# Allowed DLL import symbols (regex) for .a static library purity check
ALLOWED_IMP_SYMBOLS := \
  __imp__aligned_free|__imp__assert|__imp__wopen|__imp__errno|__imp_GetACP|\
  __imp_GlobalMemoryStatusEx|__imp_GetSystemInfo|__imp__beginthreadex|\
  __imp_CloseHandle|__imp_DeleteCriticalSection|__imp_EnterCriticalSection|\
  __imp_GetTickCount|__imp_InitializeConditionVariable|__imp_InitializeCriticalSection|\
  __imp_LeaveCriticalSection|__imp_SleepConditionVariableCS|__imp_WaitForSingleObject|\
  __imp_WakeConditionVariable|\
  __imp___acrt_iob_func|__imp_isspace|__imp_localeconv|__imp_islower|__imp_isxdigit|\
  __imp_toupper|\
  __imp_GetLastError|__imp_DuplicateHandle|__imp_GetCurrentProcess|\
  __imp_GetCurrentThread|__imp_RegisterWaitForSingleObject|\
  __imp_TlsAlloc|__imp_TlsFree|__imp_TlsGetValue|__imp_TlsSetValue|\
  __imp_UnregisterWait|__imp_InitOnceExecuteOnce|__imp__wfopen|\
  __imp__wstat64i32|__imp_MultiByteToWideChar|\
  __imp_FreeLibrary|__imp_GetProcAddress|__imp_LoadLibraryA|\
  __imp__setjmp|__imp_longjmp|\
  __imp_CreateFileA|__imp_CreateFileMappingA|__imp_GetFileSizeEx|\
  __imp_GetProcessHeap|__imp_HeapAlloc|__imp_HeapFree|__imp_HeapReAlloc|\
  __imp_MapViewOfFile|__imp_ReadFile|__imp_UnmapViewOfFile|\
  __imp_CreateFileW|__imp_GetFileSize|\
  __imp__mkdir|__imp__get_osfhandle|__imp__locking|__imp_GetModuleFileNameA|\
  __imp_isalpha|__imp__mbsrchr|__imp_GetModuleHandleA|__imp_GetFileAttributesExA|\
  __imp_GetFullPathNameA|__imp_GetLongPathNameA|__imp_GetTempPathA|__imp_GetWindowsDirectoryA|\
  __imp_FormatMessageA|__imp_GetSystemTime|\
  __imp_GetModuleFileNameW|__imp_SwitchToThread|__imp_RegCloseKey|__imp_RegQueryValueExW|\
  __imp_StringFromGUID2|__imp_LoadLibraryExW|__imp_SetThreadErrorMode|\
  __imp_GetModuleHandleExW|__imp_GetModuleHandleW|__imp_RegEnumKeyExW|__imp_RegEnumValueW|\
  __imp_RegOpenKeyExW|__imp_RegQueryInfoKeyW|__imp_FindClose|__imp_FindFirstFileW|\
  __imp_FindNextFileW|__imp_GetCurrentDirectoryW|__imp_GetEnvironmentVariableA|\
  __imp_GetEnvironmentVariableW|__imp_GetFullPathNameW|__imp_wcstombs_s|\
  __imp_fopen_s|__imp_strncpy_s|\
  __imp__aligned_malloc|\
  __imp_CreateEventA|__imp_SetEvent|__imp_QueryPerformanceCounter|\
  __imp_QueryPerformanceFrequency|__imp__aligned_realloc|\
  __imp_InitOnceBeginInitialize|__imp_InitOnceComplete|\
  __imp_GetThreadGroupAffinity|__imp_SleepConditionVariableSRW|__imp_InitializeSRWLock|__imp_WakeAllConditionVariable|\
  __imp_CreateSemaphoreA|__imp_GetProcessAffinityMask|__imp_ReleaseSemaphore|\
  __imp_Sleep|__imp__stricmp|\
  __imp_GetFileType|\
  __imp_strtok_s|__imp_LoadLibraryW|__imp_SetDllDirectoryW|__imp_WideCharToMultiByte|__imp__wcsicmp|__imp_EnumFontFamiliesExW|__imp_GetDC|__imp_ReleaseDC|__imp_WideCharToMultiByte|__imp__strnicmp|__imp_SetThreadAffinityMask|__imp_tolower|__imp__findclose|__imp_WideCharToMultiByte|__imp_GetWindowsDirectoryW|__imp_SHGetFolderPathW|__imp_WideCharToMultiByte|__imp_FormatMessageW|__imp_GetModuleHandleExA|__imp_WideCharToMultiByte|__imp__wremove|__imp_CreateDirectoryW|__imp_fseeko64|__imp_ftello64|__imp_GetFileAttributesW|\
  __imp_LoadLibraryExA|__imp_RegEnumValueA|__imp_RegOpenKeyExA|__imp_GetSidSubAuthority|__imp_GetSidSubAuthorityCount|__imp_GetTokenInformation|__imp_OpenProcessToken|__imp_CM_Get_Child|__imp_CM_Get_Device_ID_List_SizeW|__imp_CM_Get_Device_ID_ListW|__imp_CM_Get_Device_IDW|__imp_CM_Get_DevNode_Registry_PropertyW|__imp_CM_Get_DevNode_Status|__imp_CM_Get_Sibling|__imp_CM_Locate_DevNodeW|__imp_CM_Open_DevNode_Key|__imp_RegQueryValueExA|__imp_wcscat_s|__imp_wcscpy_s|\
  __imp_strtok

# Allowed DLL names (regex) for .exe whitelist check (lowercase)
ALLOWED_DLLS := \
  kernel32.dll|user32.dll|gdi32.dll|shell32.dll|ole32.dll|oleaut32.dll|crypt32.dll|ncrypt.dll|\
  secur32.dll|shlwapi.dll|ws2_32.dll|avicap32.dll|msvcrt.dll|\
  advapi32.dll|bcrypt.dll|cfgmgr32.dll|dwrite.dll|ntdll.dll|api-ms-win-core-synch-l1-2-0.dll|rpcrt4.dll|usp10.dll|wsock32.dll|\
  d3d11.dll


# === Macro definitions ===

# === Static library impurity check (with whitelist) ===
check_static_purity = \
  echo "[CHECK] Checking for DLL imports in $2..."; \
  if [ ! -f "$2" ]; then \
    echo "[ERROR] File not found: $2"; \
    exit 1; \
  fi; \
  TMPDIR=$$(mktemp -d $1/tmp.XXXXXXXXXX); \
  cd $$TMPDIR; \
  nm -g "$2" 2>/dev/null | grep '__imp_' > .tmp_imp_check || true; \
  if [ -s .tmp_imp_check ]; then \
    grep -vE '$(ALLOWED_IMP_SYMBOLS)' .tmp_imp_check > .tmp_imp_filtered || true; \
    if [ -s .tmp_imp_filtered ]; then \
      echo "[ERROR] $2 contains non-whitelisted DLL import symbols:"; \
      cat .tmp_imp_filtered; \
      rm -rf $$TMPDIR; \
      exit 1; \
    fi; \
  fi; \
  rm -rf $$TMPDIR; \
  echo "[OK] $2 is pure (no non-whitelisted DLL imports)"

# === 64-bit architecture signature ===
ARCH_64BIT_SIGNATURE = x86-64 COFF

# === 64-bit architecture check for static libraries (.o / .obj) ===
check_64bit = \
  echo "[CHECK] Checking 64bit architecture for $2..."; \
  if [ ! -f "$2" ]; then \
    echo "[ERROR] File not found: $2"; \
    exit 1; \
  fi; \
  TMPDIR=$$(mktemp -d $1/tmp.XXXXXXXXXX); \
  cd $$TMPDIR; \
  ar x "$2" > /dev/null; \
  OBJFILES=$$(ls | grep -Ei '\.o$$|\.obj$$'); \
  if [ -z "$$OBJFILES" ]; then \
    echo "[ERROR] No object files (.o or .obj) found in $2"; \
    rm -rf $$TMPDIR; \
    exit 1; \
  fi; \
  file $$OBJFILES | grep -v '$(ARCH_64BIT_SIGNATURE)' > .tmp_arch_check || true; \
  if [ -s .tmp_arch_check ]; then \
    echo "[ERROR] $1 contains non-64bit object files:"; \
    cat .tmp_arch_check; \
    rm -rf $$TMPDIR; \
    exit 1; \
  fi; \
  rm -rf $$TMPDIR; \
  echo "[OK] $2 is fully 64bit ($(ARCH_64BIT_SIGNATURE))"

# === DLL import whitelist check ===
check_dll_whitelist = \
  echo "[CHECK] Checking DLL imports in $2..."; \
  if [ ! -f "$2" ]; then \
    echo "[ERROR] File not found: $2"; \
    exit 1; \
  fi; \
  TMPDIR=$$(mktemp -d $1/tmp.XXXXXXXXXX); \
  cd $$TMPDIR; \
  objdump -p "$1" | grep 'DLL Name' | awk '{printf("  %s\n", tolower($$3))}' > .tmp_dll_check; \
  if [ -s .tmp_dll_check ]; then \
    grep -vE '$(ALLOWED_DLLS)' .tmp_dll_check > .tmp_dll_filtered || true; \
    if [ -s .tmp_dll_filtered ]; then \
      echo "[ERROR] $2 imports non-whitelisted DLLs:"; \
      cat .tmp_dll_filtered; \
      rm -rf $$TMPDIR; \
      exit 1; \
    fi; \
  fi; \
  rm -rf $$TMPDIR; \
  echo "[OK] $2 is pure (only whitelisted DLLs used)"

# === Build target definitions ===

.PHONY: all setup ffmpeg clean

## === Full build ===
all: setup $(SRC_LIBS) ffmpeg

## === MSYS2 package installation ===
setup:
	@set -e; \
	pacman -Syu --noconfirm; \
	pacman -S --needed --noconfirm $(MSYS2_PKGS)


## === libuavs3d ===
libuavs3d: $(ROOT)/uavs3d/.stamp-libuavs3d
$(ROOT)/uavs3d/.stamp-libuavs3d:
	@set -e; \
	if [ ! -d $(ROOT)/uavs3d/.git ]; then \
		rm -rf $(ROOT)/uavs3d; \
		git clone --depth 1 https://github.com/uavs3/uavs3d.git $(ROOT)/uavs3d; \
	else \
		echo "[OK] uavs3d already exists"; \
	fi; \
	cd $(ROOT)/uavs3d; \
	mkdir -p build; \
	cd build; \
	cmake -G Ninja \
		-DCMAKE_INSTALL_PREFIX=$(BUILD_DIR) \
		-DCMAKE_PREFIX_PATH=$(BUILD_DIR) \
		-DBUILD_SHARED_LIBS=OFF \
		-DCOMPILE_10BIT=ON \
		-DBUILD_TESTING=OFF \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		..; \
	ninja; \
	ninja install; \
	$(call check_64bit,$(BUILD_DIR),$(BUILD_DIR)/lib/libuavs3d.a); \
	$(call check_static_purity,$(BUILD_DIR),$(BUILD_DIR)/lib/libuavs3d.a); \
	touch $@

## === libxavs2 ===
libxavs2: $(ROOT)/xavs2/.stamp-libxavs2
$(ROOT)/xavs2/.stamp-libxavs2:
	@set -e; \
	if [ ! -d $(ROOT)/xavs2/.git ]; then \
		rm -rf $(ROOT)/xavs2; \
		git clone --depth 1 https://github.com/pkuvcl/xavs2.git $(ROOT)/xavs2; \
	else \
		echo "[OK] xavs2 already exists"; \
	fi; \
	cd $(ROOT)/xavs2/build/linux; \
	# Fix type warnings (safe function pointer cast) \
	sed -i 's/\(xavs2_threadpool_run([^,]*,\) *encoder_aec_encode_one_frame/\1 (void *(*)(void *))encoder_aec_encode_one_frame/' $(ROOT)/xavs2/source/encoder/encoder.c; \
	# Fix const correctness \
	sed -i 's/xavs2_get_configs(argc, argv)/xavs2_get_configs(argc, (const char **)argv)/' $(ROOT)/xavs2/source/encoder/parameters.c; \
	./configure \
		--prefix=$(BUILD_DIR) \
		--disable-cli \
		--enable-static \
		--disable-asm \
		--host=x86_64-w64-mingw32; \
	make clean; \
	make -j $$(nproc); \
	make install; \
	$(call check_64bit,$(BUILD_DIR),$(BUILD_DIR)/lib/libxavs2.a); \
	$(call check_static_purity,$(BUILD_DIR),$(BUILD_DIR)/lib/libxavs2.a); \
	touch $@

## === libxml2 ===
libxml2: $(ROOT)/libxml2/.stamp-libxml2
$(ROOT)/libxml2/.stamp-libxml2:
	@set -e; \
	if [ ! -d $(ROOT)/libxml2 ]; then \
		git clone https://gitlab.gnome.org/GNOME/libxml2.git $(ROOT)/libxml2; \
	else \
		echo "[OK] libxml2 already exists"; \
	fi; \
	cd $(ROOT)/libxml2; \
	cmake -S . -B build \
		-DCMAKE_INSTALL_PREFIX=$(BUILD_DIR) \
		-DCMAKE_PREFIX_PATH=$(BUILD_DIR) \
		-DBUILD_SHARED_LIBS=OFF \
		-DLIBXML2_WITH_PROGRAMS=OFF \
		-DLIBXML2_WITH_TESTS=OFF \
		-DLIBXML2_WITH_PYTHON=OFF \
		-DLIBXML2_WITH_DOCS=OFF \
		-DLIBXML2_WITH_THREADS=ON \
		-DLIBXML2_WITH_ZLIB=ON \
		-DZLIB_INCLUDE_DIR=$(BUILD_DIR)/include \
		-DZLIB_LIBRARY=$(BUILD_DIR)/lib/libzs.a \
		-DLIBXML2_WITH_LEGACY=OFF; \
	cmake --build build -j $(shell nproc); \
	cmake --install build; \
	$(call check_64bit,$(BUILD_DIR),$(BUILD_DIR)/lib/libxml2.a); \
	$(call check_static_purity,$(BUILD_DIR),$(BUILD_DIR)/lib/libxml2.a); \
	touch $@

## === libbluray ===
libbluray: $(ROOT)/libbluray/.stamp-libbluray
$(ROOT)/libbluray/.stamp-libbluray:
	@set -e; \
	if [ ! -d $(ROOT)/libbluray/.git ]; then \
		rm -rf $(ROOT)/libbluray; \
		git clone https://code.videolan.org/videolan/libbluray.git $(ROOT)/libbluray; \
	else \
		echo "[OK] libbluray already exists"; \
	fi; \
	sed -i 's/\bdec_init\b/bd_dec_init/g' $(ROOT)/libbluray/src/libbluray/disc/dec.c; \
	sed -i 's/\bdec_init\b/bd_dec_init/g' $(ROOT)/libbluray/src/libbluray/disc/dec.h; \
	sed -i 's/\bdec_init\b/bd_dec_init/g' $(ROOT)/libbluray/src/libbluray/disc/disc.c; \
	rm -rf $(ROOT)/libbluray/build; \
	cd $(ROOT)/libbluray; \
	meson setup build \
		--prefix=$(BUILD_DIR) \
		--buildtype=release \
		--default-library=static \
		-Denable_docs=false \
		-Denable_tools=false \
		-Denable_devtools=false \
		-Denable_examples=false \
		-Dembed_udfread=true; \
	ninja -C build; \
	ninja -C build install; \
	$(call check_64bit,$(BUILD_DIR),$(BUILD_DIR)/lib/libbluray.a); \
	$(call check_static_purity,$(BUILD_DIR),$(BUILD_DIR)/lib/libbluray.a); \
	touch $@

## === libjxl ===
libjxl: $(ROOT)/libjxl/.stamp-libjxl
$(ROOT)/libjxl/.stamp-libjxl:
	@set -e; \
	if [ ! -d $(ROOT)/libjxl/.git ]; then \
		rm -rf $(ROOT)/libjxl; \
		git clone https://github.com/libjxl/libjxl.git $(ROOT)/libjxl; \
	fi; \
	if [ ! -d $(ROOT)/libjxl/third_party/highway/.git ]; then \
		git clone --depth 1 https://github.com/google/highway.git $(ROOT)/libjxl/third_party/highway; \
	fi; \
	if [ ! -d $(ROOT)/libjxl/third_party/skcms/.git ]; then \
		git clone --depth 1 https://skia.googlesource.com/skcms.git $(ROOT)/libjxl/third_party/skcms; \
	fi; \
	if [ ! -d $(ROOT)/libjxl/third_party/sjpeg/.git ]; then \
		git clone --depth 1 https://github.com/webmproject/sjpeg.git $(ROOT)/libjxl/third_party/sjpeg; \
	fi; \
	rm -rf $(ROOT)/libjxl/build; \
	mkdir -p $(ROOT)/libjxl/build; \
	cd $(ROOT)/libjxl/build; \
	cmake -G "Unix Makefiles" \
		-DCMAKE_INSTALL_PREFIX=$(BUILD_DIR) \
		-DCMAKE_PREFIX_PATH=$(BUILD_DIR) \
		-DBUILD_SHARED_LIBS=OFF \
		-DJPEGXL_STATIC=ON \
		-DBUILD_TESTING=OFF \
		-DJPEGXL_ENABLE_TOOLS=OFF \
		-DJPEGXL_ENABLE_VIEWERS=OFF \
		-DJPEGXL_ENABLE_EXAMPLES=OFF \
		-DJPEGXL_ENABLE_BENCHMARK=OFF \
		-DJPEGXL_ENABLE_OPENEXR=OFF \
		-DJPEGXL_ENABLE_SJPEG=ON \
		-DJPEGXL_ENABLE_SKCMS=ON \
		-DJPEGXL_ENABLE_MANPAGES=OFF \
		-DJPEGXL_ENABLE_DOXYGEN=OFF \
		-DJPEGXL_ENABLE_PLUGINS=OFF \
		-DJPEGXL_ENABLE_TRANSCODE_JPEG=ON \
		-DJPEGXL_ENABLE_BOXES=ON \
		..; \
	make -j $(shell nproc); \
	make install; \
	$(call check_64bit,$(BUILD_DIR),$(BUILD_DIR)/lib/libjxl.a); \
	$(call check_static_purity,$(BUILD_DIR),$(BUILD_DIR)/lib/libjxl.a); \
	touch $@

## === opencl-header ===
opencl_header: $(ROOT)/opencl_header/.stamp-opencl_header
$(ROOT)/opencl_header/.stamp-opencl_header:
	@set -e; \
	if [ ! -d $(ROOT)/opencl_header ]; then \
		git clone https://github.com/KhronosGroup/OpenCL-Headers.git $(ROOT)/opencl_header; \
	else \
		echo "[OK] OpenCL-Headers already exists"; \
	fi; \
	cd $(ROOT)/opencl_header; \
	mkdir -p build; \
	cd build; \
	cmake -G Ninja \
		-DCMAKE_INSTALL_PREFIX=$(BUILD_DIR) \
		-DCMAKE_PREFIX_PATH=$(BUILD_DIR) \
		-DBUILD_SHARED_LIBS=OFF \
		..; \
	ninja; \
	ninja install; \
	touch $@

## === opencl-loader ===
opencl_loader: opencl_header $(ROOT)/opencl_loader/.stamp-opencl_loader
$(ROOT)/opencl_loader/.stamp-opencl_loader:
	@set -e; \
	if [ ! -d $(ROOT)/opencl_loader ]; then \
		git clone --recursive https://github.com/KhronosGroup/OpenCL-ICD-Loader.git $(ROOT)/opencl_loader; \
	else \
		echo "[OK] OpenCL ICD Loader already exists"; \
	fi; \
	cd $(ROOT)/opencl_loader; \
	mkdir -p build; \
	cd build; \
	cmake -G Ninja \
		-DCMAKE_INSTALL_PREFIX=$(BUILD_DIR) \
		-DCMAKE_PREFIX_PATH=$(BUILD_DIR) \
		-DOPENCL_ICD_LOADER_BUILD_SHARED_LIBS=OFF \
		-DOPENCL_ICD_LOADER_HEADERS_DIR=$(BUILD_DIR)/include \
		-DENABLE_OPENCL_LAYERINFO=OFF \
		-DCMAKE_BUILD_TYPE=Release \
		..; \
	ninja; \
	ninja install; \
	if [ -f $(BUILD_DIR)/lib/OpenCL.a ]; then \
		mv $(BUILD_DIR)/lib/OpenCL.a $(BUILD_DIR)/lib/libOpenCL.a; \
		echo "[INFO] Renamed OpenCL.a to libOpenCL.a"; \
	else \
		echo "[WARN] OpenCL.a not found. Possible link error."; \
	fi; \
	$(call check_64bit,$(BUILD_DIR),$(BUILD_DIR)/lib/libOpenCL.a); \
	$(call check_static_purity,$(BUILD_DIR),$(BUILD_DIR)/lib/libOpenCL.a); \
	touch $@

## === Vulkan-Headers (headers only) ===
vulkan_header: $(ROOT)/vulkan_header/.stamp-vulkan_header
$(ROOT)/vulkan_header/.stamp-vulkan_header:
	@set -e; \
	echo "[INFO] Fetching and installing Vulkan-Headers..."; \
	if [ ! -d $(ROOT)/vulkan_header ]; then \
		echo "[INFO] Cloning Vulkan-Headers..."; \
		git clone https://github.com/KhronosGroup/Vulkan-Headers.git $(ROOT)/vulkan_header; \
	else \
		echo "[OK] Vulkan-Headers already exists"; \
	fi; \
	cd $(ROOT)/vulkan_header; \
	mkdir -p build; \
	cd build; \
	cmake -G Ninja \
		-DCMAKE_INSTALL_PREFIX=$(BUILD_DIR) \
		-DCMAKE_PREFIX_PATH=$(BUILD_DIR) \
		-DBUILD_SHARED_LIBS=OFF \
		..; \
	ninja; \
	ninja install; \
	touch $@

## === Vulkan-Loader (build libvulkan.a) ===
vulkan_loader: vulkan_header $(ROOT)/vulkan_loader/.stamp-vulkan_loader
$(ROOT)/vulkan_loader/.stamp-vulkan_loader:
	@set -e; \
	echo "[INFO] Fetching and building Vulkan-Loader..."; \
	if [ ! -d $(ROOT)/vulkan_loader ]; then \
		echo "[INFO] Cloning Vulkan-Loader..."; \
		git clone https://github.com/KhronosGroup/Vulkan-Loader.git $(ROOT)/vulkan_loader; \
	else \
		echo "[OK] Vulkan-Loader already exists"; \
	fi; \
	cd $(ROOT)/vulkan_loader; \
	# Force building static library (.a) \
	mkdir -p build; \
	cd build; \
	cmake -G Ninja \
		-DCMAKE_INSTALL_PREFIX=$(BUILD_DIR) \
		-DCMAKE_PREFIX_PATH=$(BUILD_DIR) \
		-DVULKAN_HEADERS_INSTALL_DIR=$(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=Release \
		..; \
	ninja; \
	ninja install; \
	$(call check_64bit,$(BUILD_DIR),$(BUILD_DIR)/lib/libvulkan-1.dll.a); \
	touch $@

## === shaderc static ===
shaderc_static: $(BUILD_DIR)/lib/pkgconfig/shaderc.pc
$(BUILD_DIR)/lib/pkgconfig/shaderc.pc:
	@set -e; \
	echo "prefix=/mingw64"                >  $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo "exec_prefix=\$${prefix}"        >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo "libdir=\$${exec_prefix}/lib"    >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo "includedir=\$${prefix}/include" >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo ""                               >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo "Name: shaderc"                  >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo "Description: Tools and libraries for Vulkan shader compilation" >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo "Version: 2023.8.1"              >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo "Libs: -L\$${libdir} -lshaderc_combined -lSPIRV-Tools-opt -lSPIRV-Tools -lSPIRV-Tools-link -lglslang" >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc; \
	echo "Cflags: -I\$${includedir}"      >> $(BUILD_DIR)/lib/pkgconfig/shaderc.pc;

## === AMF SDK ===
amf_sdk: $(ROOT)/amf_sdk/.stamp-amf_sdk
$(ROOT)/amf_sdk/.stamp-amf_sdk:
	@set -e; \
	if [ ! -d $(ROOT)/amf_sdk/.git ]; then \
		echo "[INFO] Cloning AMF SDK..."; \
		rm -rf $(ROOT)/amf_sdk; \
		git clone --depth 1 https://github.com/GPUOpen-LibrariesAndSDKs/AMF.git $(ROOT)/amf_sdk; \
	else \
		echo "[OK] AMF SDK already exists"; \
	fi; \
	echo "[INFO] Copying AMF headers to $(BUILD_DIR)/include/AMF ..."; \
	mkdir -p $(BUILD_DIR)/include/AMF; \
	cp -r $(ROOT)/amf_sdk/amf/public/include/* $(BUILD_DIR)/include/AMF/; \
	touch $@


## === ffmpeg ===
# memo
#
# --pkg-config-flags="--static"
#   When using pkg-config to locate libraries, return .a instead of .dll.a.
#
# --extra-libs="-static"
#   When additional libraries are specified via -lxxx, link against libxxx.a
#   instead of libxxx.dll.a.
#
# --extra-libs="-lgomp"
#   libsoxr.a requires OpenMP's libgomp.a, but FFmpeg does not use OpenMP
#   directly, so it cannot be enabled via --enable-xx. Therefore, it must be
#   added manually via --extra-libs.
#
# --extra-libs="-lstdc++"
#   Background:
#     - Installing mingw-w64-x86_64-libbluray introduces dependencies:
#       freetype → harfbuzz → graphite2.
#     - libgraphite2.a is written in C++, so linking requires the C++ runtime
#       (libstdc++, libgcc_s, libsupc++).
#     - FFmpeg's configure assumes C libraries and does not automatically add
#       the C++ runtime.
#     - As a result, C++ ABI symbols such as __gxx_personality_seh0,
#       __cxa_call_unexpected, and __class_type_info remain unresolved.
#   Solution:
#     Add the following to configure:
#       --extra-libs="-lstdc++"
#
# --extra-libs="-liconv -lintl -lharfbuzz"
#   Background:
#     Code inside libfreetype.a attempts to call hb_shape (from HarfBuzz),
#     but the linker cannot find the function definition.
#     The final error "ERROR: harfbuzz not found using pkg-config" indicates
#     FFmpeg's configure failed to detect HarfBuzz due to this linkage issue.
#   Cause & Fix:
#     Freetype was built with HarfBuzz enabled, but FFmpeg's linker flags do
#     not include -lharfbuzz in the correct order.
#     Freetype must be linked before HarfBuzz.
#     Adjust the order of libraries in --extra-libs.
#   Corrected example:
#     --extra-libs="-static -lstdc++ -lgomp -liconv -lintl -lharfbuzz -lgraphite2 -lfreetype -lfontconfig"
#
# --extra-cflags="-DKVZ_STATIC_LIB"
#   Without this definition, kvazaar.h sets KVZ_PUBLIC to __declspec(dllimport),
#   causing configure to fail by treating symbols as imported from a DLL.
#
# --extra-cflags="-DLIBTWOLAME_STATIC"
#   Without this definition, twolame.h sets TL_API to __declspec(dllimport),
#   causing configure to fail by treating symbols as imported from a DLL.
#
# --extra-libs="-lcfgmgr32 -lole32"
#   Fixes unresolved OS-level references used by the OpenCL library:
#     __imp_CM_Get_Device_ID_List_SizeW
#     __imp_CM_Get_Device_ID_ListW
#     __imp_CM_Get_Sibling
#     __imp_StringFromGUID2
#
# --extra-ldflags="-Wl,--Bstatic -Wl,--whole-archive -lgcc_eh -Wl,--no-whole-archive"
#   During final linking, errors such as "multiple definition of _Unwind_SetGR"
#   occur because both libgcc_eh.a and libgcc_s.a are being linked.
#   We want libgcc_eh.a (static, no DLL dependency), but FFmpeg's generated
#   Makefile still adds libgcc_s.a.
#   Explanation:
#     -Wl,--Bstatic:
#       Forces the linker to prefer .a over .dll.a.
#     -Wl,--whole-archive -lgcc_eh -Wl,--no-whole-archive:
#       Forces inclusion of all symbols from libgcc_eh.a, avoiding conflicts
#       with libgcc_s.a.
#
# --extra-libs="-static-libgcc"
#   After removing -lgcc_s from ffbuild/config.mak via:
#     sed -i 's/-lgcc_s //g' ffbuild/config.mak
#   this ensures the static GCC runtime library is linked at the end.
#
# --extra-libs="-ld3d11"
#   Used by the vpp_amf extension when performing deinterlacing via d3d11.
#   If the input is not a d3d11 device, vpp_amf internally converts it using d3d11.
#
# --extra-cflags="-DVF_VPP_AMF_D3D11VA_HWACCEL"
#   Enables the extended vpp_amf code path.
#
# --extra-cflags="-DCL_TARGET_OPENCL_VERSION=300"
#   Not strictly required—FFmpeg will warn and auto-set it to 300—but this
#   suppresses the warning.
#
# --enable-libshaderc
# --extra-libs="-lshaderc"
#   As of the full build in January 2026, Vulkan requires shaderc.
#
#
ffmpeg: $(ROOT)/ffmpeg/.stamp-ffmpeg
$(ROOT)/ffmpeg/.stamp-ffmpeg:
	@set -e; \
	if [ ! -d $(ROOT)/ffmpeg/.git ]; then \
		git clone https://git.ffmpeg.org/ffmpeg.git $(ROOT)/ffmpeg; \
	else \
		echo "[OK] FFmpeg source is already fetched"; \
	fi; \
	cd $(ROOT)/ffmpeg; \
	make clean || true; \
	./configure \
		--prefix=$(BUILD_DIR) \
		--arch=x86_64 \
		--enable-gpl \
		--enable-version3 \
		--enable-nonfree \
		--disable-debug \
		--disable-w32threads \
		--enable-pthreads \
		--disable-shared \
		--enable-zlib \
		--enable-iconv \
		--enable-lzma \
		--enable-gmp \
		--enable-libvorbis \
		--enable-libtheora \
		--enable-libopus \
		--enable-libxml2 \
		--enable-libfreetype \
		--enable-libfribidi \
		--enable-fontconfig \
		--enable-libharfbuzz  \
		--enable-vaapi \
		--enable-libvpl \
		--enable-libsnappy \
		--enable-libzimg \
		--enable-libopenjpeg \
		--enable-libaom \
		--enable-libdav1d \
		--enable-libjxl \
		--enable-libkvazaar \
		--enable-librav1e \
		--enable-libsvtav1 \
		--enable-libx264 \
		--enable-libx265 \
		--enable-libvpx \
		--enable-libxavs2 \
		--enable-libuavs3d \
		--enable-libmp3lame \
		--enable-libtwolame \
		--enable-libopenmpt \
		--enable-libxvid \
		--enable-libvmaf \
		--enable-libvidstab \
		--enable-libass \
		--enable-libbluray \
		--enable-libzvbi \
		--enable-libsrt \
		--enable-libsoxr \
		--enable-opencl \
		--enable-vulkan \
		--enable-libshaderc \
		--enable-amf \
		--pkg-config-flags="--static" \
		--extra-cflags="-I$(BUILD_DIR)/include -DKVZ_STATIC_LIB -DLIBTWOLAME_STATIC -DCL_TARGET_OPENCL_VERSION=300 -DVF_VPP_AMF_D3D11_HWACCEL" \
		--extra-ldflags="-L$(BUILD_DIR)/lib -Wl,--Bstatic -Wl,--whole-archive -lgcc_eh -Wl,--no-whole-archive" \
		--extra-libs="-l:libshaderc_combined.a -static -lstdc++ -lgomp -liconv -lintl -lharfbuzz -lcfgmgr32 -lole32 -static-libgcc -ld3d11"; \
	# Remove all -lgcc_s from config.mak to prevent libgcc_s_seh-1.dll from being linked \
	sed -i 's/-lgcc_s //g' ffbuild/config.mak; \
	# Apply feature extension patches for vpp_amf \
        patch -d $(ROOT)/ffmpeg/ -p1 < $(ROOT)/patches/vf_amf_common_c.patch; \
        patch -d $(ROOT)/ffmpeg/ -p1 < $(ROOT)/patches/vf_amf_common_h.patch; \
        patch -d $(ROOT)/ffmpeg/ -p1 < $(ROOT)/patches/vf_vpp_amf_c.patch; \
	make -j $(shell nproc); \
	make install; \
	$(call check_dll_whitelist,$(BUILD_DIR),$(BUILD_DIR)/bin/ffmpeg.exe); \
	$(call check_dll_whitelist,$(BUILD_DIR),$(BUILD_DIR)/bin/ffprobe.exe); \
	mkdir -p $(INSTALL_TO)/bin; \
	mkdir -p $(INSTALL_TO)/doc; \
	mkdir -p $(INSTALL_TO)/presets; \
	cp -r $(BUILD_DIR)/bin/*.exe $(INSTALL_TO)/bin; \
	cp -r $(BUILD_DIR)/share/doc/ffmpeg/* $(INSTALL_TO)/doc; \
	cp -r $(BUILD_DIR)/share/ffmpeg/*.ffpreset $(INSTALL_TO)/presets; \
	touch $@

## === cleanup ===
clean:
	@set -e; \
	rm -rf $(BUILD_DIR); \
	rm -rf $(DOWNLOADS); \
	rm -rf $(ROOT)/uavs3d; \
	rm -rf $(ROOT)/xavs2; \
	rm -rf $(ROOT)/libxml2; \
	rm -rf $(ROOT)/libbluray; \
	rm -rf $(ROOT)/libjxl; \
	rm -rf $(ROOT)/opencl_header; \
	rm -rf $(ROOT)/opencl_loader; \
	rm -rf $(ROOT)/vulkan_header; \
	rm -rf $(ROOT)/vulkan_loader; \
	rm -rf $(ROOT)/amf_sdk; \
	rm -rf $(ROOT)/ffmpeg


