file(
 GLOB_RECURSE FINAL_INC CONFIGURE_DEPENDS
 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
 *.h *.hpp
)

file(
 GLOB_RECURSE FINAL_SRC CONFIGURE_DEPENDS
 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
 *.c *.cpp
)
#Copies this assignment9's asset folder to bin when it is built
add_custom_target(copyAssetsfinal ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/assets/
${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/)

install(FILES ${FINAL_INC} DESTINATION include/FINAL)
add_executable(final ${FINAL_SRC} ${FINAL_INC})
target_link_libraries(final PUBLIC core IMGUI assimp)
target_include_directories(final PUBLIC ${CORE_INC_DIR} ${stb_INCLUDE_DIR})

#Trigger asset copy when assignment9 is built
add_dependencies(final copyAssetsfinal)