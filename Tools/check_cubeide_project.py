#!/usr/bin/env python3
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
errors = []

for name in ('.project', '.cproject'):
    try:
        ET.parse(ROOT / name)
    except Exception as exc:
        errors.append(f'{name}: invalid XML: {exc}')

cproject = (ROOT / '.cproject').read_text(encoding='utf-8')
required = [
    'STM32F446RETx',
    'NUCLEO-F446RE',
    'managedbuild.option.target_cpuid',
    'managedbuild.option.target_coreid',
    'managedbuild.option.defaults',
    'fpv4-sp-d16',
    'floatabi.value.hard',
    'STM32F446RETX_FLASH.ld',
]
for token in required:
    if token not in cproject:
        errors.append(f'.cproject: missing required target metadata: {token}')

for generated in ('Debug', 'Release'):
    if (ROOT / generated).exists():
        errors.append(f'{generated}/ must not be packaged; CubeIDE shall generate it locally')

for path in ('App', 'Core', 'Platform', 'Protocol', 'Transport'):
    if f'name="{path}"' not in cproject:
        errors.append(f'.cproject: missing source entry {path}')

if errors:
    print('CubeIDE project metadata check FAILED')
    for error in errors:
        print(f'- {error}')
    sys.exit(1)

print('CubeIDE project metadata check PASS')
