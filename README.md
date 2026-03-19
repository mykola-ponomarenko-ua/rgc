<table>
  <tr>
    <td>
      <img src="https://raw.githubusercontent.com/mykola-ponomarenko-ua/rgc/main/images/rgc_logo.png" width="384">
    </td>
    <td>
Recursive Group Coding (RGC) is an entropy coding algorithm, an alternative to Huffman and Arithmetic Coding (AC). Its key advantage is natural adaptation to the alphabet size of the input data. For large alphabets, RGC can easily deliver a better compression ratio than any other entropy coder, while still running about twice as fast as the simplest static 8-bit AC.    </td>
  </tr>
</table>

#### Tiny C library and Python wrapper for the C implementation | v0.1.2

[![PyPI version](https://img.shields.io/pypi/v/rgclib.svg)](https://pypi.org/project/rgclib/) [![PyPI - Python Version](https://img.shields.io/pypi/pyversions/rgclib.svg)](https://pypi.org/project/rgclib/) [![PyPI - Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20macOS%20%7C%20Linux-brightgreen)](https://pypi.org/project/rgclib/) [![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

The table below shows a simple example where RGC beats AC by 15% and even outperforms strong general-purpose compressors like Brotli.

### Simple benchmark (rgc_demo.c, compression of tst.dat, 1 MB)

| Method                  | Compressed size | Bits per byte | Compared to RGC | Time   |
|-------------------------|-----------------|---------------|-----------------|--------|
| **RGC**                 | **224 071 bytes** | **1.7**       | -               | 0.01 s |
| TurboRC, rcc2senc()     | 237 380 bytes   | 1.8           |  +6 % worse     | 0.03 s |
| Brotli, quality=11      | 245 358 bytes   | 1.9           | +10 % worse     | 2 s    |
| Zstd, quality=22        | 257 360 bytes   | 2.0           | +15 % worse     | 0.35 s |
| AC, 8-bit               | 261 695 bytes   | 2.0           | +17 % worse     | -      |
| Brotli, quality=9       | 280 777 bytes   | 2.1           | +25 % worse     | 0.08 s |
| WinZip                  | 284 219 bytes   | 2.2           | +27 % worse     | -      |
| WinRar                  | 308 699 bytes   | 2.4           | +38 % worse     | -      |

The file tst.dat contains quantized DCT coefficients of 32x32 image blocks and is included in the repository, so anyone can reproduce the result in a few seconds. The source image tst.png and the preparation script prep.py are also included.

Because of recursion, RGC handles 1024-byte symbols as naturally as ordinary 8-bit symbols. This is exactly the kind of data where RGC shows its full power, while AC, Huffman coding, and even modern general-purpose compressors fall far behind.

### How RGC works

The flowchart below shows the main idea of RGC. The algorithm is extremely simple, but very powerful. For full details, see the paper and source code.

<img src="https://raw.githubusercontent.com/mykola-ponomarenko-ua/rgc/main/images/rgc_flowchart.png" width="512">

1. Count frequencies of all 8-bit symbols (`0...255`) in the input data.
2. Divide the 256 symbols into no more than 16 groups with similar frequencies. Each group size is a power of two (`2, 4, 8, ...`). Because frequencies inside a group are not exactly equal, this grouping may increase the average code length by about 1-2%.
3. Split each symbol into a prefix (group ID) and a suffix (its index inside the group). As a result, the text is split into two streams: prefixes and suffixes.
4. The suffix length depends on the group size. It ranges from 1 bit for a group of 2 symbols up to 8 bits for a group of 256 symbols. For a group containing only 1 symbol, no suffix is needed. Suffixes have uniform distribution and are not compressible, so the suffix stream is written directly to the output together with the group symbol list.
5. Since the number of groups does not exceed 16, each prefix fits into 4 bits. This makes it possible to merge neighboring prefixes pairwise: `(prefix1 << 4) | prefix2`.
6. After pairwise merging of the prefix stream, we get a new text with length twice smaller than the original text. RGC then applies the same procedure recursively to this new text.

### When RGC is strong, and what are its limitations?

RGC usually provides better compression than AC when used as the final entropy coder in a compression pipeline, especially when neighboring data elements still retain some statistical dependence. For example, in compression of time stamps for traffic monitoring, the pipeline BWT + DC (Distance Coding) + RGC provides about 20% better compression than BWT + DC + AC. In JPEG compression, RGC can replace the whole compression pipeline after quantization, providing both better compression (about 5%) and higher speed.

RGC is also very simple to implement and very fast. With moderate optimization, it runs about twice as fast as a range coder.

At the same time, in its basic form RGC needs statistically homogeneous data to work efficiently. On purely random 8-bit data, RGC will usually lose to AC by about 1-2%. RGC is also not suitable for very small files (a few kilobytes), where the group symbol lists become too large relative to the compressed data.

### Installation and usage examples

The C/C++ library consists of only two files: `rgc.c` and `rgc.h`. Just copy them into your project and compile them together with your code.

`rgclib` is a Python wrapper for the C implementation of RGC.

Install from PyPI:

```bash
pip install rgclib
```

Install directly from GitHub:

```bash
pip install git+https://github.com/mykola-ponomarenko-ua/rgc.git
```

Minimal Python usage:

```python
import rgc

compressed = rgc.compress(data)
decoded = rgc.decompress(compressed)
```

Minimal usage examples for both C and Python are included in the repository and reproduce the benchmark result of `224071 bytes` for `tst.dat`.

C example:

* `examples/rgc_demo.c`

Compile and run from `examples/` on Windows with MSVC:

```bat
cl /O2 /MD rgc_demo.c
rgc_demo.exe
```

Python example:

* `examples/rgc_demo.py`

Run from `examples/`:

```bash
python rgc_demo.py
```

The repository also contains an illustrative script explaining how the test file `tst.dat` is generated from the image `tst.png`:

* `examples/prep.py`


### References

N. Ponomarenko, V. Lukin, K. Egiazarian, J. Astola, Fast recursive coding based on grouping of symbols, Telecommunications and Radio Engineering, Vol. 68, No. 20, 2009, pp. 1857-1863.

PDF in the repository:

* https://raw.githubusercontent.com/mykola-ponomarenko-ua/rgc/main/paper/recursive_group_coding_2009.pdf
