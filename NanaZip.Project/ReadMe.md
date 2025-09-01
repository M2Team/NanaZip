# NanaZip.Project

Some MSBuild configurations shared by whole NanaZip project.

The precompiled binaries in this folder is necessary for implementing the
out-of-box NanaZip building experience. Open the `NanaZip.MaintainerTools.sln`
at the root folder of the NanaZip source code repository, find the
`NanaZip.Build.Tasks` project, you will see the source for the precompiled
binaries which contained in this folder. Also, these precompiled binaries will
be updated automatically via the GitHub Actions workflow if the implementation
of the `NanaZip.Build.Tasks` project has some changes.
