lib_LTLIBRARIES = libdatacenter.la

lib_path = /usr/include/oxool
AM_CPPFLAGS = -I$(lib_path) \
	-I$(lib_path)/src/include \
	-I${lib_path}/common \
	-I${lib_path}/net \
	-I${lib_path}/wsd \
	-I${lib_path}/kit \
	-std=c++11


libdatacenter_la_SOURCES = datacenter.cpp \
						  datacenter_file_db.cpp
