cmake_minimum_required(VERSION 2.8.8)

set(BACKEND_PACKAGE_STRING "${PACKAGE_STRING}")

# The libdir suffix must exactly match whatever LLVM's configuration used.
set(GONG_LIBDIR_SUFFIX "${LLVM_LIBDIR_SUFFIX}")

set(GONG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(GONG_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

if( CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR AND NOT MSVC_IDE )
  message(FATAL_ERROR "In-source builds are not allowed. CMake would overwrite "
"the makefiles distributed with LLVM. Please create a directory and run cmake "
"from there, passing the path to this source directory as the last argument. "
"This process created the file `CMakeCache.txt' and the directory "
"`CMakeFiles'. Please delete them.")
endif()

if( NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR )
  file(GLOB_RECURSE
    tablegenned_files_on_include_dir
    "${GONG_SOURCE_DIR}/include/gong/*.inc")
  if( tablegenned_files_on_include_dir )
    message(FATAL_ERROR "Apparently there is a previous in-source build, "
"probably as the result of running `configure' and `make' on "
"${GONG_SOURCE_DIR}. This may cause problems. The suspicious files are:\n"
"${tablegenned_files_on_include_dir}\nPlease clean the source directory.")
  endif()
endif()

# Compute the Gong version from the LLVM version.
string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" GONG_VERSION
  ${PACKAGE_VERSION})
message(STATUS "Gong version: ${GONG_VERSION}")

string(REGEX REPLACE "([0-9]+)\\.[0-9]+(\\.[0-9]+)?" "\\1" GONG_VERSION_MAJOR
  ${GONG_VERSION})
string(REGEX REPLACE "[0-9]+\\.([0-9]+)(\\.[0-9]+)?" "\\1" GONG_VERSION_MINOR
  ${GONG_VERSION})
if (${GONG_VERSION} MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+")
  set(GONG_HAS_VERSION_PATCHLEVEL 1)
  string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" GONG_VERSION_PATCHLEVEL
    ${GONG_VERSION})
else()
  set(GONG_HAS_VERSION_PATCHLEVEL 0)
endif()

# Add appropriate flags for GCC
if (LLVM_COMPILER_IS_GCC_COMPATIBLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -Woverloaded-virtual")
  if (NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing")
  endif ()

  # Enable -pedantic for Gong even if it's not enabled for LLVM.
  if (NOT LLVM_ENABLE_PEDANTIC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wno-long-long")
  endif ()

  check_cxx_compiler_flag("-Werror -Wnested-anon-types" CXX_SUPPORTS_NO_NESTED_ANON_TYPES_FLAG)
  if( CXX_SUPPORTS_NO_NESTED_ANON_TYPES_FLAG )
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-nested-anon-types" )
  endif()
endif ()

# Determine HOST_LINK_VERSION on Darwin.
set(HOST_LINK_VERSION)
if (APPLE)
  set(LD_V_OUTPUT)
  execute_process(
    COMMAND sh -c "${CMAKE_LINKER} -v 2>&1 | head -1"
    RESULT_VARIABLE HAD_ERROR
    OUTPUT_VARIABLE LD_V_OUTPUT
  )
  if (NOT HAD_ERROR)
    if ("${LD_V_OUTPUT}" MATCHES ".*ld64-([0-9.]+).*")
      string(REGEX REPLACE ".*ld64-([0-9.]+).*" "\\1" HOST_LINK_VERSION ${LD_V_OUTPUT})
    elseif ("${LD_V_OUTPUT}" MATCHES "[^0-9]*([0-9.]+).*")
      string(REGEX REPLACE "[^0-9]*([0-9.]+).*" "\\1" HOST_LINK_VERSION ${LD_V_OUTPUT})
    endif()
  else()
    message(FATAL_ERROR "${CMAKE_LINKER} failed with status ${HAD_ERROR}")
  endif()
endif()

include(CMakeParseArguments)

macro(add_gong_library name)
  cmake_parse_arguments(ARG
    ""
    ""
    "ADDITIONAL_HEADERS"
    ${ARGN})
  set(srcs)
  if(MSVC_IDE OR XCODE)
    # Add public headers
    file(RELATIVE_PATH lib_path
      ${GONG_SOURCE_DIR}/lib/
      ${CMAKE_CURRENT_SOURCE_DIR}
    )
    if(NOT lib_path MATCHES "^[.][.]")
      file( GLOB_RECURSE headers
        ${GONG_SOURCE_DIR}/include/gong/${lib_path}/*.h
        ${GONG_SOURCE_DIR}/include/gong/${lib_path}/*.def
      )
      set_source_files_properties(${headers} PROPERTIES HEADER_FILE_ONLY ON)

      file( GLOB_RECURSE tds
        ${GONG_SOURCE_DIR}/include/gong/${lib_path}/*.td
      )
      source_group("TableGen descriptions" FILES ${tds})
      set_source_files_properties(${tds}} PROPERTIES HEADER_FILE_ONLY ON)

      if(headers OR tds)
        set(srcs ${headers} ${tds})
      endif()
    endif()
  endif(MSVC_IDE OR XCODE)
  if(srcs OR ARG_ADDITIONAL_HEADERS)
    set(srcs
      ADDITIONAL_HEADERS
      ${srcs}
      ${ARG_ADDITIONAL_HEADERS} # It may contain unparsed unknown args.
      )
  endif()
  llvm_add_library(${name} ${ARG_UNPARSED_ARGUMENTS} ${srcs})

  if(TARGET ${name})
    target_link_libraries(${name} ${cmake_2_8_12_INTERFACE} ${LLVM_COMMON_LIBS})
    set_property(GLOBAL APPEND PROPERTY GONG_EXPORTS ${name})
  else()
    # Add empty "phony" target
    add_custom_target(${name})
  endif()

  set_target_properties(${name} PROPERTIES FOLDER "Gong libraries")
endmacro(add_gong_library)

macro(add_gong_executable name)
  add_llvm_executable( ${name} ${ARGN} )
  set_target_properties(${name} PROPERTIES FOLDER "Gong executables")
endmacro(add_gong_executable)

macro(add_gong_symlink name dest)
  add_llvm_tool_symlink(${name} ${dest} ALWAYS_GENERATE)
  # Always generate install targets
  llvm_install_symlink(${name} ${dest} ALWAYS_GENERATE)
endmacro()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(BEFORE
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

add_definitions( -D_GNU_SOURCE )

# Gong version information
set(GONG_EXECUTABLE_VERSION
     "${GONG_VERSION_MAJOR}.${GONG_VERSION_MINOR}" CACHE STRING
    "Version number that will be placed into the gong executable, in the form XX.YY")
mark_as_advanced(GONG_EXECUTABLE_VERSION LIBGONG_LIBRARY_VERSION)

add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(tools)

option(GONG_INCLUDE_TESTS
       "Generate build targets for the Gong unit tests."
       ${LLVM_INCLUDE_TESTS})
if( GONG_INCLUDE_TESTS )
  add_subdirectory(test)
endif()
