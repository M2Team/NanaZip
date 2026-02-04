# NanaZip Versioning

This document applies to all versions of NanaZip.

## Version Format

- Simple Version: `<Major>.<Minor> <Tag>`
  - Example: `9.0 Preview 1`
- Binary Version: `<Major>.<Minor>.<Build>.<Revision>`
  - Example: `9.0.2654.0`

## The rule for build and revision number

The build number is the number of days since August 31, 2021 because the first 
version of NanaZip is created and published on that day.

The revision number is the number of releases releases in the day corresponding
to the build number, and it counts from zero. So the first revision is 0 and 
the second revision is 1.

## Release Tags

The tag timeline for each release is:

```
"Preview {N}" (one or more) -> "Final" (one or more) -> "No Tag"
-> {"Update N Final" (one or more) -> "Update N"} (zero or more)
-> Repeat again for vNext
```

Here is the explanation for each tag:

- "Preview {N}"
  - This tag only appears in NanaZip Preview.
  - N is the preview release number, starting from 1.
  - N is also optional if there is only one preview release.
- "Final" and "Update N Final"
  - This tag only appears in NanaZip Preview.
  - The last corresponding "Final" release in NanaZip Preview has the same
    implementation as the corresponding stable release in NanaZip.
  - There will be more than one "Final" release if there are some big issues
    found before the corresponding stable release.
  - "Update N Final" is used for validating updates after the corresponding
    "No Tag" stable release.
  - "Update N Final" corresponds to the stable release "Update N".
- "No Tag" or "Update N"
  - This tag only appears in NanaZip a.k.a. the stable release.
  - This indicates the stable release of NanaZip.
  - There is only one stable release for each minor version.
  - N is the update release number, starting from 1.

Here is the example timeline:

```
6.0 Preview 1 -> 6.0 Final -> 6.0 -> 6.0 Update 1 Final -> 6.0 Update 1
-> 6.0 Update 2 Final -> 6.0 Update 2 -> 6.5 Preview -> ...
```
