set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "objdump")
set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${TOOLCHAIN_BIN}/objdump.exe")
file(GLOB_RECURSE bundled_dlls "${STAGING}/bin/*.dll")
file(GET_RUNTIME_DEPENDENCIES
  EXECUTABLES "${STAGING}/bin/daisywheel.exe"
  LIBRARIES ${bundled_dlls}
  DIRECTORIES "${TOOLCHAIN_BIN}"
  PRE_EXCLUDE_REGEXES "[Aa][Pp][Ii]-[Mm][Ss]-.*" "[Ee][Xx][Tt]-[Mm][Ss]-.*"
  POST_EXCLUDE_REGEXES "^[A-Za-z]:/[Ww][Ii][Nn][Dd][Oo][Ww][Ss]/.*"
  RESOLVED_DEPENDENCIES_VAR resolved
  UNRESOLVED_DEPENDENCIES_VAR unresolved)
if(unresolved)
  message(FATAL_ERROR "Unresolved Windows DLLs: ${unresolved}")
endif()
foreach(library IN LISTS resolved)
  get_filename_component(name "${library}" NAME)
  if(NOT EXISTS "${STAGING}/bin/${name}")
    file(COPY "${library}" DESTINATION "${STAGING}/bin")
  endif()
endforeach()
