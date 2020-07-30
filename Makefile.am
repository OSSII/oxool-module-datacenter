ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src
EXTRA_DIST= datacenter.spec \
			autogen.sh \
			autoclean.sh \
			src \
			runTimeData \
			datacenter.xml.in \
			html \
			oxoffice_oxt
distdir = $(PACKAGE)-$(VERSION)-$(BRANCH)

dev:
	make -j4
	cp src/.libs/libdatacenter.so.0.0.0 ~/work/oxool3.0/develop/libdatacenter.so
ossii:
	make -j4
	cp src/.libs/libdatacenter.so.0.0.0 ~/work/oxool3.0/ossii/libdatacenter.so
oxool:
	make -j4
	cp src/.libs/libdatacenter.so.0.0.0 ~/work/oxool-community-root/master/libdatacenter.so
