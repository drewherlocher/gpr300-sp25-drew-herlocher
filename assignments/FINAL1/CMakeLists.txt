file(
 GLOB_RECURSE ASSIGNMENT9_INC CONFIGURE_DEPENDS
 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
 *.h *.hpp
)

file(
 GLOB_RECURSE ASSIGNMENT9_SRC CONFIGURE_DEPENDS
 RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
 *.c *.cpp
)
#Copies this assignment9's asset folder to bin when it is built
add_custom_target(copyAssetsF1 ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/assets/
${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/)

install(FILES ${ASSIGNMENT9_INC} DESTINATION include/FINAL1)
add_executable(FINAL1 ${ASSIGNMENT9_SRC} ${ASSIGNMENT9_INC})
target_link_libraries(FINAL1 PUBLIC core IMGUI assimp)
target_include_directories(FINAL1 PUBLIC ${CORE_INC_DIR} ${stb_INCLUDE_DIR})

#Trigger asset copy when assignment9 is built
add_dependencies(FINAL1 copyAssetsF1)