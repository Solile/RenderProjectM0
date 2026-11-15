# D3D12 Agility SDK: a newer D3D12 runtime plus the debug layer, shipped next to the exe.
# The pinned NuGet package is downloaded at configure time and checked against its SHA256.
# No admin rights, nothing installed system-wide, no DLLs committed to the repo.

set(AGILITY_SDK_VERSION_FULL 1.620.2)
set(AGILITY_SDK_VERSION 620)  # exported by the exe as D3D12SDKVersion; must match the package
set(AGILITY_SDK_SHA256 70489669c2bbeaf318b62d72b0ac3e753d521387bfc848c9763791c4e920bfcc)

set(_agility_pkg ${CMAKE_BINARY_DIR}/_deps/agility-${AGILITY_SDK_VERSION_FULL}.nupkg)
set(_agility_dir ${CMAKE_BINARY_DIR}/_deps/agility-${AGILITY_SDK_VERSION_FULL})
set(_agility_bin ${_agility_dir}/build/native/bin/x64)

if(NOT EXISTS ${_agility_bin}/D3D12Core.dll OR NOT EXISTS ${_agility_bin}/d3d12SDKLayers.dll)
  message(STATUS "Downloading D3D12 Agility SDK ${AGILITY_SDK_VERSION_FULL}")
  file(DOWNLOAD
    https://api.nuget.org/v3-flatcontainer/microsoft.direct3d.d3d12/${AGILITY_SDK_VERSION_FULL}/microsoft.direct3d.d3d12.${AGILITY_SDK_VERSION_FULL}.nupkg
    ${_agility_pkg}
    EXPECTED_HASH SHA256=${AGILITY_SDK_SHA256}
    STATUS _agility_status)
  list(GET _agility_status 0 _agility_code)
  if(NOT _agility_code EQUAL 0)
    message(FATAL_ERROR "Agility SDK download failed: ${_agility_status}")
  endif()
  file(ARCHIVE_EXTRACT INPUT ${_agility_pkg} DESTINATION ${_agility_dir}
    PATTERNS build/native/bin/x64/D3D12Core.dll build/native/bin/x64/d3d12SDKLayers.dll)
endif()

# Copies the runtime into <exe dir>/D3D12/, which is where D3D12SDKPath points.
function(agility_sdk_deploy target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target}>/D3D12
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${_agility_bin}/D3D12Core.dll ${_agility_bin}/d3d12SDKLayers.dll
      $<TARGET_FILE_DIR:${target}>/D3D12/
    VERBATIM)
endfunction()
