openocd -f interface/stlink.cfg -f target/nrf52.cfg -c "init; halt; program Seeed_XIAO_nRF52840_Sense_bootloader-0.6.1_s140_7.3.0.hex verify; reset; exit"
