# Bundles the MediaMTX executable used by the plugin's MediaMTX controls.
#
# MEDIAMTX_EXE can be set to a local mediamtx.exe to skip the download (useful for offline builds). Otherwise the pinned
# release is downloaded at configure time and verified with its SHA-256.

set(MEDIAMTX_VERSION
    "v1.21.4"
    CACHE STRING "MediaMTX release to bundle")
set(MEDIAMTX_EXE
    ""
    CACHE FILEPATH "Path to a pre-downloaded mediamtx.exe (optional)")

if(NOT MEDIAMTX_EXE)
  set(_mtx_local "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mediamtx/mediamtx.exe")
  if(EXISTS "${_mtx_local}")
    set(MEDIAMTX_EXE "${_mtx_local}")
  else()
    set(_mtx_dir "${CMAKE_BINARY_DIR}/mediamtx-${MEDIAMTX_VERSION}")
    set(_mtx_zip "${_mtx_dir}/mediamtx_${MEDIAMTX_VERSION}_windows_amd64.zip")
    if(NOT EXISTS "${_mtx_zip}")
      message(STATUS "Downloading MediaMTX ${MEDIAMTX_VERSION}")
      file(MAKE_DIRECTORY "${_mtx_dir}")
      file(
        DOWNLOAD
        "https://github.com/bluenviron/mediamtx/releases/download/${MEDIAMTX_VERSION}/mediamtx_${MEDIAMTX_VERSION}_windows_amd64.zip"
        "${_mtx_zip}"
        EXPECTED_HASH SHA256=835645b2700ffce4d2b6cb04bfa0aadce95335da6dfc0d7c2114e6da585734f8
        STATUS _mtx_status)
      list(GET _mtx_status 0 _mtx_code)
      if(NOT _mtx_code EQUAL 0)
        list(GET _mtx_status 1 _mtx_message)
        file(REMOVE "${_mtx_zip}")
        message(FATAL_ERROR "Failed to download MediaMTX: ${_mtx_message}")
      endif()
    endif()

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E tar xf "${_mtx_zip}"
      WORKING_DIRECTORY "${_mtx_dir}"
      RESULT_VARIABLE _mtx_extract)
    if(NOT _mtx_extract EQUAL 0)
      message(FATAL_ERROR "Failed to extract ${_mtx_zip}")
    endif()

    set(MEDIAMTX_EXE "${_mtx_dir}/mediamtx.exe")
  endif()
endif()

if(NOT EXISTS "${MEDIAMTX_EXE}")
  message(FATAL_ERROR "MediaMTX executable not found: ${MEDIAMTX_EXE}")
endif()
get_filename_component(MEDIAMTX_EXE "${MEDIAMTX_EXE}" ABSOLUTE)

set(_mtx_license "")
get_filename_component(_mtx_exe_dir "${MEDIAMTX_EXE}" DIRECTORY)
foreach(_candidate "${_mtx_exe_dir}/LICENSE" "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mediamtx/LICENSE")
  if(EXISTS "${_candidate}")
    set(_mtx_license "${_candidate}")
    break()
  endif()
endforeach()

set(_mtx_dest "data/obs-plugins/${CMAKE_PROJECT_NAME}/mediamtx")
install(FILES "${MEDIAMTX_EXE}" DESTINATION "${_mtx_dest}")
if(_mtx_license)
  install(FILES "${_mtx_license}" DESTINATION "${_mtx_dest}")
endif()

if(OBS_BUILD_DIR)
  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OBS_BUILD_DIR}/${_mtx_dest}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MEDIAMTX_EXE}" "${OBS_BUILD_DIR}/${_mtx_dest}/mediamtx.exe"
    COMMENT "Copy MediaMTX to obs-studio directory"
    VERBATIM)
  if(_mtx_license)
    add_custom_command(
      TARGET ${CMAKE_PROJECT_NAME}
      POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_mtx_license}" "${OBS_BUILD_DIR}/${_mtx_dest}/LICENSE"
      COMMENT "Copy MediaMTX license to obs-studio directory"
      VERBATIM)
  endif()
endif()
