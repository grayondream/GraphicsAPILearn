
file(GLOB GLAD_SRC_FILE "${RENDER_3PARTY_PATH}/glad/src/*.c")

message("GLAD_SRC_FILE: ${GLAD_SRC_FILE}")

set(GLAD_SRC_FILES ${GLAD_SRC_FILE} CACHE STRING "glad source files" FORCE)

