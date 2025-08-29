using Microsoft.Build.Framework;
using Microsoft.Build.Utilities;
using Mile.Project.Helpers;
using System.IO;
using System.Xml;

namespace NanaZip.Build.Tasks
{
    public class RefreshAppxManifestVersion : Task
    {
        [Required]
        public string? FilePath { get; set; }

        [Required]
        public string? Version { get; set; }

        public override bool Execute()
        {
            XmlDocument Document = new XmlDocument();
            Document.PreserveWhitespace = true;
            Document.Load(FilePath);

            XmlNode? PackageNode = Document["Package"];
            if (PackageNode == null)
            {
                Log.LogError("Cannot find Package node in the manifest.");
                return false;
            }

            XmlNode? IdentityNode = PackageNode["Identity"];
            if (IdentityNode == null)
            {
                Log.LogError("Cannot find Identity node in the manifest.");
                return false;
            }

            XmlAttribute? VersionAttribute = IdentityNode.Attributes["Version"];
            if (VersionAttribute == null)
            {
                Log.LogError("Cannot find Version attribute in the manifest.");
                return false;
            }

            FileUtilities.SaveTextToFileAsUtf8Bom(
               FilePath,
               File.ReadAllText(FilePath).Replace(
                   string.Format(
                       " {0}=\"{1}\"",
                       VersionAttribute.Name,
                       VersionAttribute.Value),
                   string.Format(
                       " {0}=\"{1}\"",
                       VersionAttribute.Name,
                       Version)));

            return true;
        }
    }
}
