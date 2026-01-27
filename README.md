# Dryer

```
pio run

# run and upload to pico
pio run -t uploadfs -e pico_w
pio run -t upload -t monitor -e pico_w
pio run -t upload -t monitor -e test_hydraulic
pio run -t upload -t monitor -e pico_w

# test on local machine
pio test -vvv -e native
```

## generate fonts

https://rop.nl/truetype2gfx/