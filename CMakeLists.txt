cmake_minimum_required(VERSION 2.8.12...3.19.1)
if(CMAKE_VERSION VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_VERSION})
endif()

file(STRINGS src/game/version.h VERSION_LINE
  LIMIT_COUNT 1
  REGEX EDITOR_RELEASE_VERSION
)

if(VERSION_LINE MATCHES "\"([0-9]+)\\.([0-9]+)\\.([0-9]+|[0-9]+\\.[0-9]+)\"")
  set(VERSION_MAJOR ${CMAKE_MATCH_1})
  set(VERSION_MINOR ${CMAKE_MATCH_2})
  set(VERSION_PATCH ${CMAKE_MATCH_3})
elseif(VERSION_LINE MATCHES "\"([0-9]+)\\.([0-9]+)\"")
  set(VERSION_MAJOR ${CMAKE_MATCH_1})
  set(VERSION_MINOR ${CMAKE_MATCH_2})
  set(VERSION_PATCH "0")
else()
  message(FATAL_ERROR "Couldn't parse version from src/game/version.h")
endif()

# Extra support for CMake pre-3.0
if(NOT POLICY CMP0048)
  set(PROJECT_VERSION_MAJOR ${VERSION_MAJOR})
  set(PROJECT_VERSION_MINOR ${VERSION_MINOR})
  set(PROJECT_VERSION_PATCH ${VERSION_PATCH})
  set(PROJECT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
endif()

project(teesolar VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(ORIGINAL_CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH})
set(ORIGINAL_CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
set(ORIGINAL_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
set(OWN_CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)
set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(TARGET_BITS "32")
else()
  set(TARGET_BITS "64")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(TARGET_OS "windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(TARGET_OS "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  set(TARGET_OS "mac")
endif()

include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)
include(CheckSymbolExists)

check_symbol_exists(__i386 "" TARGET_ARCH_X86_i386)
if(TARGET_ARCH_X86_i386)
  set(TARGET_ARCH x86)
else()
  set(TARGET_ARCH)
endif()

set(AUTO_DEPENDENCIES_DEFAULT OFF)
if(TARGET_OS STREQUAL "windows")
  set(AUTO_DEPENDENCIES_DEFAULT ON)
endif()

option(DEV "Don't generate stuff necessary for packaging" OFF)

set(OpenGL_GL_PREFERENCE GLVND)

# Set the default build type to Release
if(NOT(CMAKE_BUILD_TYPE))
  if(NOT(DEV))
    set(CMAKE_BUILD_TYPE Release)
  else()
    set(CMAKE_BUILD_TYPE Debug)
  endif()
endif()

set(DBG $<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  src/game/version.h
)

set(CLIENT_EXECUTABLE TeeSolar CACHE STRING "Name of the build client executable")

########################################################################
# Compiler flags
########################################################################

function(add_c_compiler_flag_if_supported VARIABLE FLAG)
  if(ARGC GREATER 2)
    set(CHECKED_FLAG "${ARGV2}")
  else()
    set(CHECKED_FLAG "${FLAG}")
  endif()
  string(REGEX REPLACE "[^A-Za-z0-9]" "_" CONFIG_VARIABLE "FLAG_SUPPORTED${CHECKED_FLAG}")
  check_c_compiler_flag("${CHECKED_FLAG}" ${CONFIG_VARIABLE})
  if(${CONFIG_VARIABLE})
    if(${VARIABLE})
      set("${VARIABLE}" "${${VARIABLE}};${FLAG}" PARENT_SCOPE)
    else()
      set("${VARIABLE}" "${FLAG}" PARENT_SCOPE)
    endif()
  endif()
endfunction()

if(NOT MSVC)
  # Protect the stack pointer.
  # -fstack-protector-all doesn't work on MinGW.
  add_c_compiler_flag_if_supported(OUR_FLAGS -fstack-protector-strong)

  # Protect the stack from clashing.
  add_c_compiler_flag_if_supported(OUR_FLAGS -fstack-clash-protection)

  # Control-flow protection. Should protect against ROP.
  add_c_compiler_flag_if_supported(OUR_FLAGS -fcf-protection)

  # Inaccurate floating point numbers cause problems on mingw-w64-gcc when
  # compiling for x86, might cause problems elsewhere. So don't store floats
  # in registers but keep them at higher accuracy.
  if(TARGET_ARCH STREQUAL "x86")
    add_c_compiler_flag_if_supported(OUR_FLAGS -ffloat-store)
  endif()

  # gcc < 4.10 chokes on _mm_pause on x86 without SSE support.
  if(TARGET_ARCH STREQUAL "x86")
    check_c_source_compiles("#include <immintrin.h>\nint main() { _mm_pause(); return 0; }" MM_PAUSE_WORKS_WITHOUT_MSSE2)
    if(NOT MM_PAUSE_WORKS_WITHOUT_MSSE2)
      add_c_compiler_flag_if_supported(OUR_FLAGS -msse2)
    endif()
  endif()

  if(TARGET_OS STREQUAL "mac")
    add_c_compiler_flag_if_supported(OUR_FLAGS -stdlib=libc++)
    add_c_compiler_flag_if_supported(OUR_FLAGS -mmacosx-version-min=12.0)
  endif()

  add_c_compiler_flag_if_supported(OUR_FLAGS_OWN -Wall)
  if(CMAKE_VERSION VERSION_GREATER 3.3 OR CMAKE_VERSION VERSION_EQUAL 3.3)
    add_c_compiler_flag_if_supported(OUR_FLAGS_OWN
      $<$<COMPILE_LANGUAGE:C>:-Wdeclaration-after-statement>
      -Wdeclaration-after-statement
    )
  endif()
  add_c_compiler_flag_if_supported(OUR_FLAGS_OWN -Wextra)
  add_c_compiler_flag_if_supported(OUR_FLAGS_OWN -Wno-unused-parameter)
  add_c_compiler_flag_if_supported(OUR_FLAGS_OWN -Wno-missing-field-initializers)
  add_c_compiler_flag_if_supported(OUR_FLAGS_OWN -Wformat=2) # Warn about format strings.
  add_c_compiler_flag_if_supported(OUR_FLAGS_DEP -Wno-implicit-function-declaration)
endif()

if(NOT MSVC)
  check_c_compiler_flag("-O2;-Wp,-Werror;-D_FORTIFY_SOURCE=2" DEFINE_FORTIFY_SOURCE) # Some distributions define _FORTIFY_SOURCE by themselves.
endif()

########################################################################
# COMMON FUNCTIONS
########################################################################

function(set_glob VAR GLOBBING EXTS DIRECTORY) # ...
  set(GLOBS)
  foreach(ext ${EXTS})
    list(APPEND GLOBS "${DIRECTORY}/*.${ext}")
  endforeach()
  file(${GLOBBING} GLOB_RESULT ${GLOBS})
  list(SORT GLOB_RESULT)
  set(FILES)
  foreach(file ${ARGN})
    list(APPEND FILES "${PROJECT_SOURCE_DIR}/${DIRECTORY}/${file}")
  endforeach()

  if(NOT FILES STREQUAL GLOB_RESULT)
    message(AUTHOR_WARNING "${VAR} does not contain every file from directory ${DIRECTORY}")
    set(LIST_BUT_NOT_GLOB)
    if(POLICY CMP0057)
      cmake_policy(SET CMP0057 NEW)
      foreach(file ${FILES})
        if(NOT file IN_LIST GLOB_RESULT)
          list(APPEND LIST_BUT_NOT_GLOB ${file})
        endif()
      endforeach()
      if(LIST_BUT_NOT_GLOB)
        message(AUTHOR_WARNING "Entries only present in ${VAR}: ${LIST_BUT_NOT_GLOB}")
      endif()
      set(GLOB_BUT_NOT_LIST)
      foreach(file ${GLOB_RESULT})
        if(NOT file IN_LIST FILES)
          list(APPEND GLOB_BUT_NOT_LIST ${file})
        endif()
      endforeach()
      if(GLOB_BUT_NOT_LIST)
        message(AUTHOR_WARNING "Entries only present in ${DIRECTORY}: ${GLOB_BUT_NOT_LIST}")
      endif()
      if(NOT LIST_BUT_NOT_GLOB AND NOT GLOB_BUT_NOT_LIST)
        message(AUTHOR_WARNING "${VAR} is not alphabetically sorted")
      endif()
    endif()
  endif()

  set(${VAR} ${FILES} PARENT_SCOPE)
endfunction()

function(set_src VAR GLOBBING DIRECTORY) # ...
  set_glob(${VAR} ${GLOBBING} "c;cpp;h" ${DIRECTORY} ${ARGN})
  set(${VAR} ${${VAR}} PARENT_SCOPE)
endfunction()

########################################################################
# INITIALIZE TARGET LISTS
########################################################################

set(TARGETS_OWN)
set(TARGETS_DEP)

set(TARGETS_LINK) # Targets with a linking stage.

########################################################################
# DEPENDENCIES
########################################################################

if((CMAKE_OSX_ARCHITECTURES STREQUAL "arm64;x86_64" OR CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64;arm64") AND TARGET_OS STREQUAL "mac")
  set(FAT ON)
else()
  set(FAT OFF)
endif()

if(FAT)
  set(LIB_DIR "${TARGET_OS}/libfat")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  set(LIB_DIR "webasm/libwasm")
elseif(TARGET_CPU_ARCHITECTURE STREQUAL "arm" OR TARGET_CPU_ARCHITECTURE STREQUAL "arm64")
  set(LIB_DIR "${TARGET_OS}/lib${TARGET_CPU_ARCHITECTURE}")
else()
  set(LIB_DIR "${TARGET_OS}/lib${TARGET_BITS}")
endif()

function(set_extra_dirs_lib VARIABLE NAME)
  set("PATHS_${VARIABLE}_LIBDIR" PARENT_SCOPE)
  set("HINTS_${VARIABLE}_LIBDIR" PARENT_SCOPE)
  if(PREFER_BUNDLED_LIBS)
    set(TYPE HINTS)
  else()
    set(TYPE PATHS)
  endif()
  if(TARGET_BITS AND TARGET_OS)
    set(DIR "libs/${NAME}/${LIB_DIR}")
    set("${TYPE}_${VARIABLE}_LIBDIR" "${DIR}" PARENT_SCOPE)
    set("EXTRA_${VARIABLE}_LIBDIR" "${DIR}" PARENT_SCOPE)
  endif()
endfunction()

function(set_extra_dirs_include VARIABLE NAME LIBRARY)
  set("PATHS_${VARIABLE}_INCLUDEDIR" PARENT_SCOPE)
  set("HINTS_${VARIABLE}_INCLUDEDIR" PARENT_SCOPE)
  is_bundled(IS_BUNDLED "${LIBRARY}")
  if(IS_BUNDLED)
    set(TMP_TARGET_OS ${TARGET_OS})
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
     set(TMP_TARGET_OS webasm)
    endif()
    set("HINTS_${VARIABLE}_INCLUDEDIR" "libs/${NAME}/include" "libs/${NAME}/include/${TMP_TARGET_OS}" PARENT_SCOPE)
  endif()
endfunction()

if(CMAKE_CROSSCOMPILING)
  set(CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH NO_CMAKE_SYSTEM_PATH)
else()
  set(CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH)
endif()

function(is_bundled VARIABLE PATH)
  if(PATH)
    string(FIND "${PATH}" "${PROJECT_SOURCE_DIR}" LOCAL_PATH_POS)
    if(LOCAL_PATH_POS EQUAL 0 AND TARGET_BITS AND TARGET_OS)
      set("${VARIABLE}" ON PARENT_SCOPE)
    else()
      set("${VARIABLE}" OFF PARENT_SCOPE)
    endif()
  else()
    set("${VARIABLE}" OFF PARENT_SCOPE)
  endif()
endfunction()
find_package(PythonInterp)

if(NOT CMAKE_CROSSCOMPILING)
  # Check for PkgConfig once so all the other `find_package` calls can do it
  # quietly.
  find_package(PkgConfig)
endif()
find_package(ZLIB)
find_package(Crypto)
find_package(Freetype)
find_package(Git)
find_package(GTest)
find_package(Pnglite)
find_package(SDL2)
find_package(Threads)
find_package(Wavpack)


if(TARGET_OS AND TARGET_OS STREQUAL "mac")
  find_program(CMAKE_OTOOL otool)
  find_program(DMG dmg)
  find_program(HFSPLUS hfsplus)
  find_program(NEWFS_HFS newfs_hfs)
  if(DMG AND HFSPLUS AND NEWFS_HFS)
    set(DMGTOOLS_FOUND ON)
  else()
    set(DMGTOOLS_FOUND OFF)
  endif()

  find_program(HDIUTIL hdiutil)
  if(HDIUTIL)
    set(HDIUTIL_FOUND ON)
  else()
    set(HDIUTIL_FOUND OFF)
  endif()
endif()

message(STATUS "******** TeeSolar ********")
message(STATUS "Target OS: ${TARGET_OS} ${TARGET_BITS}bit")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

message(STATUS "Dependencies:")
function(show_dependency_status OUTPUT_NAME NAME)
  if(${NAME}_FOUND)
    if(${NAME}_BUNDLED)
      message(STATUS " * ${OUTPUT_NAME} not found (using bundled version)")
    else()
      message(STATUS " * ${OUTPUT_NAME} found")
    endif()
  else()
    message(STATUS " * ${OUTPUT_NAME} not found")
  endif()
endfunction()

if(TARGET_OS AND TARGET_OS STREQUAL "mac")
  show_dependency_status("Dmg tools" DMGTOOLS)
endif()
show_dependency_status("Freetype" FREETYPE)
if(TARGET_OS AND TARGET_OS STREQUAL "mac")
  show_dependency_status("Hdiutil" HDIUTIL)
endif()
show_dependency_status("OpenSSL Crypto" CRYPTO)
show_dependency_status("Pnglite" PNGLITE)
show_dependency_status("PythonInterp" PYTHONINTERP)
show_dependency_status("SDL2" SDL2)
show_dependency_status("Wavpack" WAVPACK)
show_dependency_status("Zlib" ZLIB)

if(NOT(PYTHONINTERP_FOUND))
  message(SEND_ERROR "You must install Python to compile TeeSolar")
endif()

if(CLIENT AND NOT(FREETYPE_FOUND))
  message(SEND_ERROR "You must install Freetype to compile the TeeSolar")
endif()
if(CLIENT AND NOT(SDL2_FOUND))
  message(SEND_ERROR "You must install SDL2 to compile the TeeSolar")
endif()
if(NOT(GTEST_FOUND))
  if(DOWNLOAD_GTEST)
    if(GIT_FOUND)
      message(STATUS "Automatically downloading GTest to be able to run tests")
    else()
      set(DOWNLOAD_GTEST OFF)
      message(WARNING "To automatically download GTest, you have to install Git")
    endif()
  else()
    message(STATUS "To run the tests, you have to install GTest")
  endif()
endif()

if(TARGET_OS STREQUAL "windows")
  set(PLATFORM_CLIENT)
  set(PLATFORM_CLIENT_LIBS opengl32 winmm imm32)
  set(PLATFORM_LIBS ws2_32) # Windows sockets
elseif(TARGET_OS STREQUAL "mac")
  find_library(CARBON Carbon)
  find_library(COCOA Cocoa)
  find_library(OPENGL OpenGL)
  set(PLATFORM_CLIENT
    src/osxlaunch/client.m
  )
  set(PLATFORM_CLIENT_LIBS ${COCOA} ${OPENGL})
  set(PLATFORM_LIBS ${CARBON})
else()
  set(PLATFORM_CLIENT)
  find_package(OpenGL)
  if(CMAKE_VERSION VERSION_LESS 3.10)
    set(PLATFORM_CLIENT_LIBS ${OPENGL_gl_LIBRARY})
  else()
    set(PLATFORM_CLIENT_LIBS ${OPENGL_opengl_LIBRARY})
  endif()
  set(PLATFORM_CLIENT_INCLUDE_DIRS ${OPENGL_INCLUDE_DIR})
  if(TARGET_OS STREQUAL "linux")
    set(PLATFORM_LIBS rt) # clock_gettime for glibc < 2.17
  else()
    set(PLATFORM_LIBS)
  endif()
endif()

########################################################################
# DOWNLOAD GTEST
########################################################################

if(NOT(GTEST_FOUND) AND DOWNLOAD_GTEST)
  set(TEEWORLDS_GTEST_VERSION release-1.8.1)
  configure_file(cmake/Download_GTest_CMakeLists.txt.in googletest-download/CMakeLists.txt)
  execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE result
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/googletest-download
  )
  if(result)
    message(WARNING "CMake step for googletest failed: ${result}")
    set(DOWNLOAD_GTEST OFF)
  else()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
      RESULT_VARIABLE result
      WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/googletest-download
    )
    if(result)
      message(WARNING "Build step for googletest failed: ${result}")
      set(DOWNLOAD_GTEST OFF)
    else()
      file(GLOB_RECURSE DDNET_GTEST_CMAKELISTS ${CMAKE_BINARY_DIR}/googletest-src/CMakeLists.txt)
      foreach(file ${DDNET_GTEST_CMAKELISTS})
        file(READ ${file} CONTENTS)
        string(REPLACE "cmake_minimum_required(VERSION 2.6.4)" "cmake_minimum_required(VERSION 2.8.12...3.19.1)" CONTENTS "${CONTENTS}")
        string(REPLACE "cmake_minimum_required(VERSION 2.6.4)" "cmake_minimum_required(VERSION 2.8.12...3.19.1)" CONTENTS "${CONTENTS}")
        string(REPLACE "cmake_minimum_required(VERSION 2.8.8)" "cmake_minimum_required(VERSION 2.8.12...3.19.1)" CONTENTS "${CONTENTS}")
        file(WRITE ${file} "${CONTENTS}")
      endforeach()

      # Prevent overriding the parent project's compiler/linker settings on Windows
      set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

      # Add googletest directly to our build. This defines the gtest target.
      add_subdirectory(
        ${PROJECT_BINARY_DIR}/googletest-src
        ${PROJECT_BINARY_DIR}/googletest-build
        EXCLUDE_FROM_ALL
      )

      if(MSVC)
        foreach(target gtest)
          # `/w` disables all warnings. This is needed because `gtest` enables
          # `/WX` (equivalent of `-Werror`) for some reason, breaking builds
          # when MSVS adds new warnings.
          target_compile_options(${target} PRIVATE /w)
          if(POLICY CMP0091)
            set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded$<${DBG}:Debug>)
          else()
            target_compile_options(${target} PRIVATE $<$<NOT:${DBG}>:/MT> $<${DBG}:/MTd>)
          endif()
        endforeach()
      endif()

      set(GTEST_LIBRARIES gtest)
      set(GTEST_INCLUDE_DIRS)
      if(CMAKE_VERSION VERSION_LESS 2.8.11)
        set(GTEST_INCLUDE_DIRS "${gtest_SOURCE_DIR}/include")
      endif()
    endif()
  endif()
endif()

########################################################################
# DEPENDENCY COMPILATION
########################################################################

set_src(DEP_JSON_SRC GLOB src/engine/external/json-parser json.c json.h)
add_library(json EXCLUDE_FROM_ALL OBJECT ${DEP_JSON_SRC})

set_src(DEP_MD5_SRC GLOB src/engine/external/md5 md5.c md5.h)
add_library(md5 EXCLUDE_FROM_ALL OBJECT ${DEP_MD5_SRC})

list(APPEND TARGETS_DEP json md5)
set(DEP_JSON $<TARGET_OBJECTS:json>)
set(DEP_MD5)
if(NOT CRYPTO_FOUND)
  set(DEP_MD5 $<TARGET_OBJECTS:md5>)
endif()

########################################################################
# CODE GENERATION
########################################################################

function(chash output_file)
  add_custom_command(OUTPUT ${output_file}
    COMMAND ${PYTHON_EXECUTABLE} scripts/cmd5.py ${ARGN}
      > "${PROJECT_BINARY_DIR}/${output_file}"
    DEPENDS scripts/cmd5.py ${ARGN}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endfunction()

function(generate_source output_file script_parameter)
  add_custom_command(OUTPUT ${output_file}
    COMMAND ${PYTHON_EXECUTABLE} datasrc/compile.py ${script_parameter}
      > "${PROJECT_BINARY_DIR}/${output_file}"
    DEPENDS
      datasrc/compile.py
      datasrc/content.py
      datasrc/datatypes.py
      datasrc/network.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endfunction()

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src/generated/")
if(GIT_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --git-dir
    ERROR_QUIET
    OUTPUT_VARIABLE PROJECT_GIT_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE PROJECT_GIT_DIR_ERROR
  )
  if(NOT PROJECT_GIT_DIR_ERROR)
    set(GIT_REVISION_EXTRA_DEPS
      ${PROJECT_GIT_DIR}/index
      ${PROJECT_GIT_DIR}/logs/HEAD
    )
  endif()
endif()
add_custom_command(OUTPUT ${PROJECT_BINARY_DIR}/src/generated/git_revision.cpp
  COMMAND ${PYTHON_EXECUTABLE}
    scripts/git_revision.py
    > ${PROJECT_BINARY_DIR}/src/generated/git_revision.cpp
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  DEPENDS
    ${GIT_REVISION_EXTRA_DEPS}
    scripts/git_revision.py
)
chash("src/generated/nethash.cpp"
  "src/engine/shared/protocol.h"
  "src/game/tuning.h"
  "src/game/gamecore.cpp"
  "${PROJECT_BINARY_DIR}/src/generated/protocol.h"
)
generate_source("src/generated/client_data.cpp" "client_content_source")
generate_source("src/generated/client_data.h" "client_content_header")

########################################################################
# SHARED
########################################################################

# Sources
file(GLOB_RECURSE BASE "src/base/*.cpp" "src/base/*.c" "src/base/*.h")

file(GLOB ENGINE_INTERFACE "src/engine/*.cpp" "src/engine/*.h")

file(GLOB ENGINE_SHARED "src/engine/shared/*.cpp" "src/engine/shared/*.h")

file(GLOB GAME_SHARED "src/game/*.cpp" "src/game/*.h")

set(DEPS ${DEP_JSON} ${DEP_MD5} ${ZLIB_DEP})

# Libraries
set(LIBS ${CMAKE_THREAD_LIBS_INIT} ${ZLIB_LIBRARIES} ${CRYPTO_LIBRARIES} ${PLATFORM_LIBS})

# Targets
add_library(engine-shared EXCLUDE_FROM_ALL OBJECT ${ENGINE_INTERFACE} ${ENGINE_SHARED} ${BASE})
add_library(game-shared EXCLUDE_FROM_ALL OBJECT ${GAME_SHARED})
list(APPEND TARGETS_OWN engine-shared game-shared)


########################################################################
# CLIENT
########################################################################

file(GLOB ENGINE_CLIENT "src/engine/client/*.cpp" "src/engine/client/*.h")

file(GLOB_RECURSE GAME_CLIENT "src/game/gui/*.cpp" "src/game/gui/*.h")

file(GLOB_RECURSE GAME_EDITOR "src/game/editor/*.cpp" "src/game/editor/*.h")
set(GAME_GENERATED_CLIENT
  src/generated/client_data.cpp
  src/generated/client_data.h
)
set(CLIENT_SRC ${PLATFORM_CLIENT} ${ENGINE_CLIENT} ${GAME_CLIENT} ${GAME_EDITOR} ${GAME_GENERATED_CLIENT})

set(DEPS_CLIENT ${DEPS} ${PNGLITE_DEP} ${WAVPACK_DEP})

# Libraries
set(LIBS_CLIENT
  ${LIBS}
  ${FREETYPE_LIBRARIES}
  ${PNGLITE_LIBRARIES}
  ${SDL2_LIBRARIES}
  ${WAVPACK_LIBRARIES}
  ${PLATFORM_CLIENT_LIBS}
)

if(TARGET_OS STREQUAL "windows")
  set(CLIENT_ICON "other/icons/teeworlds.rc")
  if(NOT MINGW)
    set(CLIENT_MANIFEST "other/manifest/teeworlds.manifest")
  else()
    set(CLIENT_MANIFEST "other/manifest/teeworlds.rc")
  endif()
else()
  set(CLIENT_ICON)
endif()

# Target
set(TARGET_CLIENT ${CLIENT_EXECUTABLE})
add_executable(${TARGET_CLIENT}
  ${CLIENT_SRC}
  ${CLIENT_ICON}
  ${CLIENT_MANIFEST}
  ${DEPS_CLIENT}
  $<TARGET_OBJECTS:engine-shared>
  $<TARGET_OBJECTS:game-shared>
)
target_link_libraries(${TARGET_CLIENT} ${LIBS_CLIENT})

target_include_directories(${TARGET_CLIENT} PRIVATE
  ${FREETYPE_INCLUDE_DIRS}
  ${PNGLITE_INCLUDE_DIRS}
  ${SDL2_INCLUDE_DIRS}
  ${WAVPACK_INCLUDE_DIRS}
)

set(PARAMS "${WAVPACK_INCLUDE_DIRS};${WAVPACK_INCLUDE_DIRS}")
if(NOT(WAVPACK_OPEN_FILE_INPUT_EX_PARAMS STREQUAL PARAMS))
  unset(WAVPACK_OPEN_FILE_INPUT_EX CACHE)
endif()
set(WAVPACK_OPEN_FILE_INPUT_EX_PARAMS "${PARAMS}" CACHE INTERNAL "")

set(CMAKE_REQUIRED_INCLUDES ${ORIGINAL_CMAKE_REQUIRED_INCLUDES} ${WAVPACK_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${ORIGINAL_CMAKE_REQUIRED_LIBRARIES} ${WAVPACK_LIBRARIES})
check_symbol_exists(WavpackOpenFileInputEx wavpack.h WAVPACK_OPEN_FILE_INPUT_EX)
set(CMAKE_REQUIRED_INCLUDES ${ORIGINAL_CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_LIBRARIES ${ORIGINAL_CMAKE_REQUIRED_LIBRARIES})

if(WAVPACK_OPEN_FILE_INPUT_EX)
  target_compile_definitions(${TARGET_CLIENT} PRIVATE CONF_WAVPACK_OPEN_FILE_INPUT_EX)
endif()

list(APPEND TARGETS_OWN ${TARGET_CLIENT})
list(APPEND TARGETS_LINK ${TARGET_CLIENT})

add_custom_target(everything DEPENDS ${TARGETS_OWN})

########################################################################
# TESTS
########################################################################

if(GTEST_FOUND OR DOWNLOAD_GTEST)
  set_src(TESTS GLOB src/test
    bytes_be.cpp
    compression.cpp
    datafile.cpp
    fs.cpp
    git_revision.cpp
    hash.cpp
    io.cpp
    jsonwriter.cpp
    sorted_array.cpp
    storage.cpp
    str.cpp
    test.cpp
    test.h
    thread.cpp
  )
  set(TARGET_TESTRUNNER testrunner)
  add_executable(${TARGET_TESTRUNNER} EXCLUDE_FROM_ALL
    ${TESTS}
    $<TARGET_OBJECTS:engine-shared>
    $<TARGET_OBJECTS:game-shared>
    ${DEPS}
  )
  target_link_libraries(${TARGET_TESTRUNNER} ${LIBS} ${GTEST_LIBRARIES})
  target_include_directories(${TARGET_TESTRUNNER} PRIVATE ${GTEST_INCLUDE_DIRS})

  list(APPEND TARGETS_OWN ${TARGET_TESTRUNNER})
  list(APPEND TARGETS_LINK ${TARGET_TESTRUNNER})

  add_custom_target(run_tests
    COMMAND $<TARGET_FILE:${TARGET_TESTRUNNER}> ${TESTRUNNER_ARGS}
    COMMENT Running tests
    DEPENDS ${TARGET_TESTRUNER}
    USES_TERMINAL
  )
endif()

########################################################################
# INSTALLATION
########################################################################

install(
  DIRECTORY
    "data/"
  DESTINATION
    "data"
)
install(
  TARGETS
    TeeSolar
  DESTINATION
    "."
)
install(
  FILES
    "license-teeworlds.txt"
    "LICENSE"
  DESTINATION
    "."
)

if(TARGET_OS STREQUAL "windows")

  message(STATUS "Library files found: ${FREETYPE_COPY_FILES} ${SDL2_COPY_FILES}")

  install(
    FILES  
      "${PROJECT_BINARY_DIR}/${FREETYPE_COPY_FILES}"
      "${PROJECT_BINARY_DIR}/${SDL2_COPY_FILES}"
    DEFINITION
      "."
  )
endif()

install(
  FILES
    "storage.cfg"
  DESTINATION
    "."
)

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_GENERATOR TXZ)

if(TARGET_OS STREQUAL "windows")
  set(CPACK_GENERATOR ZIP)
endif()
include(CPack)

########################################################################
# COMPILER-SPECIFICS
########################################################################

# In the future (CMake 3.8.0+), use source_group(TREE ...)
macro(source_group_tree dir)
  file(GLOB ents RELATIVE ${PROJECT_SOURCE_DIR}/${dir} ${PROJECT_SOURCE_DIR}/${dir}/*)
  foreach(ent ${ents})
    if(IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${dir}/${ent})
      source_group_tree(${dir}/${ent})
    else()
      string(REPLACE "/" "\\" group ${dir})
      source_group(${group} FILES ${PROJECT_SOURCE_DIR}/${dir}/${ent})
    endif()
  endforeach()
endmacro()
source_group_tree(src)

set(TARGETS ${TARGETS_OWN} ${TARGETS_DEP})

foreach(target ${TARGETS})
  if(MSVC)
    if(POLICY CMP0091)
      set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<${DBG}:Debug>")
    else()
      target_compile_options(${target} PRIVATE $<$<NOT:${DBG}>:/MT> $<${DBG}:/MTd>)
    endif()
    target_compile_options(${target} PRIVATE /MP) # Use multiple cores
    target_compile_options(${target} PRIVATE /EHsc) # Only catch C++ exceptions with catch.
    target_compile_options(${target} PRIVATE /GS) # Protect the stack pointer.
    target_compile_options(${target} PRIVATE /wd4996) # Use of non-_s functions.
    target_compile_options(${target} PRIVATE /utf-8) # Use UTF-8 for source files.
  endif()
  if(OUR_FLAGS)
    target_compile_options(${target} PRIVATE ${OUR_FLAGS})
  endif()
  if(DEFINE_FORTIFY_SOURCE)
    target_compile_definitions(${target} PRIVATE $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>) # Detect some buffer overflows.
  endif()
  target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS) # Enable run-time bounds-checking for the STL
endforeach()

foreach(target ${TARGETS_LINK})
  if(MSVC)
    set_property(TARGET ${target} APPEND PROPERTY LINK_FLAGS /SAFESEH:NO) # Disable SafeSEH because the shipped libraries don't support it (would cause error LNK2026 otherwise).
  endif()
  if(TARGET_OS STREQUAL "mac")
    target_link_libraries(${target} -stdlib=libc++)
    target_link_libraries(${target} -mmacosx-version-min=12.0)
  endif()
  if((MINGW OR TARGET_OS STREQUAL "linux") AND PREFER_BUNDLED_LIBS)
    # Statically link the standard libraries with on MinGW/Linux so we don't
    # have to ship them as DLLs.
    target_link_libraries(${target} -static-libgcc)
    target_link_libraries(${target} -static-libstdc++)
    if(MINGW)
      # Link pthread library statically instead of dynamically.
      # Solution from https://stackoverflow.com/a/28001261.
      target_link_libraries(${target} -Wl,-Bstatic -lstdc++ -lpthread -Wl,-Bdynamic)
    endif()
  endif()
endforeach()

foreach(target ${TARGETS_OWN})
  if(MSVC)
    target_compile_options(${target} PRIVATE /W3)
    target_compile_options(${target} PRIVATE /wd4244) # Possible loss of data (float -> int, int -> float, etc.).
    target_compile_options(${target} PRIVATE /wd4267) # Possible loss of data (size_t - int on win64).
    target_compile_options(${target} PRIVATE /wd4800) # Implicit conversion of int to bool.
  endif()
  if(TARGET_OS STREQUAL "windows")
    target_compile_definitions(${target} PRIVATE UNICODE) # Windows headers
    target_compile_definitions(${target} PRIVATE _UNICODE) # C-runtime
  endif()
  if(OUR_FLAGS_OWN)
    target_compile_options(${target} PRIVATE ${OUR_FLAGS_OWN})
  endif()
  target_include_directories(${target} PRIVATE ${PROJECT_BINARY_DIR}/src)
  target_include_directories(${target} PRIVATE src)
  target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:CONF_DEBUG>)
  target_include_directories(${target} PRIVATE ${CURL_INCLUDE_DIRS})
  target_include_directories(${target} PRIVATE ${ZLIB_INCLUDE_DIRS})
  if(CRYPTO_FOUND)
    target_compile_definitions(${target} PRIVATE CONF_OPENSSL)
    target_include_directories(${target} PRIVATE ${CRYPTO_INCLUDE_DIRS})
  endif()
  if(HEADLESS_CLIENT)
    target_compile_definitions(${target} PRIVATE CONF_HEADLESS_CLIENT)
  endif()
endforeach()

foreach(target ${TARGETS_DEP})
  if(MSVC)
    target_compile_options(${target} PRIVATE /W0)
  endif()
  if(OUR_FLAGS_DEP)
    target_compile_options(${target} PRIVATE ${OUR_FLAGS_DEP})
  endif()
endforeach()
