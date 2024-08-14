using Mile.Project.Helpers;
using System.Xml;

namespace NanaZip.RefreshPackageVersion
{
    internal class Program
    {
        static (int Major, int Minor) Version = (3, 5);
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

        static void Main(string[] args)
        {
            RefreshAppxManifestVersion();

            Console.WriteLine(
               "NanaZip.RefreshPackageVersion task has been completed.");
            Console.ReadKey();
        }
    }
}
