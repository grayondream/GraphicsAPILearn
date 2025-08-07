include_directories(${RENDER_3PARTY_PATH}/glad/include)
include_directories(${RENDER_3PARTY_PATH}/stbimage)
include_directories(${RENDER_3PARTY_PATH}/imgui/backends)

aux_source_directory(${RENDER_3PARTY_PATH}/glad/src GLAD_SRC_FILES)
aux_source_directory(${RENDER_3PARTY_PATH}/imgui/backends IMGUI_SRC_FILES)