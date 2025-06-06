# Copyright (c) 2024 by Mathias Kraus <elboberido@m-hias.de>. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

# NOTE: Contrary to the 'IceoryxPlatformSettings.cmake' this file will not be installed and
# is only used to create a header with compile time constants.

message(STATUS "[i] <<<<<<<<<<<<< Start iceoryx_platform configuration: >>>>>>>>>>>>>")

# use "/" as separator to circumvent the cruelty "C:\\\\\\\\Windows"
configure_option(
    NAME IOX_PLATFORM_TEMP_DIR
    DEFAULT_VALUE "C:/Windows/Temp/"
)

configure_option(
    NAME IOX_PLATFORM_LOCK_FILE_PATH_PREFIX
    DEFAULT_VALUE "C:/Windows/Temp/"
)

configure_option(
    NAME IOX_PLATFORM_UDS_SOCKET_PATH_PREFIX
    DEFAULT_VALUE ""
)

configure_option(
    NAME IOX_PLATFORM_DEFAULT_CONFIG_LOCATION
    DEFAULT_VALUE "C:/ProgramData/"
)

option(IOX_PLATFORM_FEATURE_ACL "Use ACLs for access control" OFF)
message(STATUS "[i] IOX_PLATFORM_FEATURE_ACL: ${IOX_PLATFORM_FEATURE_ACL}")

if(IOX_PLATFORM_FEATURE_ACL)
    message(FATAL_ERROR "ACLs are not supported on this platform! Don't use 'IOX_PLATFORM_FEATURE_ACL=ON'")
endif()

message(STATUS "[i] <<<<<<<<<<<<<< End iceoryx_platform configuration: >>>>>>>>>>>>>>")
