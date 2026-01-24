# SimpleGo Bug Tracker

## Summary: 12 Bugs Fixed

## Bug #1-5: Length Prefix Errors
- E2E key: Word16 -> 1-byte
- prevMsgHash: 1-byte -> Word16
- MsgHeader DH: Word16 -> 1-byte
- ehBody: Word16 -> 1-byte
- emHeader: Word16 -> 1-byte (124->123)

## Bug #6: Payload AAD Size
236 -> 235 bytes

## Bug #7-8: KDF Order
- Root: [new_root][chain][next_header]
- Chain IV: header_iv[64:80] BEFORE msg_iv[80:96]

## Bug #9: wolfSSL X448 Byte Order
Reverse all bytes for compatibility

## Bug #10-12: Format Bugs
- Port: space -> length prefix
- smpQueues: 1-byte -> Word16 BE
- queueMode Nothing: '0' -> empty

## Current: Investigating Tail encoding

*Last updated: January 24, 2026*
