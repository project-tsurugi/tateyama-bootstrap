add_executable(server_test server_test.cpp) 

# add_test(
#         NAME restore_backup
#         COMMAND $<TARGET_FILE:server_test> "restore_backup: name = /a/b/c, keep = true" src/tateyama/server/tateyama-server --restore_backup=/a/b/c -keep_backup
# 	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
# )

# add_test(
#         NAME restore_tag
#         COMMAND $<TARGET_FILE:server_test> "restore_tag: tag = 12345678" src/tateyama/server/tateyama-server --restore_tag=12345678
# 	WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
# )
