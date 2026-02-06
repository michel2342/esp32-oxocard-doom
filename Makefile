#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp32-doom

EXCLUDE_COMPONENTS := asio coap esp_gdbstub

include $(IDF_PATH)/make/project.mk
