#!/usr/bin/env python3
from pathlib import Path
text=Path(__file__).resolve().parents[1].joinpath('STM32F446RETX_FLASH.ld').read_text()
assert '0x08000000' in text and '512K' in text and '0x20000000' in text and '128K' in text
print('linker layout test: PASS')
