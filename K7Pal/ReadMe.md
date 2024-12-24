# NanaZip Platform Abstraction Layer (K7Pal)

***Work In Progress***

The static and dynamic library which wraps the platform infrastructures used in
NanaZip.

K7Pal should be considered as K7 Platform Abstraction Layer. K7 is the alias of
NanaZip. Here is the relationship between K7 and NanaZip: I had mentioned one of
the reasons that I call this project NanaZip because Nana is the romaji of なな
which means seven in Japanese. But I had not mentioned the way I confirm that: I
had recalled one of the Japanese VTubers called Kagura Nana when I waiting my
elder sister for dinner at Taiyanggong subway station, Beijing. For playing more
puns, NanaZip uses the K7 as the alias. (K -> Kagura, 7 -> Nana)

In the current development stage, the implementation of K7Pal is Windows only.
But the K7Pal interfaces are designed to be platform-independent. The K7Pal
interfaces will have the implementations for Linux and FreeBSD in the future,
although the implementation may not in this repository.

## Features used in NanaZip

- Cryptography
  - Hash Functions
    - MD2
    - MD4
    - MD5
    - SHA-1
    - SHA-256
    - SHA-384
    - SHA-512
