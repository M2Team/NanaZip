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

NanaZip follows a rolling release model. Preview and stable builds share the
same codebase, differing only in build mode. The "Preview" tag corresponds to
NanaZip Preview, while no tag corresponds to NanaZip. Every stable release has
a matching Preview release, but not vice versa.

Releases use `version YYMM.N`, where `YYMM` is the release year and month and
`N` starts at 1 each month. Matching Preview and stable releases share this
identifier. For example:

- Preview: `NanaZip 7.0 Preview, version 2609.1`
- Stable: `NanaZip 7.0, version 2609.1`
