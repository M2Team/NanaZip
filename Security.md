# NanaZip Security Policy

NanaZip takes security seriously. Since NanaZip is not a full-time project,
maintained by community volunteers with promising at least one major stable
version per year, we have adopted the following development policies to
minimize potential vulnerabilities.

## Support Scope

In the current stage, NanaZip mainly cares about the vulnerability type of
running unauthorized logics like shellcode, create new process, or etc. Other
vulnerability types like crypto algorithm vulnerabilities will beyond NanaZip
development team's abilities, and it will be the upstream issues in most
scenarios.

## Support Policy

- Only the latest stable and preview are supported.
- If you met the security vulnerabilities caused by NanaZip dependencies, you
  should also to report to the upstream if you can also reproduce that in the
  upstream.
- NanaZip development team will try their best to fix the issue because NanaZip
  is a community-driven project.

## Report Policy

- Ensure the issue can be reproduced by the latest stable and/or preview of
  NanaZip. Because NanaZip introduces several security mitigations, maybe the
  issue you have found will not be reproduced.
- Prefer making a fix PR directly because we believe you may have a better
  workaround solution than us, but you need to follow the rules, which are
  mentioned in https://github.com/M2Team/NanaZip/blob/main/CONTRIBUTING.md. You
  should read that word by word first.
  - To minimize the vulnerability window, please coordinate with the NanaZip
    maintainers to publish the fixed source code, fixed release and advisory
    at the same time. This is especially important for high-severity
    vulnerabilities (e.g. path traversal or code execution).
- If you don't want to make a fix PR directly, you need to provide a Proof of
  Concept (maybe a document, or samples) for that before you using private
  vulnerability reporting feature provided by GitHub, read 
  https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing-information-about-vulnerabilities/privately-reporting-a-security-vulnerability
  for more information.
  - You should provide PoC files directly, instead of generators source code
    only, which will be easier for us to reproduce and save to test assets.
  - We highly suggest you provide the fix logic source code snippets to help us
    to fix issues correctly. Because many related issues are hard for us to
    imagine how to fix them properly. This can make you and us save much time.
- We will request CVEs for vulnerabilities introduced by our own code as part of
  our security process. We may also publish fixes, advisories and/or request
  CVEs for vulnerabilities in upstream code in the following cases:
  - If the vulnerability is not reachable in upstream (e.g. because of build
    configuration);
  - If the issue has been fixed upstream without a security advisory;
  - If a CVE or fix has not been published upstream 90 days after we've been
    notified of the vulnerability.
  - In these cases, we will provide information related to the upstream issue in
    our advisory.

## Design Policy

### Principle of least privilege

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

### AMAP strategy

AMAP a.k.a. As Microsoft As Possible.

NanaZip will use more implementations from Windows APIs, especially the
cryptographic algorithm implementations, which can reduce the attack surface
and the binary size.
