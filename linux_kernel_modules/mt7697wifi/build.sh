ln -sf master.mk Makefile
make clean ; make
if [ $? -ne 0 ] ; then exit 1 ; fi

#ln -sf spi.mk Makefile
#make
#Sif [ $? -ne 0 ] ; then exit 1 ; fi
