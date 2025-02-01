using Mile.Project.Helpers;
using System.Text;
using System.Xml;

namespace NanaZip.RefreshPackageVersion
{
    internal class Program
    {
        static (int Major, int Minor) Version = (5, 1);
        static string BuildStartDay = "2021-08-31";

        static string GenerateVersionString()
        {
            return string.Format(
                "{0}.{1}.{2}.0",
                Version.Major,
                Version.Minor,
                DateTime.Today.Subtract(DateTime.Parse(BuildStartDay)).TotalDays);
        }

        static string RepositoryRoot = GitRepository.GetRootPath();

        static void RefreshAppxManifestVersion()
        {
            string FilePath = string.Format(
                @"{0}\NanaZipPackage\Package.appxmanifest",
                RepositoryRoot);

            XmlDocument Document = new XmlDocument();
            Document.PreserveWhitespace = true;
            Document.Load(FilePath);

            XmlNode? PackageNode = Document["Package"];
            if (PackageNode != null)
            {
                XmlNode? IdentityNode = PackageNode["Identity"];
                if (IdentityNode != null)
                {
                    XmlAttribute? VersionAttribute =
                        IdentityNode.Attributes["Version"];
                    if (VersionAttribute != null)
                    {
                        FileUtilities.SaveTextToFileAsUtf8Bom(
                            FilePath,
                            File.ReadAllText(FilePath).Replace(
                                VersionAttribute.Value,
                                GenerateVersionString()));
                    }
                }
            }
        }

        static void ReplaceFileContentViaStringList(
            string FilePath,
            List<string> FromList,
            List<string> ToList)
        {
            if (FromList.Count != ToList.Count)
            {
                throw new ArgumentOutOfRangeException();
            }

            string Content = File.ReadAllText(FilePath, Encoding.UTF8);

            for (int Index = 0; Index < FromList.Count; ++Index)
            {
                Content = Content.Replace(FromList[Index], ToList[Index]);
            }

            if (Path.GetExtension(FilePath) == ".rc")
            {
                File.WriteAllText(FilePath, Content, Encoding.Unicode);
            }
            else
            {
                FileUtilities.SaveTextToFileAsUtf8Bom(FilePath, Content);
            }
        }

        static bool SwitchToPreview = true;

        static List<string> ReleaseStringList = new List<string>
        {
            "DisplayName=\"NanaZip\"",
            "Name=\"40174MouriNaruto.NanaZip\"",
            "<DisplayName>NanaZip</DisplayName>",
            "CAE3F1D4-7765-4D98-A060-52CD14D56EAB",
            "return ::SHStrDupW(L\"NanaZip\", ppszName);",
            "<Content Include=\"..\\Assets\\PackageAssets\\**\\*\">",
            "Assets/NanaZip.ico",
            "Assets/NanaZipSfx.ico",
        };

        static List<string> PreviewStringList = new List<string>
        {
            "DisplayName=\"NanaZip Preview\"",
            "Name=\"40174MouriNaruto.NanaZipPreview\"",
            "<DisplayName>NanaZip Preview</DisplayName>",
            "469D94E9-6AF4-4395-B396-99B1308F8CE5",
            "return ::SHStrDupW(L\"NanaZip Preview\", ppszName);",
            "<Content Include=\"..\\Assets\\PreviewPackageAssets\\**\\*\">",
            "Assets/NanaZipPreview.ico",
            "Assets/NanaZipPreviewSfx.ico",
        };

        static List<string> FileList = new List<string>
        {
            @"{0}\NanaZipPackage\Package.appxmanifest",
            @"{0}\NanaZip.UI.Modern\NanaZip.ShellExtension.cpp",
            @"{0}\NanaZipPackage\NanaZipPackage.wapproj",
            @"{0}\NanaZip.UI.Modern\SevenZip\CPP\7zip\UI\FileManager\resource.rc",
            @"{0}\NanaZip.UI.Modern\SevenZip\CPP\7zip\UI\GUI\resource.rc",
            @"{0}\NanaZip.Core\SevenZip\CPP\7zip\Bundles\SFXCon\resource.rc",
            @"{0}\NanaZip.Core\SevenZip\CPP\7zip\Bundles\SFXSetup\resource.rc",
            @"{0}\NanaZip.Core\SevenZip\CPP\7zip\Bundles\SFXWin\resource.rc",
        };

        static void SwitchDevelopmentChannel()
        {
            foreach (var FilePath in FileList)
            {
                ReplaceFileContentViaStringList(
                    string.Format(FilePath, RepositoryRoot),
                    SwitchToPreview ? ReleaseStringList : PreviewStringList,
                    SwitchToPreview ? PreviewStringList : ReleaseStringList);
            }
        }

        static void GenerateAppxManifestResourceIdentifies()
        {
            string FilePath = string.Format(
                @"{0}\NanaZipPackage\Package.appxmanifest",
                RepositoryRoot);

            string Content = File.ReadAllText(FilePath);

            string PreviousContent = string.Empty;
            {
                string StartString = "  <Resources>\r\n";
                string EndString = "  </Resources>";

                int Start = Content.IndexOf(StartString) + StartString.Length;
                int End = Content.IndexOf(EndString);

                PreviousContent = Content.Substring(Start, End - Start);
            }

            if (string.IsNullOrEmpty(PreviousContent))
            {
                return;
            }

            string NewContent = string.Empty;
            {
                DirectoryInfo Folder = new DirectoryInfo(string.Format(
                    @"{0}\NanaZipPackage\Strings",
                    RepositoryRoot));
                foreach (var item in Folder.GetDirectories())
                {
                    NewContent += string.Format(
                        "    <Resource Language=\"{0}\" />\r\n",
                        item.Name);
                }

                int[] Scales = [100, 125, 150, 200, 400];
                foreach (var Scale in Scales)
                {
                    NewContent += string.Format(
                        "    <Resource uap:Scale=\"{0}\" />\r\n",
                        Scale.ToString());
                }
            }

            FileUtilities.SaveTextToFileAsUtf8Bom(
                FilePath,
                File.ReadAllText(FilePath).Replace(
                    PreviousContent,
                    NewContent));
        }

        static void Main(string[] args)
        {
            RefreshAppxManifestVersion();

            SwitchDevelopmentChannel();

            GenerateAppxManifestResourceIdentifies();

            Console.WriteLine(
               "NanaZip.RefreshPackageVersion task has been completed.");
            Console.ReadKey();
        }
    }
}
