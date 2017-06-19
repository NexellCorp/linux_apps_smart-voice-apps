ifndef	JOBS
JOBJS	:= 8
endif

DIR		:= 
######################################################################
# Build options

DIR += libresample
DIR += src


######################################################################
# Build
all:
	@for dir in $(DIR); do			\
	make -C $$dir || exit $?;		\
	done

clean:
	@for dir in $(DIR); do			\
	make -C $$dir clean || exit $?;	\
	done

