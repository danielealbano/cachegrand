# Copyright (C) 2018-2022 Daniele Salvatore Albano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license. See the LICENSE file for details.

add_test(
        NAME
        cachegrand-integration-tests-redis-server
        COMMAND
        bash ${CMAKE_CURRENT_SOURCE_DIR}/bootstrap.sh
            --tarball ${CMAKE_CURRENT_BINARY_DIR}/redis-latest.tar.gz
            --server $<TARGET_FILE:cachegrand-server>
            --config ${PROJECT_SOURCE_DIR}/etc/cachegrand.yaml.skel
            --junit
            --output ${CMAKE_CURRENT_BINARY_DIR}/junit-report.xml
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
