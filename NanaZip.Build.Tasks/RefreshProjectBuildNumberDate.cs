using Microsoft.Build.Framework;
using Microsoft.Build.Utilities;
using Mile.Project.Helpers;
using System.IO;
using System.Xml;

namespace NanaZip.Build.Tasks
{
    public class RefreshProjectBuildNumberDate : Task
    {
        [Required]
        public string? FilePath { get; set; }

        [Required]
        public string? BuildNumberDate { get; set; }

        public override bool Execute()
        {
            XmlDocument Document = new XmlDocument();
            Document.PreserveWhitespace = true;
            Document.Load(FilePath);

            XmlNode? ProjectNode = Document["Project"];
            if (ProjectNode == null)
            {
                Log.LogError(
                    "Cannot find Project node in the manifest.");
                return false;
            }

            XmlNode? PropertyGroupNode = ProjectNode["PropertyGroup"];
            if (PropertyGroupNode == null)
            {
                Log.LogError(
                    "Cannot find PropertyGroup node in the manifest.");
                return false;
            }

            XmlNode? NanaZipBuildNumberDateNode =
                PropertyGroupNode["NanaZipBuildNumberDate"];
            if (NanaZipBuildNumberDateNode == null)
            {
                Log.LogError(
                    "Cannot find NanaZipBuildNumberDate node in the manifest.");
                return false;
            }

            FileUtilities.SaveTextToFileAsUtf8Bom(
                FilePath,
                File.ReadAllText(FilePath).Replace(
                    string.Format(
                        "<{0}>{1}</{0}>",
                        NanaZipBuildNumberDateNode.Name,
                        NanaZipBuildNumberDateNode.InnerText),
                    string.Format(
                        "<{0}>{1}</{0}>",
                        NanaZipBuildNumberDateNode.Name,
                        BuildNumberDate)));

            return true;
        }
    }
}
