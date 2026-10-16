add_executable(opena8dj_macos_lifecycle_audit
  ${CMAKE_SOURCE_DIR}/tools/audit_macos_lifecycle.m)
target_compile_options(opena8dj_macos_lifecycle_audit PRIVATE -fobjc-arc -fblocks)
target_link_libraries(opena8dj_macos_lifecycle_audit
  "-framework Foundation"
  "-framework IOKit"
  "-framework IOUSBHost"
  "-framework CoreMIDI"
  "-framework CoreAudio"
  "-framework CoreFoundation")
