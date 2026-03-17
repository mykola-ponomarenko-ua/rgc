from pathlib import Path

import rgc


source_data = Path("tst.dat").read_bytes()
compressed_data = rgc.compress(source_data)
Path("tst.rgc").write_bytes(compressed_data)
print(f"Compressed file size: {len(compressed_data)} bytes")

decoded_data = rgc.decompress(Path("tst.rgc").read_bytes())
if decoded_data == source_data:
    print("Decoded data is identical to the source")
