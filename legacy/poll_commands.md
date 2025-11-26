# Legacy polling commands (`P`, `A`)

Manual polling is no longer supported in the active firmware. The former handlers are preserved here for reference:

```cpp
// P[CR] single-frame poll (when autopoll was off)
serviceCanRx();
BufferedFrame frame;
if (popRxFrame(frame)) {
    emitFrameToSerial(frame);
    ret = LW232_OK;
} else {
    ret = LW232_ERR;
}

// A[CR] bulk poll (when autopoll was off)
serviceCanRx();
BufferedFrame frame;
while (popRxFrame(frame)) {
    emitFrameToSerial(frame);
    Serial.write(LW232_CR);
}
Serial.print('A');
Serial.write(LW232_CR);
```

If you need manual draining behavior for debugging, refer to this snippet and port it into a throwaway branch; the mainline firmware relies on autopolling only.
