# PlatformIO pre-script. pm_send_nullfunc prints an assert and jumps to
# itself when ieee80211_getmgtframe cannot allocate a management buffer.
# The software watchdog resets from that spin. Returning straight to the
# caller is worse: two sites in pm_suspend then mark the null frame as sent,
# the radio waits for a TX completion that never happens, and the station
# stays off until a power cycle while the LED loop keeps running.
#
# Replace that jump with a call0 to griswold_nullfunc_fail. A send that got a
# buffer does not reach it. The patched object is linked ahead of -lpp.

import os
import struct
import subprocess
import tempfile

# movi a4, 0x7c6  — the assert line number inside pm_send_nullfunc.
MOVI_A4_LINE = bytes.fromhex("42a7c6")
# l32i.n a12, a1, 36 / l32i.n a13, a1, 40 / l32i.n a0, a1, 32 /
# addi a1, a1, 48 / ret.n
EPILOGUE = bytes.fromhex("c891d8a1088112c1300df0")
# s32i.n a0, a2, 48 is the sleep-state store. File bytes are swapped.
SLEEP_STORE = bytes.fromhex("09c2")
# l32i.n a0, a1, 0 / addi a1, a1, 16. Skipping 0x1f lands here.
CALLER_EPILOGUE = bytes.fromhex("080112c1")
SLOT0_OP = 20
STB_GLOBAL = 1
STT_FUNC = 2
FAIL_SYMBOL = "griswold_nullfunc_fail"

ELF32_SHDR = struct.Struct("<IIIIIIIIII")
ELF32_RELA = struct.Struct("<IIi")
ELF32_SYM = struct.Struct("<IIIBBH")


def fail(msg):
    raise SystemExit("patch_pm_nullfunc: " + msg)


def elf_header(data):
    if data[:4] != b"\x7fELF":
        fail("pm.o is not an ELF file")
    e_shoff = struct.unpack_from("<I", data, 32)[0]
    e_shentsize = struct.unpack_from("<H", data, 46)[0]
    e_shnum = struct.unpack_from("<H", data, 48)[0]
    e_shstrndx = struct.unpack_from("<H", data, 50)[0]
    return e_shoff, e_shentsize, e_shnum, e_shstrndx


def parse_sections(data):
    e_shoff, e_shentsize, e_shnum, e_shstrndx = elf_header(data)
    sections = []
    for index in range(e_shnum):
        sh = ELF32_SHDR.unpack_from(data, e_shoff + index * e_shentsize)
        sections.append({
            "index": index,
            "name_off": sh[0],
            "type": sh[1],
            "flags": sh[2],
            "addr": sh[3],
            "offset": sh[4],
            "size": sh[5],
            "link": sh[6],
            "info": sh[7],
            "addralign": sh[8],
            "entsize": sh[9],
            "raw": sh,
        })
    shstr = sections[e_shstrndx]
    for sh in sections:
        start = shstr["offset"] + sh["name_off"]
        sh["name"] = data[start:data.index(b"\x00", start)].decode()
    return sections


def section(sections, name):
    for sh in sections:
        if sh["name"] == name:
            return sh
    fail("pm.o has no %s section" % name)


def symbols(data, sections):
    symtab = section(sections, ".symtab")
    strtab = section(sections, ".strtab")
    strings = data[strtab["offset"]:strtab["offset"] + strtab["size"]]
    blob = data[symtab["offset"]:symtab["offset"] + symtab["size"]]
    entsize = symtab["entsize"] or ELF32_SYM.size
    found = []
    for off in range(0, len(blob), entsize):
        st_name, st_value, st_size, info, other, shndx = ELF32_SYM.unpack_from(blob, off)
        name = strings[st_name:strings.index(b"\x00", st_name)].decode()
        found.append({
            "index": off // entsize,
            "name": name,
            "value": st_value,
            "size": st_size,
            "info": info,
            "shndx": shndx,
        })
    return found


def function_bounds(syms):
    for sym in syms:
        if sym["name"] == "pm_send_nullfunc":
            if sym["size"] <= 0:
                fail("pm_send_nullfunc has no size")
            return sym["value"], sym["value"] + sym["size"], sym["index"]
    fail("pm.o has no pm_send_nullfunc symbol")


