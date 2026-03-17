import rgc


def test_roundtrip():
    source = bytes((37 * i + 11) % 256 for i in range(4096))
    compressed = rgc.compress(source)
    decoded = rgc.decompress(compressed)
    assert decoded == source
