Message("(badly) Targeting the Android platform")

# define the entry point for the android executables
Set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --entry=_start")

# skip the full RPATH for the build tree since it confuses the link command
SET(CMAKE_SKIP_BUILD_RPATH TRUE)

# shut up some warnings
Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-long-long -Wno-variadic-macros")

# drastically change the include paths
Set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -nostdinc -fPIC -DANDROID")
include_directories("/mnt/btrfs/android-ndk-r8e/platforms/android-3/arch-arm/usr/include")
include_directories("/mnt/btrfs/android-ndk-r8e/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86_64/lib/gcc/arm-linux-androideabi/4.4.3/include")

# drastically mess with the linking as well
Set(ANDROID_LINKER_FLAGS "--dynamic-linker /system/bin/linker -nostdlib -rpath /system/lib -L/mnt/btrfs/too-big/droid-system/system/lib")

Set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${ANDROID_LINKER_FLAGS} /mnt/btrfs/android-ndk-r8e/platforms/android-3/arch-arm/usr/lib/crtbegin_dynamic.o /mnt/btrfs/android-ndk-r8e/platforms/android-3/arch-arm/usr/lib/crtend_android.o")
Set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${ANDROID_LINKER_FLAGS}")

# drop -rdynamic AND always link to libc and libdl
Set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "-lc -ldl")

# override the actual build commands
Set(CMAKE_C_LINK_EXECUTABLE "/opt/arm-2012.03/bin/arm-none-linux-gnueabi-ld <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS>  -o <TARGET> <LINK_LIBRARIES>")
Set(CMAKE_C_CREATE_SHARED_LIBRARY "/opt/arm-2012.03/bin/arm-none-linux-gnueabi-ld <CMAKE_SHARED_LIBRARY_C_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS> -soname <TARGET_SONAME> -o <TARGET> <OBJECTS> -lc <LINK_LIBRARIES>")

# we need our own argp
add_subdirectory(win32/argp-standalone-1.3)
include_directories(${ARGPDIR})
