cmake_minimum_required(VERSION 3.10)
project(jvmci_8)

set(CMAKE_CXX_STANDARD 98)

set(HOTSPOT_ROOT ..)
set(BUILD_ROOT ${HOTSPOT_ROOT}/build)
set(VM_DIR ${VM_DIR})
set(HOTSPOT_SRC_ROOT ${HOTSPOT_ROOT}/src)

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()
if (APPLE)
    set(DARWIN TRUE)
endif()


if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    add_definitions(-DPRODUCT)
endif()


if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    add_definitions(-DASSERT)
endif()

set(ADLC_SRC_DIR ${HOTSPOT_SRC_ROOT}/share${VM_DIR}/adlc)

set(ADLC_SOURCES
        ${ADLC_SRC_DIR}/adlparse.cpp
        ${ADLC_SRC_DIR}/archDesc.cpp
        ${ADLC_SRC_DIR}/arena.cpp
        ${ADLC_SRC_DIR}/dfa.cpp
        ${ADLC_SRC_DIR}/dict2.cpp
        ${ADLC_SRC_DIR}/filebuff.cpp
        ${ADLC_SRC_DIR}/forms.cpp
        ${ADLC_SRC_DIR}/formsopt.cpp
        ${ADLC_SRC_DIR}/formssel.cpp
        ${ADLC_SRC_DIR}/main.cpp
        ${ADLC_SRC_DIR}/output_c.cpp
        ${ADLC_SRC_DIR}/output_h.cpp
        ${HOTSPOT_SRC_ROOT}/share${VM_DIR}/opto/opcodes.cpp
        )

add_executable(adlc
        ${ADLC_SOURCES}
        )

target_include_directories(adlc PUBLIC
        ${ADLC_SRC_DIR}
        ${HOTSPOT_SRC_ROOT}/share${VM_DIR}
        )

add_definitions(-DDONT_USE_PRECOMPILED_HEADER)

set(SHARED_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/share${VM_DIR})

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(HS_CPU x86)
    set(HS_ARCH amd64)
    add_definitions(-DAMD64)
endif()


