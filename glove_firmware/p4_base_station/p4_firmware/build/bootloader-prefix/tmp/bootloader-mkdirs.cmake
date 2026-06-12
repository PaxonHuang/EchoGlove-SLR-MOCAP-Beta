# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/paxon/esp/esp-idf/components/bootloader/subproject"
  "/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/glove_firmware/p4_base_station/p4_firmware/build/bootloader"
  "/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/glove_firmware/p4_base_station/p4_firmware/build/bootloader-prefix"
  "/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/glove_firmware/p4_base_station/p4_firmware/build/bootloader-prefix/tmp"
  "/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/glove_firmware/p4_base_station/p4_firmware/build/bootloader-prefix/src/bootloader-stamp"
  "/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/glove_firmware/p4_base_station/p4_firmware/build/bootloader-prefix/src"
  "/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/glove_firmware/p4_base_station/p4_firmware/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/glove_firmware/p4_base_station/p4_firmware/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/paxon/CodingProjects/EchoGloveProjects/EchoGlove-SLR-MOCAP/glove_firmware/p4_base_station/p4_firmware/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
