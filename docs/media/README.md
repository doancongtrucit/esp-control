# Media

`screens/` holds screenshots taken from a real deck with `tools/deck_screenshot.py` and the made-up sample data in that tool (no personal data). Regenerate them after a UI change:

```bash
python tools/deck_screenshot.py --port COM4 --out docs/media/screens home monitor controls:0 info:0 info:5 claude:0 claude:3 notify settings:0 break:0
```

Stop the bridge first; it keeps the serial port open.
