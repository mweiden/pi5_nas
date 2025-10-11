# Raspberry Pi 5 NAS

A homemade NAS inspired by this [blog post](https://www.jeffgeerling.com/blog/2024/radxas-sata-hat-makes-compact-pi-5-nas) (with some modifications).

## Specs

Hardware:
* [Raspberry Pi 5](https://www.raspberrypi.com/products/raspberry-pi-5/)
* [2x 4TB Seagate Iron Wolf HDDs](https://www.amazon.com/dp/B09NHV3CK9)
* [Waveshare SATA HAT+](https://www.amazon.com/dp/B0FH1NTMW5)
* [Freenove 2x16 LCD Panel](https://www.amazon.com/dp/B0B76YGDV4) ([docs](https://docs.freenove.com/projects/fnk0079/en/latest/fnk0079/codes/Raspberry_Pi/1_LCD1602.html))
* [D-Type RJ45 Through Socket](https://www.amazon.com/gp/product/B0CFVYJMT2)
* [Power button](https://www.amazon.com/gp/product/B079HR5Q4R) ([docs](https://github.com/raspberrypi/documentation/blob/develop/documentation/asciidoc/computers/raspberry-pi/power-button.adoc))
* [2.5G Ethernet USB Adapter](https://www.amazon.com/gp/product/B093FB9QWB)
* [19" Rack Mount Project Box](https://www.amazon.com/gp/product/B0FJ2RFLB8)
* [12v 6A Power Supply](https://www.amazon.com/dp/B082PCR5YS)
* [5.5mm x 2.1mm Female to 3.5mm x 1.35mm Male Plug Socket DC Power Adapter](https://www.amazon.com/dp/B07FJLZGPF)

Software:
* [OpenMediaVault](https://www.openmediavault.org/)
* [WiringPi](https://github.com/WiringPi/WiringPi)
* [log_driver.c](/lcd_driver.c) - from this repo, adapted from Freenove example

## LCD

I have my LCD displaying the storage usage, cpu temperature, and temperature of each of the HDDs.

Sending this information to the LCD requires a small C program, `lcd_driver.c`, included here in the repo.

### Compiling

```
make build
```

### Running

Note lcd_driver needs sudo to use `smartctl`.

```
sudo chmod +x lcd_driver
sudo ./lcd_driver
```

### Installing it as a `systemd` service

Ensure the binary is executable and accessible
```
sudo mv lcd_driver /usr/local/bin/
sudo chmod +x /usr/local/bin/lcd_driver
```

Enable and start the service
```
sudo systemctl daemon-reload
sudo systemctl enable lcd_driver.service
sudo systemctl start lcd_driver.service
```
