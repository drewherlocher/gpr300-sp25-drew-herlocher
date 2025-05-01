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
add_custom_target(copyAssetsC1 ALL COMMAND ${CMAKE_COMMAND} -E copy_directory
${CMAKE_CURRENT_SOURCE_DIR}/assets/
${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/)

install(FILES ${ASSIGNMENT9_INC} DESTINATION include/CSM1)
add_executable(CSM1 ${ASSIGNMENT9_SRC} ${ASSIGNMENT9_INC})
target_link_libraries(CSM1 PUBLIC core IMGUI assimp)
target_include_directories(CSM1 PUBLIC ${CORE_INC_DIR} ${stb_INCLUDE_DIR})

#Trigger asset copy when assignment9 is built
add_dependencies(CSM1 copyAssetsC1)