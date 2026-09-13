using ImageMagick;
using Mile.DotNet.Helpers;
using System.Collections.Concurrent;

namespace NanaZip.ProjectAssetsGenerator
{
    internal class Program
    {
        static string RepositoryRoot = Git.GetRootPath();

        static void Main(string[] args)
        {
            {
                string SourcePath = RepositoryRoot + @"\Assets\OriginalAssets\NanaZip";

                string OutputPath = RepositoryRoot + @"\Assets\PackageAssets";

                ConcurrentDictionary<int, MagickImage> StandardSources =
                    new ConcurrentDictionary<int, MagickImage>();
                ConcurrentDictionary<int, MagickImage> StandardIconSources =
                    new ConcurrentDictionary<int, MagickImage>();
                ConcurrentDictionary<int, MagickImage> ContrastBlackSources =
                    new ConcurrentDictionary<int, MagickImage>();
                ConcurrentDictionary<int, MagickImage> ContrastWhiteSources =
                    new ConcurrentDictionary<int, MagickImage>();

                ConcurrentDictionary<int, MagickImage> ArchiveFileSources =
                    new ConcurrentDictionary<int, MagickImage>();

                ConcurrentDictionary<int, MagickImage> SfxStubSources =
                    new ConcurrentDictionary<int, MagickImage>();

                foreach (var AssetSize in ImageAssets.AssetSizes)
                {
                    StandardSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath,
                        "Standard",
                        AssetSize));
                    StandardIconSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath.Replace("OriginalAssets", "OriginalAssetsOptimized"),
                        "Standard",
                        AssetSize));
                    ContrastBlackSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath,
                        "ContrastBlack",
                        AssetSize));
                    ContrastWhiteSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath,
                        "ContrastWhite",
                        AssetSize));

                    ArchiveFileSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath,
                        "ArchiveFile",
                        AssetSize));

                    SfxStubSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath.Replace("OriginalAssets", "OriginalAssetsOptimized"),
                        "SelfExtractingExecutable",
                        AssetSize));
                }

                ImageAssets.GeneratePackageApplicationImageAssets(
                    StandardSources,
                    ContrastBlackSources,
                    ContrastWhiteSources,
                    OutputPath);

                ImageAssets.GeneratePackageFileAssociationImageAssets(
                    ArchiveFileSources,
                    OutputPath,
                    @"ArchiveFile");

                ImageAssets.GenerateIconFile(
                    StandardIconSources,
                    OutputPath + @"\..\NanaZip.ico");

                ImageAssets.GenerateIconFile(
                    SfxStubSources,
                    OutputPath + @"\..\NanaZipSfx.ico");

                StandardSources[64].Write(
                    OutputPath + @"\..\NanaZip.png");
            }

            {
                string SourcePath = RepositoryRoot + @"\Assets\OriginalAssets\NanaZipPreview";

                string OutputPath = RepositoryRoot + @"\Assets\PreviewPackageAssets";

                ConcurrentDictionary<int, MagickImage> StandardSources =
                    new ConcurrentDictionary<int, MagickImage>();
                ConcurrentDictionary<int, MagickImage> StandardIconSources =
                    new ConcurrentDictionary<int, MagickImage>();
                ConcurrentDictionary<int, MagickImage> ContrastBlackSources =
                    new ConcurrentDictionary<int, MagickImage>();
                ConcurrentDictionary<int, MagickImage> ContrastWhiteSources =
                    new ConcurrentDictionary<int, MagickImage>();

                ConcurrentDictionary<int, MagickImage> ArchiveFileSources =
                    new ConcurrentDictionary<int, MagickImage>();

                ConcurrentDictionary<int, MagickImage> SfxStubSources =
                    new ConcurrentDictionary<int, MagickImage>();

                foreach (var AssetSize in ImageAssets.AssetSizes)
                {
                    StandardSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath,
                        "Standard",
                        AssetSize));
                    StandardIconSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath.Replace("OriginalAssets", "OriginalAssetsOptimized"),
                        "Standard",
                        AssetSize));
                    ContrastBlackSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath,
                        "ContrastBlack",
                        AssetSize));
                    ContrastWhiteSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath,
                        "ContrastWhite",
                        AssetSize));

                    ArchiveFileSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath,
                        "ArchiveFile",
                        AssetSize));

                    SfxStubSources[AssetSize] = new MagickImage(string.Format(
                        @"{0}\{1}\{1}_{2}.png",
                        SourcePath.Replace("OriginalAssets", "OriginalAssetsOptimized"),
                        "SelfExtractingExecutable",
                        AssetSize));
                }

                ImageAssets.GeneratePackageApplicationImageAssets(
                    StandardSources,
                    ContrastBlackSources,
                    ContrastWhiteSources,
                    OutputPath);

                ImageAssets.GeneratePackageFileAssociationImageAssets(
                    ArchiveFileSources,
                    OutputPath,
                    @"ArchiveFile");

                ImageAssets.GenerateIconFile(
                    StandardIconSources,
                    OutputPath + @"\..\NanaZipPreview.ico");

                ImageAssets.GenerateIconFile(
                    SfxStubSources,
                    OutputPath + @"\..\NanaZipPreviewSfx.ico");

                StandardSources[64].Write(
                    OutputPath + @"\..\NanaZipPreview.png");
            }

            Console.WriteLine(
                "NanaZip.ProjectAssetsGenerator task has been completed.");
            Console.ReadKey();
        }
    }
}
