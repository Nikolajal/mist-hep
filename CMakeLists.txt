cmake_minimum_required(VERSION 3.19)

project(mist-hep
    VERSION     1.0.0
    DESCRIPTION "MIST-HEP — ROOT-backed analysis helpers built on mist"
    LANGUAGES   CXX
)

# ------------------------------------------------------------------
# Dependencies
#
# Two consumption modes are supported:
#   1. Standalone (find_package both).
#   2. As a sibling subdir under a parent build that already added mist
#      (mist::mist target already exists — skip the find_package).
# ------------------------------------------------------------------
if(NOT TARGET mist::mist)
    find_package(mist 1 REQUIRED)   # SameMajorVersion: any mist 1.x
endif()

# ROOT — pulled in PUBLIC so any consumer of mist::hep also sees the
# ROOT include dirs and link flags (our headers expose TH1F* etc.).
#
# RooFit / RooFitCore are intentionally NOT linked (mist-hep:D-02). Nothing
# in the current surface uses them; they will be re-added when the first
# RooFit-using port lands (likely the systematics Barlow pipeline at M-5).
find_package(ROOT 6.24 REQUIRED
    COMPONENTS Core Hist Graf Gpad RIO Tree
               MathCore Matrix Minuit
)
include(${ROOT_USE_FILE})

# ------------------------------------------------------------------
# Library target
# ------------------------------------------------------------------
add_library(mist-hep STATIC
    src/hep/fit/functions.cxx
)

target_include_directories(mist-hep
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(mist-hep PUBLIC cxx_std_20)

target_link_libraries(mist-hep
    PUBLIC
        mist::mist
        ROOT::Core ROOT::Hist ROOT::Graf ROOT::Gpad ROOT::RIO ROOT::Tree
        ROOT::MathCore ROOT::Matrix ROOT::Minuit
)

# Namespaced alias — consumers write `mist::hep` regardless of whether
# they pulled us in via find_package() or add_subdirectory()/FetchContent.
add_library(mist::hep ALIAS mist-hep)

# ------------------------------------------------------------------
# Install rules — mirrors mist's shape exactly
# ------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS mist-hep
    EXPORT      mist-hepTargets
    ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY     DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME     DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT mist-hepTargets
    FILE        mist-hepTargets.cmake
    NAMESPACE   mist::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mist-hep
)

# Build-tree export — lets a downstream consumer point
# -Dmist-hep_DIR=/path/to/mist-hep/build at an un-installed build tree
# (e.g. a future mist-heavyalgo sibling). Mirrors mist's own build-tree
# export; without it, find_package(mist-hep) against the build dir would
# fail because mist-hepTargets.cmake is only written by the install step.
export(EXPORT mist-hepTargets
    NAMESPACE mist::
    FILE      "${CMAKE_CURRENT_BINARY_DIR}/mist-hepTargets.cmake"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mist-hepConfigVersion.cmake"
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mist-hepConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mist-hepConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mist-hep
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mist-hepConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mist-hepConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mist-hep
)

# ------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------
option(MIST_HEP_BUILD_TESTS "Build mist-hep test executables" OFF)
enable_testing()

if(MIST_HEP_BUILD_TESTS)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

    function(mist_hep_add_test name)
        add_executable(test_${name} test/tester_${name}.cxx)
        target_link_libraries(test_${name} PRIVATE mist::hep)
        target_compile_features(test_${name} PRIVATE cxx_std_20)
        add_test(NAME test_${name} COMMAND test_${name})
    endfunction()

    mist_hep_add_test(stats)
    mist_hep_add_test(fit)
    mist_hep_add_test(graph)
    mist_hep_add_test(histo)
    mist_hep_add_test(owned)
    mist_hep_add_test(signal)

    message(STATUS "[mist-hep] Tests enabled — binaries in ${CMAKE_BINARY_DIR}/bin")
endif()
