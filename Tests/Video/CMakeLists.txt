# Copyright (c) 2026 NVIDIA Corporation

include(Catch)

if(NRI_VIDEO_ENABLE_SANITIZERS)
    if(MSVC OR NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR "NRI_VIDEO_ENABLE_SANITIZERS requires Clang or GCC on a non-MSVC toolchain")
    endif()

    set(NRI_VIDEO_SANITIZER_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    foreach(nri_target NRI_Shared NRI_NONE NRI_Validation NRI_VK NRI_D3D12 NRI)
        if(TARGET ${nri_target})
            target_compile_options(${nri_target} PRIVATE ${NRI_VIDEO_SANITIZER_FLAGS})
        endif()
    endforeach()
    target_link_options(NRI PRIVATE -fsanitize=address,undefined)
endif()

function(nri_configure_video_test target)
    target_compile_features(${target} PRIVATE cxx_std_17)
    target_compile_options(${target}
        PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:GNU>>:
                $<$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:
                    -Wextra
                    -Werror
                >
            >
            $<$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>:
                /W4
                /WX
            >
    )
    set_target_properties(${target}
        PROPERTIES
            FOLDER "NRI/Tests"
    )
    if(NRI_VIDEO_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE ${NRI_VIDEO_SANITIZER_FLAGS})
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

set(NRI_VIDEO_TEST_SOURCE
    "VideoAV1Tests.cpp"
    "VideoAnnexBTests.cpp"
    "VideoPublicApiTests.cpp"
    "VideoStateTests.cpp"
)
if(TARGET NRI_Validation)
    list(APPEND NRI_VIDEO_TEST_SOURCE "VideoValidationTests.cpp")
endif()
if(TARGET NRI_VK)
    list(APPEND NRI_VIDEO_TEST_SOURCE "VideoVulkanSessionTests.cpp")
endif()
if(WIN32 AND TARGET NRI_D3D12 AND NRI_ENABLE_AGILITY_SDK_SUPPORT)
    list(APPEND NRI_VIDEO_TEST_SOURCE "VideoD3D12LayoutTests.cpp")
endif()

add_executable(NRI_VideoTests ${NRI_VIDEO_TEST_SOURCE})
target_link_libraries(NRI_VideoTests
    PRIVATE
        Catch2::Catch2WithMain
        NRI_Shared
        NRI
)
if(TARGET NRI_Validation)
    target_link_libraries(NRI_VideoTests PRIVATE NRI_Validation)
endif()
if(TARGET NRI_VK)
    target_link_libraries(NRI_VideoTests PRIVATE NRI_VK)
    target_include_directories(NRI_VideoTests
        PRIVATE
            "${vma_SOURCE_DIR}/include"
            "${vulkan_headers_SOURCE_DIR}/include"
    )
    target_compile_definitions(NRI_VideoTests
        PRIVATE
            $<$<BOOL:${WIN32}>:VK_USE_PLATFORM_WIN32_KHR>
            $<$<BOOL:${APPLE}>:VK_USE_PLATFORM_METAL_EXT;VK_ENABLE_BETA_EXTENSIONS>
            $<$<BOOL:${NRI_ENABLE_XLIB_SUPPORT}>:VK_USE_PLATFORM_XLIB_KHR>
            $<$<BOOL:${NRI_ENABLE_WAYLAND_SUPPORT}>:VK_USE_PLATFORM_WAYLAND_KHR>
    )
endif()
if(WIN32 AND TARGET NRI_D3D12 AND NRI_ENABLE_AGILITY_SDK_SUPPORT)
    target_include_directories(NRI_VideoTests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/Source/D3D12"
            "${agilitysdk_SOURCE_DIR}/build/native/include"
    )
endif()
target_include_directories(NRI_VideoTests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/Source/Shared"
        "${PROJECT_SOURCE_DIR}/Source/Validation"
        "${PROJECT_SOURCE_DIR}/Source/VK"
)
nri_configure_video_test(NRI_VideoTests)

catch_discover_tests(NRI_VideoTests
    TEST_PREFIX "video."
    PROPERTIES
        LABELS "video-short"
)

if(NRI_ENABLE_NONE_SUPPORT)
    add_executable(NRI_VideoIntegrationTests VideoIntegrationTests.cpp)
    target_link_libraries(NRI_VideoIntegrationTests
        PRIVATE
            Catch2::Catch2WithMain
            NRI
    )
    nri_configure_video_test(NRI_VideoIntegrationTests)
    catch_discover_tests(NRI_VideoIntegrationTests
        TEST_PREFIX "video.integration."
        PROPERTIES
            LABELS "video-short"
    )
endif()

add_library(NRI_VideoCHeaderTest OBJECT VideoCHeaderTest.c)
target_include_directories(NRI_VideoCHeaderTest
    PRIVATE
        "${PROJECT_SOURCE_DIR}/Include"
)
set_target_properties(NRI_VideoCHeaderTest
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        FOLDER "NRI/Tests"
)

add_test(
    NAME video.traceability
    COMMAND ${CMAKE_COMMAND}
        -DTRACEABILITY_FILE=${CMAKE_CURRENT_SOURCE_DIR}/Traceability.json
        -DTEST_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/VerifyTraceability.cmake
)
set_tests_properties(video.traceability
    PROPERTIES
        LABELS "video-short"
)

if(NRI_BUILD_VIDEO_HARDWARE_TESTS)
    add_executable(NRI_VideoHardwareTests VideoHardwareTests.cpp)
    target_link_libraries(NRI_VideoHardwareTests
        PRIVATE
            Catch2::Catch2WithMain
            NRI
    )
    target_compile_definitions(NRI_VideoHardwareTests
        PRIVATE
            ${COMPILE_DEFINITIONS}
            NRI_VIDEO_EXPECT_D3D12_SUPPORT=$<BOOL:${NRI_ENABLE_D3D12_SUPPORT}>
            NRI_VIDEO_EXPECT_VK_SUPPORT=$<BOOL:${NRI_ENABLE_VK_SUPPORT}>
            NRI_VIDEO_REQUIRE_FULL_CODEC_MATRIX=$<BOOL:${NRI_VIDEO_REQUIRE_FULL_CODEC_MATRIX}>
    )
    nri_configure_video_test(NRI_VideoHardwareTests)

    catch_discover_tests(NRI_VideoHardwareTests
        TEST_PREFIX "video.hardware."
        PROPERTIES
            LABELS "video-hardware-short"
            RESOURCE_LOCK "nri-video-gpu"
            TIMEOUT 120
    )
endif()

foreach(test_type Fuzz Stress Soak)
    add_executable(NRI_Video${test_type}Tests Video${test_type}Tests.cpp)
    target_link_libraries(NRI_Video${test_type}Tests
        PRIVATE
            Catch2::Catch2WithMain
            NRI_Shared
    )
    target_include_directories(NRI_Video${test_type}Tests
        PRIVATE
            "${PROJECT_SOURCE_DIR}/Source/Shared"
    )
    set_target_properties(NRI_Video${test_type}Tests
        PROPERTIES
            FOLDER "NRI/Tests/Long"
    )
    if(NRI_VIDEO_ENABLE_SANITIZERS)
        target_compile_options(NRI_Video${test_type}Tests PRIVATE ${NRI_VIDEO_SANITIZER_FLAGS})
        target_link_options(NRI_Video${test_type}Tests PRIVATE -fsanitize=address,undefined)
    endif()

    string(TOLOWER ${test_type} test_label)
    if(NRI_VIDEO_REGISTER_LONG_TESTS)
        catch_discover_tests(NRI_Video${test_type}Tests
            TEST_PREFIX "video.${test_label}."
            PROPERTIES
                LABELS "video-${test_label}"
                TIMEOUT 7200
        )
    endif()
endforeach()

add_custom_target(NRI_VideoLongTests
    DEPENDS
        NRI_VideoFuzzTests
        NRI_VideoStressTests
        NRI_VideoSoakTests
    COMMAND $<TARGET_FILE:NRI_VideoFuzzTests>
    COMMAND $<TARGET_FILE:NRI_VideoStressTests>
    COMMAND $<TARGET_FILE:NRI_VideoSoakTests>
    COMMENT "Runs long NRI video tests; invoke explicitly"
    USES_TERMINAL
    VERBATIM
)
set_target_properties(NRI_VideoLongTests
    PROPERTIES
        FOLDER "NRI/Tests"
)