add_definitions(-DTARGET_ARCH_${HS_CPU})
set(CPU_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/cpu/${HS_CPU}${VM_DIR})
file(GLOB CPU_JVM_SOURCES
        ${CPU_VM_SOURCES_DIR}/*.cpp
        )

if (LINUX)
    add_definitions(-DLINUX -D_GNU_SOURCE)
    set(HS_OS linux)
elseif(DARWIN)
    add_definitions(-D_ALLBSD_SOURCE -D_GNU_SOURCE)
    set(HS_OS bsd)
endif()

set(OS_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/os/${HS_OS}${VM_DIR})
file(GLOB OS_JVM_SOURCES
    ${OS_VM_SOURCES_DIR}/*.cpp
    )

if (${HS_OS} STREQUAL "windows")
    set(POSIX_VM_SOURCES_DIR )
    set(POSIX_JVM_SOURCES )
else()
    set(POSIX_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/os/posix${VM_DIR})
    file(GLOB POSIX_JVM_SOURCES
            ${POSIX_VM_SOURCES_DIR}/*.cpp
            )
endif()

set(HS_VARIANT compiler2)

if (${HS_VARIANT} STREQUAL "compiler2")
    add_definitions(-DCOMPILER2)
    #assume tiered
    add_definitions(-DCOMPILER1)
endif()

add_definitions(-DTARGET_OS_ARCH_${HS_OS}_${HS_CPU}))
add_definitions(-DTARGET_ARCH_${HS_CPU})
set(BUILD_DIR ${BUILD_ROOT}/${HS_OS}/${HS_OS}_${HS_ARCH}_${HS_VARIANT})
set(GENERATED_DIR ${BUILD_DIR}/generated)
set(OS_CPU_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/os_cpu/${HS_OS}_${HS_CPU}${VM_DIR})
file(GLOB OS_CPU_JVM_SOURCES
        ${OS_CPU_VM_SOURCES_DIR}/*.cpp
        )


if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-DTARGET_COMPILER_gcc)
endif()

file(GLOB SHARED_JVM_SOURCES
        ${SHARED_VM_SOURCES_DIR}/asm/*.cpp
        ${SHARED_VM_SOURCES_DIR}/asm/*.c
        ${SHARED_VM_SOURCES_DIR}/asm/*.s
        ${SHARED_VM_SOURCES_DIR}/c1/*.cpp
        ${SHARED_VM_SOURCES_DIR}/c1/*.c
        ${SHARED_VM_SOURCES_DIR}/c1/*.s
        ${SHARED_VM_SOURCES_DIR}/classfile/*.cpp
        ${SHARED_VM_SOURCES_DIR}/classfile/*.c
        ${SHARED_VM_SOURCES_DIR}/classfile/*.s
        ${SHARED_VM_SOURCES_DIR}/code/*.cpp
        ${SHARED_VM_SOURCES_DIR}/code/*.c
        ${SHARED_VM_SOURCES_DIR}/code/*.s
        ${SHARED_VM_SOURCES_DIR}/compiler/*.cpp
        ${SHARED_VM_SOURCES_DIR}/compiler/*.c
        ${SHARED_VM_SOURCES_DIR}/compiler/*.s
        # missing: gc
        ${SHARED_VM_SOURCES_DIR}/interpreter/*.cpp
        ${SHARED_VM_SOURCES_DIR}/interpreter/*.c
        ${SHARED_VM_SOURCES_DIR}/interpreter/*.s
        ${SHARED_VM_SOURCES_DIR}/jvmci/*.cpp
        ${SHARED_VM_SOURCES_DIR}/jvmci/*.c
        ${SHARED_VM_SOURCES_DIR}/jvmci/*.s
        ${SHARED_VM_SOURCES_DIR}/libadt/*.cpp
        ${SHARED_VM_SOURCES_DIR}/libadt/*.c
        ${SHARED_VM_SOURCES_DIR}/libadt/*.s
        ${SHARED_VM_SOURCES_DIR}/memory/*.cpp
        ${SHARED_VM_SOURCES_DIR}/memory/*.c
        ${SHARED_VM_SOURCES_DIR}/memory/*.s
        ${SHARED_VM_SOURCES_DIR}/oops/*.cpp
        ${SHARED_VM_SOURCES_DIR}/oops/*.c
        ${SHARED_VM_SOURCES_DIR}/oops/*.s
        ${SHARED_VM_SOURCES_DIR}/opto/*.cpp
        ${SHARED_VM_SOURCES_DIR}/opto/*.c
        ${SHARED_VM_SOURCES_DIR}/opto/*.s
        ${SHARED_VM_SOURCES_DIR}/prims/*.cpp
        ${SHARED_VM_SOURCES_DIR}/prims/*.c
        ${SHARED_VM_SOURCES_DIR}/prims/*.s
        ${SHARED_VM_SOURCES_DIR}/runtime/*.cpp
        ${SHARED_VM_SOURCES_DIR}/runtime/*.c
        ${SHARED_VM_SOURCES_DIR}/runtime/*.s
        ${SHARED_VM_SOURCES_DIR}/services/*.cpp
        ${SHARED_VM_SOURCES_DIR}/services/*.c
        ${SHARED_VM_SOURCES_DIR}/services/*.s
        # missing: shark
        # missing: trace
        ${SHARED_VM_SOURCES_DIR}/utilities/*.cpp
        ${SHARED_VM_SOURCES_DIR}/utilities/*.c
        ${SHARED_VM_SOURCES_DIR}/utilities/*.s
        ${GENERATED_DIR}/jvmtifiles/*.cpp
        )

add_library(jvm
        ${SHARED_JVM_SOURCES}
        ${CPU_JVM_SOURCES}
        ${OS_JVM_SOURCES}
        ${OS_CPU_JVM_SOURCES}
        ${POSIX_JVM_SOURCES}
        )

target_include_directories(jvm PUBLIC
        ${SHARED_VM_SOURCES_DIR}
        ${SHARED_VM_SOURCES_DIR}/prims
        ${SHARED_VM_SOURCES_DIR}/precompiled
        ${CPU_VM_SOURCES_DIR}
        ${OS_VM_SOURCES_DIR}
        ${POSIX_VM_SOURCES_DIR}
        ${OS_CPU_VM_SOURCES_DIR}
        ${GENERATED_DIR}
        )