def find_only(blob, needle, base, label):
    found = []
    pos = 0
    while True:
        at = blob.find(needle, pos)
        if at < 0:
            break
        found.append(base + at)
        pos = at + 1
    if len(found) != 1:
        fail("expected one %s in pm_send_nullfunc, found %d" % (label, len(found)))
    return found[0]


def text_bytes(data, sections, addr, size):
    text = section(sections, ".irom0.text")
    if text["addr"] != 0:
        fail(".irom0.text address is %s, expected 0" % hex(text["addr"]))
    return data[text["offset"] + addr:text["offset"] + addr + size]


def check_callers(data, sections, syms):
    # Only the two pm_suspend helpers may see the return-address skip.
    # Both store the sleep state at return+0x11 and epilogue at return+0x1f.
    target = None
    for sym in syms:
        if sym["name"] == "pm_send_nullfunc":
            target = sym["index"]
    if target is None:
        fail("pm_send_nullfunc disappeared")
    rela = section(sections, ".rela.irom0.text")
    skips = 0
    calls = 0
    count = rela["size"] // ELF32_RELA.size
    for index in range(count):
        at = rela["offset"] + index * ELF32_RELA.size
        r_offset, r_info, _r_addend = ELF32_RELA.unpack_from(data, at)
        if (r_info & 0xFF) != SLOT0_OP or (r_info >> 8) != target:
            continue
        if (text_bytes(data, sections, r_offset, 1)[0] & 0xF) != 5:
            continue
        calls += 1
        ret = r_offset + 3
        window = text_bytes(data, sections, ret, 0x24)
        store = window[0x11:0x13] == SLEEP_STORE
        epi = window[0x1f:0x23] == CALLER_EPILOGUE
        if store or window[0x11] == SLEEP_STORE[0]:
            if not (store and epi):
                fail("call at 0x%x has a partial sleep-store signature" % r_offset)
            skips += 1
    if calls != 8 or skips != 2:
        fail("expected 8 null-frame calls and 2 sleep stores, found %d and %d" % (calls, skips))


def shifted(offset, sym_end, str_end, name_len):
    moved = offset
    if offset >= sym_end:
        moved += ELF32_SYM.size
    if offset >= str_end:
        moved += name_len
    return moved


def insert_fail_symbol(data):
    e_shoff, e_shentsize, e_shnum, _e_shstrndx = elf_header(data)
    sections = parse_sections(data)
    symtab = section(sections, ".symtab")
    strtab = section(sections, ".strtab")
    if symtab["entsize"] not in (0, ELF32_SYM.size):
        fail("unexpected symbol size")
    name = FAIL_SYMBOL.encode() + b"\x00"
    if len(name) % 4:
        name += b"\x00" * (4 - (len(name) % 4))
    sym_end = symtab["offset"] + symtab["size"]
    str_end = strtab["offset"] + strtab["size"]
    if not (sym_end <= strtab["offset"] < str_end <= e_shoff):
        fail("pm.o section layout is not the one this patch knows")
    symbol = ELF32_SYM.pack(strtab["size"], 0, 0, (STB_GLOBAL << 4) | STT_FUNC, 0, 0)
    out = bytearray()
    out += data[:sym_end]
    out += symbol
    out += data[sym_end:str_end]
    out += name
    out += data[str_end:]
    new_shoff = shifted(e_shoff, sym_end, str_end, len(name))
    struct.pack_into("<I", out, 32, new_shoff)
    for sh in sections:
        new_offset = shifted(sh["offset"], sym_end, str_end, len(name))
        new_size = sh["size"]
        if sh["name"] == ".symtab":
            new_size += ELF32_SYM.size
        elif sh["name"] == ".strtab":
            new_size += len(name)
        packed = ELF32_SHDR.pack(
            sh["raw"][0], sh["raw"][1], sh["raw"][2], sh["raw"][3],
            new_offset, new_size, sh["raw"][6], sh["raw"][7], sh["raw"][8], sh["raw"][9])
        out[new_shoff + sh["index"] * e_shentsize:new_shoff + (sh["index"] + 1) * e_shentsize] = packed
    if e_shnum != struct.unpack_from("<H", out, 48)[0]:
        fail("section count changed while adding %s" % FAIL_SYMBOL)
    return bytes(out), symtab["size"] // ELF32_SYM.size


