ALL=v4lfs

all: $(ALL)

v4lfs: fs.o NinePea.o v4l.o jpeg.o
	$(CC) -o v4lfs fs.o NinePea.o v4l.o jpeg.o -ljpeg

clean:
	rm -f v4lfs *.o *~
