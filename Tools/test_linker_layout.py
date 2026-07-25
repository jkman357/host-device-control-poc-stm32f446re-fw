#!/usr/bin/env python3
"""Exercise the linker script with GNU ld and verify NOLOAD RAM sections."""

from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
LINKER_SCRIPT = ROOT / "STM32F446RETX_FLASH.ld"


def find_tool(name: str) -> str | None:
    """Return an executable path when the host tool is available."""
    return shutil.which(name)


def run(command: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    """Run a command and capture UTF-8 text output."""
    return subprocess.run(
        command,
        cwd=cwd,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def parse_section_layout(objdump_output: str, section_name: str) -> tuple[int, int] | None:
    """Return VMA and LMA from an objdump section table row."""
    pattern = re.compile(
        rf"^\s*\d+\s+{re.escape(section_name)}\s+"
        r"[0-9A-Fa-f]+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+",
        re.MULTILINE,
    )
    match = pattern.search(objdump_output)
    if match is None:
        return None
    return int(match.group(1), 16), int(match.group(2), 16)


def main() -> int:
    """Run the GNU ld linker-layout regression test."""
    cc = find_tool("cc") or find_tool("gcc")
    ld = find_tool("ld")
    objdump = find_tool("objdump")
    readelf = find_tool("readelf")

    missing = [
        name
        for name, value in (("cc/gcc", cc), ("ld", ld), ("objdump", objdump), ("readelf", readelf))
        if value is None
    ]
    if missing:
        print("GNU ld linker-layout test: SKIP (missing " + ", ".join(missing) + ")")
        return 0

    source_text = r'''
void startup_reset_handler(void) {}
__attribute__((section(".isr_vector"))) const unsigned long g_test_vector[2] = {0ul, 0ul};
unsigned long g_test_bss_word;
unsigned char g_test_bss_buffer[128];
'''

    with tempfile.TemporaryDirectory(prefix="linker-layout-") as temp_name:
        temp_dir = Path(temp_name)
        source_path = temp_dir / "layout_test.c"
        object_path = temp_dir / "layout_test.o"
        elf_path = temp_dir / "layout_test.elf"
        source_path.write_text(source_text, encoding="utf-8")

        compile_result = run([cc, "-ffunction-sections", "-fdata-sections", "-c", str(source_path), "-o", str(object_path)])
        if compile_result.returncode != 0:
            print(compile_result.stdout, end="")
            print("GNU ld linker-layout test: FAIL (host compile)")
            return 1

        test_linker_path = temp_dir / "layout_test.ld"
        linker_text = LINKER_SCRIPT.read_text(encoding="utf-8")
        linker_text = re.sub(
            r"\n\s*/DISCARD/\s*:\s*\{.*?\}\s*\n",
            "\n",
            linker_text,
            flags=re.DOTALL,
        )
        test_linker_path.write_text(linker_text, encoding="utf-8")

        link_result = run(
            [ld, "-T", str(test_linker_path), str(object_path), "-o", str(elf_path)]
        )
        linker_output = link_result.stdout
        if link_result.returncode != 0:
            print(linker_output, end="")
            print("GNU ld linker-layout test: FAIL (link)")
            return 1
        if " adjusted " in f" {linker_output.lower()} ":
            print(linker_output, end="")
            print("GNU ld linker-layout test: FAIL (LMA adjustment diagnostic)")
            return 1
        if "rwx" in linker_output.lower():
            print(linker_output, end="")
            print("GNU ld linker-layout test: FAIL (RWX diagnostic)")
            return 1

        section_result = run([objdump, "-h", str(elf_path)])
        if section_result.returncode != 0:
            print(section_result.stdout, end="")
            print("GNU ld linker-layout test: FAIL (objdump)")
            return 1

        for section_name in (".bss", "._user_heap_stack"):
            layout = parse_section_layout(section_result.stdout, section_name)
            if layout is None:
                print(f"GNU ld linker-layout test: FAIL ({section_name} missing)")
                return 1
            vma, lma = layout
            if vma != lma or not (0x20000000 <= vma < 0x20020000):
                print(section_result.stdout, end="")
                print(
                    f"GNU ld linker-layout test: FAIL ({section_name} VMA/LMA "
                    f"0x{vma:08X}/0x{lma:08X})"
                )
                return 1

        program_result = run([readelf, "-l", str(elf_path)])
        if program_result.returncode != 0:
            print(program_result.stdout, end="")
            print("GNU ld linker-layout test: FAIL (readelf)")
            return 1
        if re.search(r"\bRWE\b", program_result.stdout) is not None:
            print(program_result.stdout, end="")
            print("GNU ld linker-layout test: FAIL (RWE segment)")
            return 1

    print("GNU ld linker-layout test: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
