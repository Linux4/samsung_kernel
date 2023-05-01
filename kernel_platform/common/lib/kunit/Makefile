obj-$(CONFIG_KUNIT) +=			kunit.o

kunit-objs +=				test.o \
					mock.o \
					common-mocks.o \
					string-stream.o \
					assert.o \
					kunit-stream.o \
					try-catch.o \
					executor.o \
					strerror.o

ifeq ($(CONFIG_KUNIT_DEBUGFS),y)
kunit-objs +=				debugfs.o
endif

obj-$(CONFIG_KUNIT_TEST) +=		kunit-test.o \
					mock-macro-test.o \
					mock-test.o \
					strerror-test.o \
					string-stream-test.o

obj-$(CONFIG_KUNIT_EXAMPLE_TEST)	+= kunit-example-test.o
