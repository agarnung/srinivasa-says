Este es el comando ejecutado desde AVRDudees para subir el binario compilado en Microchip studio:

```
-c arduino_as_isp -p m328p -P COM6 -b 19200 -U flash:w:"C:\Users\Alejandro\Documents\Atmel Studio\7.0\srinivasasays\srinivasasays\Release\srinivasasays.hex":a -U lfuse:w:0x7F:m -U hfuse:w:0xD9:m -U efuse:w:0xFE:m -U lock:w:0xFF:m 
```
