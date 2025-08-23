# NanaZip Security Policy

NanaZip takes security seriously. Since NanaZip is not a full-time project,
maintained by community volunteers with promising at least one major stable
version per year, we have adopted the following development policies to
minimize potential vulnerabilities.

## Principle of least privilege

Read https://en.wikipedia.org/wiki/Principle_of_least_privilege if you don't
know what is the principle of least privilege.

NanaZip follows the principle of least privilege with considering the user
requirements and experience. So, here are some policies we are using.

- Enable several security mitigations to reduce the attack surface.
  - Disable dynamic code generation in Release builds for scenarios without
    isolations prevents generating malicious code at runtime.
  - Disable child process creation if the component doesn't need that. For
    example, NanaZip CLI and Self Extracting Executables. (Except installer mode
    of Self Extracting Executables, which compiled binaries is not provided in
    the NanaZip MSIX package.)
- Prefer the isolation mechanisms without exposing native platform details like
  WebAssembly for hosting untrusted third-party contents like external plugins.

## AMAP strategy

AMAP a.k.a. As Microsoft As Possible.

NanaZip will use more implementations from Windows APIs, especially the
cryptographic algorithm implementations, which can reduce the attack surface
and the binary size.
