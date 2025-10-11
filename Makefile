.DEFAULT_GOAL := build

build:
	gcc -o lcd_driver lcd_driver.c -lwiringPi -lwiringPiDev

clean:
	rm -f lcd_driver