def patch_pm_object(data):
    sections = parse_sections(data)
    check_callers(data, sections, symbols(data, sections))
    start, end, _sym_index = function_bounds(symbols(data, sections))
    body = text_bytes(data, sections, start, end - start)
    movi_at = find_only(body, MOVI_A4_LINE, start, "assert movi")
    epi_at = find_only(body, EPILOGUE, start, "epilogue")
    if not (start <= epi_at < movi_at < end):
        fail("epilogue is not before the assert")

    rela = section(sections, ".rela.irom0.text")
    if rela["entsize"] not in (0, ELF32_RELA.size):
        fail("unexpected relocation size %s" % rela["entsize"])
    hits = []
    count = rela["size"] // ELF32_RELA.size
    for index in range(count):
        at = rela["offset"] + index * ELF32_RELA.size
        r_offset, r_info, r_addend = ELF32_RELA.unpack_from(data, at)
        if (r_info & 0xFF) != SLOT0_OP:
            continue
        if not (movi_at < r_offset < movi_at + 16):
            continue
        if r_addend != r_offset:
            continue
        hits.append(at)
    if len(hits) != 1:
        fail("expected one jump-to-self after the assert, found %d" % len(hits))
    jump_at = ELF32_RELA.unpack_from(data, hits[0])[0]
    if text_bytes(data, sections, jump_at, 3) != b"\x06\xff\xff":
        fail("spin is not a relocatable jump")

    patched, sym_index = insert_fail_symbol(data)
    sections = parse_sections(patched)
    rela = section(sections, ".rela.irom0.text")
    text = section(sections, ".irom0.text")
    # call0 uses the same slot relocation as the jump. Opcode 5, not 6.
    patched = bytearray(patched)
    patched[text["offset"] + jump_at] = 0x05
    found = False
    count = rela["size"] // ELF32_RELA.size
    for index in range(count):
        at = rela["offset"] + index * ELF32_RELA.size
        r_offset, r_info, r_addend = ELF32_RELA.unpack_from(patched, at)
        if r_offset == jump_at and (r_info & 0xFF) == SLOT0_OP and r_addend == r_offset:
            ELF32_RELA.pack_into(patched, at, r_offset, (sym_index << 8) | SLOT0_OP, 0)
            found = True
            break
    if not found:
        fail("could not retarget the spin after adding the symbol")
    syms = symbols(bytes(patched), parse_sections(bytes(patched)))
    names = [sym["name"] for sym in syms if sym["index"] == sym_index]
    if names != [FAIL_SYMBOL]:
        fail("symbol %d is %s" % (sym_index, names))
    return bytes(patched), jump_at, epi_at


def sdk_name(env):
    # Matches tools/platformio-build.py. This project uses the default SDK.
    chosen = "NONOSDK22x_190703"
    for item in env.get("CPPDEFINES", []):
        name = item[0] if isinstance(item, (list, tuple)) else item
        if str(name).startswith("NONOSDK"):
            chosen = str(name)
    return chosen


def sdk_libpp(env):
    framework = env.PioPlatform().get_package_dir("framework-arduinoespressif8266")
    lib = os.path.join(framework, "tools", "sdk", "lib", sdk_name(env), "libpp.a")
    if not os.path.isfile(lib):
        fail("missing %s" % lib)
    return lib


def install(env):
    toolchain = env.PioPlatform().get_package_dir("toolchain-xtensa")
    ar = os.path.join(toolchain, "bin", "xtensa-lx106-elf-ar")
    archive = sdk_libpp(env)
    out = os.path.join(env.subst("$BUILD_DIR"), "pm_patched.o")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.check_call([ar, "x", archive, "pm.o"], cwd=tmp)
        with open(os.path.join(tmp, "pm.o"), "rb") as handle:
            original = handle.read()
    patched, jump, _epi = patch_pm_object(original)
    with open(out, "wb") as handle:
        handle.write(patched)
    # Ahead of -lpp, so the linker keeps this pm.o and does not pull the original.
    env.Append(LINKFLAGS=[out])
    print("patch_pm_nullfunc: alloc failure calls %s (0x%x)" % (FAIL_SYMBOL, jump))


try:
    Import("env")
except NameError:
    env = None

if env is not None:
    install(env)
