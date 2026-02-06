# CMake generated Testfile for 
# Source directory: /home/yoav/Projects/opencode-sandbox/levin/liblevin
# Build directory: /home/yoav/Projects/opencode-sandbox/levin/build-real/liblevin
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[StateMachine]=] "/home/yoav/Projects/opencode-sandbox/levin/build-real/liblevin/test_state_machine")
set_tests_properties([=[StateMachine]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;69;add_test;/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;0;")
add_test([=[DiskManager]=] "/home/yoav/Projects/opencode-sandbox/levin/build-real/liblevin/test_disk_manager")
set_tests_properties([=[DiskManager]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;74;add_test;/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;0;")
add_test([=[DiskDeletion]=] "/home/yoav/Projects/opencode-sandbox/levin/build-real/liblevin/test_disk_deletion")
set_tests_properties([=[DiskDeletion]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;79;add_test;/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;0;")
add_test([=[CAPI]=] "/home/yoav/Projects/opencode-sandbox/levin/build-real/liblevin/test_c_api")
set_tests_properties([=[CAPI]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;84;add_test;/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;0;")
add_test([=[TorrentSession]=] "/home/yoav/Projects/opencode-sandbox/levin/build-real/liblevin/test_torrent_session")
set_tests_properties([=[TorrentSession]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;89;add_test;/home/yoav/Projects/opencode-sandbox/levin/liblevin/CMakeLists.txt;0;")
subdirs("../_deps/catch2-build")
subdirs("../_deps/libtorrent-build")
