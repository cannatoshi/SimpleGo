# SimpleGo Wire Format Specification

## Length Encoding

| Strategy | Usage | Format |
|----------|-------|--------|
| Standard | <=254 bytes | 1-byte prefix |
| Large | >254 bytes | 0xFF + Word16 BE |
| Tail | Last field | NO prefix |

## AgentConfirmation

[2B version][1B 'C'][1B '1'][2B e2eVer][1B+68B key1][1B+68B key2][TAIL encConnInfo]

## EncRatchetMessage

[1B+123B emHeader][16B emAuthTag][TAIL emBody]

## EncMessageHeader (123 bytes)

[2B version][16B IV][16B authTag][1B+88B ehBody]

## MsgHeader (88 bytes)

[2B version][1B+68B dhKey][4B msgPN][4B msgNs][9B padding]

## SPKI Format

X448: 12-byte header + 56-byte key = 68 bytes
X25519: 12-byte header + 32-byte key = 44 bytes

*Last updated: January 24, 2026*
