"""Reject missing or leaking handheld capability definitions in actual build commands.

Three configurations (docs/TDECK_NATIVE_VIEW_PHASE2.md):
  production firmware (--handheld)      OPENU5_FIXED_HANDHELD_PROFILE on every shared TU,
                                        no DOS screen (no OPENU5_DOS_SCREEN_LAYOUT, no
                                        board_dos_layout.cpp), -Werror and -fstack-usage
  Native View prototype (--native-prototype)
                                        OPENU5_FIXED_HANDHELD_INTERACTION and
                                        OPENU5_DOS_SCREEN_LAYOUT on every shared TU, never
                                        the whole-profile fix, board_dos_layout.cpp built,
                                        -Werror and -fstack-usage; Phase 3
                                        (docs/TDECK_NATIVE_VIEW_PHASE3.md): the boot
                                        selector and the DOS-name extractor
                                        (native_view_prototype.cpp, dos_text_tables.cpp)
                                        built too, and neither in the production image
  desktop (neither flag)                no handheld fixing of any group
"""
import argparse
import json
from pathlib import Path


def defined(command, name):
    return f'-D{name}=1' in command or f'/D{name}=1' in command


def verify(commands, handheld, prototype=False):
    if handheld and prototype:
        raise ValueError('Choose one firmware configuration')
    selected = []
    for entry in commands:
        source = entry['file'].replace('\\', '/').lower()
        if '/native/core/src/' in source or '/native/targets/tdeck/main/' in source:
            selected.append(entry)
    if len(selected) < 70:
        raise ValueError(f'Incomplete shared source command list: {len(selected)}')
    firmware = handheld or prototype
    def builds(name):
        return any(entry['file'].replace('\\', '/').lower().endswith('/' + name) for entry in selected)
    dos_painter = builds('board_dos_layout.cpp')
    native_names = builds('native_view_prototype.cpp') and builds('dos_text_tables.cpp')
    native_any = builds('native_view_prototype.cpp') or builds('dos_text_tables.cpp')
    for entry in selected:
        command = entry.get('command', ' '.join(entry.get('arguments', [])))
        fixed = defined(command, 'OPENU5_FIXED_HANDHELD_PROFILE')
        interaction = defined(command, 'OPENU5_FIXED_HANDHELD_INTERACTION')
        presentation = defined(command, 'OPENU5_FIXED_HANDHELD_PRESENTATION')
        dos_screen = defined(command, 'OPENU5_DOS_SCREEN_LAYOUT')
        if prototype:
            if fixed or presentation or not interaction:
                raise ValueError(f'Prototype must fix the interaction only: {entry["file"]}')
            if not dos_screen:
                raise ValueError(f'Prototype TU without the DOS screen layout: {entry["file"]}')
        else:
            if fixed != handheld:
                raise ValueError(f'Wrong handheld capability for {entry["file"]}')
            # The per-group switches (platform_profile.h) must not leak into a desktop build either.
            if (interaction or presentation) and not handheld:
                raise ValueError(f'Handheld policy group leaked into {entry["file"]}')
            if handheld and dos_screen:
                raise ValueError(f'DOS screen leaked into the production firmware: {entry["file"]}')
        if firmware and ('-Werror' not in command or '-fstack-usage' not in command):
            raise ValueError(f'Lost firmware warning/stack checks for {entry["file"]}')
    if prototype and not dos_painter:
        raise ValueError('Prototype does not build board_dos_layout.cpp')
    if handheld and dos_painter:
        raise ValueError('Production firmware builds board_dos_layout.cpp')
    if prototype and not native_names:
        raise ValueError('Prototype does not build the selector and the DOS-name extractor')
    if handheld and native_any:
        raise ValueError('Production firmware builds Native View prototype sources')
    return len(selected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('commands', type=Path)
    parser.add_argument('--handheld', action='store_true', help='the production firmware')
    parser.add_argument('--native-prototype', action='store_true', help='the Native View prototype firmware')
    args = parser.parse_args()
    count = verify(json.loads(args.commands.read_text(encoding='utf-8')), args.handheld, args.native_prototype)
    kind = 'native-prototype' if args.native_prototype else f'handheld={args.handheld}'
    print(f'PASS: {count} shared translation units, {kind}')
