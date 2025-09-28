
cmake_minimum_required(VERSION 3.20)
project(EuroScopeStripPrinterPlugin LANGUAGES CXX)

if(NOT DEFINED EUROSCOPE_SDK_DIR)
  if(DEFINED ENV{EUROSCOPE_SDK_DIR})
    set(EUROSCOPE_SDK_DIR $ENV{EUROSCOPE_SDK_DIR})
  else()
    message(FATAL_ERROR "EUROSCOPE_SDK_DIR not set. Set -DEUROSCOPE_SDK_DIR= or env var EUROSCOPE_SDK_DIR to the folder containing EuroScopePlugIn.h")
  endif()
endif()

add_library(EuroScopeStripPrinterPlugin SHARED EuroScopeStripPrinterPlugin.cpp EuroScopeStripPrinterPlugin.def)

target_link_libraries(EuroScopeStripPrinterPlugin
  PRIVATE
    "C:/Program Files/EuroscopeSDK/EuroScopePlugInDll.lib"
)

target_include_directories(EuroScopeStripPrinterPlugin PRIVATE "${EUROSCOPE_SDK_DIR}")
target_compile_features(EuroScopeStripPrinterPlugin PRIVATE cxx_std_17)

# Windows only
if (MSVC)
  target_compile_definitions(EuroScopeStripPrinterPlugin PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
  set_target_properties(EuroScopeStripPrinterPlugin PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    OUTPUT_NAME "EuroScopeStripPrinterPlugin"
  )
endif()
