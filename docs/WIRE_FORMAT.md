# SimpleGo Wire Format Specification

> Complete wire format documentation for SimpleX SMP protocol

---

## Length Encoding Strategies

| Strategy | Usage | Format |
|----------|-------|--------|
| **Standard** | ByteString <= 254 bytes | 1-byte prefix |
| **Large** | ByteString > 254 bytes | 0xFF + Word16 BE |
| **Tail** | Last field in structure | NO prefix |

---

## AgentConfirmation
```
Offset  Size  Field          Encoding
----------------------------------------------
0       2     agentVersion   Word16 BE (= 7)
2       1     type           'C' (0x43)
3       1     maybeE2E       '1' (0x31)
4       2     e2eVersion     Word16 BE (= 2)
6       1     key1Len        1 byte (= 68)
7       68    key1           X448 SPKI
75      1     key2Len        1 byte (= 68)
76      68    key2           X448 SPKI
144     REST  encConnInfo    TAIL - NO PREFIX!
```

---

## EncRatchetMessage
```
Offset  Size  Field          Encoding
----------------------------------------------
0       1     emHeaderLen    1 byte (= 123)
1       123   emHeader       EncMessageHeader
124     16    emAuthTag      RAW (no prefix)
140     REST  emBody         TAIL - NO PREFIX!
```

---

## EncMessageHeader (123 bytes)
```
Offset  Size  Field          Encoding
----------------------------------------------
0       2     ehVersion      Word16 BE (= 2)
2       16    ehIV           RAW (no prefix)
18      16    ehAuthTag      RAW (no prefix)
34      1     ehBodyLen      1 byte (= 88)
35      88    ehBody         Encrypted MsgHeader
----------------------------------------------
Total: 123 bytes
```
