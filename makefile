CC = gcc
CFLAGS = -Wall -g
INCLUDES = -I/usr/local/Cellar/espeak-ng/1.51/include
LIBS = -L/usr/local/Cellar/espeak-ng/1.51/lib -lespeak-ng

TARGET = skullTalk

$(TARGET): skullSpeech.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) skullSpeech.c $(LIBS)

clean:
	rm -f $(TARGET)
