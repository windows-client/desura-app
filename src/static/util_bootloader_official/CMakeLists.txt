include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/RES
  ${DESURA_EXE_INCLUDE_DIR}
  ${UTIL_BOOTLOADER_INCLUDE_DIRS}
)

file(GLOB Sources code/*.cpp)
add_library(util_bootloader_official_s STATIC ${Sources})
SET_PROPERTY(TARGET util_bootloader_official_s                PROPERTY FOLDER "Static")

if (WIN32)
	target_link_libraries(util_bootloader_official_s
		umcf_s
		util_web_s
		${TINYXML_LIBRARIES_S}
	)
	
	add_dependencies(util_bootloader_official_s tinyxml2_s)
	SetStaticRuntime(util_bootloader_official_s)
else()
	target_link_libraries(util_bootloader_official_s
		umcf
		util_web
		${TINYXML_LIBRARIES}
	)
	
	add_dependencies(util_bootloader_official_s tinyxml2)
endif()

SET(UTIL_BOOTLOADER_OFFICIAL_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/code ${CMAKE_CURRENT_SOURCE_DIR}/RES PARENT_SCOPE)

if(NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wall -Weffc++")
endif()
