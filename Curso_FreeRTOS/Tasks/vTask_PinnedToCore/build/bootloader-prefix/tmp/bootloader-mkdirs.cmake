# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/Dell/esp/v4.4.7/esp-idf/components/bootloader/subproject"
  "C:/Users/Dell/Codigos/GITHUB/ESP32/Curso_FreeRTOS/Tasks/vTask_PinnedToCore/build/bootloader"
  "C:/Users/Dell/Codigos/GITHUB/ESP32/Curso_FreeRTOS/Tasks/vTask_PinnedToCore/build/bootloader-prefix"
  "C:/Users/Dell/Codigos/GITHUB/ESP32/Curso_FreeRTOS/Tasks/vTask_PinnedToCore/build/bootloader-prefix/tmp"
  "C:/Users/Dell/Codigos/GITHUB/ESP32/Curso_FreeRTOS/Tasks/vTask_PinnedToCore/build/bootloader-prefix/src/bootloader-stamp"
  "C:/Users/Dell/Codigos/GITHUB/ESP32/Curso_FreeRTOS/Tasks/vTask_PinnedToCore/build/bootloader-prefix/src"
  "C:/Users/Dell/Codigos/GITHUB/ESP32/Curso_FreeRTOS/Tasks/vTask_PinnedToCore/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "C:/Users/Dell/Codigos/GITHUB/ESP32/Curso_FreeRTOS/Tasks/vTask_PinnedToCore/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
