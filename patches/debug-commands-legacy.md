# Legacy debug command support

The `LW232_ENABLE_DEBUG_CMDS` flag and the accompanying `SoftwareSerial` debug
handlers were removed from the active firmware to simplify the build. The
previous implementation is preserved here for reference.

```cpp
// Compile-time flag and defaults
#ifndef LW232_ENABLE_DEBUG_CMDS
#define LW232_ENABLE_DEBUG_CMDS 0
#endif

#if LW232_ENABLE_DEBUG_CMDS
#ifndef LW232_DEBUG_RX_PIN
#define LW232_DEBUG_RX_PIN 9
#endif

#ifndef LW232_DEBUG_TX_PIN
#define LW232_DEBUG_TX_PIN 8
#endif

#ifndef LW232_DEBUG_BAUD
#define LW232_DEBUG_BAUD 115200
#endif
#endif
```

```cpp
#if LW232_ENABLE_DEBUG_CMDS
static SoftwareSerial debug(LW232_DEBUG_RX_PIN, LW232_DEBUG_TX_PIN);
#define dbg_begin(x) debug.begin(x)
#define dbg_print(x) do { if (instance()->lw232DebugEnabled) { debug.print(x); } } while (0)
#define dbg_println(x) do { if (instance()->lw232DebugEnabled) { debug.println(x); } } while (0)
#else
#define dbg_begin(x) do {} while (0)
#define dbg_print(x) do {} while (0)
#define dbg_println(x) do {} while (0)
#endif
```

```cpp
#if LW232_ENABLE_DEBUG_CMDS
    case LW232_CMD_DEBUG: {
        const size_t cmdLen = strlen((char*)lw232Message);
        const bool hasPrefix = strncmp((char*)lw232Message, "@DBG", 4) == 0;
        if (!hasPrefix || cmdLen != 6 || lw232Message[5] != LW232_CR) {
            ret = LW232_ERR;
            break;
        }
        if (lw232Message[4] == '1') {
            lw232DebugEnabled = true;
            dbg_begin(LW232_DEBUG_BAUD);
            dbg_println(F("DEBUG MODE ENABLED"));
            Serial.print(F("DEBUG ON"));
        } else if (lw232Message[4] == '0') {
            lw232DebugEnabled = false;
            Serial.print(F("DEBUG OFF"));
        } else {
            ret = LW232_ERR;
        }
        break;
    }
    case LW232_CMD_DEBUG_EXT: {
        const size_t cmdLen = strlen((char*)lw232Message);
        const bool hasExtPrefix = strncmp((char*)lw232Message, "#EXT", 4) == 0;
        if (!hasExtPrefix || cmdLen != 5 || lw232Message[4] != LW232_CR) {
            ret = LW232_ERR;
            break;
        }
        dbg_println(F("NEXT EXT FRAME DEBUG"));
        break;
    }
#else
    case LW232_CMD_DEBUG:
    case LW232_CMD_DEBUG_EXT:
        ret = LW232_ERR_UNKNOWN_CMD;
        break;
#endif
```

```cpp
#if LW232_ENABLE_DEBUG_CMDS
        Serial.print(lw232DebugEnabled ? 'D' : 'd');
#endif
```

`lw232DebugEnabled` was a `bool` member defaulting to `false` alongside other
runtime flags in the `Can232` class.
