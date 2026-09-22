"""One-time UAST v1 -> UAJS JSON v1 conversion. No legacy reader is added to the engine.

Default: validate and print a conversion preview. --write: back up originals under
Tools/bin/UassetJsonBackup, then atomically replace each file. Existing UAJS files
are left untouched. Run from any directory; --root can select a different asset root.
"""
import argparse
import json
from pathlib import Path
import struct

REPO = Path(__file__).resolve().parents[1]
LIMIT = 2**31 - 1


class Reader:
    def __init__(self, data):
        self.data, self.pos = data, 0

    def read(self, size):
        if size < 0 or size > len(self.data) - self.pos:
            raise ValueError("Truncated legacy asset")
        result = self.data[self.pos:self.pos + size]
        self.pos += size
        return result

    def unpack(self, fmt):
        return struct.unpack('<' + fmt, self.read(struct.calcsize('<' + fmt)))

    def u32(self):
        return self.unpack('I')[0]

    def string(self):
        return self.read(self.u32()).decode('utf-8')


def image(payload):
    if not payload.startswith(b'DDS '):
        raise ValueError("Invalid DDS payload")
    return {'Encoding': 'DDS', 'Offset': 0, 'ByteLength': len(payload)}


def encode(header, body, payload):
    def dump(value):
        return json.dumps(value, ensure_ascii=False, allow_nan=False, indent=2,
                          sort_keys=True).encode('utf-8')
    h, b = dump(header), dump(body)
    if 24 + len(h) + len(b) + len(payload) > LIMIT:
        raise ValueError("Asset exceeds engine file size limit")
    return struct.pack('<4sIQQ', b'UAJS', 1, len(h), len(b)) + h + b + payload


def convert(data):
    r = Reader(data)
    if r.read(4) != b'UAST' or r.u32() != 1:
        raise ValueError("Expected legacy UAST v1")
    kind, standalone = r.string(), r.unpack('B')[0]
    if standalone not in (0, 1):
        raise ValueError("Invalid standalone flag")
    deps = [r.string() for _ in range(r.u32())]
    header = {'AssetType': kind, 'SchemaVersion': 1,
              'Standalone': bool(standalone), 'Dependencies': deps}
    payload = b''
    if kind == 'UTexture2D':
        payload = r.read(r.unpack('Q')[0])
        body = {'Image': image(payload)}
        if deps:
            raise ValueError("Texture has dependencies")
    elif kind == 'UMaterial':
        color, texture = list(r.unpack('4f')), r.string()
        body = {'DiffuseColor': color, 'DiffuseTexture': texture or None}
        if deps != ([texture] if texture else []):
            raise ValueError("Material dependencies disagree")
    elif kind == 'UStaticMeshAsset':
        nv = r.u32()
        vertices = r.read(nv * 48)
        ni = r.u32()
        indices = r.read(ni * 4)
        bounds = r.unpack('6f')
        sections = [dict(zip(('FirstIndex', 'IndexCount', 'MaterialIndex'),
                            r.unpack('3I'))) for _ in range(r.u32())]
        materials = [r.string() for _ in range(r.u32())]
        body = {'Bounds': {'Min': list(bounds[:3]), 'Max': list(bounds[3:])},
                'Sections': sections, 'MaterialPaths': materials,
                'Geometry': {'VertexLayout': 'VertexSimpleV1', 'IndexFormat': 'UInt32',
                             'Vertices': {'Offset': 0, 'Count': nv, 'ByteLength': len(vertices)},
                             'Indices': {'Offset': len(vertices), 'Count': ni, 'ByteLength': len(indices)}}}
        payload = vertices + indices
        if len(deps) != len(set(deps)) or set(deps) != set(materials):
            raise ValueError("Mesh dependencies disagree")
    elif kind == 'UFontAtlasAsset':
        mode = r.unpack('B')[0]
        if mode == 1:
            body = {'Mode': 'MSDF', 'Metadata': json.loads(r.string())}
        elif mode == 0:
            fields = ('Columns', 'Rows', 'CharacterWidth', 'CharacterHeight', 'CharacterAdvance')
            body = {'Mode': 'Bitmap', 'BitmapSettings': dict(zip(fields, r.unpack('II3f')))}
        else:
            raise ValueError("Unknown font mode")
        payload = r.read(r.unpack('Q')[0])
        body['Image'] = image(payload)
        if deps:
            raise ValueError("Font has dependencies")
    else:
        raise ValueError(f"Unknown asset class: {kind}")
    if r.pos != len(data):
        raise ValueError("Trailing legacy data")
    result = encode(header, body, payload)
    # Independently re-read the envelope; preserve the binary bytes exactly.
    _, version, hlen, blen = struct.unpack_from('<4sIQQ', result)
    assert version == 1
    assert json.loads(result[24:24+hlen]) == header
    assert json.loads(result[24+hlen:24+hlen+blen]) == body
    assert result[24+hlen+blen:] == payload
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=REPO / 'EngineLib/Assets')
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    root = args.root.resolve()
    backup_root = REPO / 'Tools/bin/UassetJsonBackup'
    pending = []
    for path in sorted(root.rglob('*.uasset')):
        data = path.read_bytes()
        if data.startswith(b'UAJS'):
            continue
        converted = convert(data)
        backup = backup_root / path.relative_to(root)
        if args.write and backup.exists() and backup.read_bytes() != data:
            raise RuntimeError(f"Different backup already exists: {backup}")
        pending.append((path, backup, data, converted))
        print(path.relative_to(root))
    # Every source is parsed before any file is replaced. Retain backups even on success.
    if args.write:
        for path, backup, data, converted in pending:
            if path.read_bytes() != data:
                raise RuntimeError(f"Source changed during conversion: {path}")
            backup.parent.mkdir(parents=True, exist_ok=True)
            if not backup.exists():
                backup.write_bytes(data)
            temporary = path.with_name(path.name + '.json-migration.tmp')
            with temporary.open('xb') as stream:
                stream.write(converted)
            temporary.replace(path)
    print(f"{'Converted' if args.write else 'Validated'} {len(pending)} files; "
          f"backup directory: {backup_root}" if args.write else f"Validated {len(pending)} files; use --write to convert")


if __name__ == '__main__':
    main()
